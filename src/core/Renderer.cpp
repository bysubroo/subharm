#include "Renderer.h"

#include <atomic>
#include <thread>
#include <cstdlib>

#include <algorithm>
#include <cmath>
#include <limits>

#include "world/cheaptrick.h"
#include "world/d4c.h"
#include "world/harvest.h"
#include "world/synthesis.h"

// generated at configure time from WORLD's Synthesis() with a per-call noise seed (see CMakeLists.txt)
void SynthesisSeeded (unsigned int seed, const double* f0, int f0_length, const double* const* spectrogram,
                      const double* const* aperiodicity, int fft_size, double frame_period, int fs, int y_length, double* y);

namespace subharm
{

std::string RenderedVoice::name() const
{
    return std::string (lineName (type)) + (side < 0 ? " L" : " R");
}

namespace
{
    // Revoice-style double variation: every voice is its own "take" with a variable
    // delay (per note, early or late), its own pitch offsets per note, its own scoop
    // into notes, drift, vibrato depth, level ride and formant.
    struct HumanizeParams
    {
        double delayMinMs, delayMaxMs;   // base offset of the take
        double noteJitterMs;             // extra offset per note
        double detuneCents, noteCents, driftCents, scoopCents;
        double levelDb, formantJitter;
    };

    HumanizeParams humanizeFor (Humanize h)
    {
        switch (h)
        {
            case Humanize::Tight:   return {  6.0, 14.0,  5.0, 3.0,  3.0,  3.0,  8.0, 0.7, 0.008 };
            case Humanize::Natural: return { 12.0, 26.0, 11.0, 5.0,  6.0,  6.0, 15.0, 1.5, 0.015 };
            default:                return { 20.0, 40.0, 20.0, 9.0, 10.0, 10.0, 25.0, 2.5, 0.025 };
        }
    }

    struct DeliveryParams { double breath; double vibrato; double formant; double gain; };

    DeliveryParams deliveryFor (Delivery d)
    {
        switch (d)
        {
            case Delivery::Airy:      return { 0.30, 0.6, 0.0, 0.9 };
            case Delivery::Gentle:    return { 0.12, 0.4, -0.01, 0.95 };
            case Delivery::Full:      return { -0.04, 0.8, 0.0, 1.0 };
            case Delivery::Energetic: return { -0.06, 1.0, 0.02, 1.0 };
            default:                  return { 0.0, 0.7, 0.0, 1.0 };
        }
    }

    double smoothstep (double a, double b, double x)
    {
        double t = std::clamp ((x - a) / (b - a), 0.0, 1.0);
        return t * t * (3 - 2 * t);
    }

    // Spectral shaping per delivery, in dB at frequency f.
    double deliveryDb (Delivery d, double f)
    {
        switch (d)
        {
            case Delivery::Airy:
                return 3.5 * smoothstep (3000, 8000, f) - 1.5 * (1.0 - smoothstep (150, 350, f));
            case Delivery::Gentle:
                return -4.5 * smoothstep (1500, 5000, f);
            case Delivery::Full:
            {
                double lf = std::log2 (std::max (f, 1.0) / 450.0);
                return 2.5 * std::exp (-lf * lf / 0.9) + 1.0 * smoothstep (2000, 4000, f) * (1.0 - smoothstep (6000, 9000, f));
            }
            case Delivery::Energetic:
            {
                double lf = std::log2 (std::max (f, 1.0) / 3000.0);
                return 4.0 * std::exp (-lf * lf / 0.6) - 1.0 * (1.0 - smoothstep (120, 300, f));
            }
            default: return 0.0;
        }
    }

    double crewFormantBase (Crew c)   { return c == Crew::Deeper ? -0.07 : c == Crew::Lighter ? 0.07 : 0.0; }
    double crewFormantSpread (Crew c) { return c == Crew::Wide ? 0.07 : 0.02; }

