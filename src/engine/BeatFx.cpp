#include "BeatFx.h"

#include <algorithm>
#include <cmath>

namespace livelooper
{
namespace
{
    constexpr double kPi = 3.14159265358979323846;

    inline float smoothCoeff (double sampleRate, double ms) noexcept
    {
        return (float) (1.0 - std::exp (-1.0 / (std::max (0.01, ms) * 0.001 * sampleRate)));
    }

    /** TPT state-variable filter step: returns band, writes low and high. */
    inline float svf (float x, float g, float k, float& ic1, float& ic2, float& lp, float& hp) noexcept
    {
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + g * a1 * v3;
        const float v2 = ic2 + g * v1;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2;
        hp = x - k * v1 - v2;
        return v1;
    }

    inline float svfG (double sampleRate, double freq) noexcept
    {
        freq = std::clamp (freq, 15.0, sampleRate * 0.45);
        return (float) std::tan (kPi * freq / sampleRate);
    }

    inline float whiteNoise (uint32_t& seed) noexcept
    {
        seed = seed * 1664525u + 1013904223u;
        return (float) (int32_t) seed / 2147483648.0f;
    }

    inline bool crossed (double prevPpq, double ppq, double grid) noexcept
    {
        return std::floor (ppq / grid + 1.0e-9) != std::floor (prevPpq / grid + 1.0e-9);
    }
}

//==============================================================================
const char* getBeatFxName (BeatFxType t) noexcept
{
    switch (t)
    {
        case BeatFxType::Delay:       return "Delay";
        case BeatFxType::Echo:        return "Echo";
        case BeatFxType::PingPong:    return "Ping Pong";
        case BeatFxType::Spiral:      return "Spiral";
        case BeatFxType::Reverb:      return "Reverb";
        case BeatFxType::Trans:       return "Trans";
        case BeatFxType::Filter:      return "Filter";
        case BeatFxType::Flanger:     return "Flanger";
        case BeatFxType::Phaser:      return "Phaser";
        case BeatFxType::Pitch:       return "Pitch";
        case BeatFxType::Roll:        return "Roll";
        case BeatFxType::SlipRoll:    return "Slip Roll";
        case BeatFxType::ReverseRoll: return "Reverse Roll";
        case BeatFxType::VinylBrake:  return "Vinyl Brake";
        case BeatFxType::Helix:       return "Helix";
        case BeatFxType::Noise:       return "Noise";
        case BeatFxType::NumTypes:    break;
    }
    return "?";
}

const char* getBeatFxDepthName (BeatFxType t) noexcept
{
    switch (t)
    {
        case BeatFxType::Trans: case BeatFxType::Filter: case BeatFxType::Flanger: case BeatFxType::Phaser:
            return "Depth";
        case BeatFxType::Pitch:
            return "Pitch";
        default:
            return "Level";
    }
}

const char* getColorFxName (ColorFxType t) noexcept
{
    switch (t)
    {
        case ColorFxType::Filter:  return "Filter";
        case ColorFxType::DubEcho: return "Dub Echo";
        case ColorFxType::Space:   return "Space";
        case ColorFxType::Crush:   return "Crush";
        case ColorFxType::Noise:   return "Noise";
        case ColorFxType::Sweep:   return "Sweep";
        case ColorFxType::NumTypes: break;
    }
    return "?";
}

//============================================================================== PitchShifter
void PitchShifter::prepare (double sampleRate)
{
    for (auto& b : buf)
        b.assign ((size_t) kSize, 0.0f);
    window = std::clamp ((int) (0.045 * sampleRate), 256, kSize / 2 - 4);
    reset();
}

void PitchShifter::reset() noexcept
{
    for (auto& b : buf)
        std::fill (b.begin(), b.end(), 0.0f);
    write[0] = write[1] = 0;
    phase[0] = phase[1] = 0.0;
}

float PitchShifter::process (int ch, float x, float ratio) noexcept
{
    auto& b = buf[(size_t) ch];
    if (b.empty())
        return x;
    b[(size_t) write[ch]] = x;
    // two read taps half a window apart; the delay of each tap changes by (1 - ratio) per sample
    phase[ch] += (1.0 - (double) ratio) / (double) window;
    phase[ch] -= std::floor (phase[ch]);
    float out = 0.0f;
    for (int tap = 0; tap < 2; ++tap)
    {
        double p = phase[ch] + 0.5 * tap;
        p -= std::floor (p);
        const double delay = 2.0 + p * (double) window;
        double rp = (double) write[ch] - delay;
        while (rp < 0.0)
            rp += kSize;
        const int i0 = (int) rp;
        const float f = (float) (rp - i0);
        const float s = b[(size_t) (i0 % kSize)] * (1.0f - f) + b[(size_t) ((i0 + 1) % kSize)] * f;
        const float w = (float) std::sin (kPi * p);   // triangle-ish window, taps sum to ~1
        out += s * w * w;
    }
    write[ch] = (write[ch] + 1) % kSize;
    return out;
}

//============================================================================== BeatFx
void BeatFx::prepare (double sr, double maxSeconds)
{
    sampleRate = sr;
    size = std::max (1024, (int) (sr * maxSeconds));
    for (auto& b : buf)
        b.assign ((size_t) size, 0.0f);
    shifter.prepare (sr);
    reverb.setSampleRate (sr);
    reset();
}

void BeatFx::reset() noexcept
{
    for (auto& b : buf)
        std::fill (b.begin(), b.end(), 0.0f);
    writePos = 0;
    wetGain = inGain = 0.0f;
    tailActive = false;
    tailSamples = 0;
    fbState[0] = fbState[1] = 0.0f;
    smoothedDelay = 0.0;
    spiralPhase = 0.0;
    rolling = rollWaiting = braking = false;
    rollXfade = 0.0f;
    rollPos = 0.0;
    rollSpeed = 1.0;
    rollRepeats = 0;
    lfoPpq = 0.0;
    for (int c = 0; c < 2; ++c)
    {
        svfIc1[c] = svfIc2[c] = phaserFb[c] = noiseLp[c] = 0.0f;
        for (auto& s : apState[c])
            s = 0.0f;
    }
    reverb.reset();
    shifter.reset();
    activeType = settings.type;
}

double BeatFx::fractionSamples (int fraction, double ppqPerSample) noexcept
{
    const auto beats = kBeatFractions[(size_t) std::clamp (fraction, 0, (int) kBeatFractions.size() - 1)];
    return beats / std::max (1.0e-9, ppqPerSample);
}

float BeatFx::readBuffer (int ch, double pos) const noexcept
{
    const auto& b = buf[(size_t) ch];
    pos = std::fmod (pos, (double) size);
    if (pos < 0.0)
        pos += size;
    const int i0 = (int) pos;
    const float f = (float) (pos - i0);
    return b[(size_t) i0] * (1.0f - f) + b[(size_t) ((i0 + 1) % size)] * f;
}

void BeatFx::process (float* left, float* right, int n, double ppqStart, double ppqPerSample) noexcept
{
    if (settings.type != activeType)
    {
        // a new type starts from a clean state (the old one is cut, as on a DJ mixer)
        const auto keep = settings;
        reset();
        settings = keep;
        activeType = settings.type;
    }
    if (! settings.on && wetGain < 1.0e-4f && inGain < 1.0e-4f && ! tailActive && ! rolling && ! braking)
    {
        // idle: bit-exact pass-through, but keep the roll buffer filled
        for (int i = 0; i < n; ++i)
        {
            buf[0][(size_t) writePos] = left[i];
            buf[1][(size_t) writePos] = right[i];
            writePos = (writePos + 1) % size;
        }
        rollWaiting = false;
        return;
    }

    double fracSamples = fractionSamples (settings.fraction, ppqPerSample);
    switch (activeType)
    {
        case BeatFxType::Roll: case BeatFxType::SlipRoll: case BeatFxType::ReverseRoll:
        case BeatFxType::Helix: case BeatFxType::VinylBrake:
            processRollFamily (left, right, n, ppqStart, ppqPerSample, std::min (fracSamples, size * 0.45));
            return;
        case BeatFxType::Delay: case BeatFxType::Echo: case BeatFxType::PingPong: case BeatFxType::Spiral:
            processDelayFamily (left, right, n, std::clamp (fracSamples, 16.0, size - 4.0));
            return;
        default:
            break;
    }

    const float depth = std::clamp (settings.depth, 0.0f, 1.0f);
    const float sm = smoothCoeff (sampleRate, 8.0);
    const bool isReverb = activeType == BeatFxType::Reverb;

    if (isReverb)
    {
        // reverb: the beat fraction sets the size (short fraction = small room, 16 beats = hall)
        juce::Reverb::Parameters p;
        const float sizeAmount = (float) settings.fraction / (float) (kBeatFractions.size() - 1);
        p.roomSize = 0.45f + 0.53f * sizeAmount;
        p.damping = 0.45f - 0.25f * sizeAmount;
        p.wetLevel = 0.35f;
        p.dryLevel = 0.0f;
        p.width = 1.0f;
        reverb.setParameters (p);

        if (! settings.on && inGain > 1.0e-4f && ! tailActive)
        {
            tailActive = true;
            tailSamples = (int) (sampleRate * (2.0 + 6.0 * sizeAmount));
        }
        if (settings.on)
            tailActive = false;

        float tmpL[256], tmpR[256];
        for (int done = 0; done < n;)
        {
            const int m = std::min (256, n - done);
            for (int i = 0; i < m; ++i)
            {
                inGain += ((settings.on ? 1.0f : 0.0f) - inGain) * sm;
                const float wetTarget = (settings.on || tailActive) ? depth : 0.0f;
                wetGain += (wetTarget - wetGain) * sm;
                tmpL[i] = left[done + i] * inGain;
                tmpR[i] = right[done + i] * inGain;
                buf[0][(size_t) writePos] = left[done + i];
                buf[1][(size_t) writePos] = right[done + i];
                writePos = (writePos + 1) % size;
            }
            reverb.processStereo (tmpL, tmpR, m);
            for (int i = 0; i < m; ++i)
            {
                left[done + i] += tmpL[i] * wetGain;
                right[done + i] += tmpR[i] * wetGain;
            }
            done += m;
        }
        if (tailActive && (tailSamples -= n) <= 0)
            tailActive = false;
        return;
    }

    // ---- modulation effects: wet follows on / off, depth scales the effect
    const double period = std::max (1.0e-3, kBeatFractions[(size_t) std::clamp (settings.fraction, 0, (int) kBeatFractions.size() - 1)]);
    const float target = settings.on ? 1.0f : 0.0f;
    const float pitchRatio = (float) std::pow (2.0, (depth * 24.0 - 12.0) / 12.0);

    for (int i = 0; i < n; ++i)
    {
        const double ppq = ppqStart + i * ppqPerSample;
        double ph = std::fmod (ppq / period, 1.0);
        if (ph < 0.0)
            ph += 1.0;
        wetGain += (target - wetGain) * sm;
        const float w = wetGain;
        const float xl = left[i], xr = right[i];
        buf[0][(size_t) writePos] = xl;
        buf[1][(size_t) writePos] = xr;

        float yl = xl, yr = xr;
        switch (activeType)
        {
            case BeatFxType::Trans:
            {
                // gate: open for the first half of every fraction; depth = how far it closes
                const float open = ph < 0.5 ? 1.0f : 0.0f;
                const float goal = 1.0f - depth * (1.0f - open);
                fbState[0] += (goal - fbState[0]) * smoothCoeff (sampleRate, 1.5);
                const float g = 1.0f - w * (1.0f - fbState[0]);
                yl = xl * g; yr = xr * g;
                break;
            }
            case BeatFxType::Filter:
            {
                // resonant low-pass whose cutoff sweeps once per fraction (cosine LFO)
                const double lfo = 0.5 - 0.5 * std::cos (2.0 * kPi * ph);
                const float g = svfG (sampleRate, 250.0 * std::pow (2.0, 6.2 * lfo));
                const float k = 1.0f / (0.8f + 3.0f * depth);
                float lp, hp;
                svf (xl, g, k, svfIc1[0], svfIc2[0], lp, hp); yl = xl + w * (lp - xl);
                svf (xr, g, k, svfIc1[1], svfIc2[1], lp, hp); yr = xr + w * (lp - xr);
                break;
            }
            case BeatFxType::Flanger:
            {
                const double lfo = 0.5 - 0.5 * std::cos (2.0 * kPi * ph);
                const double d = (0.0008 + 0.0052 * lfo) * sampleRate;
                const float dl = readBuffer (0, writePos - d), dr = readBuffer (1, writePos - d);
                buf[0][(size_t) writePos] = xl + dl * 0.65f;
                buf[1][(size_t) writePos] = xr + dr * 0.65f;
                const float mixAmt = 0.5f * w * (0.4f + 0.6f * depth);
                yl = xl * (1.0f - mixAmt) + dl * mixAmt * 1.4f;
                yr = xr * (1.0f - mixAmt) + dr * mixAmt * 1.4f;
                break;
            }
            case BeatFxType::Phaser:
            {
                const double lfo = 0.5 - 0.5 * std::cos (2.0 * kPi * ph);
                const double f = 300.0 * std::pow (2.0, 4.0 * lfo);
                const float t = (float) std::tan (kPi * f / sampleRate);
                const float a = (t - 1.0f) / (t + 1.0f);
                float* outs[2] = { &yl, &yr };
                const float ins[2] = { xl, xr };
                for (int c = 0; c < 2; ++c)
                {
                    float s = ins[c] + phaserFb[c] * (0.3f + 0.4f * depth);
                    for (auto& st : apState[c])
                    {
                        const float y = a * s + st;
                        st = s - a * y;
                        s = y;
                    }
                    phaserFb[c] = s;
                    *outs[c] = ins[c] * (1.0f - 0.5f * w) + s * 0.5f * w;
                }
                break;
            }
            case BeatFxType::Pitch:
            {
                const float sl = shifter.process (0, xl, pitchRatio), sr = shifter.process (1, xr, pitchRatio);
                yl = xl + w * (sl - xl);
                yr = xr + w * (sr - xr);
                break;
            }
            case BeatFxType::Noise:
            {
                // white noise through a high-pass that sweeps once per fraction, added on top
                const double lfo = 0.5 - 0.5 * std::cos (2.0 * kPi * ph);
                const float g = svfG (sampleRate, 400.0 * std::pow (2.0, 5.0 * lfo));
                float lp, hp;
                svf (whiteNoise (noiseSeed), g, 1.2f, svfIc1[0], svfIc2[0], lp, hp);
                const float nz = hp * 0.3f * depth * w;
                yl = xl + nz;
                yr = xr + nz;
                break;
            }
            default:
                break;
        }
        writePos = (writePos + 1) % size;
        left[i] = yl;
        right[i] = yr;
    }
}

void BeatFx::processDelayFamily (float* l, float* r, int n, double delaySamples) noexcept
{
    const float depth = std::clamp (settings.depth, 0.0f, 1.0f);
    const float sm = smoothCoeff (sampleRate, 8.0);
    const auto type = activeType;
    const bool tails = type != BeatFxType::Delay;
    if (smoothedDelay <= 0.0)
        smoothedDelay = delaySamples;

    float fb = 0.0f;
    switch (type)
    {
        case BeatFxType::Delay:    fb = 0.0f; break;                    // one clean repeat, like a DJ delay
        case BeatFxType::Echo:     fb = 0.45f + 0.4f * depth; break;    // repeats fade out
        case BeatFxType::PingPong: fb = 0.5f + 0.35f * depth; break;
        case BeatFxType::Spiral:   fb = 0.72f + 0.2f * depth; break;    // long, rising repeats
        default: break;
    }
    if (! settings.on && inGain > 1.0e-4f && tails && ! tailActive)
    {
        tailActive = true;
        tailSamples = (int) (sampleRate * 8.0);
    }
    if (settings.on)
        tailActive = false;
    const float lpG = svfG (sampleRate, type == BeatFxType::Spiral ? 7000.0 : 4500.0);
    const float lpA = lpG / (1.0f + lpG);
    const double glide = 1.0 - std::exp (-1.0 / (0.05 * sampleRate));

    for (int i = 0; i < n; ++i)
    {
        inGain += ((settings.on ? 1.0f : 0.0f) - inGain) * sm;
        const float wetTarget = (settings.on || tailActive) ? (type == BeatFxType::Delay ? 0.85f * depth + 0.15f : depth) : 0.0f;
        wetGain += (wetTarget - wetGain) * sm;

        double d = delaySamples;
        if (type == BeatFxType::Spiral)
        {
            // the delay time shrinks over four fractions and starts again: each repeat rises
            spiralPhase += 1.0 / (4.0 * delaySamples);
            if (spiralPhase >= 1.0)
                spiralPhase -= 1.0;
            d = delaySamples * (1.0 - 0.45 * spiralPhase);
        }
        smoothedDelay += (d - smoothedDelay) * glide;
        const double rp = (double) writePos - smoothedDelay;
        const float dl = readBuffer (0, rp), dr = readBuffer (1, rp);
        // feedback through a gentle low-pass (darker each repeat)
        fbState[0] += (dl - fbState[0]) * lpA;
        fbState[1] += (dr - fbState[1]) * lpA;
        const float inL = l[i] * inGain, inR = r[i] * inGain;
        if (type == BeatFxType::PingPong)
        {
            buf[0][(size_t) writePos] = (inL + inR) * 0.5f + fbState[1] * fb;
            buf[1][(size_t) writePos] = fbState[0] * fb;
        }
        else
        {
            buf[0][(size_t) writePos] = inL + fbState[0] * fb;
            buf[1][(size_t) writePos] = inR + fbState[1] * fb;
        }
        writePos = (writePos + 1) % size;
        l[i] += dl * wetGain;
        r[i] += dr * wetGain;
    }
    if (tailActive && (tailSamples -= n) <= 0)
        tailActive = false;
}

void BeatFx::processRollFamily (float* l, float* r, int n, double ppqStart, double ppqPerSample, double fracSamples) noexcept
{
    const float depth = std::clamp (settings.depth, 0.0f, 1.0f);
    const float xs = smoothCoeff (sampleRate, 3.0);
    const auto type = activeType;
    const double fracPpq = fracSamples * ppqPerSample;
    const double grid = std::min (fracPpq, 1.0);                    // starts on the next fraction, at most a beat away
    const double slipGrid = fracPpq < 1.0 ? 1.0 : fracPpq * 2.0;   // slip roll: follows the music every beat
    const double seam = std::min (0.002 * sampleRate, fracSamples * 0.1);
    double prevPpq = ppqStart - ppqPerSample;

    if (settings.on && ! rolling && ! braking)
        rollWaiting = true;
    if (! settings.on)
    {
        rollWaiting = false;
        rolling = braking = false;
    }

    for (int i = 0; i < n; ++i)
    {
        const double ppq = ppqStart + i * ppqPerSample;
        buf[0][(size_t) writePos] = l[i];
        buf[1][(size_t) writePos] = r[i];

        if (rollWaiting && (type == BeatFxType::VinylBrake || crossed (prevPpq, ppq, grid)))
        {
            rollWaiting = false;
            rollLen = std::max (16.0, fracSamples);
            rollPos = 0.0;
            rollRepeats = 0;
            rollSpeed = 1.0;
            if (type == BeatFxType::VinylBrake)
            {
                braking = true;
                brakeSpeed = 1.0;
                brakeRead = (double) writePos;
            }
            else
            {
                rolling = true;
                // forward rolls record the slice from now on; reverse / helix replay the slice just played
                rollStart = (type == BeatFxType::ReverseRoll || type == BeatFxType::Helix) ? (double) writePos - rollLen
                                                                                           : (double) writePos;
            }
        }
        if (rolling && type == BeatFxType::SlipRoll && rollRepeats > 0 && crossed (prevPpq, ppq, slipGrid))
        {
            rollStart = (double) writePos;
            rollPos = 0.0;
        }
        prevPpq = ppq;

        float wl = l[i], wr = r[i];
        const bool engaged = rolling || braking;
        rollXfade += ((engaged ? 1.0f : 0.0f) - rollXfade) * xs;

        if (rolling)
        {
            double rp = type == BeatFxType::ReverseRoll ? rollStart + (rollLen - 1.0 - rollPos) : rollStart + rollPos;
            wl = readBuffer (0, rp);
            wr = readBuffer (1, rp);
            // short fades at the slice seams
            const double edge = std::min (rollPos, rollLen - rollPos);
            if (rollRepeats > 0 || type != BeatFxType::Roll)
            {
                const float e = (float) std::clamp (edge / std::max (1.0, seam), 0.0, 1.0);
                wl *= e; wr *= e;
            }
            rollPos += rollSpeed;
            if (rollPos >= rollLen)
            {
                rollPos -= rollLen;
                ++rollRepeats;
                if (type == BeatFxType::Helix)
                    rollSpeed = std::min (2.0, rollSpeed * std::pow (2.0, (1.0 + 2.0 * depth) / 12.0));
            }
        }
        else if (braking)
        {
            wl = readBuffer (0, brakeRead);
            wr = readBuffer (1, brakeRead);
            brakeRead += brakeSpeed;
            brakeSpeed = std::max (0.0, brakeSpeed - 1.0 / std::max (0.05 * sampleRate, fracSamples));
        }
        writePos = (writePos + 1) % size;

        const float m = rollXfade * (0.25f + 0.75f * depth);
        l[i] = l[i] * (1.0f - m) + wl * m;
        r[i] = r[i] * (1.0f - m) + wr * m;
    }
    wetGain = rollXfade;
}

//============================================================================== ColorFx
void ColorFx::prepare (double sr)
{
    sampleRate = sr;
    for (auto& d : delay)
        d.assign ((size_t) std::max (1024, (int) (sr * 2.0)), 0.0f);
    reverb.setSampleRate (sr);
    juce::Reverb::Parameters p;
    p.roomSize = 0.86f;
    p.damping = 0.35f;
    p.wetLevel = 0.35f;
    p.dryLevel = 0.0f;
    p.width = 1.0f;
    reverb.setParameters (p);
    reset();
}

void ColorFx::reset() noexcept
{
    smoothed = wet = 0.0f;
    active = false;
    for (int c = 0; c < 2; ++c)
        ic1[c] = ic2[c] = sendIc1[c] = sendIc2[c] = fbLp[c] = hold[c] = 0.0f;
    for (auto& d : delay)
        std::fill (d.begin(), d.end(), 0.0f);
    delayWrite = 0;
    holdCounter = 0.0f;
    sweepPhase = 0.0;
    reverb.reset();
}

void ColorFx::process (float* left, float* right, int n, ColorFxType type, float amount, double ppqPerSample) noexcept
{
    amount = std::clamp (amount, -1.0f, 1.0f);
    const bool on = std::abs (amount) > kDeadZone;
    if (type != lastType)
    {
        reset();
        lastType = type;
    }
    if (! on && ! active)
        return;                                     // bit-exact when the knob is centred
    active = true;

    const float sm = smoothCoeff (sampleRate, 15.0);
    const bool sendType = type == ColorFxType::DubEcho || type == ColorFxType::Space;
    const int dSize = (int) delay[0].size();
    const double echoDelay = std::clamp (0.75 / std::max (1.0e-9, ppqPerSample), 64.0, dSize - 4.0);   // dotted 8th
    float energy = 0.0f;

    float tmpL[256], tmpR[256];
    for (int done = 0; done < n;)
    {
        const int m = std::min (256, n - done);
        for (int i = 0; i < m; ++i)
        {
            const int k = done + i;
            smoothed += ((on ? amount : 0.0f) - smoothed) * sm;
            wet += ((on ? 1.0f : 0.0f) - wet) * sm;
            const float a = std::abs (smoothed);
            const bool low = smoothed < 0.0f;
            const float xl = left[k], xr = right[k];
            float yl = xl, yr = xr;

            switch (type)
            {
                case ColorFxType::Filter:
                {
                    const double f = low ? 20000.0 * std::pow (90.0 / 20000.0, (double) a) : 20.0 * std::pow (7000.0 / 20.0, (double) a);
                    const float g = svfG (sampleRate, f);
                    const float kk = 1.0f / (0.75f + 0.9f * a);
                    float lp, hp;
                    svf (xl, g, kk, ic1[0], ic2[0], lp, hp); yl = low ? lp : hp;
                    svf (xr, g, kk, ic1[1], ic2[1], lp, hp); yr = low ? lp : hp;
                    yl = xl + wet * (yl - xl);
                    yr = xr + wet * (yr - xr);
                    break;
                }
                case ColorFxType::DubEcho:
                {
                    double rp = (double) delayWrite - echoDelay;
                    if (rp < 0.0) rp += dSize;
                    const int i0 = (int) rp % dSize;
                    const float dl = delay[0][(size_t) i0], dr = delay[1][(size_t) i0];
                    const float g = svfG (sampleRate, low ? 1400.0 - 900.0 * a : 700.0 + 1800.0 * a);
                    float lp, hp;
                    svf (dl, g, 1.0f, sendIc1[0], sendIc2[0], lp, hp); const float fl = low ? lp : hp;
                    svf (dr, g, 1.0f, sendIc1[1], sendIc2[1], lp, hp); const float fr = low ? lp : hp;
                    const float fb = 0.5f + 0.35f * a;
                    const float send = on ? a : 0.0f;
                    delay[0][(size_t) delayWrite] = xl * send + fr * fb;   // a little ping-pong for width
                    delay[1][(size_t) delayWrite] = xr * send + fl * fb;
                    delayWrite = (delayWrite + 1) % dSize;
                    yl = xl + dl;
                    yr = xr + dr;
                    energy = std::max (energy, std::abs (dl) + std::abs (dr));
                    break;
                }
                case ColorFxType::Space:
                    tmpL[i] = xl * (on ? a : 0.0f);
                    tmpR[i] = xr * (on ? a : 0.0f);
                    break;
                case ColorFxType::Crush:
                {
                    const float bits = 16.0f - 13.0f * a;
                    const float q = std::pow (2.0f, bits - 1.0f);
                    const float rate = 1.0f + 24.0f * a * a;
                    holdCounter += 1.0f;
                    if (holdCounter >= rate)
                    {
                        holdCounter -= rate;
                        hold[0] = std::round (xl * q) / q;
                        hold[1] = std::round (xr * q) / q;
                    }
                    float cl = hold[0], cr = hold[1];
                    // left: darker, right: thinner
                    const float g = svfG (sampleRate, low ? 9000.0 - 6000.0 * a : 60.0 + 900.0 * a);
                    float lp, hp;
                    svf (cl, g, 1.2f, ic1[0], ic2[0], lp, hp); cl = low ? lp : hp;
                    svf (cr, g, 1.2f, ic1[1], ic2[1], lp, hp); cr = low ? lp : hp;
                    yl = xl + wet * (cl - xl);
                    yr = xr + wet * (cr - xr);
                    break;
                }
                case ColorFxType::Noise:
                {
                    const float nz = whiteNoise (noiseSeed);
                    const float g = svfG (sampleRate, low ? 12000.0 * std::pow (0.05, (double) a) : 300.0 * std::pow (30.0, (double) a));
                    float lp, hp;
                    svf (nz, g, 1.0f, ic1[0], ic2[0], lp, hp);
                    const float s = (low ? lp : hp) * 0.25f * a * wet;
                    yl = xl + s;
                    yr = xr + s;
                    break;
                }
                case ColorFxType::Sweep:
                {
                    if (low)
                    {
                        // gate on 16th notes, deeper with the knob
                        sweepPhase += ppqPerSample * 4.0;
                        sweepPhase -= std::floor (sweepPhase);
                        const float goal = sweepPhase < 0.5 ? 1.0f : 1.0f - a;
                        hold[0] += (goal - hold[0]) * smoothCoeff (sampleRate, 1.5);
                        yl = xl * (1.0f - wet + wet * hold[0]);
                        yr = xr * (1.0f - wet + wet * hold[0]);
                    }
                    else
                    {
                        // band-pass that sweeps up with the knob
                        const float g = svfG (sampleRate, 150.0 * std::pow (2.0, 6.5 * (double) a));
                        float lp, hp;
                        const float bl = svf (xl, g, 0.35f, ic1[0], ic2[0], lp, hp);
                        const float br = svf (xr, g, 0.35f, ic1[1], ic2[1], lp, hp);
                        yl = xl + wet * (bl * 1.6f - xl);
                        yr = xr + wet * (br * 1.6f - xr);
                    }
                    break;
                }
                case ColorFxType::NumTypes:
                    break;
            }
            left[k] = yl;
            right[k] = yr;
        }
        if (type == ColorFxType::Space)
        {
            reverb.processStereo (tmpL, tmpR, m);
            const float a = std::abs (smoothed);
            const float g = svfG (sampleRate, smoothed < 0.0f ? 6000.0 - 4500.0 * a : 200.0 + 1200.0 * a);
            for (int i = 0; i < m; ++i)
            {
                float lp, hp;
                svf (tmpL[i], g, 1.0f, sendIc1[0], sendIc2[0], lp, hp); const float fl = smoothed < 0.0f ? lp : hp;
                svf (tmpR[i], g, 1.0f, sendIc1[1], sendIc2[1], lp, hp); const float fr = smoothed < 0.0f ? lp : hp;
                left[done + i] += fl;
                right[done + i] += fr;
                energy = std::max (energy, std::abs (fl) + std::abs (fr));
            }
        }
        done += m;
    }

    // inactive again once the knob is centred and every tail has died away
    if (! on && wet < 1.0e-3f && (! sendType || energy < 1.0e-5f))
        reset();
}

//============================================================================== BeatRepeat
void BeatRepeat::prepare (double sr, double maxSeconds)
{
    sampleRate = sr;
    size = std::max (1024, (int) (sr * maxSeconds));
    for (auto& b : buf)
        b.assign ((size_t) size, 0.0f);
    reset();
}

void BeatRepeat::reset() noexcept
{
    for (auto& b : buf)
        std::fill (b.begin(), b.end(), 0.0f);
    writePos = 0;
    repeating = false;
    mix = 0.0f;
    pos = 0.0;
    recorded = 0;
    sampleCounter = 0;
    startedAtSample = -1;
}

void BeatRepeat::process (float* left, float* right, int n, double ppqStart, double ppqPerSample) noexcept
{
    const float xs = smoothCoeff (sampleRate, 2.0);
    if (! want)
        repeating = false;
    if (! want && mix < 1.0e-5f)
    {
        // idle: bit-exact pass-through, only the buffer is kept
        for (int i = 0; i < n; ++i)
        {
            buf[0][(size_t) writePos] = left[i];
            buf[1][(size_t) writePos] = right[i];
            writePos = (writePos + 1) % size;
        }
        mix = 0.0f;
        sampleCounter += n;
        return;
    }

    const int rate = std::clamp (rateIndex, 0, (int) kRepeatPpq.size() - 1);
    const double grid = kRepeatPpq[(size_t) rate];
    double prevPpq = ppqStart - ppqPerSample;

    for (int i = 0; i < n; ++i)
    {
        const double ppq = ppqStart + i * ppqPerSample;
        buf[0][(size_t) writePos] = left[i];
        buf[1][(size_t) writePos] = right[i];

        // start (or a new timing) exactly on the next grid line of the timing
        if (want && (! repeating || rate != activeRate) && crossed (prevPpq, ppq, grid))
        {
            const double len = std::clamp (grid / std::max (1.0e-9, ppqPerSample), 16.0, size * 0.45);
            if (! repeating)
            {
                sliceStart = writePos;
                recorded = 0;
                startedAtSample = sampleCounter;
            }
            else
                recorded = (int) len;      // a new timing keeps the slice start
            sliceLen = len;
            pos = 0.0;
            activeRate = rate;
            repeating = true;
        }
        prevPpq = ppq;

        float rl = left[i], rr = right[i];
        if (repeating)
        {
            const int idx = (int) (((long long) sliceStart + (long long) pos) % size);
            rl = buf[0][(size_t) idx];
            rr = buf[1][(size_t) idx];
            const double seam = std::max (1.0, std::min (0.0015 * sampleRate, sliceLen * 0.1));
            float g = 1.0f;
            if (recorded >= (int) sliceLen)     // seam fades (the first pass is the live signal itself)
                g = (float) std::clamp (std::min (pos + 1.0, sliceLen - pos) / seam, 0.0, 1.0);
            if (stutterMode)
            {
                // stutter: only the first half of every repeat sounds - chopped, machine-gun style
                const double open = sliceLen * 0.5;
                g = std::min (g, pos < open ? (float) std::clamp ((open - pos) / seam, 0.0, 1.0) : 0.0f);
            }
            rl *= g;
            rr *= g;
            if (recorded < (int) sliceLen)
                ++recorded;
            pos += 1.0;
            if (pos >= sliceLen)
                pos -= sliceLen;
        }
        mix += ((repeating ? 1.0f : 0.0f) - mix) * xs;
        left[i] = left[i] * (1.0f - mix) + rl * mix;
        right[i] = right[i] * (1.0f - mix) + rr * mix;
        writePos = (writePos + 1) % size;
        ++sampleCounter;
    }
}
} // namespace livelooper
