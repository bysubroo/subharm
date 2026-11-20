#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <unordered_map>

namespace subharm
{

static constexpr double kMaxRecordSeconds = 300.0;   // 5 minutes per take

// =====================================================================================
void Take::computePeaks()
{
    auto make = [] (const std::vector<float>& x, std::vector<float>& p)
    {
        p.assign ((x.size() + kPeakBlock - 1) / kPeakBlock, 0.0f);
        for (size_t i = 0; i < x.size(); ++i)
            p[i / kPeakBlock] = std::max (p[i / kPeakBlock], std::abs (x[i]));
    };
    make (vocal, vocalPeaks);
    make (beat, beatPeaks);
}

juce::String StackEntry::label() const
{
    juce::StringArray names;
    for (auto t : kLineDisplayOrder)
        if (hasLine (settings.lines, t)) names.add (lineName (t));
    return names.joinIntoString (juce::String::fromUTF8 (" · "));
}

juce::StringArray SubHarmProcessor::keyChoices()
{
    juce::StringArray k { "Auto (detect)" };
    for (int t = 0; t < 12; ++t)
    {
        k.add (pitchClassName (t) + " major");
        k.add (pitchClassName (t) + " minor");
    }
    return k;
}

bool SubHarmProcessor::forcedKey (Key& out) const
{
    const int c = (int) apvts.getRawParameterValue ("key")->load();
    if (c <= 0) return false;
    out = { (c - 1) / 2, ((c - 1) % 2) == 1 };
    return true;
}

// =====================================================================================
juce::AudioProcessorValueTreeState::ParameterLayout SubHarmProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "stackLevel", 1 }, "Stack level",
                                                  NormalisableRange<float> (-30.0f, 6.0f, 0.1f), 0.0f,
                                                  AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "sauce", 1 }, "Sauce",
                                                  NormalisableRange<float> (0.0f, 100.0f, 1.0f), 27.0f,
                                                  AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "leadThru", 1 }, "Lead through", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key", keyChoices(), 0));
    const auto defaults = presetLines (StackSize::Tall);   // UNIS + 3RD UP + OCT UP + OCT DOWN
    for (int i = 0; i < kNumLineTypes; ++i)
        l.add (std::make_unique<AudioParameterBool> (ParameterID { "line" + String (i), 1 }, lineName ((LineType) i),
                                                     hasLine (defaults, (LineType) i)));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "flavour", 1 }, "Flavour", StringArray { "R&B", "Pop", "Clean", "Gospel" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "humanize", 1 }, "Humanize", StringArray { "Tight", "Natural", "Loose" }, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "crew", 1 }, "Voices", StringArray { "Your voice", "Wide crew", "Deeper crew", "Lighter crew" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "delivery", 1 }, "Delivery", StringArray { "Natural", "Airy", "Gentle", "Full voice", "Energetic" }, 0));
    return l;
}

SubHarmProcessor::SubHarmProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Side chain", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SubHarm", createLayout())
{
    stackLevelParam = apvts.getRawParameterValue ("stackLevel");
    sauceParam = apvts.getRawParameterValue ("sauce");
    leadThruParam = apvts.getRawParameterValue ("leadThru");
    sessionId = juce::Uuid().toDashedString();
    startTimerHz (30);
}

SubHarmProcessor::~SubHarmProcessor()
{
    stopTimer();
    pool.removeAllJobs (true, 10000);
}

bool SubHarmProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (in != out) return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto& sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void SubHarmProcessor::prepareToPlay (double, int) {}

