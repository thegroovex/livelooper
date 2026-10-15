#pragma once

#include "BeatFx.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

namespace livelooper
{
/**
    v1.6 Punch FX and Send FX: quick "punch-in" effects you hold while playing, and one send
    effect on the loop bus.

    PunchFx     twelve momentary effects (hold = on, release = off, several at once combine):
                 1 Pitch Scatter   2 Slice Swap   3 Grains        4 Beat Repeat
                 5 Tape Stop       6 Filter Wobble 7 Low-Pass     8 High-Pass
                 9 Send Throw     10 Tremolo      11 Octave Drop  12 Decimator
                AMOUNT (pad pressure / velocity) sets how strong, X / Y shape each effect
                (see getPunchXName / getPunchYName). Every effect fades in / out over 5 ms.
    SendFx      one stereo send effect (Delay, Reverb, Distortion, Chorus, Filter, Compressor)
                fed from the loop bus with LEVEL; X / Y are its two knobs. "Send Throw" pushes
                more of the loops into it while held.

    Real-time safe: memory is reserved in prepare(), process() never allocates.
*/
inline constexpr int kNumPunchFx = 12;

enum class SendFxType : int { Off = 0, Delay, Reverb, Distortion, Chorus, Filter, Compressor, NumTypes };

/** Where the punch FX work. */
enum class PunchTarget : int { Loops = 0, Input, Master, Drums, NumTargets };

const char* getPunchFxName (int index) noexcept;      // 0 .. 11, English ("Tape Stop")
const char* getPunchXName (int index) noexcept;       // what X does ("Stop time")
const char* getPunchYName (int index) noexcept;       // what Y does
const char* getSendFxName (SendFxType t) noexcept;
const char* getSendXName (SendFxType t) noexcept;
const char* getSendYName (SendFxType t) noexcept;

/** Amount from a note velocity (K.O. II pads): soft hits still do something. */
inline float punchAmountFromVelocity (int velocity) noexcept
{
    return 0.25f + 0.75f * (float) juce::jlimit (0, 127, velocity) / 127.0f;
}
/** Beat repeat length (in beats) for an amount: 1/4 note at 0 .. 1/32 note at 1 (harder = shorter). */
double punchRepeatBeats (float amount) noexcept;

struct SendFxSettings
{
    SendFxType type = SendFxType::Off;
    float x = 0.5f, y = 0.5f;
    float level = 0.0f;       // 0 .. 1 send amount
};

class SendFx
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;
    void setSettings (const SendFxSettings& s) noexcept { settings = s; }
    const SendFxSettings& getSettings() const noexcept { return settings; }
    /** Adds the wet send of the signal to it, in place. extraSend: more send while "Send Throw" is held. */
    void process (float* l, float* r, int n, float extraSend) noexcept;
    bool isActive() const noexcept { return sendGain > 1.0e-4f || tail > 0; }

private:
    SendFxSettings settings;
    double sampleRate = 44100.0;
    std::array<std::vector<float>, 2> delay;
    int dSize = 1, dWrite = 0;
    juce::Reverb reverb;
    float lp[2] {}, ic1[2] {}, ic2[2] {}, env = 0.0f;
    double lfo = 0.0;
    float sendGain = 0.0f;
    SendFxType active = SendFxType::Off;
    int tail = 0;
    std::vector<float> wetL, wetR;
};

struct PunchFxSettings
{
    std::array<bool, kNumPunchFx> held {};
    float amount = 0.6f;     // 0 .. 1
    float x = 0.5f, y = 0.5f;
};

class PunchFx
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset() noexcept;
    void setSettings (const PunchFxSettings& s) noexcept { settings = s; }
    const PunchFxSettings& getSettings() const noexcept { return settings; }
    /** True while any punch FX is held or still fading out. */
    bool isActive() const noexcept;
    bool isOn (int index) const noexcept { return gains[(size_t) index] > 1.0e-3f; }
    /** Send Throw: extra send into the Send FX this block (0 .. 1). */
    float getSendThrow() const noexcept { return sendThrow; }

    /** ppqStart / ppqPerSample: grid for the synced effects (beat repeat, slice swap, scatter). */
    void process (float* l, float* r, int n, double ppqStart, double ppqPerSample) noexcept;

private:
    float readHistory (int ch, double samplesAgo) const noexcept;
    void svf (int which, float x[2], float cutoffHz, float q, float out[2], int mode) noexcept;

    PunchFxSettings settings;
    double sampleRate = 44100.0;
    std::array<float, kNumPunchFx> gains {};
    float sendThrow = 0.0f;

    // history of the input (slice swap, grains, beat repeat, tape stop)
    std::array<std::vector<float>, 2> hist;
    int hSize = 1, hWrite = 0;

    // beat repeat
    bool repActive = false;
    double repStartAgo = 0.0, repPos = 0.0, repLen = 1.0;
    int repCount = 0;
    double lastRepCell = -1.0;
    // slice swap
    double swapAgo = 0.0, lastSwapCell = -1.0;
    bool swapOn = false;
    juce::Random rng { 1234 };
    // grains
    struct Grain { double ago = 0.0, pos = 0.0, len = 1.0; bool on = false; };
    std::array<Grain, 4> grains {};
    double grainClock = 0.0;
    // tape stop
    double tapeLag = 0.0, tapeSpeed = 1.0;
    // pitch scatter / octave drop
    PitchShifter scatterShift, dropShift;
    float scatterRatio = 1.0f;
    double lastScatterCell = -1.0;
    float dropRatio = 1.0f;
    // filters: state per filter (wobble, low-pass, high-pass) x channel
    float fic1[3][2] {}, fic2[3][2] {};
    double wobblePhase = 0.0, tremPhase = 0.0;
    // decimator
    float decHold[2] {};
    float decCounter = 0.0f;
    std::vector<float> dryL, dryR;
};
} // namespace livelooper
