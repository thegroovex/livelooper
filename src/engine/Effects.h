#pragma once

#include "Types.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace livelooper
{
enum class FxType : int
{
    LowPass = 0, HighPass, BandPass, Phaser, Flanger, Delay, Reverb,
    Slicer, Tremolo, AutoPan, LoFi, Overdrive,
    NumTypes
};

const char* getFxTypeName (FxType t) noexcept;
const char* getFxParam1Name (FxType t) noexcept;
const char* getFxParam2Name (FxType t) noexcept;

struct FxSettings
{
    bool   enabled = false;
    FxType type    = FxType::LowPass;
    float  p1      = 0.5f;   // 0..1, meaning depends on type
    float  p2      = 0.5f;   // 0..1, meaning depends on type
    float  mix     = 1.0f;   // 0..1 dry/wet
};

/** One effect slot. Processes stereo audio in place; tempo-synced types follow the timeline. */
class FxUnit
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;
    void setSettings (const FxSettings& s) noexcept;

    /** ppqStart: timeline position of the first sample, ppqPerSample: timeline advance per sample. */
    void process (float* left, float* right, int numSamples, double ppqStart, double ppqPerSample, double bpm) noexcept;

private:
    FxSettings settings;
    FxType activeType = FxType::LowPass;
    double sampleRate = 44100.0;

    // state variable filter
    float ic1[2] {}, ic2[2] {};
    // phaser
    float apState[2][6] {};
    float phaserFb[2] {};
    // delay line (also used by the flanger)
    std::vector<float> delayLine[2];
    int   writePos = 0;
    // reverb
    juce::Reverb reverb;
    // lo-fi
    float holdL = 0.0f, holdR = 0.0f, holdCounter = 0.0f;
    // overdrive tone filter
    float toneState[2] {};
    // free running LFO (phaser / flanger)
    double lfoPhase = 0.0;
    // smoothed wet gain to avoid clicks when switching
    float wetGain = 0.0f;
};

/** A chain of four slots (A-D), used for the input FX and for the track FX bus. */
class FxChain
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;
    void setSlot (int index, const FxSettings& s) noexcept { units[(size_t) index].setSettings (s); }
    void process (float* left, float* right, int numSamples, double ppqStart, double ppqPerSample, double bpm) noexcept;

private:
    std::array<FxUnit, kNumFxSlots> units;
};
} // namespace livelooper
