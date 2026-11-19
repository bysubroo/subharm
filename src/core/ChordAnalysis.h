#pragma once
#include "Types.h"

namespace subharm
{

struct ChordAnalysisResult
{
    std::vector<ChordSeg> chords;   // seconds from take start, contiguous
    Key key;
};

// Reads chords and key from the side chain (beat / chord bus), one decision per beat.
//   bpm              host tempo (<= 0 -> 0.5 s analysis steps)
//   firstBeatOffset  seconds from the start of the audio to the first beat line
//   forcedKey        if set: the key is fixed and chords outside it are strongly penalised
ChordAnalysisResult analyseChords (const float* mono, int numSamples, double sampleRate,
                                   double bpm, double firstBeatOffset, const Key* forcedKey = nullptr);

// Krumhansl-Kessler key estimate from a 12-bin chroma profile.
Key detectKey (const std::array<double, 12>& chroma);

} // namespace subharm
