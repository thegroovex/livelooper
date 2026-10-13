#pragma once

#include "Types.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace livelooper
{
/**
    Tape stop for the loop bus (v1.4, Push touch strip).

    depth 0 = the loops run normally (bit-exact pass-through), depth 1 = the tape stands still.
    The audio is written into a ring buffer and read back at the (smoothed) tape speed, so it
    slows down and drops in pitch like a stopping turntable / tape machine. When the depth goes
    back to 0 the tape spins up and crossfades into the live, in-sync signal: the timeline never
    stopped, so the music continues exactly where it would be.
*/
class TapeStop
{
public:
    void prepare (double sampleRate, double maxSeconds = 8.0);
    void reset() noexcept;
    /** 0 = normal speed, 1 = stopped. Read once per block. */
    void setDepth (float d) noexcept { depth = d < 0.0f ? 0.0f : (d > 1.0f ? 1.0f : d); }
    bool isActive() const noexcept   { return active; }
    float getSpeed() const noexcept  { return (float) speed; }
    void process (float* left, float* right, int numSamples) noexcept;

private:
    std::vector<float> bufL, bufR;
    int size = 0, writePos = 0;
    double readPos = 0.0;
    double speed = 1.0;
    double sampleRate = 44100.0;
    float depth = 0.0f;
    bool active = false;
    // return to the live signal
    bool returning = false;
    int returnLeft = 0, returnTotal = 1;
};

/**
    DJ-style performance filter for the loop bus (v1.4, SHIFT + Push touch strip).
    amount -1 .. 0 = low-pass closing towards 150 Hz, 0 = off (bit-exact), 0 .. 1 = high-pass
    opening towards 6 kHz. The wet signal is faded in and out, so switching never clicks.
*/
class PerformanceFilter
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    void setAmount (float a) noexcept { amount = a < -1.0f ? -1.0f : (a > 1.0f ? 1.0f : a); }
    bool isActive() const noexcept    { return wet > 0.0f || std::abs (amount) > kDeadZone; }
    void process (float* left, float* right, int numSamples) noexcept;

    static constexpr float kDeadZone = 0.02f;

private:
    double sampleRate = 44100.0;
    float amount = 0.0f;
    float smoothed = 0.0f;
    float wet = 0.0f;
    float ic1[2] {}, ic2[2] {};
};
} // namespace livelooper
