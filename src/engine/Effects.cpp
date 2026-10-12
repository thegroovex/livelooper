#include "Effects.h"

#include <algorithm>

namespace livelooper
{
namespace
{
    constexpr double kPi = 3.14159265358979323846;
    constexpr int kChunk = 256;

    // 16-step gate patterns for the slicer (bit i = step i open)
    constexpr std::array<uint16_t, 8> kSlicerPatterns {
        0b0101010101010101, // straight 8ths
        0b0111011101110111, // 3 of 4
        0b1011011010110110, // syncopated
        0b0011001100110011, // 8th pairs
        0b1101101101101101, // triplet feel
        0b0001000100010001, // quarters
        0b1111111101010101, // half open
        0b1001001010010010  // broken
    };

    constexpr std::array<double, 6> kDelayPpq   { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 };
    constexpr std::array<double, 6> kLfoPeriods { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125 };

    template <typename Array>
    auto pick (const Array& a, float p) noexcept
    {
        const auto n = a.size();
        return a[std::min (n - 1, (size_t) (std::clamp (p, 0.0f, 1.0f) * (float) n))];
    }

    inline float expMap (float p, float lo, float hi) noexcept
    {
        return lo * std::pow (hi / lo, std::clamp (p, 0.0f, 1.0f));
    }
}

const char* getFxTypeName (FxType t) noexcept
{
    switch (t)
    {
        case FxType::LowPass:   return "LPF";
        case FxType::HighPass:  return "HPF";
        case FxType::BandPass:  return "BPF";
        case FxType::Phaser:    return "Phaser";
        case FxType::Flanger:   return "Flanger";
        case FxType::Delay:     return "Delay";
        case FxType::Reverb:    return "Reverb";
        case FxType::Slicer:    return "Slicer";
        case FxType::Tremolo:   return "Tremolo";
        case FxType::AutoPan:   return "Auto Pan";
        case FxType::LoFi:      return "Lo-Fi";
        case FxType::Overdrive: return "Overdrive";
        case FxType::NumTypes:  break;
    }
    return "?";
}

const char* getFxParam1Name (FxType t) noexcept
{
    switch (t)
    {
        case FxType::LowPass: case FxType::HighPass: case FxType::BandPass: return "Cutoff";
        case FxType::Phaser: case FxType::Slicer: case FxType::Tremolo: case FxType::AutoPan: return "Depth";
        case FxType::Flanger: case FxType::Delay: return "Feedback";
        case FxType::Reverb:    return "Size";
        case FxType::LoFi:      return "Bits";
        case FxType::Overdrive: return "Drive";
        case FxType::NumTypes:  break;
    }
    return "P1";
}

const char* getFxParam2Name (FxType t) noexcept
{
    switch (t)
    {
        case FxType::LowPass: case FxType::HighPass: case FxType::BandPass: return "Reso";
        case FxType::Phaser: case FxType::Flanger: return "Rate";
        case FxType::Delay:     return "Time";
        case FxType::Reverb:    return "Damp";
        case FxType::Slicer:    return "Pattern";
        case FxType::Tremolo: case FxType::AutoPan: return "Rate";
        case FxType::LoFi:      return "Rate";
        case FxType::Overdrive: return "Tone";
        case FxType::NumTypes:  break;
    }
    return "P2";
}

//==============================================================================
void FxUnit::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    const auto delaySize = (size_t) (sampleRate * 4.5) + 8;
    for (auto& d : delayLine)
        d.assign (delaySize, 0.0f);
    reverb.setSampleRate (sampleRate);
    reset();
}

void FxUnit::reset() noexcept
{
    for (int ch = 0; ch < 2; ++ch)
    {
        ic1[ch] = ic2[ch] = 0.0f;
        phaserFb[ch] = 0.0f;
        toneState[ch] = 0.0f;
        for (auto& s : apState[ch])
            s = 0.0f;
        std::fill (delayLine[ch].begin(), delayLine[ch].end(), 0.0f);
    }
    writePos = 0;
    holdL = holdR = holdCounter = 0.0f;
    lfoPhase = 0.0;
    reverb.reset();
}

void FxUnit::setSettings (const FxSettings& s) noexcept
{
    if (s.type != activeType)
    {
        activeType = s.type;
        reset();
    }
    settings = s;
}