    // Smooth random curve in [-1, 1]: sum of slow sines.
    struct SlowNoise
    {
        double f[3], ph[3];
        explicit SlowNoise (Rng& r, double minHz = 0.3, double maxHz = 1.4)
        {
            for (int i = 0; i < 3; ++i) { f[i] = minHz + (maxHz - minHz) * r.uniform(); ph[i] = 2 * kPi * r.uniform(); }
        }
        double at (double t) const
        {
            double v = 0;
            for (int i = 0; i < 3; ++i) v += std::sin (2 * kPi * f[i] * t + ph[i]);
            return v / 3.0;
        }
    };

    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        float process (float x)
        {
            double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return (float) y;
        }
        static Biquad highpass (double fs, double f, double q = 0.707)
        {
            Biquad b; double w = 2 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q), a0 = 1 + al;
            b.b0 = (1 + c) / 2 / a0; b.b1 = -(1 + c) / a0; b.b2 = (1 + c) / 2 / a0; b.a1 = -2 * c / a0; b.a2 = (1 - al) / a0;
            return b;
        }
        static Biquad highShelf (double fs, double f, double dB)
        {
            Biquad b; double A = std::pow (10.0, dB / 40.0), w = 2 * kPi * f / fs, c = std::cos (w), s = std::sin (w);
            double al = s / 2 * std::sqrt (2.0), sa = 2 * std::sqrt (A) * al;
            double a0 = (A + 1) - (A - 1) * c + sa;
            b.b0 = A * ((A + 1) + (A - 1) * c + sa) / a0;
            b.b1 = -2 * A * ((A - 1) + (A + 1) * c) / a0;
            b.b2 = A * ((A + 1) + (A - 1) * c - sa) / a0;
            b.a1 = 2 * ((A - 1) - (A + 1) * c) / a0;
            b.a2 = ((A + 1) - (A - 1) * c - sa) / a0;
            return b;
        }
    };
}

std::shared_ptr<LeadAnalysis> analyseLead (const float* vocal, int n, int fs, const ProgressFn& progress)
{
    auto report = [&] (float p) { return progress ? progress (p) : true; };
    if (n < fs / 20) return nullptr;

    auto la = std::make_shared<LeadAnalysis>();
    la->sampleRate = fs;
    la->numSamples = n;
    la->framePeriod = 5.0;

    std::vector<double> x (vocal, vocal + n);

    HarvestOption ho; InitializeHarvestOption (&ho);
    ho.frame_period = la->framePeriod; ho.f0_floor = 65.0; ho.f0_ceil = 1100.0;
    const int F = GetSamplesForHarvest (fs, n, la->framePeriod);
    std::vector<double> tpos (F);
    la->f0.assign (F, 0.0);
    Harvest (x.data(), n, fs, &ho, tpos.data(), la->f0.data());
    if (! report (0.55f)) return nullptr;

    // Octave-error repair: a short run at ~2x or ~0.5x of its surroundings is a tracking
    // mistake, not a sung note (it caused audible jumps in the harmonies).
    {
        auto& f = la->f0;
        std::vector<double> ref (F, 0.0);
        for (int i = 0; i < F; ++i)
        {
            if (f[i] <= 0) continue;
            std::vector<double> w;
            for (int j = std::max (0, i - 25); j <= std::min (F - 1, i + 25); ++j)
                if (f[j] > 0) w.push_back (std::log2 (f[j]));
            std::nth_element (w.begin(), w.begin() + (long) w.size() / 2, w.end());
            ref[i] = w[w.size() / 2];
        }
        for (int i = 0; i < F; ++i)
        {
            if (f[i] <= 0) continue;
            const double d = std::log2 (f[i]) - ref[i];
            if (std::abs (d - 1.0) < 0.2) f[i] *= 0.5;
            else if (std::abs (d + 1.0) < 0.2) f[i] *= 2.0;
        }
    }

    CheapTrickOption co; InitializeCheapTrickOption (fs, &co);
    co.f0_floor = 71.0;
    co.fft_size = GetFFTSizeForCheapTrick (fs, &co);
    la->fftSize = co.fft_size;
    const int bins = la->fftSize / 2 + 1;

    la->sp.assign (F, std::vector<double> (bins));
    la->ap.assign (F, std::vector<double> (bins));
    std::vector<double*> spP (F), apP (F);
    for (int i = 0; i < F; ++i) { spP[i] = la->sp[i].data(); apP[i] = la->ap[i].data(); }

    // CheapTrick and D4C only need f0: run them side by side
    std::thread ct ([&] { CheapTrick (x.data(), n, fs, tpos.data(), la->f0.data(), F, &co, spP.data()); });
    D4COption dop; InitializeD4COption (&dop);
    D4C (x.data(), n, fs, tpos.data(), la->f0.data(), F, la->fftSize, &dop, apP.data());
    ct.join();
    if (! report (1.0f)) return nullptr;

    la->notes = segmentNotes (la->f0, la->framePeriod, la->frameNote);
    return la;
}

