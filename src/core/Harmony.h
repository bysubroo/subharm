#pragma once
#include "Types.h"

namespace subharm
{

// Turns a WORLD f0 contour (Hz, 0 = unvoiced) into sung notes.
// frameNote receives, per frame, the index of the note it belongs to (-1 = unvoiced).
std::vector<NoteSeg> segmentNotes (const std::vector<double>& f0, double framePeriodMs,
                                   std::vector<int>& frameNote);

struct VoiceLine
{
    std::vector<int> semis;   // shift in semitones per lead note
    LineType type = LineType::ThirdUp;
};

// The voicing brain. Interval lines (3rd/5th/octave/unison) move strictly in the key,
// chord lines pick chord tones (flavour colours them) with voice leading.
// One VoiceLine per enabled LineType; the renderer sings each one twice (L + R).
//   partOffset  seconds from take start to the part start (notes are part-relative)
std::vector<VoiceLine> generateHarmony (const std::vector<NoteSeg>& notes,
                                        const std::vector<ChordSeg>& chords,
                                        double partOffset,
                                        const Key& key,
                                        const StackSettings& settings);

} // namespace subharm
