#include "Dsp.h"

namespace atlas
{
static float medianOf (std::vector<float>& v)
{
    if (v.empty()) return 0.0f;
    auto mid = v.begin() + (long) (v.size() / 2);
    std::nth_element (v.begin(), mid, v.end());
    return *mid;
}

float medianRange (const Analysis& a, int s, int e, float fallback)
{
    std::vector<float> v;
    for (int f = std::max (0, s); f < std::min (e, a.numFrames()); ++f)
        if (a.f0[(size_t) f] > 0.0f)
            v.push_back (a.f0[(size_t) f]);
    return v.empty() ? fallback : medianOf (v);
}

// ---------------------------------------------------------------- analysis
std::shared_ptr<Analysis> analyse (const float* x, int n, double sr)
{
    auto a = std::make_shared<Analysis>();
    a->sampleRate = sr;
    a->numSamples = n;
    a->hop = sr > 70000.0 ? 512 : 256;
    const int hop = a->hop;
    const int nf = n / hop + 1;
    a->f0.assign ((size_t) nf, 0.0f);
    a->peak.assign ((size_t) nf, 0.0f);

    for (int f = 0; f < nf; ++f)
    {
        const int s0 = std::max (0, f * hop - hop / 2);
        const int s1 = std::min (n, f * hop + hop / 2);
        float m = 0.0f;
        for (int i = s0; i < s1; ++i) m = std::max (m, std::abs (x[i]));
        a->peak[(size_t) f] = m;
    }

    // decimate to ~11 kHz (box filter) to keep YIN cheap
    const int D = std::max (1, (int) std::lround (sr / 11025.0));
    const double dsr = sr / D;
    const int m = n / D;
    if (m < 1024) return a;

    std::vector<float> y ((size_t) m);
    for (int i = 0; i < m; ++i)
    {
        float s = 0.0f;
        for (int k = 0; k < D; ++k) s += x[i * D + k];
        y[(size_t) i] = s / (float) D;
    }

    const int tauMax = (int) (dsr / 70.0);
    const int tauMin = std::max (2, (int) (dsr / 1000.0));
    const int W = tauMax;
    std::vector<float> d ((size_t) tauMax + 2), cm ((size_t) tauMax + 2);

    for (int f = 0; f < nf; ++f)
    {
        const int centre = (f * hop) / D;
        const int s = centre - (W + tauMax) / 2;
        if (s < 0 || s + W + tauMax + 1 >= m) continue;

        float e = 0.0f;
        for (int j = 0; j < W; ++j) e += y[(size_t) (s + j)] * y[(size_t) (s + j)];
        if (e / (float) W < 1.0e-5f) continue;

        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float sum = 0.0f;
            for (int j = 0; j < W; ++j)
            {
                const float diff = y[(size_t) (s + j)] - y[(size_t) (s + j + tau)];
                sum += diff * diff;
            }
            d[(size_t) tau] = sum;
        }

        float running = 0.0f;
        cm[0] = 1.0f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            running += d[(size_t) tau];
            cm[(size_t) tau] = running > 0.0f ? d[(size_t) tau] * (float) tau / running : 1.0f;
        }

        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (cm[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < tauMax && cm[(size_t) tau + 1] < cm[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        }
        if (best < 0)
        {
            float mn = 1.0e9f;
            for (int tau = tauMin; tau < tauMax; ++tau)
                if (cm[(size_t) tau] < mn) { mn = cm[(size_t) tau]; best = tau; }
            if (mn > 0.3f) continue;
        }

        float tauF = (float) best;
        if (best > 1 && best < tauMax - 1)
        {
            const float a0 = cm[(size_t) best - 1], b0 = cm[(size_t) best], c0 = cm[(size_t) best + 1];
            const float den = a0 - 2.0f * b0 + c0;
            if (std::abs (den) > 1.0e-9f) tauF = (float) best + 0.5f * (a0 - c0) / den;
        }

        const float hz = (float) dsr / tauF;
        if (hz < 70.0f || hz > 1000.0f) continue;
        a->f0[(size_t) f] = hzToMidi (hz);
    }

    // median filter (5)
    {
        std::vector<float> g = a->f0;
        for (int f = 2; f < nf - 2; ++f)
        {
            if (a->f0[(size_t) f] <= 0.0f) continue;
            float vals[5];
            int c = 0;
            for (int k = -2; k <= 2; ++k)
                if (a->f0[(size_t) (f + k)] > 0.0f) vals[c++] = a->f0[(size_t) (f + k)];
            if (c >= 3) { std::sort (vals, vals + c); g[(size_t) f] = vals[c / 2]; }
        }
        a->f0 = g;
    }

    // drop voiced runs shorter than 4 frames
    for (int i = 0; i < nf;)
    {
        if (a->f0[(size_t) i] <= 0.0f) { ++i; continue; }
        int j = i;
        while (j < nf && a->f0[(size_t) j] > 0.0f) ++j;
        if (j - i < 4) for (int k = i; k < j; ++k) a->f0[(size_t) k] = 0.0f;
        i = j;
    }

    // fill short gaps (<= 4 frames) between similar pitches
    for (int i = 0; i < nf;)
    {
        if (a->f0[(size_t) i] > 0.0f) { ++i; continue; }
        int j = i;
        while (j < nf && a->f0[(size_t) j] <= 0.0f) ++j;
        if (i > 0 && j < nf && j - i <= 4 && std::abs (a->f0[(size_t) i - 1] - a->f0[(size_t) j]) < 1.0f)
        {
            const float p0 = a->f0[(size_t) i - 1], p1 = a->f0[(size_t) j];
            for (int k = i; k < j; ++k)
                a->f0[(size_t) k] = p0 + (p1 - p0) * (float) (k - i + 1) / (float) (j - i + 1);
        }
        i = j;
    }
    return a;
}

// ------------------------------------------------------------ segmentation
std::vector<Note> segmentNotes (const Analysis& a, std::atomic<int>& nextId)
{
    std::vector<Note> out;
    const int nf = a.numFrames();
    int i = 0;
    while (i < nf)
    {
        if (a.f0[(size_t) i] <= 0.0f) { ++i; continue; }
        const int start = i;
        std::vector<float> vals;
        int j = i, lastVoiced = i;
        while (j < nf)
        {
            const float p = a.f0[(size_t) j];
            if (p <= 0.0f) break;
            if (vals.size() >= 6)
            {
                const size_t take = std::min<size_t> (vals.size(), 24);
                std::vector<float> recent (vals.end() - (long) take, vals.end());
                const float med = medianOf (recent);
                if (std::abs (p - med) > 0.9f)
                {
                    int cnt = 0, k = j;
                    while (k < nf && cnt < 6 && a.f0[(size_t) k] > 0.0f && std::abs (a.f0[(size_t) k] - med) > 0.9f)
                    {
                        ++k; ++cnt;
                    }
                    if (cnt >= 6) break;
                }
            }
            vals.push_back (p);
            lastVoiced = j;
            ++j;
        }
        const int end = lastVoiced + 1;
        if (end - start >= 5)
        {
            Note n;
            n.id = nextId++;
            n.start = start;
            n.end = end;
            n.center = medianOf (vals);
            n.target = n.center;
            n.layer = 0;
            out.push_back (n);
        }
        i = end;
    }
    return out;
}

// ------------------------------------------------------------- key finder
static double correlate (const double* x, const double* y)
{
    double mx = 0, my = 0;
    for (int i = 0; i < 12; ++i) { mx += x[i]; my += y[i]; }
    mx /= 12; my /= 12;
    double sxy = 0, sxx = 0, syy = 0;
    for (int i = 0; i < 12; ++i)
    {
        sxy += (x[i] - mx) * (y[i] - my);
        sxx += (x[i] - mx) * (x[i] - mx);
        syy += (y[i] - my) * (y[i] - my);
    }
    return (sxx > 0 && syy > 0) ? sxy / std::sqrt (sxx * syy) : 0.0;
}

std::pair<int, int> detectKey (const std::vector<Note>& notes)
{
    double hist[12] = {};
    double total = 0;
    for (auto& n : notes)
    {
        if (n.layer != 0) continue;
        const int pc = (((int) std::lround (n.center)) % 12 + 12) % 12;
        hist[pc] += (double) (n.end - n.start);
        total += (double) (n.end - n.start);
    }
    if (total <= 0) return { -1, 0 };

    static const double maj[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    static const double min[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

    double bestScore = -2.0;
    std::pair<int, int> best { 0, 0 };
    for (int k = 0; k < 12; ++k)
    {
        double rot[12];
        for (int i = 0; i < 12; ++i) rot[i] = hist[(i + k) % 12];
        const double cMaj = correlate (rot, maj), cMin = correlate (rot, min);
        if (cMaj > bestScore) { bestScore = cMaj; best = { k, 0 }; }
        if (cMin > bestScore) { bestScore = cMin; best = { k, 1 }; }
    }
    return best;
}

// ------------------------------------------------------------------ render
void renderLayer (const float* src, int N, const Analysis& a,
                  const std::vector<Note>& notes, int layer,
                  float variation, float glideMs, bool keepOriginal,
                  std::vector<float>& out, const std::atomic<bool>& abort)
{
    const int hop = a.hop;
    const double sr = a.sampleRate;
    const int nf = a.numFrames();
    out.assign ((size_t) N, 0.0f);
    if (nf < 2 || N < 1024) return;

    std::vector<float> wt ((size_t) N, 0.0f);

    // hold-filled source pitch
    std::vector<float> f0h ((size_t) nf, 0.0f);
    {
        float last = 0.0f;
        for (int f = 0; f < nf; ++f) { if (a.f0[(size_t) f] > 0.0f) last = a.f0[(size_t) f]; f0h[(size_t) f] = last; }
        float next = 0.0f;
        for (int f = nf - 1; f >= 0; --f) { if (a.f0[(size_t) f] > 0.0f) next = a.f0[(size_t) f]; if (f0h[(size_t) f] <= 0.0f) f0h[(size_t) f] = next; }
    }

    // per-frame step target / centre for this layer
    std::vector<float> step ((size_t) nf, -1.0f), cen ((size_t) nf, 0.0f);
    for (auto& nt : notes)
    {
        if (nt.layer != layer) continue;
        for (int f = std::max (0, nt.start); f < std::min (nf, nt.end); ++f)
        {
            step[(size_t) f] = nt.target;
            cen[(size_t) f] = nt.center;
        }
    }

    // glide: box-smooth the step function so jumps between notes become slides
    std::vector<float> stepS = step;
    const int gl = (int) std::lround (glideMs * 0.001 * sr / hop);
    if (gl >= 2)
    {
        const int hw = gl / 2;
        for (int f = 0; f < nf; ++f)
        {
            if (step[(size_t) f] <= 0.0f) continue;
            float sum = 0.0f; int c = 0;
            for (int k = std::max (0, f - hw); k <= std::min (nf - 1, f + hw); ++k)
                if (step[(size_t) k] > 0.0f) { sum += step[(size_t) k]; ++c; }
            if (c > 0) stepS[(size_t) f] = sum / (float) c;
        }
    }

    auto tgtFrame = [&] (int f) { return stepS[(size_t) f] + variation * (f0h[(size_t) f] - cen[(size_t) f]); };

    for (auto& nt : notes)
    {
        if (nt.layer != layer) continue;
        if (abort.load()) return;

        const int fs = juce::jlimit (0, nf - 1, nt.start);
        const int fe = juce::jlimit (fs + 1, nf, nt.end);

        int voiced = 0;
        for (int f = fs; f < fe; ++f) if (a.f0[(size_t) f] > 0.0f) ++voiced;
        if (voiced < 3 || voiced < (fe - fs) / 2) continue;

        const int s0 = fs * hop - hop / 2;
        const int s1 = fe * hop - hop / 2;
        const int fade = std::max (16, std::min ((int) (0.010 * sr), (s1 - s0) / 2));
        const int rs = std::max (0, s0 - fade / 2);
        const int re = std::min (N, s1 + fade / 2);
        if (re - rs < 64) continue;

        auto frameVal = [&] (double sample, auto&& fn)
        {
            const double pos = sample / (double) hop;
            const int i0 = (int) std::floor (pos);
            const double fr = pos - (double) i0;
            const int c0 = juce::jlimit (fs, fe - 1, i0);
            const int c1 = juce::jlimit (fs, fe - 1, i0 + 1);
            return (float) ((1.0 - fr) * (double) fn (c0) + fr * (double) fn (c1));
        };
        auto srcAt = [&] (double s) { return frameVal (s, [&] (int f) { return f0h[(size_t) f]; }); };
        auto tgtAt = [&] (double s) { return juce::jlimit (30.0f, 100.0f, frameVal (s, tgtFrame)); };

        // pitch marks (waveform peaks one period apart)
        float mx = -1.0e9f, mn = 1.0e9f;
        for (int i = rs; i < re; i += 3) { mx = std::max (mx, src[i]); mn = std::min (mn, src[i]); }
        const float pol = (mx >= -mn) ? 1.0f : -1.0f;

        auto argmax = [&] (int lo, int hi)
        {
            lo = juce::jlimit (0, N - 1, lo);
            hi = juce::jlimit (0, N - 1, hi);
            int best = lo;
            float bv = -1.0e9f;
            for (int i = lo; i <= hi; ++i)
                if (pol * src[i] > bv) { bv = pol * src[i]; best = i; }
            return best;
        };

        std::vector<int> marks;
        {
            const double T0 = sr / (double) midiToHz (srcAt ((double) rs));
            int m = argmax (rs - (int) (T0 * 0.5), rs + (int) (T0 * 0.5));
            marks.push_back (m);
            while (m < re)
            {
                const double T = sr / (double) midiToHz (srcAt ((double) m));
                const int c = m + (int) std::lround (T);
                const int qlo = c - (int) (T * 0.25), qhi = c + (int) (T * 0.25);
                if (qhi >= N) break;
                int nm = argmax (qlo, qhi);
                if (nm <= marks.back()) nm = marks.back() + std::max (1, (int) T);
                marks.push_back (nm);
                m = nm;
            }
        }
        if (marks.size() < 2) continue;

        // overlap-add synthesis, time unchanged, spacing = target period
        std::vector<float> loc ((size_t) (re - rs), 0.0f), lw ((size_t) (re - rs), 0.0f);
        double ts = (double) rs;
        size_t mi = 0;
        const float pi = juce::MathConstants<float>::pi;
        while (ts < (double) re)
        {
            const double Ts = sr / (double) midiToHz (srcAt (ts));
            const double Tt = sr / (double) midiToHz (tgtAt (ts));
            while (mi + 1 < marks.size() && std::abs ((double) marks[mi + 1] - ts) <= std::abs ((double) marks[mi] - ts)) ++mi;
            const int mk = marks[mi];
            const int L = std::max (16, (int) std::lround (Ts));
            const int ctr = (int) std::lround (ts);
            for (int k = -L; k <= L; ++k)
            {
                const int ni = mk + k, no = ctr + k;
                if (ni < 0 || ni >= N || no < rs || no >= re) continue;
                const float w = 0.5f * (1.0f + std::cos (pi * (float) k / (float) L));
                loc[(size_t) (no - rs)] += src[ni] * w;
                lw[(size_t) (no - rs)] += w;
            }
            ts += std::max (16.0, Tt);
        }

        auto ramp = [pi] (float t) { return t <= 0.0f ? 0.0f : (t >= 1.0f ? 1.0f : 0.5f - 0.5f * std::cos (pi * t)); };
        for (int n = rs; n < re; ++n)
        {
            const float syn = loc[(size_t) (n - rs)] / std::max (lw[(size_t) (n - rs)], 0.25f);
            float fin = 1.0f, fout = 1.0f;
            if (n < s0 + fade / 2) fin = ramp ((float) (n - (s0 - fade / 2)) / (float) fade);
            if (n > s1 - fade / 2) fout = ramp ((float) ((s1 + fade / 2) - n) / (float) fade);
            const float w = std::min (fin, fout);
            out[(size_t) n] += syn * w;
            wt[(size_t) n] += w;
        }
    }

    for (int n = 0; n < N; ++n)
    {
        const float w = wt[(size_t) n];
        if (keepOriginal)
            out[(size_t) n] = w > 1.0f ? out[(size_t) n] / w : out[(size_t) n] + src[n] * (1.0f - w);
        else
            out[(size_t) n] = out[(size_t) n] / std::max (1.0f, w);
    }
}
} // namespace atlas
