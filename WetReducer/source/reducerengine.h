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
constexpr int kBands = 5;
constexpr int kXovers = kBands - 1;
constexpr int kVoices = 3;               // tones per band: the loudest notes in it, up to three
constexpr double kShapeStart = 0.25;     // the square comes in over the last 75% of the travel
constexpr int kLatency = 8192;           // half of the largest window, fixed for every rate
constexpr int kMaxWindow = 2 * kLatency;

// How quickly the tones follow the music: how long a note must last before a band's tone moves
// to it, how fast the tone glides there, and over how many analysis hops its level is averaged.
struct Response
{
    double holdMs, glide, levelHops;
};
constexpr int kResponses = 4;
constexpr Response kResponse[kResponses] = {{500.0, 0.0008, 8.0},   // SLOW - the one to start from (Ronald, 2026-10-02)
                                            {150.0, 0.003, 4.0},    // MEDIUM
                                            {50.0, 0.02, 1.0},      // FAST - what the first prototypes did
                                            {15.0, 0.08, 1.0}};     // VERY FAST
constexpr int kResponseDefault = 0;
// The response as a 0..1 parameter value, and back.
inline double responseNorm(int index) { return index / static_cast<double>(kResponses - 1); }
inline int responseIndex(double norm)
{
    return std::clamp(static_cast<int>(norm * (kResponses - 1) + 0.5), 0, kResponses - 1);
}

// Crossovers: every split can go anywhere from 30 Hz to 16 kHz (fixed ranges put a whole vocal
// range in one band, 2026-10-02); the engine keeps them in order, at least kXoverGap apart.
constexpr double kXoverMin[kXovers] = {30.0, 30.0, 30.0, 30.0};
constexpr double kXoverMax[kXovers] = {16000.0, 16000.0, 16000.0, 16000.0};
constexpr double kXoverGap = 1.25;          // a band is at least a major third wide
constexpr double kXoverDefaultHz[kXovers] = {123.4708, 493.8833, 2093.0045, 6271.9270};    // B2 B4 C7 G8

// The crossovers move in semitones (Ronald: "the sliders should map to notes"), from B0 to B9.
constexpr int kXoverLowNote = 23;            // B0, 30.9 Hz
constexpr int kXoverNotes = 100;             // up to B9, 15.8 kHz
constexpr int kXoverGapNotes = 4;            // neighbouring splits at least a major third apart

inline double noteHz(int midi) { return 440.0 * std::pow(2.0, (midi - 69) / 12.0); }
inline int xoverNote(double norm)
{
    return kXoverLowNote + static_cast<int>(std::lround(std::clamp(norm, 0.0, 1.0) * kXoverNotes));
}
inline double xoverHz(int /*i*/, double norm) { return noteHz(xoverNote(norm)); }
// BASS's low edge and AIR's high edge share the crossovers' note range; at its ends they are off
// and the bands reach the bottom and the top. Outside an edge the music passes as it is, so
// BASS does not have to follow everything from 30 Hz up.
inline double edgeBottomHz() { return noteHz(kXoverLowNote); }
inline double edgeTopHz() { return noteHz(kXoverLowNote + kXoverNotes); }
inline bool lowEdgeOn(double hz) { return hz > edgeBottomHz() * 1.01; }
inline bool highEdgeOn(double hz) { return hz < edgeTopHz() * 0.99; }
inline double xoverNorm(int /*i*/, double hz)
{
    const int midi = static_cast<int>(std::lround(69.0 + 12.0 * std::log2(std::max(hz, 1.0) / 440.0)));
    return std::clamp(midi - kXoverLowNote, 0, kXoverNotes) / static_cast<double>(kXoverNotes);
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
    void set(const double fc[Reducer::kXovers], double fs);
    void reset();
    void split(double x, double out[Reducer::kBands]);

private:
    LR4 lo[Reducer::kXovers], hi[Reducer::kXovers];
};

//------------------------------------------------------------------------
// The tone of one band: tracker plus oscillator.
class BandTone
{
public:
    void prepare(double fs, double lowEdge, double highEdge);
    void setEdges(double lowEdge, double highEdge);
    void setResponse(int index);
    void reset();
    // Feed one undelayed mono band sample; returns the tone for the sample now being played
    // (kLatency samples ago), at the given shape (0 sine, 1 square).
    double tick(double bandSample, double shape);
    double currentLevel() const { return level; }     // RMS of the band being played
    int noteChanges() const { return changes; }       // counts each move to a new held note

private:
    void analyse();

    double fs = 44100.0, lo = 20.0, hi = 20000.0;
    int window = 2048, hop = 256, hold = 9;
    Reducer::Response response = Reducer::kResponse[Reducer::kResponseDefault];
    std::vector<double> ring;       // undelayed band signal
    int writePos = 0, sinceHop = 0;
    std::vector<double> hann;
    std::vector<std::complex<double>> fftBuf;
    // One voice per note found in the band. Each holds its own note and carries its share of
    // the band's level, so all voices together are as loud as the band.
    struct Voice
    {
        std::vector<int> history;   // snapped notes, newest last, for the hold
        int held = -1;
        double freq = 0.0, freqTarget = 0.0;
        double weight = 0.0, weightTarget = 0.0;
        double phase = 0.0;
    };
    std::array<Voice, Reducer::kVoices> voices;
    double weightGlide = 0.001;
    int changes = 0;
    double level = 0.0, levelTarget = 0.0, levelStep = 0.0;
};

//------------------------------------------------------------------------
class ReducerEngine
{
public:
    struct Settings
    {
        std::array<double, Reducer::kBands> amount{};
        std::array<double, Reducer::kXovers> xover{{Reducer::kXoverDefaultHz[0], Reducer::kXoverDefaultHz[1],
                                                    Reducer::kXoverDefaultHz[2], Reducer::kXoverDefaultHz[3]}};
        std::array<bool, Reducer::kBands> solo{};
        int response = Reducer::kResponseDefault;
        double lowEdge = Reducer::edgeBottomHz();
        double highEdge = Reducer::edgeTopHz();
    };

    void prepare(double sampleRate);
    void reset();
    void setSettings(const Settings& s);
    const Settings& settings() const { return current; }
    double bandLevel(int b) const { return tones[b].currentLevel(); }
    int bandNoteChanges(int b) const { return tones[b].noteChanges(); }
    void process(const float* inL, const float* inR, float* outL, float* outR, int n);

private:
    void applyXovers(bool resetTrackers);

    double fs = 44100.0;
    Settings current;
    Splitter trackSplit;            // mono, undelayed: feeds the trackers
    Splitter drySplit[2];           // per channel, delayed: the music that is played
    std::array<BandTone, Reducer::kBands> tones;
    // The band edges: the trackers see only the band, the music outside an edge passes as it is.
    LR4 lowTrack, highTrack;                     // mono, undelayed: BASS above its edge, AIR below its edge
    LR4 lowDryLo[2], lowDryHi[2], highDryLo[2], highDryHi[2];
    bool lowOn = false, highOn = false;
    std::vector<float> delay[2];
    int delayPos = 0;
};

} // namespace Yonie
