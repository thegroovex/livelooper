#pragma once

#include "Types.h"

namespace livelooper
{
/**
    Built-in drum machine. All sounds are synthesised (no samples), patterns are 16 steps
    of 16th notes that follow the timeline, so the rhythm is always locked to the host
    tempo or the internal clock.
*/
class RhythmGenerator
{
public:
    static int getNumPatterns() noexcept;
    static const char* getPatternName (int index) noexcept;

    void prepare (double sampleRate);
    void reset() noexcept;

    void setPattern (int index) noexcept  { pattern = index; }
    void setLevel (float l) noexcept      { level = l; }

    /** Adds the rhythm to the output. timelineRunning = false produces silence (voices decay). */
    void process (float* outL, float* outR, int numSamples, double ppqStart, double ppqPerSample,
                  int beatsPerMeasure, bool timelineRunning) noexcept;

private:
    void trigger (long long step, int beatsPerMeasure) noexcept;

    struct Voice
    {
        float env = 0.0f, decay = 0.999f, phase = 0.0f, freq = 0.0f, freqEnd = 0.0f, sweep = 0.999f;
        float amp = 1.0f;
    };

    double sampleRate = 44100.0;
    int    pattern = 0;
    float  level = 0.7f;
    long long lastStep = -1;

    Voice kick, snareTone, click;
    float snareNoiseEnv = 0.0f, snareNoiseDecay = 0.999f;
    float hatEnv = 0.0f, hatDecay = 0.99f, hatHp = 0.0f, hatPrev = 0.0f;
    float snareBp1 = 0.0f, snareBp2 = 0.0f;
    uint32_t noiseSeed = 0x12345678u;
};
} // namespace livelooper
