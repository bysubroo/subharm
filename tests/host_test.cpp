// Simulates a DAW session against the real SubHarm processor:
// record with side chain -> analyse -> draw part -> stack -> play back locked to the
// transport -> save/restore project -> export MIX/STEMS -> snapshot the UI.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../src/plugin/PluginProcessor.h"
#include "../src/plugin/PluginEditor.h"

using namespace subharm;

struct FakePlayHead : juce::AudioPlayHead
{
    bool playing = false;
    juce::int64 time = 0;
    double bpm = 96.0, sr = 48000.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setIsPlaying (playing);
        p.setTimeInSamples (time);
        p.setBpm (bpm);
        p.setPpqPosition (time / sr * bpm / 60.0);
        p.setTimeSignature (TimeSignature { 4, 4 });
        return p;
    }
};

static juce::AudioBuffer<float> readWav (const juce::File& f)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> r (wav.createReaderFor (f.createInputStream().release(), true));
    juce::AudioBuffer<float> b ((int) r->numChannels, (int) r->lengthInSamples);
    r->read (&b, 0, b.getNumSamples(), 0, true, true);
    return b;
}

static void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    std::printf ("[%s] %s\n", ok ? " OK " : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    const juce::File songDir (argc > 2 ? juce::String (argv[2]) : outDir.getFullPathName());

    auto lead = readWav (songDir.getChildFile ("lead.wav"));
    auto beat = readWav (songDir.getChildFile ("beat.wav"));
    const double sr = 48000.0;
    const int block = 512;
    const int n = lead.getNumSamples();

    auto proc = std::make_unique<SubHarmProcessor>();
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (juce::AudioChannelSet::stereo());
    layout.inputBuses.add (juce::AudioChannelSet::stereo());
    layout.outputBuses.add (juce::AudioChannelSet::stereo());
    check (proc->setBusesLayout (layout), "stereo in/out + stereo side chain layout");
    proc->setRateAndBufferSizeDetails (sr, block);
    proc->prepareToPlay (sr, block);

    FakePlayHead ph; ph.sr = sr;
    proc->setPlayHead (&ph);

    // DAW plays from bar 3 (so the take does not start at 0 on the timeline)
    const juce::int64 songStart = (juce::int64) (2 * 4 * 60.0 / 96.0 * sr);
    juce::AudioBuffer<float> buf (4, block);
    juce::MidiBuffer midi;

    auto runTransport = [&] (juce::int64 from, int samples, std::vector<float>* captureL)
    {
        ph.playing = true;
        for (int pos = 0; pos < samples; pos += block)
        {
            const int todo = std::min (block, samples - pos);
            buf.setSize (4, todo, false, false, true);
            for (int i = 0; i < todo; ++i)
            {
                const int si = pos + i;
                const float v = si < n ? lead.getSample (0, si) : 0.0f;
                const float b = si < n ? beat.getSample (0, si) : 0.0f;
                buf.setSample (0, i, v); buf.setSample (1, i, v);
                buf.setSample (2, i, b); buf.setSample (3, i, b);
            }
            ph.time = from + pos;
            proc->processBlock (buf, midi);
            if (captureL) for (int i = 0; i < todo; ++i) captureL->push_back (buf.getSample (0, i));
        }
        ph.playing = false;
        buf.clear();
        proc->processBlock (buf, midi);   // host stops
    };

    // ---------- 1. record ----------
    proc->toggleRecord();
    check (proc->getRecState() == SubHarmProcessor::RecState::Armed, "armed after Record this track");
    runTransport (songStart, n, nullptr);
    pump (300);
    check (! proc->getTake().isEmpty(), "take captured: " + juce::String (proc->getTake().lengthSeconds(), 2) + " s");
    check (proc->getTake().hostStartSample == songStart, "take anchored to host timeline position");
    check (proc->getTake().hasSidechain, "side chain audio captured");

    // ---------- 2. analysis ----------
    for (int i = 0; i < 100 && ! proc->getTake().analysed; ++i) pump (100);
    const auto& take = proc->getTake();
    check (take.analysed, "chords/key analysed");
    juce::String chordStr;
    for (auto& c : take.chords) chordStr << c.chord.name() << " ";
    std::printf ("       key %s, chords: %s\n", take.key.name().c_str(), chordStr.toRawUTF8());
    check (take.key.tonic == 0 && ! take.key.minor, "key = C major");
    check (chordStr.trim() == "C G Am F", "chords = C G Am F");

    // ---------- 2b. key chosen by hand -> chords re-read inside that key ----------
    {
        auto* kp = proc->apvts.getParameter ("key");
        kp->setValueNotifyingHost (kp->convertTo0to1 (1.0f));   // "C major"
        pump (200);
        for (int i = 0; i < 100 && proc->isAnalysing(); ++i) pump (100);
        pump (200);
        juce::String cs2;
        for (auto& c : proc->getTake().chords) cs2 << c.chord.name() << " ";
        check (proc->getTake().keyForced && proc->getTake().key.tonic == 0 && ! proc->getTake().key.minor
               && cs2.trim() == "C G Am F", "key set to C major by hand, chords re-read: " + cs2.trim());
    }

    // ---------- 3. parts + stacks ----------
    const double bar = 4 * 60.0 / 96.0;
    int partA = proc->addPart (0.0, 2 * bar);
    int partB = proc->addPart (2 * bar, 4 * bar - 0.05);
    check (partA > 0 && partB > 0, "two parts drawn");
    proc->stackHarmonies();
    check (proc->getStacks().size() == 2, "stack harmonies queued both parts");
    auto t0 = juce::Time::getMillisecondCounterHiRes();
    for (int i = 0; i < 1200; ++i)
    {
        bool done = true;
        for (auto& s : proc->getStacks()) done &= s->state.load() >= StackEntry::Done;
        if (done) break;
        pump (100);
    }
    pump (200);
    bool allDone = true;
    for (auto& s : proc->getStacks()) allDone &= s->state.load() == StackEntry::Done;
    check (allDone, "both stacks rendered in " + juce::String ((juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0, 1) + " s");

    // re-stack selected part with other settings -> 3rd stack (uses cached analysis)
    proc->apvts.getParameter ("flavour")->setValueNotifyingHost (proc->apvts.getParameter ("flavour")->convertTo0to1 (3));
    proc->apvts.getParameter ("delivery")->setValueNotifyingHost (proc->apvts.getParameter ("delivery")->convertTo0to1 (1));
    proc->selectedPart = partA;
    t0 = juce::Time::getMillisecondCounterHiRes();
    proc->stackHarmonies();
    for (int i = 0; i < 600 && proc->getStacks().back()->state.load() < StackEntry::Done; ++i) pump (100);
    pump (200);
    check (proc->getStacks().size() == 3 && proc->getStacks().back()->state.load() == StackEntry::Done,
           "re-stack of part A (Gospel, Airy) in " + juce::String ((juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0, 1) + " s");
    proc->setStackEnabled (proc->getStacks().back()->id, false);

    // ---------- 4. playback locked to the timeline ----------
    proc->apvts.getParameter ("leadThru")->setValueNotifyingHost (0.0f);   // hear only the stacks
    std::vector<float> outL;
    runTransport (songStart, n, &outL);
    double e = 0; for (float s : outL) e += s * s;
    const double rms = std::sqrt (e / outL.size());
    check (rms > 0.005, "stacks play on the timeline (rms " + juce::String (rms, 4) + ")");

    // playing from a different place = silence before take start
    std::vector<float> early;
    runTransport (0, (int) (songStart / 2), &early);
    double e2 = 0; for (float s : early) e2 += s * s;
    check (e2 < 1e-9, "nothing plays before the take start");

    // offline bounce equivalent: same output again
    std::vector<float> outL2;
    runTransport (songStart, n, &outL2);
    double diff = 0; for (size_t i = 0; i < outL.size(); ++i) diff += std::abs (outL[i] - outL2[i]);
    check (diff < 1e-3, "playback is deterministic (bounce = playback)");

    // ---------- 5. export ----------
    auto mix = proc->exportForDrag (false);
    auto stems = proc->exportForDrag (true);
    check (mix.size() == 1, "MIX file written");
    {
        juce::StringArray stemNames;
        for (auto& f : stems) stemNames.add (juce::File (f).getFileNameWithoutExtension());
        std::printf ("       stems: %s\n", stemNames.joinIntoString (", ").toRawUTF8());
        check (stems.size() == 10 && stemNames.contains ("SubHarm 3RD UP L") && stemNames.contains ("SubHarm CHORD HARMONY HIGH R")
               && stemNames.contains ("SubHarm OCTAVE DOWN L") && stemNames.contains ("SubHarm UNIS R"),
               "STEMS: " + juce::String (stems.size()) + " files named by line + side (Full preset = 5 lines x L/R)");
    }
    if (mix.size() == 1)
    {
        auto m = readWav (juce::File (mix[0]));
        check (m.getNumSamples() == n && m.getNumChannels() == 2, "MIX starts at take start, full take length, stereo");
        juce::File (mix[0]).copyFileTo (outDir.getChildFile ("plugin_MIX.wav"));
    }

    // ---------- 6. save + restore ----------
    juce::MemoryBlock state;
    proc->getStateInformation (state);
    {
        auto proc2 = std::make_unique<SubHarmProcessor>();
        proc2->setBusesLayout (layout);
        proc2->setRateAndBufferSizeDetails (sr, block);
        proc2->prepareToPlay (sr, block);
        proc2->setStateInformation (state.getData(), (int) state.getSize());
        check (proc2->getTake().vocal.size() == take.vocal.size(), "restored take audio");
        check (proc2->getParts().size() == 2 && proc2->getStacks().size() == 3, "restored parts + stacks");
        check (proc2->getTake().chords.size() == take.chords.size(), "restored chords");
        check (! proc2->getStacks().back()->enabled, "restored tick state");

        // ---------- 7. UI snapshot ----------
        proc2->selectedPart = 1;
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc2->createEditorIfNeeded());
        ed->setSize (1180, 880);
        pump (200);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        auto f = outDir.getChildFile ("subharm_ui.png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        png.writeImageToStream (img, os);
        check (f.getSize() > 1000, "UI snapshot written");
        proc2->editorBeingDeleted (ed.get());
        ed.reset();
    }

    proc->setPlayHead (nullptr);
    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures);
    return failures == 0 ? 0 : 1;
}
