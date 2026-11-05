#pragma once

#include <algorithm>
#include <cmath>

namespace livelooper::velocity
{
/** v1.7 velocity curves per controller (Push 1, K.O. II, generic MIDI): host parameters vel_push,
    vel_ko2, vel_midi. Soft = light hits come out louder, Hard = you have to hit harder, Fixed = every
    hit is full velocity. */
enum class Curve : int { Soft = 0, Normal, Hard, Fixed };

inline constexpr int kFixedVelocity = 127;

/** Maps a note-on velocity 1 .. 127 through the curve (0 stays 0 = note off). */
inline int apply (Curve c, int v) noexcept
{
    if (v <= 0)
        return 0;
    v = std::min (127, v);
    const double x = (double) v / 127.0;
    switch (c)
    {
        case Curve::Soft:   return std::clamp ((int) std::lround (127.0 * std::pow (x, 0.5)), 1, 127);
        case Curve::Hard:   return std::clamp ((int) std::lround (127.0 * std::pow (x, 2.0)), 1, 127);
        case Curve::Fixed:  return kFixedVelocity;
        case Curve::Normal: break;
    }
    return v;
}

inline Curve fromIndex (int i) noexcept { return (Curve) std::clamp (i, 0, 3); }
} // namespace livelooper::velocity
