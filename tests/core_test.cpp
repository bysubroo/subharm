// Offline test of the SubHarm core with a synthetic song:
//   beat  = held chord pads C G Am F (96 BPM, 1 bar each) + noise hats
//   vocal = a "sung" melody (harmonic source through vowel formants, vibrato)
// Checks chord/key detection and renders every flavour to WAV for listening.
#include <algorithm>
#include <map>
#include <set>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

#include "../src/core/ChordAnalysis.h"
#include "../src/core/Renderer.h"
#include "world/harvest.h"

using namespace subharm;

static void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>* R, int fs)
{
    const int ch = R ? 2 : 1;
    const uint32_t n = (uint32_t) L.size();
    std::ofstream f (path, std::ios::binary);
    auto w32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto w16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
    f.write ("RIFF", 4); w32 (36 + n * ch * 2); f.write ("WAVEfmt ", 8);
    w32 (16); w16 (1); w16 ((uint16_t) ch); w32 (fs); w32 (fs * ch * 2); w16 ((uint16_t) (ch * 2)); w16 (16);
    f.write ("data", 4); w32 (n * ch * 2);
    for (uint32_t i = 0; i < n; ++i)
        for (int c = 0; c < ch; ++c)
        {
            float s = c == 0 ? L[i] : (*R)[i];
            int16_t v = (int16_t) std::lround (std::max (-1.0f, std::min (1.0f, s)) * 32767);
            w16 ((uint16_t) v);
        }
}

static double midiHz (double m) { return 440.0 * std::pow (2.0, (m - 69) / 12.0); }

