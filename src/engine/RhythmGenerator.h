#pragma once

#include "Types.h"

#include <array>
#include <vector>

namespace livelooper
{
/**
    Built-in drum machine. All sounds are synthesised (no samples), patterns are 16 steps
    of 16th notes that follow the timeline, so the rhythm is always locked to the host
    tempo or the internal clock.

    Since 1.3:
    - four kits (Studio, 808, 909, Lo-Fi) - each voice is shaped per kit
    - voices: kick, snare, hand clap, closed / open hi-hat (with choke), rimshot, shaker, tom
    - every step has a velocity (accent, normal, ghost note) and hi-hat rolls
    - swing (MPC style, 50 % = straight, 66 % = triplet feel) and humanize (timing and
      velocity variation), a short room on snare / clap / rim and a bus saturator
    - metronome sounds: beep, woodblock, click, rimshot
*/
class RhythmGenerator
{
public:
    static int getNumPatterns() noexcept;
    static const char* getPatternName (int index) noexcept;
    static int getNumKits() noexcept;
    static const char* getKitName (int index) noexcept;
    static constexpr int kNumClickSounds = 4;   // 0 = beep, 1 = woodblock, 2 = click, 3 = rimshot

    void prepare (double sampleRate);
    void reset() noexcept;

    void setPattern (int index) noexcept  { pattern = index; }
    void setLevel (float l) noexcept      { level = l; }
    void setClickSound (int s) noexcept   { clickSound = s; }
    /** v1.5: metronome accent on the first beat of the bar (default on). */
    void setClickAccent (bool a) noexcept { clickAccent = a; }
    void setKit (int k) noexcept          { kit = k; }
    /** 0.5 = straight ... 0.75 = hard swing (the odd 16th notes are delayed). */
    void setSwing (float s) noexcept      { swing = s; }
    /** 0 = machine-exact, 1 = loose (timing up to 8 ms late, velocity variation). */
    void setHumanize (float h) noexcept   { humanize = h; }

    /** Adds the rhythm to the output. timelineRunning = false produces silence (voices decay). */
    void process (float* outL, float* outR, int numSamples, double ppqStart, double ppqPerSample,
                  int beatsPerMeasure, bool timelineRunning, float* kickOut = nullptr) noexcept;

    /** Simple TPT state-variable filter (public for tests). */
    struct Svf
    {
        float g = 0.0f, k = 1.0f, ic1 = 0.0f, ic2 = 0.0f;
        void set (double sampleRate, double freq, double q) noexcept;
        void reset() noexcept { ic1 = ic2 = 0.0f; }
        /** returns band-pass, writes low / high pass */
        float process (float x, float& lp, float& hp) noexcept;
    };

    enum Voice : uint8_t { Kick, Snare, Clap, HatClosed, HatOpen, Rim, Shaker, Tom, Click };

    /** Step offset (in 16th notes) of a step for a given swing (public for tests). */
    static double swingOffset (int step, float swing) noexcept;

private:
    void scheduleStep (long long step, int beatsPerMeasure) noexcept;
    void addEvent (double delaySamples, Voice v, float velocity, uint8_t variant = 0) noexcept;
    void fire (Voice v, float velocity, uint8_t variant) noexcept;
    float decayFor (double seconds) const noexcept;
    float noise() noexcept;
    float random01() noexcept;

    double sampleRate = 44100.0;
    double samplesPerStep = 5512.5;
    int    pattern = 0;
    int    clickSound = 0;
    bool   clickAccent = true;
    int    kit = 0;
    float  swing = 0.5f, humanize = 0.25f;
    float  level = 0.7f;
    long long lastStep = -1;

    struct Event { double delay; Voice voice; float velocity; uint8_t variant; };
    std::array<Event, 48> events {};
    int numEvents = 0;

    // kick
    float kickEnv = 0.0f, kickDecay = 0.999f, kickPhase = 0.0f, kickPitch = 0.0f, kickPitchDecay = 0.99f;
    float kickClick = 0.0f, kickClickDecay = 0.9f, kickVel = 1.0f;
    Svf   kickClickFilter;
    // snare
    float snBodyEnv = 0.0f, snBodyDecay = 0.999f, snPh1 = 0.0f, snPh2 = 0.0f, snPitch = 0.0f, snPitchDecay = 0.99f;
    float snNoiseEnv = 0.0f, snNoiseDecay = 0.999f, snSnap = 0.0f, snSnapDecay = 0.9f, snVel = 1.0f;
    Svf   snBand, snHigh;
    // clap
    float clapEnv = 0.0f, clapDecay = 0.999f, clapVel = 1.0f;
    int   clapBurst = 0, clapBurstLeft = 0;
    Svf   clapFilter, clapHigh;
    // hats
    float hatEnv = 0.0f, hatDecay = 0.99f, hatVel = 1.0f;
    float hatPhase[6] {};
    Svf   hatBand, hatHighPass;
    // rimshot
    float rimEnv = 0.0f, rimDecay = 0.99f, rimPh1 = 0.0f, rimPh2 = 0.0f, rimVel = 1.0f;
    Svf   rimFilter;
    // shaker
    float shEnv = 0.0f, shDecay = 0.99f, shAttack = 0.0f, shAttackCoeff = 0.1f, shVel = 1.0f;
    Svf   shBand;
    // tom
    float tomEnv = 0.0f, tomDecay = 0.999f, tomPhase = 0.0f, tomPitch = 0.0f, tomPitchDecay = 0.99f, tomVel = 1.0f;
    // bus
    Svf   busLowPass;
    // small room (4 combs + 2 all-passes) for snare / clap / rim
    std::vector<float> combBuf[4], apBuf[2];
    int   combPos[4] {}, apPos[2] {};
    float combStore[4] {};
    // metronome
    float clickEnv = 0.0f, clickDecay = 0.99f, clickPh1 = 0.0f, clickPh2 = 0.0f, clickF1 = 0.0f, clickF2 = 0.0f, clickAmp = 1.0f;
    float clickGlide = 0.0f, clickGlideDecay = 0.9f, clickKnock = 0.0f, clickKnockDecay = 0.9f;
    int   activeClickSound = 0;
    Svf   clickFilter;

    uint32_t noiseSeed = 0x12345678u, randSeed = 0x9e3779b9u;
};
} // namespace livelooper
