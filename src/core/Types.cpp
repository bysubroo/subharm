#include "Types.h"

namespace subharm
{

static const char* kPcNames[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

std::string pitchClassName (int pc)
{
    return kPcNames[((pc % 12) + 12) % 12];
}

std::vector<int> Chord::intervals() const
{
    switch (quality)
    {
        case Quality::Maj:  return { 0, 4, 7 };
        case Quality::Min:  return { 0, 3, 7 };
        case Quality::Dom7: return { 0, 4, 7, 10 };
        case Quality::Min7: return { 0, 3, 7, 10 };
        case Quality::Maj7: return { 0, 4, 7, 11 };
        case Quality::Dim:  return { 0, 3, 6 };
        case Quality::Sus4: return { 0, 5, 7 };
        default:            return { 0, 4, 7 };
    }
}

std::string Chord::name() const
{
    if (isNoChord())
        return "N.C.";

    std::string n = pitchClassName (root);
    switch (quality)
    {
        case Quality::Maj:  break;
        case Quality::Min:  n += "m"; break;
        case Quality::Dom7: n += "7"; break;
        case Quality::Min7: n += "m7"; break;
        case Quality::Maj7: n += "maj7"; break;
        case Quality::Dim:  n += "dim"; break;
        case Quality::Sus4: n += "sus4"; break;
        default: break;
    }
    return n;
}

std::string Key::name() const
{
    return pitchClassName (tonic) + (minor ? " minor" : " major");
}

std::vector<int> Key::scalePitchClasses() const
{
    static const int maj[7] = { 0, 2, 4, 5, 7, 9, 11 };
    static const int min[7] = { 0, 2, 3, 5, 7, 8, 10 };
    std::vector<int> r;
    for (int i = 0; i < 7; ++i)
        r.push_back ((tonic + (minor ? min[i] : maj[i])) % 12);
    return r;
}

int numDistinctLines (StackSize s) { return s == StackSize::Short ? 3 : 4; }
int numVoices (StackSize s)        { return s == StackSize::Short ? 3 : (s == StackSize::Tall ? 4 : 8); }

const char* toString (StackSize s)
{
    switch (s) { case StackSize::Short: return "Short stack"; case StackSize::Tall: return "Tall stack"; default: return "Full stack"; }
}
const char* toString (Flavour f)
{
    switch (f) { case Flavour::RnB: return "R&B"; case Flavour::Pop: return "Pop"; case Flavour::Clean: return "Clean"; default: return "Gospel"; }
}
const char* toString (Humanize h)
{
    switch (h) { case Humanize::Tight: return "Tight"; case Humanize::Natural: return "Natural"; default: return "Loose"; }
}
const char* toString (Crew c)
{
    switch (c) { case Crew::Natural: return "Your voice"; case Crew::Wide: return "Wide crew"; case Crew::Deeper: return "Deeper crew"; default: return "Lighter crew"; }
}

const char* toString (Delivery d)
{
    switch (d) { case Delivery::Natural: return "Natural"; case Delivery::Airy: return "Airy"; case Delivery::Gentle: return "Gentle"; case Delivery::Full: return "Full voice"; default: return "Energetic"; }
}

const ChordSeg* chordAt (const std::vector<ChordSeg>& chords, double t)
{
    for (const auto& c : chords)
        if (t >= c.start && t < c.end)
            return &c;
    return nullptr;
}

} // namespace subharm
