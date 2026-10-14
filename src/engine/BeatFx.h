#pragma once

#include "Types.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

namespace livelooper
{
/**
    v1.5 performance effects in the style of a DJ mixer's beat FX section.

    BeatFx      one beat-synced effect (16 types) with a beat fraction (1/32 .. 16 beats),
                a level / depth control and on / off. It works on the channel selected
                with the FX target (master, loops, input or one track).
    ColorFx     a one-knob effect per channel: knob centre = off, left / right = more of it
                (filter LPF/HPF, dub echo, space, crush, noise, sweep).
    BeatRepeat  beat repeat and stutter: from the next grid line of the chosen timing
                (1/2 .. 1/32 note) the slice is captured and repeated while held / latched;
                the live signal keeps running underneath, so releasing it is always in sync.

    All classes are real-time safe: memory is reserved in prepare(), process() never allocates.
*/

enum class BeatFxType : int
{
    Delay = 0, Echo, PingPong, Spiral, Reverb, Trans, Filter, Flanger, Phaser, Pitch,
    Roll, SlipRoll, ReverseRoll, VinylBrake, Helix, Noise,
    NumTypes
};

/** English display name ("Ping Pong"). */
const char* getBeatFxName (BeatFxType t) noexcept;
/** What the LEVEL/DEPTH knob does for a type ("Level", "Depth", "Pitch"). */
const char* getBeatFxDepthName (BeatFxType t) noexcept;

/** Beat fractions of the beat FX, in beats (quarter notes): 1/32 .. 16 (= 4 bars in 4/4). */
inline constexpr std::array<double, 11> kBeatFractions { 1.0 / 32.0, 1.0 / 16.0, 1.0 / 8.0, 1.0 / 4.0, 1.0 / 2.0, 3.0 / 4.0,
                                                         1.0, 2.0, 4.0, 8.0, 16.0 };
inline constexpr int kDefaultBeatFraction = 6;   // 1 beat

/** Where the beat FX (and beat repeat / stutter) work, like the channel select of a DJ mixer. */
enum class FxTarget : int { Master = 0, Loops, Input, Track1 };   // Track1 + n = track n + 1
inline constexpr int kNumFxTargets = 3 + kNumTracks;

/** Timings of beat repeat / stutter as note values: 1/2, 1/4, 1/8, 1/16, 1/32 note. */
inline constexpr std::array<double, 5> kRepeatPpq { 2.0, 1.0, 0.5, 0.25, 0.125 };

struct BeatFxSettings
{
    bool       on       = false;
    BeatFxType type     = BeatFxType::Echo;
    int        fraction = kDefaultBeatFraction;  // index into kBeatFractions
    float      depth    = 0.5f;                  // 0 .. 1
};

/** Simple two-tap (crossfaded) delay-line pitch shifter, used by Pitch and Helix. */
class PitchShifter
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;
    /** ratio 0.5 .. 2 (one octave down .. up). */
    float process (int channel, float x, float ratio) noexcept;

private:
    static constexpr int kSize = 8192;
    std::array<std::vector<float>, 2> buf;
    int write[2] {};
    double phase[2] {};
    int window = 2048;
};

class BeatFx
{
public:
    void prepare (double sampleRate, double maxSeconds = 10.0);
    void reset() noexcept;
    void setSettings (const BeatFxSettings& s) noexcept { settings = s; }
    const BeatFxSettings& getSettings() const noexcept { return settings; }
    /** True while the effect is on or a tail (echo, reverb) is still ringing out. */
    bool isActive() const noexcept { return wetGain > 1.0e-4f || inGain > 1.0e-4f || tailActive; }

    /** ppqStart: grid position of the first sample, ppqPerSample: grid advance per sample. */
    void process (float* left, float* right, int numSamples, double ppqStart, double ppqPerSample) noexcept;

    /** Length of the beat fraction in samples at the given tempo (public for tests). */
    static double fractionSamples (int fraction, double ppqPerSample) noexcept;

private:
    void processDelayFamily (float* l, float* r, int n, double fracSamples) noexcept;
    void processRollFamily (float* l, float* r, int n, double ppqStart, double ppqPerSample, double fracSamples) noexcept;
    float readBuffer (int ch, double pos) const noexcept;

