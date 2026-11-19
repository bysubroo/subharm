#include "ChordAnalysis.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "world/fft.h"

namespace subharm
{

namespace
{
    constexpr int kNumQualities = (int) Quality::Count;

    // How much each quality is trusted a priori: plain triads win ties against 7ths.
    const double kQualityPrior[kNumQualities] = { 0.0, 0.0, -0.05, -0.05, -0.06, -0.09, -0.09 };

    int nextPow2 (int v) { int p = 1; while (p < v) p <<= 1; return p; }

    std::array<double, 12> makeTemplate (int root, Quality q)
    {
        std::array<double, 12> t {};
        Chord c { root, q };
        auto iv = c.intervals();
        const double w[4] = { 1.0, 0.9, 0.75, 0.65 };
        for (size_t i = 0; i < iv.size(); ++i)
            t[(root + iv[i]) % 12] = w[std::min<size_t> (i, 3)];
        return t;
    }

    double cosine (const std::array<double, 12>& a, const std::array<double, 12>& b)
    {
        double ab = 0, aa = 0, bb = 0;
        for (int i = 0; i < 12; ++i) { ab += a[i] * b[i]; aa += a[i] * a[i]; bb += b[i] * b[i]; }
        if (aa <= 0 || bb <= 0) return 0;
        return ab / std::sqrt (aa * bb);
    }
}

Key detectKey (const std::array<double, 12>& chroma)
{
    static const double majP[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    static const double minP[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

    auto corr = [&] (const double* prof, int tonic)
    {
        double mx = 0, my = 0;
        for (int i = 0; i < 12; ++i) { mx += chroma[(i + tonic) % 12]; my += prof[i]; }
        mx /= 12; my /= 12;
        double sxy = 0, sxx = 0, syy = 0;
        for (int i = 0; i < 12; ++i)
        {
            double x = chroma[(i + tonic) % 12] - mx, y = prof[i] - my;
            sxy += x * y; sxx += x * x; syy += y * y;
        }
        return (sxx <= 0 || syy <= 0) ? -1.0 : sxy / std::sqrt (sxx * syy);
    };

    Key best; double bestScore = -2;
    for (int t = 0; t < 12; ++t)
    {
        double a = corr (majP, t), b = corr (minP, t);
        if (a > bestScore) { bestScore = a; best = { t, false }; }
        if (b > bestScore) { bestScore = b; best = { t, true }; }
    }
    return best;
}

ChordAnalysisResult analyseChords (const float* x, int n, double fs, double bpm, double firstBeatOffset)
{
    ChordAnalysisResult result;
    if (n <= 0 || fs <= 0)
        return result;

    // ---- Chroma per STFT frame (treble 80..2100 Hz, bass 40..260 Hz) ----
    const int N = nextPow2 ((int) (fs * 0.17));       // ~8192 @ 48k
    const int hop = N / 4;
    std::vector<double> window (N);
    for (int i = 0; i < N; ++i)
        window[i] = 0.5 - 0.5 * std::cos (2.0 * kPi * i / (N - 1));

    std::vector<double> in (N);
    std::vector<std::array<double, 2>> outBuf ((size_t) N / 2 + 1);
    auto* out = reinterpret_cast<fft_complex*> (outBuf.data());
    fft_plan plan = fft_plan_dft_r2c_1d (N, in.data(), out, FFT_ESTIMATE);

    // Bin -> semitone (MIDI) map. We build a linear-magnitude semitone spectrum first,
    // then remove the energy that is just overtones of lower notes (the 3rd harmonic of
    // E is a B, which otherwise turns every C into Cmaj7), then fold to chroma.
    const int loMidi = 28, hiMidi = 100;   // E1 .. E7
    std::vector<int> binMidi (N / 2 + 1, -1);
    std::vector<double> binW (N / 2 + 1, 0.0);
    for (int k = 1; k <= N / 2; ++k)
    {
        double f = k * fs / N;
        double midi = 69.0 + 12.0 * std::log2 (f / 440.0);
        double nearest = std::round (midi);
        if (nearest < loMidi || nearest > hiMidi) continue;
        binMidi[k] = (int) nearest;
        binW[k] = std::max (0.0, 1.0 - 2.0 * std::abs (midi - nearest));
    }
    const int harmSemis[4] = { 12, 19, 24, 28 };       // harmonics 2..5
    const double harmAmp[4] = { 0.5, 0.33, 0.25, 0.2 };

    struct Frame { double t; std::array<double, 12> treble, bass; double energy; };
    std::vector<Frame> frames;
    std::vector<double> semi (hiMidi + 1);
    for (int start = 0; start + N / 2 < n; start += hop)
    {
        for (int i = 0; i < N; ++i)
        {
            int idx = start - N / 2 + i;
            in[i] = (idx >= 0 && idx < n) ? x[idx] * window[i] : 0.0;
        }
        fft_execute (plan);

        std::fill (semi.begin(), semi.end(), 0.0);
        Frame fr {};
        fr.t = start / fs;
        for (int k = 1; k <= N / 2; ++k)
        {
            double mag = std::sqrt (out[k][0] * out[k][0] + out[k][1] * out[k][1]);
            fr.energy += mag * mag;
            if (binMidi[k] >= 0) semi[binMidi[k]] = std::max (semi[binMidi[k]], mag * binW[k]);
        }
        // local-peak picking: a semitone only counts if it stands above its neighbours
        std::vector<double> peak (hiMidi + 1, 0.0);
        for (int m = loMidi; m <= hiMidi; ++m)
        {
            double l = m > loMidi ? semi[m - 1] : 0, r = m < hiMidi ? semi[m + 1] : 0;
            if (semi[m] >= l && semi[m] >= r) peak[m] = semi[m];
        }
        for (int m = loMidi; m <= hiMidi; ++m)          // low to high: strip overtones
        {
            if (peak[m] <= 0) continue;
            for (int h = 0; h < 4; ++h)
            {
                int mh = m + harmSemis[h];
                if (mh <= hiMidi) peak[mh] = std::max (0.0, peak[mh] - peak[m] * harmAmp[h]);
            }
        }
        for (int m = loMidi; m <= hiMidi; ++m)
        {
            if (peak[m] <= 0) continue;
            double v = std::log1p (peak[m] * 4.0);
            int pc = m % 12;
            double hz = 440.0 * std::pow (2.0, (m - 69) / 12.0);
            if (hz >= 80.0 && hz <= 2100.0) fr.treble[pc] += v;
            if (hz <= 260.0) fr.bass[pc] += v;
        }
        frames.push_back (fr);
    }
    fft_destroy_plan (plan);

    // ---- Beat grid ----
    const double beatLen = bpm > 0 ? 60.0 / bpm : 0.5;
    const double dur = n / fs;
    std::vector<double> bounds;
    bounds.push_back (0.0);
    double first = firstBeatOffset;
    while (first > beatLen) first -= beatLen;
    if (first > 0.05) bounds.push_back (first);
    for (double b = first + beatLen; b < dur - 0.05; b += beatLen)
        if (b > bounds.back() + 0.05) bounds.push_back (b);
    bounds.push_back (dur);

    const int B = (int) bounds.size() - 1;
    std::vector<std::array<double, 12>> beatTreble (B), beatBass (B);
    std::vector<double> beatEnergy (B, 0.0);
    std::array<double, 12> total {};
    double maxEnergy = 0;

    for (int b = 0; b < B; ++b)
    {
        std::array<double, 12> tr {}, bs {};
        double e = 0; int cnt = 0;
        for (const auto& fr : frames)
            if (fr.t >= bounds[b] && fr.t < bounds[b + 1])
            {
                for (int i = 0; i < 12; ++i) { tr[i] += fr.treble[i]; bs[i] += fr.bass[i]; }
                e += fr.energy; ++cnt;
            }
        if (cnt > 0) e /= cnt;
        // remove the "floor" so broadband noise (drums) does not look like every chord
        double mn = *std::min_element (tr.begin(), tr.end());
        for (auto& v : tr) v = std::max (0.0, v - mn);
        double mnb = *std::min_element (bs.begin(), bs.end());
        for (auto& v : bs) v = std::max (0.0, v - mnb);
        double mxb = *std::max_element (bs.begin(), bs.end());
        if (mxb > 0) for (auto& v : bs) v /= mxb;

        beatTreble[b] = tr; beatBass[b] = bs; beatEnergy[b] = e;
        maxEnergy = std::max (maxEnergy, e);
        for (int i = 0; i < 12; ++i) total[i] += tr[i];
    }

    Key chromaKey = detectKey (total);
    result.key = chromaKey;

    // ---- Viterbi over chord states ----
    const int S = 12 * kNumQualities + 1;   // last state = N.C.
    const int NC = S - 1;
    std::vector<std::array<double, 12>> templates (S - 1);
    for (int r = 0; r < 12; ++r)
        for (int q = 0; q < kNumQualities; ++q)
            templates[r * kNumQualities + q] = makeTemplate (r, (Quality) q);

    auto scalePcs = result.key.scalePitchClasses();
    auto inKey = [&] (int pc) { return std::find (scalePcs.begin(), scalePcs.end(), pc) != scalePcs.end(); };

    std::vector<std::vector<double>> emit (B, std::vector<double> (S, 0.0));
    for (int b = 0; b < B; ++b)
    {
        bool silent = beatEnergy[b] < maxEnergy * 1e-4 || maxEnergy <= 0;
        for (int s = 0; s < S - 1; ++s)
        {
            int r = s / kNumQualities, q = s % kNumQualities;
            double sc = cosine (beatTreble[b], templates[s]);
            sc += 0.22 * beatBass[b][r];                  // bass note supports the root
            sc += kQualityPrior[q];
            sc += inKey (r) ? 0.02 : 0.0;                 // gentle diatonic preference
            emit[b][s] = silent ? -1.0 : sc;
        }
        emit[b][NC] = silent ? 1.0 : 0.25;
    }

    const double changePenalty = 0.12;
    std::vector<std::vector<double>> score (B, std::vector<double> (S, 0.0));
    std::vector<std::vector<int>> back (B, std::vector<int> (S, 0));
    for (int s = 0; s < S; ++s) score[0][s] = emit[0][s];
    for (int b = 1; b < B; ++b)
    {
        int bestPrev = 0;
        for (int s = 1; s < S; ++s) if (score[b - 1][s] > score[b - 1][bestPrev]) bestPrev = s;
        for (int s = 0; s < S; ++s)
        {
            double stay = score[b - 1][s];
            double move = score[b - 1][bestPrev] - changePenalty;
            if (stay >= move) { score[b][s] = stay + emit[b][s]; back[b][s] = s; }
            else              { score[b][s] = move + emit[b][s]; back[b][s] = bestPrev; }
        }
    }
    std::vector<int> path (B);
    int last = 0;
    for (int s = 1; s < S; ++s) if (score[B - 1][s] > score[B - 1][last]) last = s;
    path[B - 1] = last;
    for (int b = B - 1; b > 0; --b) path[b - 1] = back[b][path[b]];

    for (int b = 0; b < B; ++b)
    {
        Chord c;
        if (path[b] != NC) { c.root = path[b] / kNumQualities; c.quality = (Quality) (path[b] % kNumQualities); }
        if (! result.chords.empty() && result.chords.back().chord == c)
            result.chords.back().end = bounds[b + 1];
        else
            result.chords.push_back ({ bounds[b], bounds[b + 1], c });
    }

    // Refine the key with the chord sequence: diatonic chords, tonic chord and the
    // first/last chord of the take all vote. The chroma key gets a head start.
    {
        double bestScore = -1e9; Key best = chromaKey;
        for (int t = 0; t < 12; ++t)
            for (int mi = 0; mi < 2; ++mi)
            {
                Key k { t, mi == 1 };
                auto sc = k.scalePitchClasses();
                auto in = [&] (int pc) { return std::find (sc.begin(), sc.end(), pc) != sc.end(); };
                double score = (k.tonic == chromaKey.tonic && k.minor == chromaKey.minor) ? 0.5 : 0.0;
                double totalDur = 0;
                for (size_t i = 0; i < result.chords.size(); ++i)
                {
                    const auto& c = result.chords[i];
                    if (c.chord.isNoChord()) continue;
                    double d = c.end - c.start; totalDur += d;
                    bool diatonic = true;
                    for (int iv : c.chord.intervals()) diatonic &= in ((c.chord.root + iv) % 12);
                    score += d * (diatonic ? 1.0 : -0.5);
                    bool tonicChord = c.chord.root == k.tonic
                                      && ((k.minor && (c.chord.quality == Quality::Min || c.chord.quality == Quality::Min7))
                                          || (! k.minor && (c.chord.quality == Quality::Maj || c.chord.quality == Quality::Maj7)));
                    if (tonicChord) score += d * 0.6 + ((i == 0 || i + 1 == result.chords.size()) ? 0.4 * d : 0.0);
                }
                if (totalDur > 0 && score > bestScore) { bestScore = score; best = k; }
            }
        result.key = best;
    }
    return result;
}

} // namespace subharm
