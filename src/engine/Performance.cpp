#include "Performance.h"

#include <algorithm>
#include <cmath>

namespace livelooper
{
//============================================================================== TapeStop
void TapeStop::prepare (double newSampleRate, double maxSeconds)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    const int want = std::max (1024, (int) (sampleRate * maxSeconds));
    if (want != size)
    {
        size = want;
        bufL.assign ((size_t) size, 0.0f);
        bufR.assign ((size_t) size, 0.0f);
    }
    reset();
}

void TapeStop::reset() noexcept
{
    std::fill (bufL.begin(), bufL.end(), 0.0f);
    std::fill (bufR.begin(), bufR.end(), 0.0f);
    writePos = 0;
    readPos = 0.0;
    speed = 1.0;
    active = returning = false;
    returnLeft = 0;
}

void TapeStop::process (float* left, float* right, int numSamples) noexcept
{
    if (size <= 0)
        return;

    const double target = 1.0 - depth;
    if (! active && depth <= 0.0f)
    {
        // idle: just keep the history, the signal passes untouched
        for (int i = 0; i < numSamples; ++i)
        {
            bufL[(size_t) writePos] = left[i];
            bufR[(size_t) writePos] = right[i];
            if (++writePos >= size)
                writePos = 0;
        }
        readPos = (double) writePos;
        return;
    }

    if (! active)
    {
        active = true;
        returning = false;
        readPos = (double) writePos;
        speed = 1.0;
    }

    // brake: ~60 ms to follow the strip, spin up on release is faster (~40 ms)
    const double brakeCoef = 1.0 - std::exp (-1.0 / (0.06 * sampleRate));
    const double spinCoef  = 1.0 - std::exp (-1.0 / (0.04 * sampleRate));
    if (depth <= 0.0f && ! returning)
    {
        returning = true;
        returnTotal = returnLeft = std::max (1, (int) (0.12 * sampleRate));
    }
    else if (depth > 0.0f && returning)
        returning = false;     // grabbed again while spinning up

    for (int i = 0; i < numSamples; ++i)
    {
        const float dryL = left[i], dryR = right[i];
        bufL[(size_t) writePos] = dryL;
        bufR[(size_t) writePos] = dryR;

        speed += ((returning ? 1.0 : target) - speed) * (returning ? spinCoef : brakeCoef);

        // the read head may not overtake the write head and may fall back at most the buffer length
        double lag = (double) writePos - readPos;
        if (lag < 0.0) lag += size;
        if (lag > size - 4)
        {
            readPos = (double) writePos - (size - 4);
            if (readPos < 0.0) readPos += size;
        }

        const int i0 = (int) readPos;
        const int i1 = (i0 + 1) % size;
        const float frac = (float) (readPos - i0);
        float wetL = bufL[(size_t) i0] + (bufL[(size_t) i1] - bufL[(size_t) i0]) * frac;
        float wetR = bufR[(size_t) i0] + (bufR[(size_t) i1] - bufR[(size_t) i0]) * frac;
        // below ~15 % speed the tape fades out (no DC when it stands still)
        const float s = (float) std::clamp (speed / 0.15, 0.0, 1.0);
        const float g = s * s * (3.0f - 2.0f * s);
        wetL *= g;
        wetR *= g;

        if (returning)
        {
            const float w = 1.0f - (float) returnLeft / (float) returnTotal;     // 0 -> 1: towards the live signal
            left[i]  = wetL * (1.0f - w) + dryL * w;
            right[i] = wetR * (1.0f - w) + dryR * w;
        }
        else
        {
            left[i] = wetL;
            right[i] = wetR;
        }

        readPos += std::min (speed, lag + 1.0);
        if (readPos >= size)
            readPos -= size;
        if (++writePos >= size)
            writePos = 0;

        if (returning && --returnLeft <= 0)
        {
            // back in sync: from here on the signal passes untouched again
            active = returning = false;
            speed = 1.0;
            readPos = (double) writePos;
            for (int k = i + 1; k < numSamples; ++k)
            {
                bufL[(size_t) writePos] = left[k];
                bufR[(size_t) writePos] = right[k];
                if (++writePos >= size)
                    writePos = 0;
            }
            readPos = (double) writePos;
            return;
        }
    }
}

//============================================================================== PerformanceFilter
void PerformanceFilter::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    reset();
}

void PerformanceFilter::reset() noexcept
{
    smoothed = 0.0f;
    wet = 0.0f;
    for (int c = 0; c < 2; ++c)
        ic1[c] = ic2[c] = 0.0f;
}

void PerformanceFilter::process (float* left, float* right, int numSamples) noexcept
{
    const bool on = std::abs (amount) > kDeadZone;
    if (! on && wet <= 0.0f)
    {
        smoothed = 0.0f;
        for (int c = 0; c < 2; ++c)
            ic1[c] = ic2[c] = 0.0f;
        return;                     // bit-exact pass-through
    }

    // smooth the knob over ~20 ms, fade the wet path over ~10 ms
    const float knobCoef = 1.0f - std::exp (-1.0f / (0.02f * (float) sampleRate));
    const float wetStep = 1.0f / (0.01f * (float) sampleRate);
    const float k = 1.0f / 0.9f;     // resonance (Q ~ 0.9)

    float* ch[2] = { left, right };
    for (int i = 0; i < numSamples; ++i)
    {
        smoothed += ((on ? amount : 0.0f) - smoothed) * knobCoef;
        wet = on ? std::min (1.0f, wet + wetStep) : std::max (0.0f, wet - wetStep);

        const bool lowPass = smoothed < 0.0f;
        const float a = std::abs (smoothed);
        // low-pass: 20 kHz -> 150 Hz, high-pass: 20 Hz -> 6 kHz (exponential)
        const float fc = lowPass ? 20000.0f * std::pow (150.0f / 20000.0f, a)
                                 : 20.0f * std::pow (6000.0f / 20.0f, a);
        const float g = std::tan (juce::MathConstants<float>::pi * std::min (fc, 0.45f * (float) sampleRate) / (float) sampleRate);
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;

        for (int c = 0; c < 2; ++c)
        {
            const float x = ch[c][i];
            const float v3 = x - ic2[c];
            const float v1 = a1 * ic1[c] + a2 * v3;
            const float v2 = ic2[c] + a2 * ic1[c] + a3 * v3;
            ic1[c] = 2.0f * v1 - ic1[c];
            ic2[c] = 2.0f * v2 - ic2[c];
            const float lp = v2;
            const float hp = x - k * v1 - v2;
            const float y = lowPass ? lp : hp;
            ch[c][i] = x + (y - x) * wet;
        }
    }
}
} // namespace livelooper