    BeatFxSettings settings;
    BeatFxType activeType = BeatFxType::Echo;
    double sampleRate = 44100.0;

    // shared ring buffer (delays, rolls, brake)
    std::array<std::vector<float>, 2> buf;
    int size = 1, writePos = 0;

    float wetGain = 0.0f;      // smoothed wet level (0 .. depth)
    float inGain = 0.0f;       // smoothed send into delay-type effects (tails ring out after off)
    bool tailActive = false;
    int tailSamples = 0;
    float fbState[2] {};       // feedback low-pass
    double smoothedDelay = 0.0;
    double spiralPhase = 0.0;

    // roll family
    bool rolling = false, rollWaiting = false;
    double rollStart = 0.0;    // ring position of the slice start
    double rollPos = 0.0;      // position inside the slice (samples)
    double rollLen = 1.0;
    double rollSpeed = 1.0;    // helix: rises every repeat
    int rollRepeats = 0;
    double brakeSpeed = 1.0, brakeRead = 0.0;
    bool braking = false;
    float rollXfade = 0.0f;    // 0 = live, 1 = roll

    // modulation
    double lfoPpq = 0.0;
    float svfIc1[2] {}, svfIc2[2] {};
    float apState[2][6] {};
    float phaserFb[2] {};
    juce::Reverb reverb;
    PitchShifter shifter;
    uint32_t noiseSeed = 0x2545F491u;
    float noiseLp[2] {};
};

/** One-knob colour effect for one channel. */
enum class ColorFxType : int { Filter = 0, DubEcho, Space, Crush, Noise, Sweep, NumTypes };
const char* getColorFxName (ColorFxType t) noexcept;

class ColorFx
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;
    /** amount -1 .. 1, 0 = off (bit-exact pass-through once the effect has faded out). */
    void process (float* left, float* right, int numSamples, ColorFxType type, float amount, double ppqPerSample) noexcept;
    bool isActive() const noexcept { return active; }

    static constexpr float kDeadZone = 0.02f;

private:
    double sampleRate = 44100.0;
    ColorFxType lastType = ColorFxType::Filter;
    float smoothed = 0.0f;
    float wet = 0.0f;
    bool active = false;
    float ic1[2] {}, ic2[2] {};
    float sendIc1[2] {}, sendIc2[2] {};
    std::array<std::vector<float>, 2> delay;
    int delayWrite = 0;
    float fbLp[2] {};
    juce::Reverb reverb;
    float hold[2] {};
    float holdCounter = 0.0f;
    uint32_t noiseSeed = 0x1234567u;
    double sweepPhase = 0.0;
};

/** Beat repeat (and stutter = beat repeat with a chopped gate) on one stereo signal. */
class BeatRepeat
{
public:
    void prepare (double sampleRate, double maxSeconds = 6.0);
    void reset() noexcept;
    /** wantOn: held or latched. rate: index into kRepeatPpq. stutter: gate every repeat. */
    void set (bool wantOn, int rate, bool stutter) noexcept { want = wantOn; rateIndex = rate; stutterMode = stutter; }
    bool isRepeating() const noexcept { return repeating; }
    bool isWaiting() const noexcept { return want && ! repeating; }
    /** Ring position where the current slice starts / its length (tests). */
    double getSliceLength() const noexcept { return sliceLen; }
    long long getStartSample() const noexcept { return startedAtSample; }
    void process (float* left, float* right, int numSamples, double ppqStart, double ppqPerSample) noexcept;

private:
    std::array<std::vector<float>, 2> buf;
    int size = 1, writePos = 0;
    double sampleRate = 44100.0;
    bool want = false, repeating = false, stutterMode = false;
    int rateIndex = 1, activeRate = 1;
    int sliceStart = 0;        // ring position of the slice start
    double sliceLen = 1.0;     // samples
    double pos = 0.0;          // position inside the slice
    int recorded = 0;          // samples of the slice already captured
    float mix = 0.0f;          // 0 = live, 1 = repeat
    long long sampleCounter = 0, startedAtSample = -1;
};
} // namespace livelooper