// =====================================================================================
// Audio thread
// =====================================================================================
void SubHarmProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    auto mainIn = getBusBuffer (buffer, true, 0);
    const int mainCh = mainIn.getNumChannels();

    juce::AudioBuffer<float> side;
    bool haveSide = getBusCount (true) > 1 && getBus (true, 1)->isEnabled();
    if (haveSide)
    {
        side = getBusBuffer (buffer, true, 1);
        haveSide = side.getNumChannels() > 0;
    }
    sidechainConnected = haveSide;

    // side chain level for the LED
    float scRms = 0.0f;
    if (haveSide)
        for (int c = 0; c < side.getNumChannels(); ++c)
            scRms = std::max (scRms, side.getRMSLevel (c, 0, numSamples));
    sidechainLevel = std::max (scRms, sidechainLevel.load() * 0.9f);

    // transport
    bool playing = false;
    juce::int64 t = 0;
    double ppq = 0.0, bpm = 0.0;
    int beatsPerBar = 4;
    bool hostTransport = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            hostTransport = true;
            playing = pos->getIsPlaying();
            t = pos->getTimeInSamples().orFallback (0);
            ppq = pos->getPpqPosition().orFallback (0.0);
            bpm = pos->getBpm().orFallback (0.0);
            if (auto ts = pos->getTimeSignature()) beatsPerBar = ts->numerator;
        }
    if (! hostTransport)
    {
        // Standalone app (no DAW transport): an internal clock runs while recording
        const auto st = (RecState) recState.load();
        playing = st == RecState::Armed || st == RecState::Recording;
        t = internalTime;
        internalTime += numSamples;
    }
    hostPlaying = playing;
    hostTime = t;

    // ---- record ----
    const auto rs = (RecState) recState.load();
    if (rs == RecState::Armed && playing)
    {
        recStartSample = t; recStartPpq = ppq; recBpm = bpm; recBeatsPerBar = beatsPerBar;
        recPos = 0;
        recState = (int) RecState::Recording;
    }
    else if (rs == RecState::Recording)
    {
        const bool jumped = lastBlockEnd >= 0 && std::llabs (t - lastBlockEnd) > 64;   // loop / locate
        if (! playing || jumped)
            recState = (int) RecState::Finished;
    }

    if ((RecState) recState.load() == RecState::Recording)
    {
        int pos = recPos.load();
        const int cap = (int) recVocal.size();
        const int todo = std::min (numSamples, cap - pos);
        for (int i = 0; i < todo; ++i)
        {
            float v = 0.0f;
            for (int c = 0; c < mainCh; ++c) v += mainIn.getSample (c, i);
            recVocal[(size_t) (pos + i)] = v / (float) juce::jmax (1, mainCh);
            float b = 0.0f;
            if (haveSide)
            {
                for (int c = 0; c < side.getNumChannels(); ++c) b += side.getSample (c, i);
                b /= (float) side.getNumChannels();
            }
            recBeat[(size_t) (pos + i)] = b;
        }
        recPos = pos + todo;
        if (pos + todo >= cap) recState = (int) RecState::Finished;
    }
    lastBlockEnd = t + numSamples;

    // ---- lead through (or not) ----
    if (leadThruParam->load() < 0.5f)
        for (int c = 0; c < getTotalNumOutputChannels(); ++c)
            buffer.clear (c, 0, numSamples);
    for (int c = getMainBusNumOutputChannels(); c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, numSamples);   // never leak the side chain to the output

    // ---- play stacks locked to the timeline ----
    stackOutLevel = stackOutLevel.load() * 0.85f;
    if (! playing) return;
    const juce::SpinLock::ScopedTryLockType lock (playLock);
    if (! lock.isLocked() || playback == nullptr || playback->items.empty()) return;

    const auto& set = *playback;
    const double ratio = set.sampleRate / juce::jmax (1.0, getSampleRate());
    const float gain = juce::Decibels::decibelsToGain (stackLevelParam->load(), -60.0f);
    const int outCh = getMainBusNumOutputChannels();
    float* outL = buffer.getWritePointer (0);
    float* outR = outCh > 1 ? buffer.getWritePointer (1) : nullptr;

    float peak = 0.0f;
    for (const auto& item : set.items)
    {
        const juce::int64 len = (juce::int64) item.L.size();
        for (int i = 0; i < numSamples; ++i)
        {
            const double posInTake = (double) (t + i - set.takeHostStart) * ratio;
            const double idx = posInTake - (double) item.startInTake;
            if (idx < 0.0 || idx >= (double) (len - 1)) continue;
            const auto i0 = (size_t) idx;
            const float fr = (float) (idx - (double) i0);
            const float l = item.L[i0] + fr * (item.L[i0 + 1] - item.L[i0]);
            const float r = item.R[i0] + fr * (item.R[i0 + 1] - item.R[i0]);
            if (outR) { outL[i] += l * gain; outR[i] += r * gain; }
            else outL[i] += 0.5f * (l + r) * gain;
            peak = std::max (peak, std::max (std::abs (l), std::abs (r)) * gain);
        }
    }
    stackOutLevel = std::max (stackOutLevel.load(), peak);
}

// =====================================================================================
// Recording
// =====================================================================================
void SubHarmProcessor::toggleRecord()
{
    auto rs = getRecState();
    if (rs == RecState::Idle)
    {
        const double sr = juce::jmax (8000.0, getSampleRate());
        const auto cap = (size_t) (sr * kMaxRecordSeconds);
        if (recVocal.size() != cap) { recVocal.assign (cap, 0.0f); recBeat.assign (cap, 0.0f); }
        recPos = 0;
        lastBlockEnd = -1;
        recState = (int) RecState::Armed;
    }
    else if (rs == RecState::Armed)
        recState = (int) RecState::Idle;
    else if (rs == RecState::Recording)
        recState = (int) RecState::Finished;
    sendChangeMessage();
}

