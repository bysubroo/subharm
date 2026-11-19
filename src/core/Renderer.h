#pragma once
#include <functional>
#include <memory>
#include "Harmony.h"

namespace subharm
{

// Progress callback: receives 0..1, return false to cancel.
using ProgressFn = std::function<bool (float)>;

struct RenderedVoice
{
    std::vector<float> audio;   // mono, dry, same length as the part
    float pan = 0.0f;           // -1..1 (before Sauce width)
    float gain = 1.0f;
    int slot = 0;
    bool isDouble = false;
};

struct RenderedStack
{
    int sampleRate = 48000;
    std::vector<RenderedVoice> voices;
    std::vector<NoteSeg> notes;
    std::vector<VoiceLine> lines;
};

// WORLD analysis of one part of the lead (f0, envelope, aperiodicity) + its notes.
// Expensive (~0.3x real time); cache it so re-stacking a part with other settings is quick.
struct LeadAnalysis
{
    int sampleRate = 48000, numSamples = 0, fftSize = 2048;
    double framePeriod = 5.0;   // ms
    std::vector<double> f0;
    std::vector<std::vector<double>> sp, ap;
    std::vector<NoteSeg> notes;
    std::vector<int> frameNote;
};

std::shared_ptr<LeadAnalysis> analyseLead (const float* vocal, int numSamples, int sampleRate,
                                           const ProgressFn& progress = {});

bool renderStack (const LeadAnalysis& lead, const std::vector<ChordSeg>& chords, double partOffset,
                  const Key& key, const StackSettings& settings, RenderedStack& out,
                  const ProgressFn& progress = {});

// Renders the harmony stack for one part of the lead vocal.
//   vocal/n     part audio (mono)
//   partOffset  seconds from take start to the part start (to look up chords)
// The engine is the "render mode" seam: today DSP (WORLD), later an AI engine can
// take the same harmony plan and re-sing it.
bool renderStack (const float* vocal, int n, int sampleRate,
                  const std::vector<ChordSeg>& chords, double partOffset,
                  const Key& key, const StackSettings& settings,
                  RenderedStack& out, const ProgressFn& progress = {});

// Stereo mix of one stack. sauce 0..1: width + air + a touch of saturation.
void mixStack (const RenderedStack& stack, float sauce, std::vector<float>& left, std::vector<float>& right);

} // namespace subharm