bool renderStack (const LeadAnalysis& la, const std::vector<ChordSeg>& chords, double partOffset,
                  const Key& key, const StackSettings& st, RenderedStack& out, const ProgressFn& progress)
{
    out = {};
    out.sampleRate = la.sampleRate;
    const int F = (int) la.f0.size();
    if (F == 0) return false;

    const int fs = la.sampleRate, n = la.numSamples, fftSize = la.fftSize, bins = fftSize / 2 + 1;
    const double framePeriod = la.framePeriod;
    const auto& f0 = la.f0;
    const auto& sp = la.sp;
    const auto& ap = la.ap;
    const auto& frameNote = la.frameNote;

    out.notes = la.notes;
    out.lines = generateHarmony (out.notes, chords, partOffset, key, st);

    // Smoothed log-f0 (for vibrato depth control)
    std::vector<double> logf (F, 0.0), logfSmooth (F, 0.0);
    for (int i = 0; i < F; ++i) if (f0[i] > 0) logf[i] = std::log (f0[i]);
    for (int i = 0; i < F; ++i)
    {
        if (f0[i] <= 0) continue;
        double s = 0; int c = 0;
        for (int j = std::max (0, i - 15); j <= std::min (F - 1, i + 15); ++j)
            if (f0[j] > 0) { s += logf[j]; ++c; }
        logfSmooth[i] = s / c;
    }

    const HumanizeParams hp = humanizeFor (st.humanize);
    const DeliveryParams dp = deliveryFor (st.delivery);

    std::vector<double> deliveryGain (bins), breathCurve (bins);
    for (int k = 0; k < bins; ++k)
    {
        const double f = k * (double) fs / fftSize;
        deliveryGain[k] = std::pow (10.0, deliveryDb (st.delivery, f) / 10.0);   // power domain
        breathCurve[k] = smoothstep (1200.0, 5000.0, f);                         // breath lives up high
    }

    const int V = (int) out.lines.size() * 2;   // every line sung twice: L + R
    if (V == 0) return false;
    auto panFor = [] (LineType t)
    {
        switch (t)
        {
            case LineType::Double:     return 0.8f;    // doubles wide, like a stereo doubler
            case LineType::PedalRoot:  return 0.35f;
            case LineType::PedalFifth: return 0.5f;
            case LineType::ThirdUp:    case LineType::ThirdDown: return 0.45f;
            case LineType::FifthUp:    case LineType::FifthDown: return 0.6f;
            case LineType::OctaveUp:   case LineType::OctaveDown: return 0.7f;
            default:                   return 0.5f;
        }
    };
    const float norm = 0.5f / std::sqrt (V / 4.0f);

    out.voices.resize (V);
    std::atomic<int> nextVoice { 0 }, doneVoices { 0 };
    std::atomic<bool> cancelled { false };

    auto renderVoice = [&] (int v, std::vector<std::vector<double>>& vsp, std::vector<std::vector<double>>& vap,
                            std::vector<const double*>& vspP, std::vector<const double*>& vapP,
                            std::vector<double>& vf0, std::vector<double>& y, std::vector<double>& tmp)
    {
        const auto& line = out.lines[(size_t) v / 2];
        const int side = (v % 2 == 0) ? -1 : 1;
        const bool lowLine = line.type == LineType::ThirdDown || line.type == LineType::FifthDown
                             || line.type == LineType::OctaveDown || line.type == LineType::ChordLow;
        Rng rng (st.seed * 7919u + (uint32_t) v * 104729u + 17u);

        // Semitone shift per source frame (unvoiced frames hold the nearest note)
        std::vector<double> semis (F, 0.0);
        {
            int last = -1;
            for (int i = 0; i < F; ++i) { if (frameNote[i] >= 0) last = frameNote[i]; semis[i] = last >= 0 ? line.semis[last] : std::numeric_limits<double>::quiet_NaN(); }
            int next = -1;
            for (int i = F - 1; i >= 0; --i) { if (frameNote[i] >= 0) next = frameNote[i]; if (std::isnan (semis[i])) semis[i] = next >= 0 ? line.semis[next] : 0.0; }
            std::vector<double> s2 (semis);
            for (int i = 0; i < F; ++i)          // ~35 ms glide between notes
            {
                double s = 0; int c = 0;
                for (int j = std::max (0, i - 3); j <= std::min (F - 1, i + 3); ++j) { s += semis[j]; ++c; }
                s2[i] = s / c;
            }
            semis.swap (s2);
        }

        // ---------------- this voice's "take" ----------------
        const size_t numNotes = out.notes.size();
        const double sign = side < 0 ? 1.0 : (rng.uniform() < 0.6 ? -1.0 : 1.0);   // L sits late, R may push
        const double baseMs = sign * (hp.delayMinMs + (hp.delayMaxMs - hp.delayMinMs) * rng.uniform());
        std::vector<double> noteOffMs (numNotes), noteCents (numNotes), noteScoop (numNotes);
        for (size_t k = 0; k < numNotes; ++k)
        {
            noteOffMs[k] = baseMs + hp.noteJitterMs * rng.bipolar();
            noteCents[k] = hp.noteCents * rng.bipolar();
            noteScoop[k] = hp.scoopCents * (0.3 + 0.7 * rng.uniform());
        }

        // timing offset per output frame: held per note, smooth moves between notes
        std::vector<double> offFr (F, baseMs / framePeriod);
        {
            int last = -1;
            for (int i = 0; i < F; ++i) { if (frameNote[i] >= 0) last = frameNote[i]; if (last >= 0) offFr[i] = noteOffMs[(size_t) last] / framePeriod; }
            int next = -1;
            for (int i = F - 1; i >= 0; --i) { if (frameNote[i] >= 0) next = frameNote[i]; else if (next >= 0 && i < (numNotes ? out.notes[0].firstFrame : 0)) offFr[i] = noteOffMs[(size_t) next] / framePeriod; }
            // moving average (120 ms): slope stays < 1 so time never runs backwards
            const int W = std::max (1, (int) std::lround (120.0 / framePeriod));
            std::vector<double> sm (F);
            double acc = 0; std::vector<double> pre (F + 1, 0.0);
            for (int i = 0; i < F; ++i) pre[(size_t) i + 1] = pre[(size_t) i] + offFr[(size_t) i];
            for (int i = 0; i < F; ++i)
            {
                const int a0 = std::max (0, i - W / 2), a1 = std::min (F, i + W / 2 + 1);
                sm[(size_t) i] = (pre[(size_t) a1] - pre[(size_t) a0]) / (a1 - a0);
            }
            (void) acc;
            offFr.swap (sm);
        }

        // pitch variation per source frame (cents -> semitones): per-note offset + scoop into the note
        std::vector<double> varSemis (F, 0.0);
        for (int j = 0; j < F; ++j)
        {
            const int k = frameNote[j];
            if (k < 0) continue;
            const double intoNote = (j - out.notes[(size_t) k].firstFrame) * framePeriod;   // ms
            varSemis[(size_t) j] = (noteCents[(size_t) k] - noteScoop[(size_t) k] * std::exp (-intoNote / 45.0)) / 100.0;
        }

        SlowNoise timeNoise (rng, 0.5, 2.0), pitchNoise (rng, 0.2, 0.9), levelNoise (rng, 0.3, 1.2);
        const double detune = hp.detuneCents * rng.bipolar() / 100.0;
        const double vibrato = dp.vibrato * (0.55 + 0.45 * rng.uniform());
        double alpha = 1.0 + crewFormantBase (st.crew) + crewFormantSpread (st.crew) * rng.bipolar() + dp.formant
                       + hp.formantJitter * rng.bipolar();
        if (lowLine) alpha -= 0.015;          // low lines sound like lower singers
        const double breath = dp.breath + 0.03 * rng.uniform();
        const double pedalHz = line.pedalMidi >= 0 ? 440.0 * std::pow (2.0, (line.pedalMidi - 69) / 12.0) : 0.0;

        for (int i = 0; i < F; ++i)
        {
            const double t = i * framePeriod / 1000.0;
            double src = i - offFr[(size_t) i] - 0.25 * hp.noteJitterMs * timeNoise.at (t) / framePeriod;
            src = std::clamp (src, 0.0, (double) (F - 1));
            const int i0 = (int) src, i1 = std::min (F - 1, i0 + 1);
            const double fr = src - i0;

            double f;
            if (f0[i0] > 0 && f0[i1] > 0)
            {
                double lf = (1 - fr) * logf[i0] + fr * logf[i1];
                double ls = (1 - fr) * logfSmooth[i0] + fr * logfSmooth[i1];
                lf = ls + vibrato * (lf - ls);
                f = std::exp (lf);
            }
            else
                f = fr < 0.5 ? f0[i0] : f0[i1];

            if (f > 0)
            {
                const double var = (1 - fr) * varSemis[(size_t) i0] + fr * varSemis[(size_t) i1]
                                   + detune + hp.driftCents / 100.0 * pitchNoise.at (t);
                if (pedalHz > 0)
                {
                    // UNIS: one constant note; keep only a little of the lead's own life (vibrato, tuning)
                    const int k = frameNote[(size_t) (fr < 0.5 ? i0 : i1)];
                    double dev = 0.0;
                    if (k >= 0) dev = 12.0 * std::log2 (f / (440.0 * std::pow (2.0, (out.notes[(size_t) k].midi - 69) / 12.0)));
                    dev = std::clamp (dev, -0.5, 0.5) * 0.6;
                    f = pedalHz * std::pow (2.0, (dev + var) / 12.0);
                }
                else
                {
                    const double sh = (1 - fr) * semis[i0] + fr * semis[i1] + var;
                    f *= std::pow (2.0, sh / 12.0);
                }
            }
            vf0[i] = f;

            for (int k = 0; k < bins; ++k) tmp[k] = (1 - fr) * sp[i0][k] + fr * sp[i1][k];
            for (int k = 0; k < bins; ++k)
            {
                double sk = k / alpha;
                int k0 = std::min ((int) sk, bins - 1), k1 = std::min (k0 + 1, bins - 1);
                double kf = sk - (int) sk;
                vsp[i][k] = std::max (1e-16, ((1 - kf) * tmp[k0] + kf * tmp[k1]) * deliveryGain[k]);
                double a = (1 - fr) * ap[i0][k] + fr * ap[i1][k] + breath * breathCurve[k];
                vap[i][k] = std::clamp (a, 0.001, 0.999);
            }
        }

        // own noise seed per voice: identical breath noise in every voice = phasing
        SynthesisSeeded (st.seed * 2654435761u + (unsigned int) v * 97u + 1u,
                         vf0.data(), F, vspP.data(), vapP.data(), fftSize, framePeriod, fs, n, y.data());

        // slow level ride (a real singer never sits at one level)
        for (int s2 = 0; s2 < n; ++s2)
            y[(size_t) s2] *= std::pow (10.0, hp.levelDb * levelNoise.at (s2 / (double) fs) / 20.0);

        RenderedVoice rv;
        rv.type = line.type;
        rv.side = side;
        rv.audio.resize (n);
        const int fade = std::max (1, fs / 100);
        for (int s = 0; s < n; ++s)
        {
            float g = 1.0f;
            if (s < fade) g = s / (float) fade;
            if (n - 1 - s < fade) g = std::min (g, (n - 1 - s) / (float) fade);
            rv.audio[s] = (float) y[s] * g;
        }
        rv.pan = panFor (line.type) * (float) side;
        rv.gain = norm * (float) dp.gain * (lowLine ? 0.9f : 1.0f) * (line.type == LineType::OctaveUp ? 0.8f : 1.0f);
        out.voices[v] = std::move (rv);
    };

    auto worker = [&]
    {
        std::vector<std::vector<double>> vsp (F, std::vector<double> (bins)), vap (F, std::vector<double> (bins));
        std::vector<const double*> vspP (F), vapP (F);
        for (int i = 0; i < F; ++i) { vspP[i] = vsp[i].data(); vapP[i] = vap[i].data(); }
        std::vector<double> vf0 (F), y (n), tmp (bins);
        for (;;)
        {
            int v = nextVoice++;
            if (v >= V || cancelled) break;
            renderVoice (v, vsp, vap, vspP, vapP, vf0, y, tmp);
            ++doneVoices;
        }
    };

    // Each worker needs its own spectrogram copy (~16 MB per 10 s), so cap the thread count.
    const int hw = (int) std::max (1u, std::thread::hardware_concurrency());
    const int T = std::clamp (std::min (hw - 1, V), 1, 6);
    std::vector<std::thread> threads;
    for (int t = 1; t < T; ++t) threads.emplace_back (worker);

    // The calling thread works too, and reports progress between voices.
    {
        std::vector<std::vector<double>> vsp (F, std::vector<double> (bins)), vap (F, std::vector<double> (bins));
        std::vector<const double*> vspP (F), vapP (F);
        for (int i = 0; i < F; ++i) { vspP[i] = vsp[i].data(); vapP[i] = vap[i].data(); }
        std::vector<double> vf0 (F), y (n), tmp (bins);
        for (;;)
        {
            if (progress && ! progress (doneVoices / (float) V)) cancelled = true;
            int v = nextVoice++;
            if (v >= V || cancelled) break;
            renderVoice (v, vsp, vap, vspP, vapP, vf0, y, tmp);
            ++doneVoices;
        }
    }
    for (auto& t : threads) t.join();
    if (progress) progress (1.0f);
    return ! cancelled;
}

