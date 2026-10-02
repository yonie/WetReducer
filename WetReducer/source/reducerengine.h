//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------
// WetReducer's sound: four bands, each reduced to one tone.
//
// The input is split at three crossovers (fourth-order Linkwitz-Riley, so the bands sum back
// flat). In each band a tracker finds the loudest frequency, snaps it to the nearest semitone
// and holds it for HOLD_MS, and follows the band's level. One knob per band, 0..1:
//   0     the band as it was
//   noon  the band and a sine at the snapped note, half and half
//   1     the tone alone, as a square wave
// The mix runs over the whole travel; the shape goes sine -> square from kShapeStart to 1.
//
// Latency: the lowest band needs a long analysis window, so the music is delayed by half of
// it (kLatency samples) and every band's tone is read from a window centred on the sample
// being played. The host compensates for that delay.
//------------------------------------------------------------------------

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

namespace Yonie {

namespace Reducer {
constexpr int kBands = 4;
constexpr double kShapeStart = 0.25;     // the square comes in over the last 75% of the travel
constexpr double kHoldMs = 50.0;
constexpr int kLatency = 8192;           // half of the largest window, fixed for every rate
constexpr int kMaxWindow = 2 * kLatency;

// Crossover choices, three per split, in Hz.
// Crossovers, continuous on a log scale; the ranges cannot cross. Moved down on 2026-10-02 (at
// 70 Hz / 3 kHz / 8 kHz the second band held ~90% of a song), then made knobs the same day.
// Every split can go anywhere (Ronald, 2026-10-02: fixed ranges put the whole vocal range in one
// band); the engine keeps them in order, at least kXoverGap apart.
constexpr double kXoverMin[3] = {30.0, 30.0, 30.0};
constexpr double kXoverMax[3] = {16000.0, 16000.0, 16000.0};
constexpr double kXoverGap = 1.25;          // a band is at least a major third wide
constexpr double kXoverDefaultHz[3] = {146.8324, 587.3295, 2637.0205};     // D3, D5, E7

inline double xoverHz(int i, double norm)
{
    return kXoverMin[i] * std::pow(kXoverMax[i] / kXoverMin[i], std::clamp(norm, 0.0, 1.0));
}
inline double xoverNorm(int i, double hz)
{
    return std::log(std::clamp(hz, kXoverMin[i], kXoverMax[i]) / kXoverMin[i]) / std::log(kXoverMax[i] / kXoverMin[i]);
}
} // namespace Reducer

//------------------------------------------------------------------------
class Biquad
{
public:
    void setLowpass(double fc, double fs) { set(fc, fs, false); }
    void setHighpass(double fc, double fs) { set(fc, fs, true); }
    double tick(double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0.0; }

private:
    void set(double fc, double fs, bool high);
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
};

// One fourth-order Linkwitz-Riley section: two identical Butterworth biquads in series.
struct LR4
{
    Biquad a, b;
    double tick(double x) { return b.tick(a.tick(x)); }
    void reset() { a.reset(); b.reset(); }
};

// The three-way split of one channel into four bands.
class Splitter
{
public:
    void set(const double fc[3], double fs);
    void reset();
    void split(double x, double out[Reducer::kBands]);

private:
    LR4 lo[3], hi[3];
};

//------------------------------------------------------------------------
// The tone of one band: tracker plus oscillator.
class BandTone
{
public:
    void prepare(double fs, double lowEdge, double highEdge);
    void setEdges(double lowEdge, double highEdge);
    void reset();
    // Feed one undelayed mono band sample; returns the tone for the sample now being played
    // (kLatency samples ago), at the given shape (0 sine, 1 square).
    double tick(double bandSample, double shape);

private:
    void analyse();

    double fs = 44100.0, lo = 20.0, hi = 20000.0;
    int window = 2048, hop = 256, hold = 9;
    std::vector<double> ring;       // undelayed band signal
    int writePos = 0, sinceHop = 0;
    std::vector<double> hann;
    std::vector<std::complex<double>> fftBuf;
    std::vector<int> history;       // snapped notes, newest last, for the hold
    double freq = 0.0, freqTarget = 0.0;
    double level = 0.0, levelTarget = 0.0, levelStep = 0.0;
    double phase = 0.0;
};

//------------------------------------------------------------------------
class ReducerEngine
{
public:
    struct Settings
    {
        std::array<double, Reducer::kBands> amount{{0.0, 0.0, 0.0, 0.0}};
        std::array<double, 3> xover{{Reducer::kXoverDefaultHz[0], Reducer::kXoverDefaultHz[1], Reducer::kXoverDefaultHz[2]}};
    };

    void prepare(double sampleRate);
    void reset();
    void setSettings(const Settings& s);
    const Settings& settings() const { return current; }
    void process(const float* inL, const float* inR, float* outL, float* outR, int n);

private:
    void applyXovers(bool resetTrackers);

    double fs = 44100.0;
    Settings current;
    Splitter trackSplit;            // mono, undelayed: feeds the trackers
    Splitter drySplit[2];           // per channel, delayed: the music that is played
    std::array<BandTone, Reducer::kBands> tones;
    std::vector<float> delay[2];
    int delayPos = 0;
};

} // namespace Yonie