void SubHarmProcessor::resetSong()
{
    pool.removeAllJobs (true, 5000);
    {
        const juce::SpinLock::ScopedLockType l (playLock);
        playback.reset();
    }
    parts.clear();
    stacks.clear();
    lastSeenState.clear();
    selectedPart = -1;
    {
        std::lock_guard<std::mutex> g (cacheMutex);
        leadCache.clear(); leadCacheOrder.clear();
    }
    sessionId = juce::Uuid().toDashedString();
}

void SubHarmProcessor::finaliseRecording()
{
    const int n = recPos.load();
    recState = (int) RecState::Idle;
    if (n < (int) (getSampleRate() * 0.5))   // ignore accidental sub-second takes
    {
        sendChangeMessage();
        return;
    }

    resetSong();
    take = Take();
    take.sampleRate = getSampleRate();
    take.hostStartSample = recStartSample.load();
    take.hostStartPpq = recStartPpq.load();
    take.bpm = recBpm.load();
    take.beatsPerBar = recBeatsPerBar.load();
    take.vocal.assign (recVocal.begin(), recVocal.begin() + n);
    take.beat.assign (recBeat.begin(), recBeat.begin() + n);

    double sumSq = 0;
    for (float b : take.beat) sumSq += (double) b * b;
    take.hasSidechain = std::sqrt (sumSq / n) > 1.0e-4;
    take.computePeaks();

    // free the big record buffers
    std::vector<float>().swap (recVocal);
    std::vector<float>().swap (recBeat);

    saveTakeAudio();
    startTakeAnalysis();
    sendChangeMessage();
}

void SubHarmProcessor::startTakeAnalysis()
{
    analysing = true;
    analysisDone = false;
    auto vocal = take.vocal; auto beat = take.beat;
    const double sr = take.sampleRate, bpm = take.bpm, ppq = take.hostStartPpq;
    const bool hasSide = take.hasSidechain;
    Key fk;
    const bool forced = forcedKey (fk);
    take.keyForced = forced;
    lastKeyChoice = (int) apvts.getRawParameterValue ("key")->load();

    pool.addJob ([this, vocal = std::move (vocal), beat = std::move (beat), sr, bpm, ppq, hasSide, forced, fk]
    {
        // seconds from the take start to the first beat line
        double firstBeat = 0.0;
        if (bpm > 0.0)
        {
            double frac = ppq - std::floor (ppq);
            firstBeat = (frac < 1.0e-6 ? 0.0 : 1.0 - frac) * 60.0 / bpm;
        }
        ChordAnalysisResult r;
        if (hasSide)
            r = analyseChords (beat.data(), (int) beat.size(), sr, bpm, firstBeat, forced ? &fk : nullptr);
        else
        {
            // no chords reachable: key from the vocal itself, harmonies move diatonically
            auto v = analyseChords (vocal.data(), (int) vocal.size(), sr, bpm, firstBeat, forced ? &fk : nullptr);
            r.key = forced ? fk : v.key;
            r.chords.push_back ({ 0.0, vocal.size() / sr, Chord {} });
        }
        pendingAnalysis = std::move (r);
        analysisDone = true;
    });
}

// =====================================================================================
// Chords editing
// =====================================================================================
void SubHarmProcessor::setChord (int i, Chord c)
{
    if (i < 0 || i >= (int) take.chords.size()) return;
    take.chords[(size_t) i].chord = c;
    sendChangeMessage();
}

void SubHarmProcessor::splitChord (int i, double at)
{
    if (i < 0 || i >= (int) take.chords.size()) return;
    auto seg = take.chords[(size_t) i];
    if (at <= seg.start + 0.05 || at >= seg.end - 0.05) return;
    auto second = seg; second.start = at;
    take.chords[(size_t) i].end = at;
    take.chords.insert (take.chords.begin() + i + 1, second);
    sendChangeMessage();
}

void SubHarmProcessor::mergeChordWithNext (int i)
{
    if (i < 0 || i + 1 >= (int) take.chords.size()) return;
    take.chords[(size_t) i].end = take.chords[(size_t) i + 1].end;
    take.chords.erase (take.chords.begin() + i + 1);
    sendChangeMessage();
}

