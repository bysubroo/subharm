// SubHarm - chord-aware vocal stack generator
// Core types shared by analysis, harmony generation and rendering.
// No JUCE dependency: the core builds and tests on its own.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace subharm
{

enum class Quality : int { Maj = 0, Min, Dom7, Min7, Maj7, Dim, Sus4, Count };

struct Chord
{
    int root = -1;               // pitch class 0..11, -1 = no chord (N.C.)
    Quality quality = Quality::Maj;

    bool isNoChord() const { return root < 0; }
    std::string name() const;
    std::vector<int> intervals() const;   // chord tones relative to root
    bool operator== (const Chord& o) const { return root == o.root && (root < 0 || quality == o.quality); }
    bool operator!= (const Chord& o) const { return ! (*this == o); }
};

struct ChordSeg
{
    double start = 0.0, end = 0.0;   // seconds from take start
    Chord chord;
};

struct Key
{
    int tonic = 0;
    bool minor = false;
    std::string name() const;
    std::vector<int> scalePitchClasses() const;
    bool contains (int pc) const;
};

struct NoteSeg
{
    double start = 0.0, end = 0.0;   // seconds from part start
    int midi = 60;                   // quantised note the lead is singing
    float midiExact = 60.0f;         // median sung pitch (keeps the singer's own tuning)
    int firstFrame = 0, lastFrame = 0;
};

enum class StackSize { Short = 0, Tall, Full };       // presets for the line matrix

// Every line is rendered as an L + R pair (two takes of the same part).
// (values are stored in projects: only ever append)
enum class LineType : int
{
    Double = 0,         // DOUBLE: your melody again (Revoice-style double)
    ThirdUp, ThirdDown, FifthUp, FifthDown, OctaveUp, OctaveDown, ChordHigh, ChordLow,
    PedalRoot,          // UNIS: one constant note on the key's tonic (F# in F# minor)
    PedalFifth,         // UNIS 5TH: one constant note on the key's 5th (G in C major)
    Count
};
constexpr int kNumLineTypes = (int) LineType::Count;
// order used in the UI and for stems
constexpr LineType kLineDisplayOrder[kNumLineTypes] = {
    LineType::PedalRoot, LineType::PedalFifth, LineType::Double, LineType::ThirdUp, LineType::ThirdDown, LineType::FifthUp,
    LineType::FifthDown, LineType::OctaveUp, LineType::OctaveDown, LineType::ChordHigh, LineType::ChordLow };
const char* lineName (LineType);          // "3RD UP", "CHORD HARMONY LOW", ...
uint32_t presetLines (StackSize);         // bit mask of LineType
inline bool hasLine (uint32_t mask, LineType t) { return (mask >> (int) t) & 1u; }
enum class Flavour   { RnB = 0, Pop, Clean, Gospel };
enum class Humanize  { Tight = 0, Natural, Loose };
enum class Crew      { Natural = 0, Wide, Deeper, Lighter };
enum class Delivery  { Natural = 0, Airy, Gentle, Full, Energetic };

constexpr double kPi = 3.14159265358979323846;

struct StackSettings
{
    uint32_t lines = 0;                     // bit mask of LineType (0 -> Full preset)
    Flavour flavour = Flavour::RnB;
    Humanize humanize = Humanize::Natural;
    Crew crew = Crew::Natural;
    Delivery delivery = Delivery::Natural;
    uint32_t seed = 1;
};

int numLines (uint32_t mask);

const char* toString (StackSize);
const char* toString (Flavour);
const char* toString (Humanize);
const char* toString (Crew);
const char* toString (Delivery);

std::string pitchClassName (int pc);

// Chord active at time t (seconds from take start); nullptr if none.
const ChordSeg* chordAt (const std::vector<ChordSeg>& chords, double t);

// Small deterministic RNG so the same seed always renders the same stack.
struct Rng
{
    uint64_t s;
    explicit Rng (uint64_t seed) : s (seed * 0x9E3779B97F4A7C15ull + 0x1234567ull) {}
    uint32_t next()
    {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        return (uint32_t) (s >> 16);
    }
    double uniform() { return (next() & 0xFFFFFF) / double (0x1000000); }   // 0..1
    double bipolar() { return uniform() * 2.0 - 1.0; }                     // -1..1
};

} // namespace subharm
