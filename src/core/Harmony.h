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
    int slot = 0;             // which line of the stack (0 = upper third, 1 = lower, ...)
    int doubleOf = -1;        // >= 0: this voice doubles that line (full stack)
};

// The voicing brain: picks a harmony note for every lead note from the chord under it.
//   partOffset  seconds from take start to the part start (notes are part-relative)
std::vector<VoiceLine> generateHarmony (const std::vector<NoteSeg>& notes,
                                        const std::vector<ChordSeg>& chords,
                                        double partOffset,
                                        const Key& key,
                                        const StackSettings& settings);

} // namespace subharm
