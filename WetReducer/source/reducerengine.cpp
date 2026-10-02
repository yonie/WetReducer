//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "reducerengine.h"

#include <algorithm>

namespace Yonie {

namespace {
constexpr double kPi = 3.14159265358979323846;

int nextPow2(int v)
{
    int p = 1;
    while (p < v)
        p <<= 1;
    return p;
}

// In-place iterative radix-2 FFT.
void fft(std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * kPi / static_cast<double>(len);
        const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w(1.0, 0.0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = a[i + k];
                const auto v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

// Band-limited step correction for the square wave.
double polyBlep(double t, double dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0;
    }
    if (t > 1.0 - dt)
    {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    return 0.0;
}
} // namespace

//------------------------------------------------------------------------
void Biquad::set(double fc, double fs, bool high)
{
    const double w = 2.0 * kPi * fc / fs;
    const double c = std::cos(w), s = std::sin(w);
    const double alpha = s / (2.0 * 0.70710678118654752);   // Butterworth Q
    const double a0 = 1.0 + alpha;
    if (high)
    {
        b0 = (1.0 + c) / 2.0 / a0;
        b1 = -(1.0 + c) / a0;
    }
    else
    {
        b0 = (1.0 - c) / 2.0 / a0;
        b1 = (1.0 - c) / a0;
    }
    b2 = b0;
    a1 = -2.0 * c / a0;
    a2 = (1.0 - alpha) / a0;
}

//------------------------------------------------------------------------
void Splitter::set(const double fc[3], double fs)
{
    for (int i = 0; i < 3; ++i)
    {
        lo[i].a.setLowpass(fc[i], fs);
        lo[i].b.setLowpass(fc[i], fs);
        hi[i].a.setHighpass(fc[i], fs);
        hi[i].b.setHighpass(fc[i], fs);
    }
}

void Splitter::reset()
{
    for (int i = 0; i < 3; ++i)
    {
        lo[i].reset();
        hi[i].reset();
    }
}

void Splitter::split(double x, double out[Reducer::kBands])
{
    double rest = x;
    for (int i = 0; i < 3; ++i)
    {
        out[i] = lo[i].tick(rest);
        rest = hi[i].tick(rest);
    }
    out[3] = rest;
}

//------------------------------------------------------------------------
void BandTone::prepare(double sampleRate, double lowEdge, double highEdge)
{
    fs = sampleRate;
    ring.assign(4 * Reducer::kMaxWindow, 0.0);
    setEdges(lowEdge, highEdge);
    reset();
}

void BandTone::setEdges(double lowEdge, double highEdge)
{
    lo = lowEdge;
    hi = highEdge;
    // Long enough to tell neighbouring notes apart at the band's bottom edge.
    window = std::min(Reducer::kMaxWindow, nextPow2(std::max(2048, static_cast<int>(8.0 * fs / std::max(lo, 20.0)))));
    hop = window >= 8192 ? 1024 : 256;      // a low band does not change note 170 times a second
    hold = std::max(1, static_cast<int>(Reducer::kHoldMs / 1000.0 * fs / hop)) | 1;
    hann.resize(window);
    for (int i = 0; i < window; ++i)
        hann[i] = 0.5 - 0.5 * std::cos(2.0 * kPi * i / (window - 1));
    fftBuf.assign(window, {});
    history.clear();
}

void BandTone::reset()
{
    std::fill(ring.begin(), ring.end(), 0.0);
    writePos = sinceHop = 0;
    history.clear();
    freq = freqTarget = 0.0;
    level = levelTarget = levelStep = 0.0;
    phase = 0.0;
}

void BandTone::analyse()
{
    // The window ends (kLatency - window/2) samples ago, so its centre is the sample being
    // played right now.
    const int size = static_cast<int>(ring.size());
    const int end = writePos - (Reducer::kLatency - window / 2);
    for (int i = 0; i < window; ++i)
    {
        const int p = ((end - window + i) % size + size) % size;
        fftBuf[i] = {ring[p] * hann[i], 0.0};
    }
    fft(fftBuf);

    const int k0 = std::max(1, static_cast<int>(lo * window / fs));
    const int k1 = std::min(window / 2 - 2, static_cast<int>(hi * window / fs) + 1);
    int best = k0;
    double bestMag = -1.0;
    for (int k = k0; k <= k1; ++k)
    {
        const double m = std::abs(fftBuf[k]);
        if (m > bestMag)
        {
            bestMag = m;
            best = k;
        }
    }
    double kf = best;
    if (best > 0 && best < window / 2 - 1)
    {
        const double a = std::log(std::abs(fftBuf[best - 1]) + 1e-12);
        const double b = std::log(std::abs(fftBuf[best]) + 1e-12);
        const double c = std::log(std::abs(fftBuf[best + 1]) + 1e-12);
        const double d = a - 2.0 * b + c;
        if (std::abs(d) > 1e-12)
            kf = best + 0.5 * (a - c) / d;
    }
    const double f = std::max(1.0, kf * fs / window);
    const int note = static_cast<int>(std::lround(69.0 + 12.0 * std::log2(f / 440.0)));

    // Hold: the tone follows the median of the last `hold` snapped notes.
    history.push_back(note);
    if (static_cast<int>(history.size()) > hold)
        history.erase(history.begin());
    std::vector<int> sorted(history);
    std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
    const int held = sorted[sorted.size() / 2];
    freqTarget = 440.0 * std::pow(2.0, (held - 69) / 12.0);
    if (freq <= 0.0)
        freq = freqTarget;

    // Level: RMS of one hop around the played sample, reached by the next analysis.
    double sum = 0.0;
    const int centre = end - window / 2;
    for (int i = -hop / 2; i < hop / 2; ++i)
    {
        const double v = ring[((centre + i) % size + size) % size];
        sum += v * v;
    }
    levelTarget = std::sqrt(sum / hop);
    levelStep = (levelTarget - level) / hop;
}

double BandTone::tick(double bandSample, double shape)
{
    ring[writePos] = bandSample;
    writePos = (writePos + 1) % static_cast<int>(ring.size());
    if (++sinceHop >= hop)
    {
        sinceHop = 0;
        analyse();
    }
    level += levelStep;
    if ((levelStep > 0 && level > levelTarget) || (levelStep < 0 && level < levelTarget))
        level = levelTarget;
    freq += (freqTarget - freq) * 0.02;     // a few milliseconds of glide, no clicks on a note change

    const double dt = freq / fs;
    phase += dt;
    if (phase >= 1.0)
        phase -= 1.0;
    // Equal loudness at both ends: a sine of peak sqrt(2)*level and a square of peak level
    // both have RMS = level.
    const double sine = std::sin(2.0 * kPi * phase) * std::sqrt(2.0);
    double square = phase < 0.5 ? 1.0 : -1.0;
    square += polyBlep(phase, dt);
    square -= polyBlep(std::fmod(phase + 0.5, 1.0), dt);
    return level * ((1.0 - shape) * sine + shape * square);
}

//------------------------------------------------------------------------
void ReducerEngine::prepare(double sampleRate)
{
    fs = sampleRate;
    for (auto& d : delay)
        d.assign(Reducer::kLatency, 0.0f);
    applyXovers();
    reset();
}

void ReducerEngine::reset()
{
    trackSplit.reset();
    for (auto& s : drySplit)
        s.reset();
    for (auto& t : tones)
        t.reset();
    for (auto& d : delay)
        std::fill(d.begin(), d.end(), 0.0f);
    delayPos = 0;
}

void ReducerEngine::applyXovers()
{
    double fc[3];
    for (int i = 0; i < 3; ++i)
        fc[i] = Reducer::kXover[i][std::clamp(current.xover[i], 0, 2)];
    trackSplit.set(fc, fs);
    for (auto& s : drySplit)
        s.set(fc, fs);
    const double edges[5] = {20.0, fc[0], fc[1], fc[2], std::min(20000.0, fs * 0.45)};
    for (int b = 0; b < Reducer::kBands; ++b)
    {
        tones[b].prepare(fs, edges[b], edges[b + 1]);
    }
}

void ReducerEngine::setSettings(const Settings& s)
{
    const bool xoverChanged = s.xover != current.xover;
    current = s;
    if (xoverChanged)
        applyXovers();
}

void ReducerEngine::process(const float* inL, const float* inR, float* outL, float* outR, int n)
{
    double shape[Reducer::kBands];
    for (int b = 0; b < Reducer::kBands; ++b)
        shape[b] = std::clamp((current.amount[b] - Reducer::kShapeStart) / (1.0 - Reducer::kShapeStart), 0.0, 1.0);

    for (int i = 0; i < n; ++i)
    {
        const double l = inL[i], r = inR[i];

        // Trackers run on the undelayed mono band signal.
        double mono[Reducer::kBands];
        trackSplit.split(0.5 * (l + r), mono);
        double tone[Reducer::kBands];
        for (int b = 0; b < Reducer::kBands; ++b)
            tone[b] = tones[b].tick(mono[b], shape[b]);

        // The music is played kLatency samples late, so the tone lines up with it.
        const double dl = delay[0][delayPos], dr = delay[1][delayPos];
        delay[0][delayPos] = static_cast<float>(l);
        delay[1][delayPos] = static_cast<float>(r);
        delayPos = (delayPos + 1) % Reducer::kLatency;

        double bl[Reducer::kBands], br[Reducer::kBands];
        drySplit[0].split(dl, bl);
        drySplit[1].split(dr, br);
        double yl = 0.0, yr = 0.0;
        for (int b = 0; b < Reducer::kBands; ++b)
        {
            const double v = current.amount[b];
            yl += (1.0 - v) * bl[b] + v * tone[b];
            yr += (1.0 - v) * br[b] + v * tone[b];
        }
        outL[i] = static_cast<float>(yl);
        outR[i] = static_cast<float>(yr);
    }
}

} // namespace Yonie