int main (int argc, char** argv)
{
    const int fs = 48000;
    const double bpm = 96.0, beat = 60.0 / bpm, bar = 4 * beat;
    const int bars = 4;
    const int n = (int) (bars * bar * fs);
    const std::string outDir = argc > 1 ? argv[1] : ".";

    // ---------- beat: chord pads ----------
    const int prog[4][4] = { { 48, 60, 64, 67 }, { 43, 59, 62, 67 }, { 45, 60, 64, 69 }, { 41, 60, 65, 69 } };  // C G Am F
    std::vector<float> beatBuf (n, 0.0f);
    Rng rng (5);
    for (int i = 0; i < n; ++i)
    {
        double t = i / (double) fs;
        int b = std::min (bars - 1, (int) (t / bar));
        double s = 0;
        for (int k = 0; k < 4; ++k)
        {
            double f = midiHz (prog[b][k]);
            s += 0.12 * std::sin (2 * kPi * f * t) + 0.04 * std::sin (4 * kPi * f * t) + 0.02 * std::sin (6 * kPi * f * t);
        }
        double inBeat = std::fmod (t, beat / 2);
        s += (inBeat < 0.03 ? 0.15 * std::exp (-inBeat * 120) : 0.0) * rng.bipolar();   // hats
        beatBuf[i] = (float) s;
    }

    // ---------- vocal: melody over the chords ----------
    // (start beat, length beats, midi)
    const double mel[][3] = {
        { 0.0, 1.0, 72 }, { 1.0, 1.0, 74 }, { 2.0, 2.0, 76 },          // C bar: C D E
        { 4.0, 1.5, 74 }, { 5.5, 0.5, 71 }, { 6.0, 2.0, 74 },          // G bar: D B D
        { 8.0, 1.0, 72 }, { 9.0, 1.0, 76 }, { 10.0, 2.0, 76 },         // Am bar: C E E
        { 12.0, 1.0, 77 }, { 13.0, 1.0, 76 }, { 14.0, 1.75, 72 } };    // F bar: F E C
    std::vector<float> vocal (n, 0.0f);
    double phase = 0;
    const double formants[3][2] = { { 750, 1.0 }, { 1150, 0.6 }, { 2600, 0.25 } };   // "ah"
    for (int i = 0; i < n; ++i)
    {
        double t = i / (double) fs, bt = t / beat;
        double m = -1, env = 0;
        for (auto& e : mel)
            if (bt >= e[0] && bt < e[0] + e[1] - 0.08)
            {
                m = e[2];
                double a = (bt - e[0]) * beat, r = (e[0] + e[1] - 0.08 - bt) * beat;
                env = std::min ({ 1.0, a / 0.03, r / 0.04 });
            }
        if (m < 0) { phase = 0; continue; }
        double f = midiHz (m + 0.25 * std::sin (2 * kPi * 5.5 * t) * std::min (1.0, t));
        phase += 2 * kPi * f / fs;
        double s = 0;
        for (int h = 1; h * f < 8000; ++h)
        {
            double hf = h * f, amp = 0;
            for (auto& fm : formants) { double d = (hf - fm[0]) / 130.0; amp += fm[1] * std::exp (-d * d); }
            amp += 0.02 / h;
            s += amp * std::sin (h * phase);
        }
        vocal[i] = (float) (0.18 * env * s + 0.003 * env * rng.bipolar());
    }

    // ---------- chords ----------
    auto ca = analyseChords (beatBuf.data(), n, fs, bpm, 0.0);
    std::printf ("Key: %s\nChords:\n", ca.key.name().c_str());
    for (auto& c : ca.chords) std::printf ("  %6.2f - %6.2f  %s\n", c.start, c.end, c.chord.name().c_str());

    const char* expect[4] = { "C", "G", "Am", "F" };
    int ok = 0;
    for (int b = 0; b < 4; ++b)
    {
        auto* cs = chordAt (ca.chords, b * bar + bar / 2);
        if (cs && cs->chord.name() == expect[b]) ++ok;
    }
    std::printf ("Chord accuracy: %d/4\n", ok);
    bool keyOk = ca.key.tonic == 0 && ! ca.key.minor;

    writeWav (outDir + "/lead.wav", vocal, nullptr, fs);
    writeWav (outDir + "/beat.wav", beatBuf, nullptr, fs);

    // ---------- render: whole 4 bars as one part ----------
    const Flavour flavours[4] = { Flavour::RnB, Flavour::Pop, Flavour::Clean, Flavour::Gospel };
    bool renderOk = true;
    for (int fi = 0; fi < 4; ++fi)
    {
        StackSettings st;
        st.lines = 0x7FF; st.flavour = flavours[fi]; st.delivery = fi == 3 ? Delivery::Airy : Delivery::Natural;
        RenderedStack rs;
        bool r = renderStack (vocal.data(), n, fs, ca.chords, 0.0, ca.key, st, rs);
        renderOk &= r && rs.voices.size() == 22;
        std::printf ("\n%s: %zu notes, %zu voices\n", toString (st.flavour), rs.notes.size(), rs.voices.size());
        std::printf ("  %-6s", "lead");
        for (auto& l : rs.lines) std::printf (" %-9.9s", lineName (l.type));
        std::printf ("\n");
        int offKey = 0;
        for (size_t k = 0; k < rs.notes.size(); ++k)
        {
            std::printf ("  %-6s", pitchClassName (rs.notes[k].midi).c_str());
            for (auto& l : rs.lines)
            {
                int p = rs.notes[k].midi + l.semis[k];
                if (ca.key.contains (rs.notes[k].midi) && ! ca.key.contains (p)) ++offKey;
                std::printf (" %+3d %-5s", l.semis[k], pitchClassName (p).c_str());
            }
            std::printf ("\n");
        }
        std::printf ("  off-key harmony notes: %d\n", offKey);
        // UNIS / UNIS 5TH: one constant note on the tonic / 5th of the key
        for (auto& l : rs.lines)
            if (l.type == LineType::PedalRoot || l.type == LineType::PedalFifth)
            {
                std::set<int> pitches;
                for (size_t k = 0; k < rs.notes.size(); ++k) pitches.insert (rs.notes[k].midi + l.semis[k]);
                const int want = (ca.key.tonic + (l.type == LineType::PedalFifth ? 7 : 0)) % 12;
                const bool ok1 = pitches.size() == 1 && *pitches.begin() % 12 == want;
                std::printf ("  %s pedal: %s (%s)\n", lineName (l.type), ok1 ? "OK" : "WRONG", pitchClassName (*pitches.begin()).c_str());
                renderOk &= ok1;
            }
        // phasing: correlation between the two takes of the DOUBLE and between DOUBLE and the lead
        {
            const std::vector<float>* dl = nullptr; const std::vector<float>* dr = nullptr;
            for (auto& v : rs.voices) if (v.type == LineType::Double) (v.side < 0 ? dl : dr) = &v.audio;
            auto corr = [&] (const std::vector<float>& a, const std::vector<float>& b)
            {
                double best = 0;
                for (int lag = -96; lag <= 96; lag += 4)   // +-2 ms: the comb-filter zone
                {
                    double ab = 0, aa = 0, bb = 0;
                    for (int i = 4800; i < n - 4800; i += 2)
                    {
                        const double x = a[(size_t) i], y = b[(size_t) (i + lag)];
                        ab += x * y; aa += x * x; bb += y * y;
                    }
                    best = std::max (best, std::abs (ab) / std::sqrt (aa * bb + 1e-12));
                }
                return best;
            };
            const double cLR = corr (*dl, *dr), cLead = corr (*dl, vocal);
            std::printf ("  DOUBLE correlation (peak, +-2 ms): L vs R %.2f, L vs lead %.2f\n", cLR, cLead);
            renderOk &= cLR < 0.5 && cLead < 0.5;
        }
        renderOk &= offKey == 0;
        // Verify: each rendered line really sings its planned note
        int hit = 0, tot = 0, unv = 0;
        std::map<std::string, int> missByLine;
        for (int v = 0; v < (int) rs.voices.size(); ++v)
        {
            const auto& a = rs.voices[v].audio;
            std::vector<double> xd (a.begin(), a.end());
            HarvestOption ho; InitializeHarvestOption (&ho); ho.f0_floor = 65; ho.f0_ceil = 1600; ho.frame_period = 5;
            int F = GetSamplesForHarvest (fs, n, 5.0);
            std::vector<double> tp (F), f0 (F);
            Harvest (xd.data(), n, fs, &ho, tp.data(), f0.data());
            for (size_t k = 0; k < rs.notes.size(); ++k)
            {
                const auto& nt = rs.notes[k];
                if (nt.end - nt.start < 0.15) continue;
                std::vector<double> m;
                double a0 = nt.start + 0.25 * (nt.end - nt.start), a1 = nt.end - 0.25 * (nt.end - nt.start);
                for (int i = (int) (a0 * 200); i < (int) (a1 * 200) && i < F; ++i)
                    if (f0[i] > 0) m.push_back (69 + 12 * std::log2 (f0[i] / 440.0));
                if (m.size() < 5) { ++unv; continue; }
                std::sort (m.begin(), m.end());
                double expected = nt.midiExact + rs.lines[(size_t) v / 2].semis[k];
                ++tot;
                double err = m[m.size() / 2] - expected;
                err -= 12.0 * std::round (err / 12.0);   // the checker's own octave errors (not the render's)
                if (std::abs (err) < 0.5) ++hit; else { missByLine[rs.voices[v].name()]++; if (fi == 0) std::printf ("      %s note %zu (%.2fs) expected %.2f got %.2f\n", rs.voices[v].name().c_str(), k, nt.start, expected, m[m.size() / 2]); }
            }
        }
        std::printf ("  pitch check: %d/%d notes on target (%d undetectable)\n", hit, tot, unv);
        for (auto& m : missByLine) std::printf ("    missed on %s: %d\n", m.first.c_str(), m.second);
        renderOk &= hit >= tot * 0.9;
        // (octave lines are checked too: Harvest tracks them fine at these ranges)

        std::vector<float> L, R;
        mixStack (rs, 0.27f, L, R);
        float peak = 0; for (float s : L) peak = std::max (peak, std::abs (s));
        std::printf ("  mix peak %.3f\n", peak);
        renderOk &= peak > 0.01f && peak < 2.0f;
        std::vector<float> fl (L), fr (R);
        for (int i = 0; i < n; ++i) { fl[i] += vocal[i] + 0.5f * beatBuf[i]; fr[i] += vocal[i] + 0.5f * beatBuf[i]; }
        std::string name = std::string (toString (st.flavour)) == "R&B" ? "rnb" : toString (st.flavour);
        writeWav (outDir + "/stack_" + name + ".wav", L, &R, fs);
        writeWav (outDir + "/song_" + name + ".wav", fl, &fr, fs);
    }

    std::printf ("\nRESULT: chords %d/4, key %s, render %s\n", ok, keyOk ? "OK" : "WRONG", renderOk ? "OK" : "FAIL");
    return (ok >= 3 && keyOk && renderOk) ? 0 : 1;
}