// =====================================================================================
// Parts + stacks
// =====================================================================================
int SubHarmProcessor::addPart (double s, double e)
{
    if (take.isEmpty()) return -1;
    s = juce::jlimit (0.0, take.lengthSeconds(), s);
    e = juce::jlimit (0.0, take.lengthSeconds(), e);
    if (e - s < 0.25) return -1;
    Part p;
    p.id = nextPartId++;
    p.start = s; p.end = e;
    p.colourIndex = (p.id - 1) % 8;
    parts.push_back (p);
    selectedPart = p.id;
    sendChangeMessage();
    return p.id;
}

void SubHarmProcessor::setPartBounds (int id, double s, double e)
{
    for (auto& p : parts)
        if (p.id == id)
        {
            p.start = juce::jlimit (0.0, take.lengthSeconds(), std::min (s, e));
            p.end = juce::jlimit (0.0, take.lengthSeconds(), std::max (s, e));
        }
    sendChangeMessage();
}

void SubHarmProcessor::removePart (int id)
{
    parts.erase (std::remove_if (parts.begin(), parts.end(), [id] (const Part& p) { return p.id == id; }), parts.end());
    if (selectedPart == id) selectedPart = -1;
    sendChangeMessage();
}

StackSettings SubHarmProcessor::currentSettings() const
{
    StackSettings s;
    s.lines = 0;
    for (int i = 0; i < kNumLineTypes; ++i)
        if (apvts.getRawParameterValue ("line" + juce::String (i))->load() > 0.5f) s.lines |= 1u << i;
    s.flavour = (Flavour) (int) apvts.getRawParameterValue ("flavour")->load();
    s.humanize = (Humanize) (int) apvts.getRawParameterValue ("humanize")->load();
    s.crew = (Crew) (int) apvts.getRawParameterValue ("crew")->load();
    s.delivery = (Delivery) (int) apvts.getRawParameterValue ("delivery")->load();
    s.seed = (uint32_t) juce::Random::getSystemRandom().nextInt (1 << 30);
    return s;
}

void SubHarmProcessor::applyLinePreset (StackSize preset)
{
    const auto m = presetLines (preset);
    for (int i = 0; i < kNumLineTypes; ++i)
        if (auto* p = apvts.getParameter ("line" + juce::String (i)))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (hasLine (m, (LineType) i) ? 1.0f : 0.0f);
            p->endChangeGesture();
        }
}

int SubHarmProcessor::numPendingParts() const
{
    int n = 0;
    for (const auto& p : parts)
    {
        bool has = false;
        for (const auto& s : stacks) has |= s->partId == p.id;
        n += has ? 0 : 1;
    }
    return n;
}

void SubHarmProcessor::stackHarmonies()
{
    if (take.isEmpty() || ! take.analysed) return;
    if (currentSettings().lines == 0) return;   // no lines ticked
    std::vector<int> targets;
    for (const auto& p : parts)
    {
        bool has = false;
        for (const auto& s : stacks) has |= s->partId == p.id;
        if (! has) targets.push_back (p.id);
    }
    if (targets.empty() && selectedPart > 0)
        targets.push_back (selectedPart);   // re-stack the selected part with the current settings

    for (int pid : targets)
    {
        auto e = std::make_shared<StackEntry>();
        e->id = nextStackId++;
        e->partId = pid;
        e->settings = currentSettings();
        stacks.push_back (e);
        launchRender (e);
    }
    sendChangeMessage();
}

void SubHarmProcessor::launchRender (std::shared_ptr<StackEntry> e)
{
    const Part* part = nullptr;
    for (const auto& p : parts) if (p.id == e->partId) part = &p;
    if (part == nullptr) { e->state = StackEntry::Failed; return; }

    const double sr = take.sampleRate;
    const auto s0 = (size_t) juce::jlimit (0.0, (double) take.vocal.size(), part->start * sr);
    const auto s1 = (size_t) juce::jlimit ((double) s0, (double) take.vocal.size(), part->end * sr);
    e->partStart = s0 / sr;

    std::vector<float> audio (take.vocal.begin() + (long) s0, take.vocal.begin() + (long) s1);
    auto chords = take.chords;
    auto key = take.key;
    const double offset = e->partStart;
    const juce::String cacheKey = juce::String ((juce::int64) s0) + ":" + juce::String ((juce::int64) s1);

    pool.addJob ([this, e, audio = std::move (audio), chords = std::move (chords), key, offset, sr, cacheKey]
    {
        e->state = StackEntry::Rendering;
        std::shared_ptr<LeadAnalysis> la;
        {
            std::lock_guard<std::mutex> g (cacheMutex);
            auto it = leadCache.find (cacheKey);
            if (it != leadCache.end()) la = it->second;
        }
        if (la == nullptr)
        {
            la = analyseLead (audio.data(), (int) audio.size(), (int) sr,
                              [e] (float p) { e->progress = 0.4f * p; return true; });
            if (la != nullptr)
            {
                std::lock_guard<std::mutex> g (cacheMutex);
                leadCache[cacheKey] = la;
                leadCacheOrder.push_back (cacheKey);
                while (leadCacheOrder.size() > 3)
                {
                    leadCache.erase (leadCacheOrder.front());
                    leadCacheOrder.erase (leadCacheOrder.begin());
                }
            }
        }
        auto result = std::make_shared<RenderedStack>();
        bool ok = la != nullptr && renderStack (*la, chords, offset, key, e->settings, *result,
                                                [e] (float p) { e->progress = 0.4f + 0.6f * p; return true; });
        {
            const juce::ScopedLock sl (resultLock);
            if (ok) e->result = result;
        }
        e->progress = 1.0f;
        e->state = ok ? StackEntry::Done : StackEntry::Failed;
    });
}

