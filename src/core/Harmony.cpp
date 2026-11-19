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
            bool isSeventh = (i == 3);
            if (f == Flavour::Clean && isSeventh) continue;   // clean = plain triads
            add (iv[i], false);
        }

        const bool minorish = c.quality == Quality::Min || c.quality == Quality::Min7;
        const bool majorish = c.quality == Quality::Maj || c.quality == Quality::Maj7 || c.quality == Quality::Dom7;

        switch (f)
        {
            case Flavour::Clean: break;
            case Flavour::Pop:
                if (majorish || minorish || c.quality == Quality::Sus4) add (2, true);       // add9
                break;
            case Flavour::RnB:
                if (c.quality == Quality::Maj) add (11, true);                               // maj7
                if (c.quality == Quality::Min || c.quality == Quality::Sus4) add (10, true); // m7 / 7sus
                if (c.quality != Quality::Dim) add (2, true);                                // 9
                break;
            case Flavour::Gospel:
                if (c.quality == Quality::Maj) add (11, true);
                if (c.quality == Quality::Min || c.quality == Quality::Sus4) add (10, true);
                if (c.quality != Quality::Dim) add (2, true);                                // 9
                if (majorish) add (9, true);                                                 // 13
                if (minorish) add (5, true);                                                 // 11
                break;
        }
        return s;
    }

    // Target interval (semitones from lead) for each line of the stack, in priority order.
    const int kSlotTarget[4]   = { +4, -4, -8, +8 };
    // Same lines expressed as diatonic steps, used for passing tones / no chord.
    const int kSlotDegrees[4]  = { +2, -2, -5, +4 };

    int diatonicShift (int leadMidi, int degrees, const Key& key)
    {
        auto scale = key.scalePitchClasses();
        std::vector<int> pitches;
        for (int m = leadMidi - 30; m <= leadMidi + 30; ++m)
            if (std::find (scale.begin(), scale.end(), ((m % 12) + 12) % 12) != scale.end())
                pitches.push_back (m);
        // nearest scale pitch at or below the lead
        int idx = 0;
        for (int i = 0; i < (int) pitches.size(); ++i) if (pitches[i] <= leadMidi) idx = i;
        int t = std::clamp (idx + degrees, 0, (int) pitches.size() - 1);
        return pitches[t] - leadMidi;
    }
}

std::vector<VoiceLine> generateHarmony (const std::vector<NoteSeg>& notes, const std::vector<ChordSeg>& chords,
                                        double partOffset, const Key& key, const StackSettings& st)
{
    const int L = numDistinctLines (st.size);
    std::vector<VoiceLine> lines (L);
    for (int v = 0; v < L; ++v) { lines[v].slot = v; lines[v].semis.assign (notes.size(), 0); }

    const bool gospel = st.flavour == Flavour::Gospel;
    const bool rnb = st.flavour == Flavour::RnB;
    const double extBonus = gospel ? 0.9 : rnb ? 0.55 : st.flavour == Flavour::Pop ? 0.15 : 0.0;
    const double leadSemiClash = gospel ? 1.2 : 3.0;
    const double leadToneClash = gospel ? 0.15 : rnb ? 0.4 : 0.8;
    const double voiceSemiClash = gospel ? 0.9 : 2.5;

    std::vector<int> prevPitch (L, std::numeric_limits<int>::min());
    double prevEnd = -10.0;

    for (size_t ni = 0; ni < notes.size(); ++ni)
    {
        const auto& n = notes[ni];
        const int lead = n.midi;
        const int leadPc = ((lead % 12) + 12) % 12;
        const double mid = partOffset + 0.5 * (n.start + n.end);
        const ChordSeg* cs = chordAt (chords, mid);
        const bool connected = (n.start - prevEnd) < 0.5;

        bool useDiatonic = (cs == nullptr || cs->chord.isNoChord());
        PcSet set;
        if (! useDiatonic)
        {
            set = chordSet (cs->chord, st.flavour);
            // Lead on a tone outside the chord colour (passing / suspension) -> move in parallel diatonically
            if (! set.allowed[leadPc])
                useDiatonic = true;
        }

        std::vector<int> chosen;
        for (int v = 0; v < L; ++v)
        {
            int best = 0;
            if (useDiatonic)
            {
                best = lead + diatonicShift (lead, kSlotDegrees[v], key);
                if (std::find (chosen.begin(), chosen.end(), best) != chosen.end())
                    best = lead + diatonicShift (lead, kSlotDegrees[v] + (kSlotDegrees[v] > 0 ? 1 : -1), key);
            }
            else
            {
                const int target = lead + kSlotTarget[v];
                double bestCost = std::numeric_limits<double>::max();
                for (int pass = 0; pass < 2 && bestCost == std::numeric_limits<double>::max(); ++pass)
                {
                    for (int p = lead - 15; p <= lead + 10; ++p)
                    {
                        int pc = ((p % 12) + 12) % 12;
                        if (! set.allowed[pc] || p == lead) continue;
                        if (pass == 0 && ((kSlotTarget[v] > 0) != (p > lead))) continue;  // keep line on its side
                        if (std::find (chosen.begin(), chosen.end(), p) != chosen.end()) continue;

                        double c = std::abs (p - target);
                        if (connected && prevPitch[v] != std::numeric_limits<int>::min())
                            c += 0.6 * std::abs (p - prevPitch[v]);
                        int d = std::abs (p - lead) % 12;
                        if (d == 1 || d == 11) c += leadSemiClash;
                        if (std::abs (p - lead) == 2) c += leadToneClash;
                        if (std::abs (p - lead) == 12) c += 2.0;
                        for (int o : chosen)
                            if (std::abs (o - p) == 1) c += voiceSemiClash;
                        if (set.extension[pc]) c -= extBonus;
                        if (c < bestCost) { bestCost = c; best = p; }
                    }
                }
                if (bestCost == std::numeric_limits<double>::max())
                    best = lead + diatonicShift (lead, kSlotDegrees[v], key);
            }
            chosen.push_back (best);
            lines[v].semis[ni] = best - lead;
            prevPitch[v] = best;
        }
        prevEnd = n.end;
    }

    if (st.size == StackSize::Full)
        for (int v = 0; v < L; ++v)
        {
            VoiceLine d = lines[v];
            d.doubleOf = v;
            lines.push_back (d);
        }
    return lines;
}

} // namespace subharm
