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
};

struct NoteSeg
{
    double start = 0.0, end = 0.0;   // seconds from part start
    int midi = 60;                   // quantised note the lead is singing
    float midiExact = 60.0f;         // median sung pitch (keeps the singer's own tuning)
    int firstFrame = 0, lastFrame = 0;
};

enum class StackSize { Short = 0, Tall, Full };       // 3, 4, 8 voices
enum class Flavour   { RnB = 0, Pop, Clean, Gospel };
enum class Humanize  { Tight = 0, Natural, Loose };
enum class Crew      { Natural = 0, Wide, Deeper, Lighter };
enum class Delivery  { Natural = 0, Airy, Gentle, Full, Energetic };

constexpr double kPi = 3.14159265358979323846;

struct StackSettings
{
    StackSize size = StackSize::Full;
    Flavour flavour = Flavour::RnB;
    Humanize humanize = Humanize::Natural;
    Crew crew = Crew::Natural;
    Delivery delivery = Delivery::Natural;
    uint32_t seed = 1;
};

int numDistinctLines (StackSize s);   // 3, 4, 4
int numVoices (StackSize s);          // 3, 4, 8 (full = 4 lines doubled)

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