void SubHarmProcessor::setStackEnabled (int id, bool en)
{
    for (auto& s : stacks) if (s->id == id) s->enabled = en;
    rebuildPlayback();
    sendChangeMessage();
}

void SubHarmProcessor::removeStack (int id)
{
    stacks.erase (std::remove_if (stacks.begin(), stacks.end(), [id] (const auto& s) { return s->id == id; }), stacks.end());
    rebuildPlayback();
    sendChangeMessage();
}

std::shared_ptr<RenderedStack> SubHarmProcessor::getResult (const StackEntry& e) const
{
    const juce::ScopedLock sl (resultLock);
    return e.result;
}

double SubHarmProcessor::getPlayheadInTake() const
{
    if (take.isEmpty()) return -1.0;
    const double sr = juce::jmax (1.0, getSampleRate());
    return (double) (hostTime.load() - take.hostStartSample) / sr;
}

// =====================================================================================
// Playback set
// =====================================================================================
void SubHarmProcessor::rebuildPlayback()
{
    auto set = std::make_shared<PlaybackSet>();
    set->sampleRate = take.sampleRate;
    set->takeHostStart = take.hostStartSample;
    const float sauce = sauceParam->load() / 100.0f;
    lastSauce = sauce;

    for (const auto& s : stacks)
    {
        if (! s->enabled || s->state.load() != StackEntry::Done) continue;
        auto r = getResult (*s);
        if (r == nullptr) continue;
        PlaybackSet::Item item;
        item.startInTake = (juce::int64) std::llround (s->partStart * take.sampleRate);
        mixStack (*r, sauce, item.L, item.R);
        set->items.push_back (std::move (item));
    }

    std::shared_ptr<PlaybackSet> old;
    {
        const juce::SpinLock::ScopedLockType l (playLock);
        old = std::move (playback);
        playback = std::move (set);
    }
    // 'old' is released here, on the message thread, never on the audio thread
}

// =====================================================================================
// Timer: finish recordings, collect analysis + renders
// =====================================================================================
void SubHarmProcessor::timerCallback()
{
    bool changed = false;

    if (getRecState() == RecState::Finished)
        finaliseRecording();

    // Key changed after recording: read the chords again inside the new key
    const int keyChoice = (int) apvts.getRawParameterValue ("key")->load();
    if (keyChoice != lastKeyChoice && ! take.isEmpty() && ! analysing.load())
    {
        startTakeAnalysis();
        changed = true;
    }
    else if (take.isEmpty())
        lastKeyChoice = keyChoice;

    if (analysisDone.exchange (false))
    {
        take.chords = pendingAnalysis.chords;
        take.key = pendingAnalysis.key;
        take.analysed = true;
        analysing = false;
        changed = true;
    }

    bool needRebuild = false;
    for (const auto& s : stacks)
    {
        const int st = s->state.load();
        auto it = lastSeenState.find (s->id);
        if (it == lastSeenState.end() || it->second != st)
        {
            lastSeenState[s->id] = st;
            if (st == StackEntry::Done) { saveStackAudio (*s); needRebuild = true; }
            changed = true;
        }
        if (st == StackEntry::Rendering) changed = true;   // progress bars
    }

    const float sauce = sauceParam->load() / 100.0f;
    if (std::abs (sauce - lastSauce) > 0.001f) sauceDirtyTicks = 5;   // debounce knob moves
    if (sauceDirtyTicks > 0 && --sauceDirtyTicks == 0) needRebuild = true;

    if (needRebuild) rebuildPlayback();
    if (changed) sendChangeMessage();
}

