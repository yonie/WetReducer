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
void Splitter::set(const double fc[Reducer::kXovers], double fs)
{
    for (int i = 0; i < Reducer::kXovers; ++i)
    {
        lo[i].a.setLowpass(fc[i], fs);
        lo[i].b.setLowpass(fc[i], fs);
        hi[i].a.setHighpass(fc[i], fs);
        hi[i].b.setHighpass(fc[i], fs);
    }
}

void Splitter::reset()
{
    for (int i = 0; i < Reducer::kXovers; ++i)
    {
        lo[i].reset();
        hi[i].reset();
    }
}

void Splitter::split(double x, double out[Reducer::kBands])
{
    double rest = x;
    for (int i = 0; i < Reducer::kXovers; ++i)
    {
        out[i] = lo[i].tick(rest);
        rest = hi[i].tick(rest);
    }
    out[Reducer::kXovers] = rest;
}

//------------------------------------------------------------------------
void BandTone::prepare(double sampleRate, double lowEdge, double highEdge)
{
    fs = sampleRate;
    ring.assign(4 * Reducer::kMaxWindow, 0.0);
    hann.clear();
    setEdges(lowEdge, highEdge);
    reset();
}

void BandTone::setEdges(double lowEdge, double highEdge)
{
    lo = lowEdge;
    hi = highEdge;
    // Long enough to tell neighbouring notes apart at the band's bottom edge.
    const int newWindow = std::min(Reducer::kMaxWindow,
                                   nextPow2(std::max(2048, static_cast<int>(8.0 * fs / std::max(lo, 20.0)))));
    if (newWindow == window && !hann.empty())
        return;                              // a knob turning inside one window size: nothing to rebuild
    window = newWindow;
    hop = window >= 8192 ? 1024 : 256;      // a low band does not change note 170 times a second
    hold = std::max(1, static_cast<int>(response.holdMs / 1000.0 * fs / hop)) | 1;
    hann.resize(window);
    for (int i = 0; i < window; ++i)
        hann[i] = 0.5 - 0.5 * std::cos(2.0 * kPi * i / (window - 1));
    fftBuf.assign(window, {});
    sinceHop = 0;
}

void BandTone::setResponse(int index)
{
    response = Reducer::kResponse[std::clamp(index, 0, Reducer::kResponses - 1)];
    hold = std::max(1, static_cast<int>(response.holdMs / 1000.0 * fs / hop)) | 1;
}