void FxUnit::process (float* left, float* right, int numSamples, double ppqStart, double ppqPerSample, double bpm) noexcept
{
    const float targetWet = settings.enabled ? settings.mix : 0.0f;
    if (targetWet <= 0.0f && wetGain <= 1.0e-4f)
    {
        wetGain = 0.0f;
        return;
    }

    // move the wet gain towards its target within ~10 ms (click-free on/off and mix changes)
    const float maxWetStep = (float) (1.0 / (0.01 * sampleRate));
    const float p1 = settings.p1, p2 = settings.p2;
    const double sr = sampleRate;
    const int delaySize = (int) delayLine[0].size();

    auto mixOut = [this] (float& dry, float wet) noexcept { dry += (wet - dry) * wetGain; };

    switch (activeType)
    {
        case FxType::LowPass:
        case FxType::HighPass:
        case FxType::BandPass:
        {
            const float lo = activeType == FxType::HighPass ? 20.0f : 40.0f;
            const float cutoff = std::min (expMap (p1, lo, 18000.0f), (float) (sr * 0.45));
            const float q = 0.5f + p2 * 11.5f;
            const float g = (float) std::tan (kPi * cutoff / sr);
            const float k = 1.0f / q;
            const float a1 = 1.0f / (1.0f + g * (g + k));
            const float a2 = g * a1;
            const float a3 = g * a2;
            for (int i = 0; i < numSamples; ++i)
            {
                wetGain += std::clamp (targetWet - wetGain, -maxWetStep, maxWetStep);
                float* chans[2] { left + i, right + i };
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float x = *chans[ch];
                    const float v3 = x - ic2[ch];
                    const float v1 = a1 * ic1[ch] + a2 * v3;
                    const float v2 = ic2[ch] + a2 * ic1[ch] + a3 * v3;
                    ic1[ch] = 2.0f * v1 - ic1[ch];
                    ic2[ch] = 2.0f * v2 - ic2[ch];
                    float y = v2;
                    if (activeType == FxType::HighPass)      y = x - k * v1 - v2;
                    else if (activeType == FxType::BandPass) y = v1 * k;
                    mixOut (*chans[ch], y);
                }
            }
            break;
        }

        case FxType::Phaser:
        {
            const double rate = expMap (p2, 0.05f, 5.0f);
            const double inc = rate / sr;
            for (int i = 0; i < numSamples; ++i)
            {
                wetGain += std::clamp (targetWet - wetGain, -maxWetStep, maxWetStep);
                lfoPhase += inc;
                if (lfoPhase >= 1.0) lfoPhase -= 1.0;
                const float lfo = 0.5f - 0.5f * (float) std::cos (2.0 * kPi * lfoPhase);
                const float f = 200.0f * std::pow (20.0f, lfo * (0.2f + 0.8f * p1));
                const float t = (float) std::tan (kPi * std::min ((double) f, sr * 0.45) / sr);
                const float a = (1.0f - t) / (1.0f + t);
                float* chans[2] { left + i, right + i };
                for (int ch = 0; ch < 2; ++ch)
                {
                    float x = *chans[ch] + phaserFb[ch] * 0.6f;
                    for (auto& s : apState[ch])
                    {
                        const float y = -a * x + s;
                        s = x + a * y;
                        x = y;
                    }
                    phaserFb[ch] = x;
                    mixOut (*chans[ch], 0.5f * (*chans[ch] + x));
                }
            }
            break;
        }

        case FxType::Flanger:
        {
            const double rate = expMap (p2, 0.05f, 3.0f);
            const double inc = rate / sr;
            const float fb = p1 * 0.85f;
            for (int i = 0; i < numSamples; ++i)
            {
                wetGain += std::clamp (targetWet - wetGain, -maxWetStep, maxWetStep);
                lfoPhase += inc;
                if (lfoPhase >= 1.0) lfoPhase -= 1.0;
                const float lfo = 0.5f - 0.5f * (float) std::cos (2.0 * kPi * lfoPhase);
                const float delaySamples = (float) (sr * (0.0005 + 0.0045 * lfo));
                float* chans[2] { left + i, right + i };
                for (int ch = 0; ch < 2; ++ch)
                {
                    float readPos = (float) writePos - delaySamples;
                    while (readPos < 0.0f) readPos += (float) delaySize;
                    const int i0 = (int) readPos;
                    const int i1 = (i0 + 1) % delaySize;
                    const float frac = readPos - (float) i0;
                    const float d = delayLine[ch][(size_t) i0] * (1.0f - frac) + delayLine[ch][(size_t) i1] * frac;
                    delayLine[ch][(size_t) writePos] = *chans[ch] + d * fb;
                    mixOut (*chans[ch], 0.5f * (*chans[ch] + d));
                }
                if (++writePos >= delaySize) writePos = 0;
            }
            break;
        }

        case FxType::Delay:
        {
            const double ppq = pick (kDelayPpq, p2);
            const int delaySamples = std::clamp ((int) (ppq * 60.0 / std::max (20.0, bpm) * sr), 1, delaySize - 1);
            const float fb = p1 * 0.9f;
            for (int i = 0; i < numSamples; ++i)
            {
                wetGain += std::clamp (targetWet - wetGain, -maxWetStep, maxWetStep);
                int readPos = writePos - delaySamples;
                if (readPos < 0) readPos += delaySize;
                float* chans[2] { left + i, right + i };
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float d = delayLine[ch][(size_t) readPos];
                    delayLine[ch][(size_t) writePos] = *chans[ch] + d * fb;
                    const float x = *chans[ch];
                    *chans[ch] = x + d * wetGain;   // delay keeps the dry signal, mix sets the echo level
                }
                if (++writePos >= delaySize) writePos = 0;
            }
            break;
        }

        case FxType::Reverb:
        {
            juce::Reverb::Parameters rp;
            rp.roomSize = 0.3f + 0.69f * p1;
            rp.damping = p2;
            rp.wetLevel = 1.0f;
            rp.dryLevel = 0.0f;
            rp.width = 1.0f;
            reverb.setParameters (rp);

            float wl[kChunk], wr[kChunk];
            for (int start = 0; start < numSamples; start += kChunk)
            {
                const int n = std::min (kChunk, numSamples - start);
                std::copy (left + start, left + start + n, wl);
                std::copy (right + start, right + start + n, wr);
                reverb.processStereo (wl, wr, n);
                for (int i = 0; i < n; ++i)
                {
                    wetGain += std::clamp (targetWet - wetGain, -maxWetStep, maxWetStep);
                    left[start + i]  += wl[i] * 0.5f * wetGain;
                    right[start + i] += wr[i] * 0.5f * wetGain;
                }
            }
            break;
        }

        case FxType::Slicer:
        case FxType::Tremolo:
        case FxType::AutoPan:
        {
            const auto pattern = pick (kSlicerPatterns, p2);
            const double period = pick (kLfoPeriods, p2);
            const float smooth = 1.0f - std::exp (-1.0f / (float) (sr * 0.002));
            for (int i = 0; i < numSamples; ++i)
            {
                wetGain += std::clamp (targetWet - wetGain, -maxWetStep, maxWetStep);
                const double ppq = ppqStart + ppqPerSample * i;
                if (activeType == FxType::Slicer)
                {
                    const int step = (int) positiveModulo (std::floor (ppq * 4.0), 16.0);
                    const float gate = ((pattern >> step) & 1) != 0 ? 1.0f : 0.0f;
                    holdCounter += (gate - holdCounter) * smooth;   // smoothed gate
                    const float g = 1.0f - p1 * (1.0f - holdCounter);
                    mixOut (left[i],  left[i] * g);
                    mixOut (right[i], right[i] * g);
                }
                else
                {
                    const double phase = positiveModulo (ppq / period, 1.0);
                    const float lfo = 0.5f - 0.5f * (float) std::cos (2.0 * kPi * phase);
                    if (activeType == FxType::Tremolo)
                    {
                        const float g = 1.0f - p1 * lfo;
                        mixOut (left[i],  left[i] * g);
                        mixOut (right[i], right[i] * g);
                    }
                    else
                    {
                        const float pan = (lfo * 2.0f - 1.0f) * p1;
                        mixOut (left[i],  left[i] * std::min (1.0f, 1.0f - pan));
                        mixOut (right[i], right[i] * std::min (1.0f, 1.0f + pan));
                    }
                }
            }
            break;
        }

        case FxType::LoFi:
        {
            const float bits = 16.0f - p1 * 13.0f;
            const float steps = std::pow (2.0f, bits - 1.0f);
            const float hold = 1.0f + p2 * 31.0f;
            for (int i = 0; i < numSamples; ++i)
            {
                wetGain += std::clamp (targetWet - wetGain, -maxWetStep, maxWetStep);
                holdCounter += 1.0f;
                if (holdCounter >= hold)
                {
                    holdCounter -= hold;
                    holdL = std::round (left[i] * steps) / steps;
                    holdR = std::round (right[i] * steps) / steps;
                }
                mixOut (left[i], holdL);
                mixOut (right[i], holdR);
            }
            break;
        }

        case FxType::Overdrive:
        {
            const float drive = 1.0f + p1 * 30.0f;
            const float norm = 1.0f / std::tanh (std::min (drive, 3.0f));
            const float cutoff = expMap (p2, 800.0f, 14000.0f);
            const float coeff = 1.0f - std::exp (-2.0f * (float) kPi * cutoff / (float) sr);
            for (int i = 0; i < numSamples; ++i)
            {
                wetGain += std::clamp (targetWet - wetGain, -maxWetStep, maxWetStep);
                float* chans[2] { left + i, right + i };
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float y = std::tanh (*chans[ch] * drive) * norm * 0.7f;
                    toneState[ch] += (y - toneState[ch]) * coeff;
                    mixOut (*chans[ch], toneState[ch]);
                }
            }
            break;
        }

        case FxType::NumTypes:
            break;
    }

}

//==============================================================================
void FxChain::prepare (double sampleRate)
{
    for (auto& u : units)
        u.prepare (sampleRate);
}

void FxChain::reset() noexcept
{
    for (auto& u : units)
        u.reset();
}

void FxChain::process (float* left, float* right, int numSamples, double ppqStart, double ppqPerSample, double bpm) noexcept
{
    for (auto& u : units)
        u.process (left, right, numSamples, ppqStart, ppqPerSample, bpm);
}
} // namespace livelooper