// =====================================================================================
// Files: session folder, WAVs, export
// =====================================================================================
juce::File SubHarmProcessor::getSessionFolder() const
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("SubHarm").getChildFile ("Sessions").getChildFile (sessionId);
}

bool SubHarmProcessor::writeWav (const juce::File& f, const std::vector<float>* L, const std::vector<float>* R,
                                 int numChannels, juce::int64 timeRef)
{
    f.getParentDirectory().createDirectory();
    f.deleteFile();
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
    if (os == nullptr) return false;

    juce::WavAudioFormat wav;
    auto meta = juce::WavAudioFormat::createBWAVMetadata ("SubHarm stack", "SubHarm", "", juce::Time::getCurrentTime(),
                                                          timeRef, "");
    std::unordered_map<juce::String, juce::String> metaMap;
    for (auto& k : meta.getAllKeys()) metaMap[k] = meta[k];
    auto opts = juce::AudioFormatWriterOptions{}.withSampleRate (take.sampleRate)
                                                .withNumChannels (numChannels)
                                                .withBitsPerSample (24)
                                                .withMetadataValues (metaMap);
    auto w = wav.createWriterFor (os, opts);
    if (w == nullptr) return false;

    const size_t n = L ? L->size() : 0;
    juce::AudioBuffer<float> buf (numChannels, (int) juce::jmin<size_t> (n, 65536));
    for (size_t pos = 0; pos < n; pos += (size_t) buf.getNumSamples())
    {
        const int todo = (int) std::min<size_t> ((size_t) buf.getNumSamples(), n - pos);
        buf.copyFrom (0, 0, L->data() + pos, todo);
        if (numChannels > 1) buf.copyFrom (1, 0, (R ? R : L)->data() + pos, todo);
        w->writeFromAudioSampleBuffer (buf, 0, todo);
    }
    return true;
}

void SubHarmProcessor::saveTakeAudio()
{
    auto dir = getSessionFolder();
    writeWav (dir.getChildFile ("vocal.wav"), &take.vocal, nullptr, 1, take.hostStartSample);
    writeWav (dir.getChildFile ("beat.wav"), &take.beat, nullptr, 1, take.hostStartSample);
}

void SubHarmProcessor::saveStackAudio (const StackEntry& e)
{
    auto r = getResult (e);
    if (r == nullptr) return;
    auto dir = getSessionFolder().getChildFile ("stacks");
    for (size_t v = 0; v < r->voices.size(); ++v)
        writeWav (dir.getChildFile ("stack" + juce::String (e.id) + "_v" + juce::String ((int) v) + ".wav"),
                  &r->voices[v].audio, nullptr, 1, 0);
}

void SubHarmProcessor::buildExportBuffers (bool stems, std::vector<std::vector<float>>& chans,
                                           std::vector<juce::String>& names, int& chPerFile)
{
    // Every exported file starts at the take start, so it lines up when dropped
    // on the bar where you started recording (BWF time stamp carries the position too).
    const size_t len = take.vocal.size();
    chans.clear(); names.clear();
    const float sauce = sauceParam->load() / 100.0f;

    if (! stems)
    {
        chPerFile = 2;
        std::vector<float> L (len, 0.0f), R (len, 0.0f);
        for (const auto& s : stacks)
        {
            if (! s->enabled || s->state.load() != StackEntry::Done) continue;
            auto r = getResult (*s); if (r == nullptr) continue;
            std::vector<float> l, rr;
            mixStack (*r, sauce, l, rr);
            const auto off = (size_t) std::llround (s->partStart * take.sampleRate);
            for (size_t i = 0; i < l.size() && off + i < len; ++i) { L[off + i] += l[i]; R[off + i] += rr[i]; }
        }
        chans.push_back (std::move (L)); chans.push_back (std::move (R));
        names.push_back ("SubHarm MIX");
        return;
    }

    // STEMS: one dry mono track per line + side ("3RD UP L"), all parts of that line summed
    chPerFile = 1;
    for (auto lineType : kLineDisplayOrder)
        for (int side : { -1, 1 })
        {
            std::vector<float> m;
            for (const auto& s : stacks)
            {
                if (! s->enabled || s->state.load() != StackEntry::Done) continue;
                auto r = getResult (*s); if (r == nullptr) continue;
                for (const auto& voice : r->voices)
                {
                    if (voice.type != lineType || voice.side != side) continue;
                    if (m.empty()) m.assign (len, 0.0f);
                    const auto off = (size_t) std::llround (s->partStart * take.sampleRate);
                    for (size_t i = 0; i < voice.audio.size() && off + i < len; ++i) m[off + i] += voice.audio[i] * voice.gain;
                }
            }
            if (m.empty()) continue;
            chans.push_back (std::move (m));
            names.push_back (juce::String ("SubHarm ") + lineName (lineType) + (side < 0 ? " L" : " R"));
        }
}