void BandTone::reset()
{
    std::fill(ring.begin(), ring.end(), 0.0);
    writePos = sinceHop = 0;
    for (auto& v : voices)
        v = Voice();
    weightGlide = 1.0 - std::exp(-1.0 / (0.01 * fs));     // a voice fades in or out over ~10 ms
    level = levelTarget = levelStep = 0.0;
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

    // Every spectral peak in the band, as a snapped note; one entry per note, the loudest wins.
    const int k0 = std::max(1, static_cast<int>(lo * window / fs));
    const int k1 = std::min(window / 2 - 2, static_cast<int>(hi * window / fs) + 1);
    struct Peak
    {
        int note;
        double mag;
    };
    std::vector<Peak> peaks;
    for (int k = k0; k <= k1; ++k)
    {
        const double m = std::abs(fftBuf[k]);
        if (m <= 0.0 || m <= std::abs(fftBuf[k - 1]) || m < std::abs(fftBuf[k + 1]))
            continue;
        const double a = std::log(std::abs(fftBuf[k - 1]) + 1e-12);
        const double b = std::log(m + 1e-12);
        const double c = std::log(std::abs(fftBuf[k + 1]) + 1e-12);
        const double d = a - 2.0 * b + c;
        const double kf = std::abs(d) > 1e-12 ? k + 0.5 * (a - c) / d : k;
        const double f = std::max(1.0, kf * fs / window);
        const int note = static_cast<int>(std::lround(69.0 + 12.0 * std::log2(f / 440.0)));
        auto same = std::find_if(peaks.begin(), peaks.end(), [note](const Peak& p) { return p.note == note; });
        if (same == peaks.end())
            peaks.push_back({note, m});
        else
            same->mag = std::max(same->mag, m);
    }
    // The loudest `active` notes, ignoring any more than 12 dB under the loudest.
    std::sort(peaks.begin(), peaks.end(), [](const Peak& x, const Peak& y) { return x.mag > y.mag; });
    if (peaks.size() > static_cast<size_t>(active))
        peaks.resize(active);
    if (!peaks.empty())
    {
        const double floorMag = peaks.front().mag * 0.25;
        peaks.erase(std::remove_if(peaks.begin(), peaks.end(), [floorMag](const Peak& p) { return p.mag < floorMag; }),
                    peaks.end());
    }
    double power = 0.0;
    for (const auto& p : peaks)
        power += p.mag * p.mag;
    const double norm = power > 0.0 ? 1.0 / std::sqrt(power) : 0.0;

    // Each note goes to the free voice nearest to it, so a voice keeps following one line.
    bool used[Reducer::kVoices] = {};
    for (const auto& p : peaks)
    {
        int best = 0;
        int bestCost = 1 << 30;
        for (int v = 0; v < active; ++v)
        {
            if (used[v])
                continue;
            const int cost = voices[v].held < 0 || voices[v].weightTarget <= 0.0 ? 48 : std::abs(voices[v].held - p.note);
            if (cost < bestCost)
            {
                bestCost = cost;
                best = v;
            }
        }
        used[best] = true;
        Voice& vc = voices[best];
        // Hold: the voice follows the median of its last `hold` notes.
        vc.history.push_back(p.note);
        if (static_cast<int>(vc.history.size()) > hold)
            vc.history.erase(vc.history.begin());
        std::vector<int> sorted(vc.history);
        std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
        const int held = sorted[sorted.size() / 2];
        if (held != vc.held)
        {
            vc.held = held;
            ++changes;
        }
        vc.freqTarget = 440.0 * std::pow(2.0, (held - 69) / 12.0);
        if (vc.freq <= 0.0 || vc.weight < 0.01)
            vc.freq = vc.freqTarget;     // a voice coming in starts on its note, no glide from an old one
        vc.weightTarget = p.mag * norm;
    }
    for (int v = 0; v < Reducer::kVoices; ++v)
        if (!used[v])
            voices[v].weightTarget = 0.0;

    // Level: RMS around the played sample over levelHops hops, reached by the next analysis.
    double sum = 0.0;
    const int centre = end - window / 2;
    const int span = std::max(hop, static_cast<int>(hop * response.levelHops));
    for (int i = -span / 2; i < span / 2; ++i)
    {
        const double v = ring[((centre + i) % size + size) % size];
        sum += v * v;
    }
    levelTarget = std::sqrt(sum / span);
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

    // Equal loudness at both ends: a sine of peak sqrt(2) and a square of peak 1 both have
    // RMS 1, and the voices' weights add up (as power) to 1, so the band keeps its level.
    double out = 0.0;
    for (auto& v : voices)
    {
        v.weight += (v.weightTarget - v.weight) * weightGlide;
        if (v.weight < 1e-5 || v.freq <= 0.0)
            continue;
        v.freq += (v.freqTarget - v.freq) * response.glide;     // glide, so a note change does not click
        const double dt = v.freq / fs;
        v.phase += dt;
        if (v.phase >= 1.0)
            v.phase -= 1.0;
        const double sine = std::sin(2.0 * kPi * v.phase) * std::sqrt(2.0);
        double square = v.phase < 0.5 ? 1.0 : -1.0;
        square += polyBlep(v.phase, dt);
        square -= polyBlep(std::fmod(v.phase + 0.5, 1.0), dt);
        out += v.weight * ((1.0 - shape) * sine + shape * square);
    }
    return level * out;
}

//------------------------------------------------------------------------
void ReducerEngine::prepare(double sampleRate)
{
    fs = sampleRate;
    for (auto& d : delay)
        d.assign(Reducer::kLatency, 0.0f);
    applyXovers(true);
    reset();
}

void ReducerEngine::reset()
{
    lowTrack.reset();
    highTrack.reset();
    for (int ch = 0; ch < 2; ++ch)
    {
        lowDryLo[ch].reset();
        lowDryHi[ch].reset();
        highDryLo[ch].reset();
        highDryHi[ch].reset();
    }
    trackSplit.reset();
    for (auto& s : drySplit)
        s.reset();
    for (auto& t : tones)
        t.reset();
    for (auto& d : delay)
        std::fill(d.begin(), d.end(), 0.0f);
    delayPos = 0;
}

