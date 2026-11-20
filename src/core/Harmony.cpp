#include "Harmony.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace subharm
{

// =====================================================================================
// Note segmentation
// =====================================================================================
std::vector<NoteSeg> segmentNotes (const std::vector<double>& f0, double framePeriodMs, std::vector<int>& frameNote)
{
    const int F = (int) f0.size();
    const double fp = framePeriodMs / 1000.0;
    frameNote.assign (F, -1);

    std::vector<double> midi (F, 0.0);
    for (int i = 0; i < F; ++i)
        if (f0[i] > 0) midi[i] = 69.0 + 12.0 * std::log2 (f0[i] / 440.0);

    // 5-frame median over voiced frames
    std::vector<double> sm (midi);
    for (int i = 0; i < F; ++i)
    {
        if (f0[i] <= 0) continue;
        double w[5]; int c = 0;
        for (int j = i - 2; j <= i + 2; ++j)
            if (j >= 0 && j < F && f0[j] > 0) w[c++] = midi[j];
        std::sort (w, w + c);
        sm[i] = w[c / 2];
    }

    const int bridgeGap = 4;     // unvoiced runs up to 20 ms (consonants) do not end a note
    const int switchHold = 6;    // 30 ms away from the current note -> new note
    const double switchDist = 0.75;

    struct Raw { int a, b; };
    std::vector<Raw> raw;
    int cur = -1, segStart = -1, lastVoiced = -1, away = 0, awayStart = -1;
    double curNote = 0;

    auto close = [&] (int endFrame) { if (segStart >= 0 && endFrame >= segStart) raw.push_back ({ segStart, endFrame }); segStart = -1; cur = -1; };

    for (int i = 0; i < F; ++i)
    {
        if (f0[i] <= 0)
        {
            if (cur >= 0 && i - lastVoiced > bridgeGap) close (lastVoiced);
            continue;
        }
        if (cur < 0)
        {
            cur = 1; segStart = i; curNote = std::round (sm[i]); away = 0;
        }
        else if (std::abs (sm[i] - curNote) > switchDist)
        {
            if (away == 0) awayStart = i;
            if (++away >= switchHold)
            {
                close (awayStart - 1);
                cur = 1; segStart = awayStart; curNote = std::round (sm[i]); away = 0;
            }
        }
        else away = 0;
        lastVoiced = i;
    }
    if (cur >= 0) close (lastVoiced);

    // merge very short blips (< 40 ms) into the previous segment
    std::vector<Raw> merged;
    for (const auto& r : raw)
    {
        if (r.b - r.a + 1 < 8 && ! merged.empty() && r.a - merged.back().b <= bridgeGap + 1)
            merged.back().b = r.b;
        else
            merged.push_back (r);
    }

    std::vector<NoteSeg> notes;
    for (const auto& r : merged)
    {
        std::vector<double> v;
        for (int i = r.a; i <= r.b; ++i) if (f0[i] > 0) v.push_back (midi[i]);
        if (v.empty()) continue;
        std::sort (v.begin(), v.end());
        NoteSeg n;
        n.midiExact = (float) v[v.size() / 2];
        n.midi = (int) std::lround (n.midiExact);
        n.firstFrame = r.a; n.lastFrame = r.b;
        n.start = r.a * fp; n.end = (r.b + 1) * fp;
        int idx = (int) notes.size();
        for (int i = r.a; i <= r.b; ++i) frameNote[i] = idx;
        notes.push_back (n);
    }
    return notes;
}

// =====================================================================================
// Harmony generation
// =====================================================================================
namespace
{
    struct PcSet { bool allowed[12] {}; bool extension[12] {}; };

    PcSet chordSet (const Chord& c, Flavour f)
    {
        PcSet s;
        auto add = [&] (int iv, bool ext) { int pc = (c.root + iv) % 12; s.allowed[pc] = true; if (ext) s.extension[pc] = true; };

        auto iv = c.intervals();
        for (size_t i = 0; i < iv.size(); ++i)
        {
            if (f == Flavour::Clean && i == 3) continue;   // clean = plain triads
            add (iv[i], false);
        }
        const bool minorish = c.quality == Quality::Min || c.quality == Quality::Min7;
        const bool majorish = c.quality == Quality::Maj || c.quality == Quality::Maj7 || c.quality == Quality::Dom7;
        switch (f)
        {
            case Flavour::Clean: break;
            case Flavour::Pop:
                if (majorish || minorish || c.quality == Quality::Sus4) add (2, true);
                break;
            case Flavour::RnB:
                if (c.quality == Quality::Maj) add (11, true);
                if (c.quality == Quality::Min || c.quality == Quality::Sus4) add (10, true);
                if (c.quality != Quality::Dim) add (2, true);
                break;
            case Flavour::Gospel:
                if (c.quality == Quality::Maj) add (11, true);
                if (c.quality == Quality::Min || c.quality == Quality::Sus4) add (10, true);
                if (c.quality != Quality::Dim) add (2, true);
                if (majorish) add (9, true);
                if (minorish) add (5, true);
                break;
        }
        return s;
    }

    // Scale pitches around the lead, used for diatonic intervals.
    struct ScaleLadder
    {
        std::vector<int> pitches;
        ScaleLadder (int lead, const Key& key)
        {
            for (int m = lead - 36; m <= lead + 36; ++m)
                if (key.contains (m)) pitches.push_back (m);
        }
        // index of the scale pitch nearest to m (ties go down)
        int indexOf (int m) const
        {
            int best = 0;
            for (int i = 0; i < (int) pitches.size(); ++i)
                if (std::abs (pitches[i] - m) < std::abs (pitches[best] - m)) best = i;
            return best;
        }
        int shift (int lead, int steps) const   // semitones from lead to the pitch 'steps' scale steps away
        {
            int t = std::clamp (indexOf (lead) + steps, 0, (int) pitches.size() - 1);
            return pitches[t] - lead;
        }
        int stepsBetween (int lead, int p) const { return indexOf (p) - indexOf (lead); }
    };
}

std::vector<VoiceLine> generateHarmony (const std::vector<NoteSeg>& notes, const std::vector<ChordSeg>& chords,
                                        double partOffset, const Key& key, const StackSettings& st)
{
    const uint32_t mask = st.lines != 0 ? st.lines : presetLines (StackSize::Tall);
    const bool gospel = st.flavour == Flavour::Gospel;
    const bool rnb = st.flavour == Flavour::RnB;
    const double extBonus = gospel ? 0.9 : rnb ? 0.55 : st.flavour == Flavour::Pop ? 0.15 : 0.0;
    const double leadSemiClash = gospel ? 1.5 : 3.0;

    // Pedal octave: the tonic / 5th nearest to where the lead mostly sings in this part
    double weighted = 0.0, totalDur = 0.0;
    for (const auto& n : notes) { weighted += n.midi * (n.end - n.start); totalDur += n.end - n.start; }
    const double centre = totalDur > 0 ? weighted / totalDur : 60.0;
    auto pedalPitch = [&] (int pc)
    {
        int best = 0; double bestD = 1e9;
        for (int m = 24; m < 108; ++m)
            if (((m % 12) + 12) % 12 == pc)
            {
                const double d = std::abs (m - centre) + (m > centre ? 0.01 : 0.0);   // ties go down
                if (d < bestD) { bestD = d; best = m; }
            }
        return best;
    };
    const int pedalRoot = pedalPitch (key.tonic);
    const int pedalFifth = pedalPitch ((key.tonic + 7) % 12);

    std::vector<VoiceLine> lines;
    for (auto type : kLineDisplayOrder)
    {
        if (! hasLine (mask, type)) continue;

        VoiceLine line;
        line.type = type;
        if (type == LineType::PedalRoot) line.pedalMidi = pedalRoot;
        if (type == LineType::PedalFifth) line.pedalMidi = pedalFifth;
        line.semis.assign (notes.size(), 0);

        int prevPitch = std::numeric_limits<int>::min();
        int prevSteps = std::numeric_limits<int>::min();
        double prevEnd = -10.0;

        for (size_t ni = 0; ni < notes.size(); ++ni)
        {
            const auto& n = notes[ni];
            const int lead = n.midi;
            const bool connected = (n.start - prevEnd) < 0.5;

            // Very short blips (glides, ornaments, tracking hiccups) keep the previous
            // interval instead of getting a decision of their own: no jumps.
            const bool fixedInterval = type == LineType::Double || type == LineType::OctaveUp || type == LineType::OctaveDown
                                       || type == LineType::PedalRoot || type == LineType::PedalFifth;
            if (! fixedInterval && ni > 0 && connected && (n.end - n.start) < 0.07
                && (! key.contains (notes[ni - 1].midi) || key.contains (lead + line.semis[ni - 1])))
            {
                line.semis[ni] = line.semis[ni - 1];
                prevEnd = n.end;
                continue;
            }

            const ScaleLadder ladder (lead, key);
            int semis = 0;
            switch (type)
            {
                case LineType::Double:     semis = 0; break;
                case LineType::PedalRoot:  semis = pedalRoot - lead; break;
                case LineType::PedalFifth: semis = pedalFifth - lead; break;
                case LineType::OctaveUp:   semis = 12; break;
                case LineType::OctaveDown: semis = -12; break;
                case LineType::ThirdUp:    semis = ladder.shift (lead, +2); break;
                case LineType::ThirdDown:  semis = ladder.shift (lead, -2); break;
                case LineType::FifthUp:    semis = ladder.shift (lead, +4); break;
                case LineType::FifthDown:  semis = ladder.shift (lead, -4); break;
                case LineType::ChordHigh:
                case LineType::ChordLow:
                {
                    const int dir = type == LineType::ChordHigh ? 1 : -1;
                    const int target = lead + ladder.shift (lead, dir * 2);
                    const ChordSeg* cs = chordAt (chords, partOffset + 0.5 * (n.start + n.end));
                    PcSet set;
                    bool useChord = cs != nullptr && ! cs->chord.isNoChord();
                    if (useChord) set = chordSet (cs->chord, st.flavour);
                    if (useChord && ! set.allowed[((lead % 12) + 12) % 12] && (n.end - n.start) < 0.25)
                        useChord = false;   // short passing tone in the lead: move in parallel

                    int best = target;
                    if (useChord)
                    {
                        double bestCost = std::numeric_limits<double>::max();
                        for (int p = lead + dir; dir > 0 ? p <= lead + 9 : p >= lead - 9; p += dir)
                        {
                            const int pc = ((p % 12) + 12) % 12;
                            if (! set.allowed[pc] || ! key.contains (pc)) continue;   // never leave the key
                            double c = 0.8 * std::abs (p - target);
                            if (connected && prevPitch != std::numeric_limits<int>::min())
                                c += 0.5 * std::abs (p - prevPitch);
                            if (connected && prevSteps != std::numeric_limits<int>::min()
                                && p == lead + ladder.shift (lead, prevSteps))
                                c -= 0.8;                                             // keep moving in parallel
                            const int d = std::abs (p - lead);
                            if (d == 1) c += leadSemiClash;
                            if (d == 2) c += gospel ? 0.6 : 1.6;   // a 2nd against the lead sounds wrong outside gospel
                            if (set.extension[pc]) c -= extBonus;
                            if (c < bestCost) { bestCost = c; best = p; }
                        }
                    }
                    semis = best - lead;
                    prevSteps = ladder.stepsBetween (lead, best);
                    break;
                }
                default: break;
            }
            line.semis[ni] = semis;
            prevPitch = lead + semis;
            prevEnd = n.end;
        }
        lines.push_back (std::move (line));
    }
    return lines;
}

} // namespace subharm