juce::StringArray SubHarmProcessor::exportForDrag (bool stems)
{
    juce::StringArray files;
    auto dir = getSessionFolder().getChildFile ("export");
    std::vector<std::vector<float>> chans; std::vector<juce::String> names; int cpf = 2;
    buildExportBuffers (stems, chans, names, cpf);
    for (size_t i = 0; i < names.size(); ++i)
    {
        auto f = dir.getChildFile (names[i] + ".wav");
        const auto* L = &chans[i * (size_t) cpf];
        const auto* R = cpf > 1 ? &chans[i * (size_t) cpf + 1] : nullptr;
        if (writeWav (f, L, R, cpf, take.hostStartSample)) files.add (f.getFullPathName());
    }
    return files;
}

bool SubHarmProcessor::exportToFolder (const juce::File& folder, bool stems)
{
    auto files = exportForDrag (stems);
    bool ok = ! files.isEmpty();
    for (auto& path : files)
        ok &= juce::File (path).copyFileTo (folder.getChildFile (juce::File (path).getFileName()));
    return ok;
}

// =====================================================================================
// State: song model in XML, audio in the session folder
// =====================================================================================
void SubHarmProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    auto* song = xml->createNewChildElement ("Song");
    song->setAttribute ("session", sessionId);
    song->setAttribute ("sampleRate", take.sampleRate);
    song->setAttribute ("hostStart", juce::String (take.hostStartSample));
    song->setAttribute ("hostStartPpq", take.hostStartPpq);
    song->setAttribute ("bpm", take.bpm);
    song->setAttribute ("beatsPerBar", take.beatsPerBar);
    song->setAttribute ("length", (int) take.vocal.size());
    song->setAttribute ("hasSidechain", take.hasSidechain);
    song->setAttribute ("analysed", take.analysed);
    song->setAttribute ("keyForced", take.keyForced);
    song->setAttribute ("keyTonic", take.key.tonic);
    song->setAttribute ("keyMinor", take.key.minor);
    song->setAttribute ("nextPart", nextPartId);
    song->setAttribute ("nextStack", nextStackId);
    song->setAttribute ("selectedPart", selectedPart);

    for (const auto& c : take.chords)
    {
        auto* e = song->createNewChildElement ("Chord");
        e->setAttribute ("s", c.start); e->setAttribute ("e", c.end);
        e->setAttribute ("root", c.chord.root); e->setAttribute ("q", (int) c.chord.quality);
    }
    for (const auto& p : parts)
    {
        auto* e = song->createNewChildElement ("Part");
        e->setAttribute ("id", p.id); e->setAttribute ("s", p.start); e->setAttribute ("e", p.end);
        e->setAttribute ("colour", p.colourIndex);
    }
    for (const auto& s : stacks)
    {
        if (s->state.load() != StackEntry::Done) continue;   // unfinished renders are not saved
        auto r = getResult (*s); if (r == nullptr) continue;
        auto* e = song->createNewChildElement ("Stack");
        e->setAttribute ("id", s->id); e->setAttribute ("part", s->partId);
        e->setAttribute ("partStart", s->partStart); e->setAttribute ("enabled", s->enabled);
        e->setAttribute ("lines", (int) s->settings.lines); e->setAttribute ("flavour", (int) s->settings.flavour);
        e->setAttribute ("humanize", (int) s->settings.humanize); e->setAttribute ("crew", (int) s->settings.crew);
        e->setAttribute ("delivery", (int) s->settings.delivery); e->setAttribute ("seed", (int) s->settings.seed);
        for (const auto& v : r->voices)
        {
            auto* ve = e->createNewChildElement ("Voice");
            ve->setAttribute ("pan", v.pan); ve->setAttribute ("gain", v.gain);
            ve->setAttribute ("type", (int) v.type); ve->setAttribute ("side", v.side);
        }
    }
    copyXmlToBinary (*xml, dest);
}

static bool readMonoWav (const juce::File& f, std::vector<float>& out)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> r (wav.createReaderFor (f.createInputStream().release(), true));
    if (r == nullptr) return false;
    juce::AudioBuffer<float> b (1, (int) r->lengthInSamples);
    r->read (&b, 0, (int) r->lengthInSamples, 0, true, false);
    out.assign (b.getReadPointer (0), b.getReadPointer (0) + b.getNumSamples());
    return true;
}

void SubHarmProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr) return;

    auto* song = xml->getChildByName ("Song");
    if (song != nullptr) xml->removeChildElement (song, false);
    std::unique_ptr<juce::XmlElement> songOwner (song);
    if (xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
    if (song == nullptr) return;

    resetSong();
    sessionId = song->getStringAttribute ("session", sessionId);
    take = Take();
    take.sampleRate = song->getDoubleAttribute ("sampleRate", 48000.0);
    take.hostStartSample = song->getStringAttribute ("hostStart").getLargeIntValue();
    take.hostStartPpq = song->getDoubleAttribute ("hostStartPpq");
    take.bpm = song->getDoubleAttribute ("bpm");
    take.beatsPerBar = song->getIntAttribute ("beatsPerBar", 4);
    take.hasSidechain = song->getBoolAttribute ("hasSidechain");
    take.analysed = song->getBoolAttribute ("analysed");
    take.keyForced = song->getBoolAttribute ("keyForced");
    lastKeyChoice = (int) apvts.getRawParameterValue ("key")->load();
    take.key = { song->getIntAttribute ("keyTonic"), song->getBoolAttribute ("keyMinor") };
    nextPartId = song->getIntAttribute ("nextPart", 1);
    nextStackId = song->getIntAttribute ("nextStack", 1);
    selectedPart = song->getIntAttribute ("selectedPart", -1);

    auto dir = getSessionFolder();
    readMonoWav (dir.getChildFile ("vocal.wav"), take.vocal);
    readMonoWav (dir.getChildFile ("beat.wav"), take.beat);
    if (take.beat.size() != take.vocal.size()) take.beat.resize (take.vocal.size(), 0.0f);
    take.computePeaks();

    for (auto* e : song->getChildWithTagNameIterator ("Chord"))
        take.chords.push_back ({ e->getDoubleAttribute ("s"), e->getDoubleAttribute ("e"),
                                 Chord { e->getIntAttribute ("root", -1), (Quality) e->getIntAttribute ("q") } });
    for (auto* e : song->getChildWithTagNameIterator ("Part"))
        parts.push_back ({ e->getIntAttribute ("id"), e->getDoubleAttribute ("s"), e->getDoubleAttribute ("e"), e->getIntAttribute ("colour") });

    for (auto* e : song->getChildWithTagNameIterator ("Stack"))
    {
        auto s = std::make_shared<StackEntry>();
        s->id = e->getIntAttribute ("id"); s->partId = e->getIntAttribute ("part");
        s->partStart = e->getDoubleAttribute ("partStart"); s->enabled = e->getBoolAttribute ("enabled", true);
        s->settings.lines = (uint32_t) e->getIntAttribute ("lines"); s->settings.flavour = (Flavour) e->getIntAttribute ("flavour");
        s->settings.humanize = (Humanize) e->getIntAttribute ("humanize"); s->settings.crew = (Crew) e->getIntAttribute ("crew");
        s->settings.delivery = (Delivery) e->getIntAttribute ("delivery"); s->settings.seed = (uint32_t) e->getIntAttribute ("seed");

        auto r = std::make_shared<RenderedStack>();
        r->sampleRate = (int) take.sampleRate;
        int vi = 0; bool ok = true;
        for (auto* ve : e->getChildWithTagNameIterator ("Voice"))
        {
            RenderedVoice v;
            v.pan = (float) ve->getDoubleAttribute ("pan"); v.gain = (float) ve->getDoubleAttribute ("gain", 0.5);
            v.type = (LineType) ve->getIntAttribute ("type"); v.side = ve->getIntAttribute ("side", -1);
            ok &= readMonoWav (dir.getChildFile ("stacks").getChildFile ("stack" + juce::String (s->id) + "_v" + juce::String (vi) + ".wav"), v.audio);
            r->voices.push_back (std::move (v));
            ++vi;
        }
        if (! ok || r->voices.empty()) continue;   // audio missing (moved session) -> drop the stack
        s->result = r;
        s->progress = 1.0f;
        s->state = StackEntry::Done;
        lastSeenState[s->id] = StackEntry::Done;
        stacks.push_back (s);
    }
    rebuildPlayback();
    sendChangeMessage();
}

juce::AudioProcessorEditor* SubHarmProcessor::createEditor() { return new SubHarmEditor (*this); }

} // namespace subharm

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new subharm::SubHarmProcessor(); }
