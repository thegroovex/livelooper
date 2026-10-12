#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace livelooper
{
constexpr int kNumTracks  = 5;
constexpr int kNumFxSlots = 4;

/** Musical position of one audio block, as delivered by the host (or the internal clock). */
struct TimeInfo
{
    double bpm          = 120.0;
    bool   hostPlaying  = false;   // host transport is running and reports a ppq position
    double ppq          = 0.0;     // host position in quarter notes at the first sample of the block
    int    numerator    = 4;
    int    denominator  = 4;
};

enum class Quantize   { Off = 0, Beat, Measure };
enum class RecAction  { RecDub = 0, RecPlay };
enum class StartMode  { Immediate = 0, FadeIn };
enum class StopMode   { Immediate = 0, FadeOut, LoopEnd };
enum class DubMode    { Overdub = 0, Replace };
enum class InputRoute { Stereo = 0, Left, Right };

/** Measures for the first recording of a track: Auto = free / rounded by quantize, otherwise
    the recording finishes by itself after the given number of measures. */
constexpr std::array<int, 6> kMeasureChoices { 0, 1, 2, 4, 8, 16 };

/** Per-track settings that are read once per block from the parameters. */
struct TrackSettings
{
    float      level     = 1.0f;   // 0..2 (0..200 %)
    float      pan       = 0.0f;   // -1..1
    bool       reverse   = false;
    bool       oneShot   = false;
    bool       loopSync  = true;
    bool       trackFx   = false;  // route this track through the track FX bus
    InputRoute input     = InputRoute::Stereo;
    StartMode  startMode = StartMode::Immediate;
    StopMode   stopMode  = StopMode::Immediate;
    DubMode    dubMode   = DubMode::Overdub;
    int        measures  = 0;      // 0 = auto
};

struct GlobalSettings
{
    Quantize  quantize      = Quantize::Measure;
    RecAction recAction     = RecAction::RecDub;
    bool      syncToHost    = true;
    double    internalTempo = 120.0;
    float     fadeMs        = 500.0f;
    float     masterLevel   = 1.0f;
    float     monitorLevel  = 1.0f;  // input pass-through (after input FX)
};

inline double positiveModulo (double a, double b) noexcept
{
    auto r = std::fmod (a, b);
    return r < 0.0 ? r + b : r;
}
} // namespace livelooper
