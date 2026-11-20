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

bool Key::contains (int pc) const
{
    for (int s : scalePitchClasses()) if (s == ((pc % 12) + 12) % 12) return true;
    return false;
}

int numLines (uint32_t mask)
{
    int n = 0;
    for (int i = 0; i < kNumLineTypes; ++i) n += (mask >> i) & 1;
    return n;
}

const char* lineName (LineType t)
{
    switch (t)
    {
        case LineType::Double:     return "DOUBLE";
        case LineType::PedalRoot:  return "UNIS";
        case LineType::PedalFifth: return "UNIS 5TH";
        case LineType::ThirdUp:    return "3RD UP";
        case LineType::ThirdDown:  return "3RD DOWN";
        case LineType::FifthUp:    return "5TH UP";
        case LineType::FifthDown:  return "5TH DOWN";
        case LineType::OctaveUp:   return "OCTAVE UP";
        case LineType::OctaveDown: return "OCTAVE DOWN";
        case LineType::ChordHigh:  return "CHORD HARMONY HIGH";
        case LineType::ChordLow:   return "CHORD HARMONY LOW";
        default:                   return "?";
    }
}

uint32_t presetLines (StackSize s)
{
    auto bit = [] (LineType t) { return 1u << (int) t; };
    switch (s)
    {
        case StackSize::Short: return bit (LineType::PedalRoot) | bit (LineType::ThirdUp);
        case StackSize::Tall:  // "SubRoo": the default stack
            return bit (LineType::PedalRoot) | bit (LineType::ThirdUp) | bit (LineType::OctaveUp) | bit (LineType::OctaveDown);
        default:
            return bit (LineType::Double) | bit (LineType::PedalRoot) | bit (LineType::PedalFifth) | bit (LineType::ThirdUp)
                 | bit (LineType::ThirdDown) | bit (LineType::OctaveUp) | bit (LineType::OctaveDown) | bit (LineType::ChordHigh);
    }
}

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