bool renderStack (const float* vocal, int n, int fs, const std::vector<ChordSeg>& chords, double partOffset,
                  const Key& key, const StackSettings& st, RenderedStack& out, const ProgressFn& progress)
{
    auto la = analyseLead (vocal, n, fs, [&] (float p) { return progress ? progress (0.4f * p) : true; });
    if (! la) return false;
    return renderStack (*la, chords, partOffset, key, st, out,
                        [&] (float p) { return progress ? progress (0.4f + 0.6f * p) : true; });
}

void mixStack (const RenderedStack& stack, float sauce, std::vector<float>& L, std::vector<float>& R)
{
    size_t n = 0;
    for (const auto& v : stack.voices) n = std::max (n, v.audio.size());
    L.assign (n, 0.0f); R.assign (n, 0.0f);
    sauce = std::clamp (sauce, 0.0f, 1.0f);
    const float width = 0.25f + 0.75f * std::min (1.0f, sauce * 1.5f);

    for (const auto& v : stack.voices)
    {
        float p = std::clamp (v.pan * width, -1.0f, 1.0f);
        float ang = (p + 1.0f) * 0.25f * (float) kPi;
        float gl = std::cos (ang) * v.gain, gr = std::sin (ang) * v.gain;
        for (size_t i = 0; i < v.audio.size(); ++i) { L[i] += v.audio[i] * gl; R[i] += v.audio[i] * gr; }
    }

    const double fs = stack.sampleRate;
    Biquad hpL = Biquad::highpass (fs, 110.0), hpR = hpL;
    Biquad shL = Biquad::highShelf (fs, 9000.0, 5.0 * sauce), shR = shL;
    const float drive = 1.0f + 2.0f * sauce;
    const float satMix = 0.35f * sauce;
    for (size_t i = 0; i < n; ++i)
    {
        float l = shL.process (hpL.process (L[i]));
        float r = shR.process (hpR.process (R[i]));
        if (satMix > 0)
        {
            l = (1 - satMix) * l + satMix * std::tanh (l * drive) / drive;
            r = (1 - satMix) * r + satMix * std::tanh (r * drive) / drive;
        }
        L[i] = l; R[i] = r;
    }
}

} // namespace subharm
