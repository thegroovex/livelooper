#pragma once

#include "Types.h"

namespace livelooper
{
/**
    Built-in drum machine. All sounds are synthesised (no samples), patterns are 16 steps
    of 16th notes that follow the timeline, so the rhythm is always locked to the host
    tempo or the internal clock.

    Sounds (since 1.2): punchy kick with click transient and sub, snare with two tuned
    bodies and filtered noise, a layered hand clap, metallic hi-hats from six square
    oscillators, a soft bus saturator. The metronome has two sounds (beep, woodblock).
*/
class RhythmGenerator
{
public:
    static int getNumPatterns() noexcept;
    static const char* getPatternName (int index) noexcept;
    static constexpr int kNumClickSounds = 2;   // 0 = beep, 1 = woodblock

    void prepare (double sampleRate);
    void reset() noexcept;

    void setPattern (int index) noexcept  { pattern = index; }
    void setLevel (float l) noexcept      { level = l; }
    void setClickSound (int s) noexcept   { clickSound = s; }

    /** Adds the rhythm to the output. timelineRunning = false produces silence (voices decay). */
    void process (float* outL, float* outR, int numSamples, double ppqStart, double ppqPerSample,
                  int beatsPerMeasure, bool timelineRunning) noexcept;

    /** Simple TPT state-variable filter (public for tests). */
    struct Svf
    {
        float g = 0.0f, k = 1.0f, ic1 = 0.0f, ic2 = 0.0f;
        void set (double sampleRate, double freq, double q) noexcept;
        void reset() noexcept { ic1 = ic2 = 0.0f; }
        /** returns band-pass, writes low / high pass */
        float process (float x, float& lp, float& hp) noexcept;
    };

private:
    void trigger (long long step, int beatsPerMeasure) noexcept;
    float decayFor (double seconds) const noexcept;
    float noise() noexcept;

    double sampleRate = 44100.0;
    int    pattern = 0;
    int    clickSound = 0;
    float  level = 0.7f;
    long long lastStep = -1;

    // kick
    float kickEnv = 0.0f, kickDecay = 0.999f, kickPhase = 0.0f, kickPitch = 0.0f, kickPitchDecay = 0.99f;
    float kickClick = 0.0f, kickClickDecay = 0.9f;
    // snare
    float snBodyEnv = 0.0f, snBodyDecay = 0.999f, snPh1 = 0.0f, snPh2 = 0.0f;
    float snNoiseEnv = 0.0f, snNoiseDecay = 0.999f;
    Svf   snFilter;
    // clap
    float clapEnv = 0.0f, clapDecay = 0.999f;
    int   clapBurst = 0, clapBurstLeft = 0;
    Svf   clapFilter;
    // hats
    float hatEnv = 0.0f, hatDecay = 0.99f;
    float hatPhase[6] {};
    Svf   hatFilter, hatHighPass;
    // metronome
    float clickEnv = 0.0f, clickDecay = 0.99f, clickPh1 = 0.0f, clickPh2 = 0.0f, clickF1 = 0.0f, clickF2 = 0.0f, clickAmp = 1.0f;
    Svf   woodFilter;

    uint32_t noiseSeed = 0x12345678u;
};
} // namespace livelooper