void ReducerEngine::applyXovers(bool resetTrackers)
{
    // Keep the splits in order: a later one never sits below an earlier one. Only the
    // coefficients change while a handle moves, so the sound does not drop out.
    double fc[Reducer::kXovers];
    for (int i = 0; i < Reducer::kXovers; ++i)
        fc[i] = std::clamp(current.xover[i], Reducer::kXoverMin[i], Reducer::kXoverMax[i]);
    for (int i = 1; i < Reducer::kXovers; ++i)
        fc[i] = std::max(fc[i], fc[i - 1] * std::pow(2.0, Reducer::kXoverGapNotes / 12.0) * 0.999);
    const double top = std::min(20000.0, fs * 0.45);
    fc[Reducer::kXovers - 1] = std::min(fc[Reducer::kXovers - 1], top / Reducer::kXoverGap);
    trackSplit.set(fc, fs);
    for (auto& s : drySplit)
        s.set(fc, fs);
    // BASS starts at its low edge and AIR ends at its high edge, each kept a major third clear of
    // the crossover next to it.
    const double gap = std::pow(2.0, Reducer::kXoverGapNotes / 12.0);
    lowOn = Reducer::lowEdgeOn(current.lowEdge);
    highOn = Reducer::highEdgeOn(current.highEdge) && current.highEdge < top;
    const double low = lowOn ? std::min(current.lowEdge, fc[0] / gap) : 20.0;
    const double high = highOn ? std::clamp(current.highEdge, fc[Reducer::kXovers - 1] * gap, top) : top;
    auto setPair = [this](LR4& lo, LR4& hi, double f) {
        lo.a.setLowpass(f, fs);
        lo.b.setLowpass(f, fs);
        hi.a.setHighpass(f, fs);
        hi.b.setHighpass(f, fs);
    };
    LR4 unused;
    setPair(unused, lowTrack, low);
    setPair(highTrack, unused, high);
    for (int ch = 0; ch < 2; ++ch)
    {
        setPair(lowDryLo[ch], lowDryHi[ch], low);
        setPair(highDryLo[ch], highDryHi[ch], high);
    }

    double edges[Reducer::kBands + 1];
    edges[0] = low;
    for (int i = 0; i < Reducer::kXovers; ++i)
        edges[i + 1] = fc[i];
    edges[Reducer::kBands] = high;
    for (int b = 0; b < Reducer::kBands; ++b)
    {
        if (resetTrackers)
            tones[b].prepare(fs, edges[b], edges[b + 1]);
        else
            tones[b].setEdges(edges[b], edges[b + 1]);
        tones[b].setResponse(current.response);
        tones[b].setVoices(current.voices);
    }
}

void ReducerEngine::setSettings(const Settings& s)
{
    const bool xoverChanged = s.xover != current.xover || s.lowEdge != current.lowEdge || s.highEdge != current.highEdge;
    const bool responseChanged = s.response != current.response;
    current = s;
    if (xoverChanged)
        applyXovers(false);
    else if (responseChanged)
        for (auto& t : tones)
            t.setResponse(current.response);
    for (auto& t : tones)
        t.setVoices(current.voices);
}

void ReducerEngine::process(const float* inL, const float* inR, float* outL, float* outR, int n)
{
    double shape[Reducer::kBands];
    for (int b = 0; b < Reducer::kBands; ++b)
        shape[b] = std::clamp((current.amount[b] - Reducer::kShapeStart) / (1.0 - Reducer::kShapeStart), 0.0, 1.0);

    bool anySolo = false;
    for (bool on : current.solo)
        anySolo = anySolo || on;

    for (int i = 0; i < n; ++i)
    {
        const double l = inL[i], r = inR[i];

        // Trackers run on the undelayed mono band signal.
        double mono[Reducer::kBands];
        trackSplit.split(0.5 * (l + r), mono);
        if (lowOn)
            mono[0] = lowTrack.tick(mono[0]);
        if (highOn)
            mono[Reducer::kBands - 1] = highTrack.tick(mono[Reducer::kBands - 1]);
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
        // outside the band edges the music passes as it is
        double passL = 0.0, passR = 0.0;
        const int top = Reducer::kBands - 1;
        if (lowOn)
        {
            const double xl = bl[0], xr = br[0];
            passL += lowDryLo[0].tick(xl);
            passR += lowDryLo[1].tick(xr);
            bl[0] = lowDryHi[0].tick(xl);
            br[0] = lowDryHi[1].tick(xr);
        }
        if (highOn)
        {
            const double xl = bl[top], xr = br[top];
            passL += highDryHi[0].tick(xl);
            passR += highDryHi[1].tick(xr);
            bl[top] = highDryLo[0].tick(xl);
            br[top] = highDryLo[1].tick(xr);
        }
        double yl = anySolo ? 0.0 : passL, yr = anySolo ? 0.0 : passR;
        for (int b = 0; b < Reducer::kBands; ++b)
        {
            if (anySolo && !current.solo[b])
                continue;
            const double v = current.amount[b];
            yl += (1.0 - v) * bl[b] + v * tone[b];
            yr += (1.0 - v) * br[b] + v * tone[b];
        }
        outL[i] = static_cast<float>(yl);
        outR[i] = static_cast<float>(yr);
    }
}

} // namespace Yonie
