#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <mutex>
#include <atomic>
#include <map>
#include <memory>

#include "ChordAnalysis.h"
#include "Renderer.h"

namespace subharm
{

// ------------------------------------------------------------------------------------
// Song model (message thread owns it; render jobs get copies)
// ------------------------------------------------------------------------------------
struct Take
{
    double sampleRate = 48000.0;
    juce::int64 hostStartSample = 0;   // host timeline sample where recording started
    double hostStartPpq = 0.0;
    double bpm = 0.0;
    int beatsPerBar = 4;
    std::vector<float> vocal, beat;    // mono
    std::vector<float> vocalPeaks, beatPeaks;   // max |x| per kPeakBlock samples
    std::vector<ChordSeg> chords;
    Key key;
    bool analysed = false;
    bool hasSidechain = false;

    static constexpr int kPeakBlock = 256;
    bool isEmpty() const { return vocal.empty(); }
    double lengthSeconds() const { return vocal.size() / sampleRate; }
    void computePeaks();
};

struct Part
{
    int id = 0;
    double start = 0.0, end = 0.0;   // seconds from take start
    int colourIndex = 0;
};

struct StackEntry
{
    enum State { Queued = 0, Rendering, Done, Failed };

    int id = 0;
    int partId = 0;
    double partStart = 0.0;          // seconds from take start (when rendered)
    StackSettings settings;
    bool enabled = true;
    std::atomic<int> state { Queued };
    std::atomic<float> progress { 0.0f };
    std::shared_ptr<RenderedStack> result;   // guarded by SubHarmProcessor::resultLock
    juce::String label() const;
};

// What the audio thread plays: pre-mixed stereo per enabled stack.
struct PlaybackSet
{
    struct Item { juce::int64 startInTake = 0; std::vector<float> L, R; };
    double sampleRate = 48000.0;
    juce::int64 takeHostStart = 0;
    std::vector<Item> items;
};

// ------------------------------------------------------------------------------------
class SubHarmProcessor : public juce::AudioProcessor,
                         public juce::ChangeBroadcaster,
                         private juce::Timer
{
public:
    SubHarmProcessor();
    ~SubHarmProcessor() override;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "SubHarm"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---------------- UI-facing API (message thread) ----------------
    juce::AudioProcessorValueTreeState apvts;

    enum class RecState { Idle = 0, Armed, Recording, Finished };
    RecState getRecState() const { return (RecState) recState.load(); }
    void toggleRecord();
    double getRecordedSeconds() const { return recPos.load() / juce::jmax (1.0, getSampleRate()); }
    float getSidechainLevel() const { return sidechainLevel.load(); }
    bool isSidechainConnected() const { return sidechainConnected.load(); }
    bool isHostPlaying() const { return hostPlaying.load(); }
    double getPlayheadInTake() const;   // seconds, <0 if outside

    const Take& getTake() const { return take; }
    bool isAnalysing() const { return analysing.load(); }
    void setChord (int segIndex, Chord c);
    void splitChord (int segIndex, double atSeconds);
    void mergeChordWithNext (int segIndex);

    const std::vector<Part>& getParts() const { return parts; }
    int addPart (double start, double end);
    void setPartBounds (int partId, double start, double end);
    void removePart (int partId);
    int selectedPart = -1;

    const std::vector<std::shared_ptr<StackEntry>>& getStacks() const { return stacks; }
    int numPendingParts() const;
    void stackHarmonies();             // stacks every part without a stack, else re-stacks the selected one
    void setStackEnabled (int stackId, bool enabled);
    void removeStack (int stackId);
    StackSettings currentSettings() const;

    juce::StringArray exportForDrag (bool stems);   // writes WAVs, returns paths
    bool exportToFolder (const juce::File& folder, bool stems);
    juce::File getSessionFolder() const;

    std::shared_ptr<RenderedStack> getResult (const StackEntry& e) const;
    juce::CriticalSection resultLock;

private:
    void timerCallback() override;
    void finaliseRecording();
    void startTakeAnalysis();
    void launchRender (std::shared_ptr<StackEntry> e);
    void rebuildPlayback();
    void saveTakeAudio();
    void saveStackAudio (const StackEntry& e);
    bool writeWav (const juce::File& f, const std::vector<float>* L, const std::vector<float>* R, int numChannels, juce::int64 timeRef);
    void buildExportBuffers (bool stems, std::vector<std::vector<float>>& chans, std::vector<juce::String>& names, int& numChannelsPerFile);
    void resetSong();

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // song
    Take take;
    std::vector<Part> parts;
    std::vector<std::shared_ptr<StackEntry>> stacks;
    int nextPartId = 1, nextStackId = 1;
    juce::String sessionId;
    std::atomic<bool> analysing { false };
    std::atomic<bool> analysisDone { false };
    ChordAnalysisResult pendingAnalysis;

    // analysis cache (lead WORLD analysis per part bounds)
    std::mutex cacheMutex;
    std::map<juce::String, std::shared_ptr<LeadAnalysis>> leadCache;
    std::vector<juce::String> leadCacheOrder;

    // render workers
    juce::ThreadPool pool { 2 };
    std::map<int, int> lastSeenState;

    // recording (audio thread writes, message thread reads after Finished)
    std::vector<float> recVocal, recBeat;
    std::atomic<int> recState { (int) RecState::Idle };
    std::atomic<int> recPos { 0 };
    std::atomic<juce::int64> recStartSample { 0 };
    std::atomic<double> recStartPpq { 0.0 }, recBpm { 0.0 };
    std::atomic<int> recBeatsPerBar { 4 };
    juce::int64 lastBlockEnd = -1;

    // live info
    std::atomic<float> sidechainLevel { 0.0f };
    std::atomic<bool> sidechainConnected { false }, hostPlaying { false };
    std::atomic<juce::int64> hostTime { 0 };

    // playback
    juce::SpinLock playLock;
    std::shared_ptr<PlaybackSet> playback;
    float lastSauce = -1.0f;
    int sauceDirtyTicks = 0;

    std::atomic<float>* stackLevelParam = nullptr;
    std::atomic<float>* sauceParam = nullptr;
    std::atomic<float>* leadThruParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubHarmProcessor)
};

} // namespace subharm
