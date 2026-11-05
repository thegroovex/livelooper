#include "PunchFx.h"

#include <cmath>
#include <cstring>

namespace livelooper
{
namespace
{
    constexpr float kTwoPi = juce::MathConstants<float>::twoPi;
    constexpr const char* kPunchNames[kNumPunchFx] = { "Pitch Scatter", "Slice Swap", "Grains", "Beat Repeat", "Tape Stop", "Filter Wobble",
                                                       "Low-Pass", "High-Pass", "Send Throw", "Tremolo", "Octave Drop", "Decimator" };
    constexpr const char* kPunchX[kNumPunchFx] = { "Rate", "Slice size", "Density", "Gate", "Stop time", "Depth",
                                                   "Resonance", "Resonance", "Send FX X", "Depth", "Interval", "Bits" };
    constexpr const char* kPunchY[kNumPunchFx] = { "Probability", "Source", "Spray", "Decay", "Restart", "Resonance",
                                                   "Drive", "Drive", "Send FX Y", "Shape", "Glide", "Sample rate" };
}

const char* getPunchFxName (int i) noexcept { return juce::isPositiveAndBelow (i, kNumPunchFx) ? kPunchNames[i] : ""; }
const char* getPunchXName (int i) noexcept  { return juce::isPositiveAndBelow (i, kNumPunchFx) ? kPunchX[i] : ""; }
const char* getPunchYName (int i) noexcept  { return juce::isPositiveAndBelow (i, kNumPunchFx) ? kPunchY[i] : ""; }

const char* getSendFxName (SendFxType t) noexcept
{
    switch (t)
    {
        case SendFxType::Off:        return "Off";
        case SendFxType::Delay:      return "Delay";
        case SendFxType::Reverb:     return "Reverb";
        case SendFxType::Distortion: return "Distortion";
        case SendFxType::Chorus:     return "Chorus";
        case SendFxType::Filter:     return "Filter";
        case SendFxType::Compressor: return "Compressor";
        case SendFxType::NumTypes:   break;
    }
    return "";
}

const char* getSendXName (SendFxType t) noexcept
{
    switch (t)
    {
        case SendFxType::Delay:      return "Length";
        case SendFxType::Reverb:     return "Length";
        case SendFxType::Distortion: return "Drive";
        case SendFxType::Chorus:     return "Modulation";
        case SendFxType::Filter:     return "Cutoff";
        case SendFxType::Compressor: return "Drive";
        default:                     return "X";
    }
}

const char* getSendYName (SendFxType t) noexcept
{
    switch (t)
    {
        case SendFxType::Delay:      return "Feedback";
        case SendFxType::Reverb:     return "Color";
        case SendFxType::Distortion: return "Color";
        case SendFxType::Chorus:     return "Feedback";
        case SendFxType::Filter:     return "Resonance";
        case SendFxType::Compressor: return "Speed";
        default:                     return "Y";
    }
}

double punchRepeatBeats (float amount) noexcept
{
    // 1/4 note .. 1/32 note in four steps (harder = shorter), as on the hardware punch-in repeat
    static constexpr double beats[] = { 1.0, 0.5, 0.25, 0.125 };
    return beats[juce::jlimit (0, 3, (int) (juce::jlimit (0.0f, 1.0f, amount) * 3.999f))];
}

//============================================================================== SendFx
void SendFx::prepare (double sr)
{
    sampleRate = sr;
    dSize = juce::jmax (1, (int) (sr * 2.1));
    for (auto& d : delay)
        d.assign ((size_t) dSize, 0.0f);
    wetL.assign (8192, 0.0f);
    wetR.assign (8192, 0.0f);
    reverb.setSampleRate (sr);
    reset();
}

void SendFx::reset() noexcept
{
    for (auto& d : delay)
        std::fill (d.begin(), d.end(), 0.0f);
    dWrite = 0;
    reverb.reset();
    for (int c = 0; c < 2; ++c)
        lp[c] = ic1[c] = ic2[c] = 0.0f;
    env = 0.0f;
    sendGain = 0.0f;
    tail = 0;
}

void SendFx::process (float* l, float* r, int n, float extraSend) noexcept
{
    if (settings.type != active)
    {
        // a new type starts clean (no old echoes in the new effect)
        active = settings.type;
        for (auto& d : delay)
            std::fill (d.begin(), d.end(), 0.0f);
        reverb.reset();
        for (int c = 0; c < 2; ++c)
            lp[c] = ic1[c] = ic2[c] = 0.0f;
    }
    const float target = active == SendFxType::Off ? 0.0f : juce::jlimit (0.0f, 1.0f, settings.level + extraSend);
    if (target <= 1.0e-4f && sendGain <= 1.0e-4f && tail <= 0)
        return;
    tail = target > 1.0e-4f ? (int) (sampleRate * 4.0) : juce::jmax (0, tail - n);
    const float x = juce::jlimit (0.0f, 1.0f, settings.x), y = juce::jlimit (0.0f, 1.0f, settings.y);
    const float coeff = 1.0f - std::exp (-1.0f / (0.005f * (float) sampleRate));

    for (int start = 0; start < n; start += (int) wetL.size())
    {
        const int len = juce::jmin (n - start, (int) wetL.size());
        float* wl = wetL.data();
        float* wr = wetR.data();
        for (int i = 0; i < len; ++i)
        {
            sendGain += (target - sendGain) * coeff;
            wl[i] = l[start + i] * sendGain;
            wr[i] = r[start + i] * sendGain;
        }

        switch (active)
        {
            case SendFxType::Delay:
            case SendFxType::Chorus:
            {
                const bool chorus = active == SendFxType::Chorus;
                const float fb = chorus ? y * 0.7f : y * 0.92f;
                for (int i = 0; i < len; ++i)
                {
                    double d;
                    if (chorus)
                    {
                        lfo += (0.1 + 4.0 * x) / sampleRate;
                        if (lfo >= 1.0) lfo -= 1.0;
                        d = sampleRate * (0.012 + 0.006 * std::sin (juce::MathConstants<double>::twoPi * lfo));
                    }
                    else
                        d = sampleRate * (0.05 + 0.95 * x);
                    for (int c = 0; c < 2; ++c)
                    {
                        const double rp = std::fmod ((double) dWrite - d + 2.0 * dSize, (double) dSize);
                        const int i0 = (int) rp;
                        const float frac = (float) (rp - i0);
                        const auto& buf = delay[(size_t) c];
                        const float out = buf[(size_t) i0] + (buf[(size_t) ((i0 + 1) % dSize)] - buf[(size_t) i0]) * frac;
                        float* w = c == 0 ? wl : wr;
                        delay[(size_t) c][(size_t) dWrite] = w[i] + out * fb;
                        w[i] = out;
                    }
                    if (++dWrite >= dSize) dWrite = 0;
                }
                break;
            }
            case SendFxType::Reverb:
            {
                juce::Reverb::Parameters p;
                p.roomSize = 0.3f + 0.69f * x;
                p.damping = 1.0f - y;
                p.wetLevel = 1.0f;
                p.dryLevel = 0.0f;
                p.width = 1.0f;
                reverb.setParameters (p);
                reverb.processStereo (wl, wr, len);
                break;
            }
            case SendFxType::Distortion:
            {
                const float drive = 1.0f + 30.0f * x * x;
                const float tone = 0.05f + 0.9f * y;
                for (int i = 0; i < len; ++i)
                    for (int c = 0; c < 2; ++c)
                    {
                        float* w = c == 0 ? wl : wr;
                        const float s = std::tanh (w[i] * drive) / std::sqrt (drive);
                        lp[c] += (s - lp[c]) * tone;
                        w[i] = lp[c] * 1.4f;
                    }
                break;
            }
            case SendFxType::Filter:
            {
                // bipolar DJ filter: left = cut highs, right = cut lows, middle = open
                const float q = 0.6f + 6.0f * y;
                const float k = 1.0f / q;
                const bool high = x > 0.5f;
                const float amt = std::abs (x - 0.5f) * 2.0f;
                const float fc = high ? 20.0f * std::pow (400.0f, amt) : 20000.0f * std::pow (0.0035f, amt);
                const float g = std::tan (juce::MathConstants<float>::pi * juce::jlimit (20.0f, (float) sampleRate * 0.45f, fc) / (float) sampleRate);
                const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
                for (int i = 0; i < len; ++i)
                    for (int c = 0; c < 2; ++c)
                    {
                        float* w = c == 0 ? wl : wr;
                        const float v3 = w[i] - ic2[c];
                        const float v1 = a1 * ic1[c] + a2 * v3;
                        const float v2 = ic2[c] + a2 * ic1[c] + a3 * v3;
                        ic1[c] = 2.0f * v1 - ic1[c];
                        ic2[c] = 2.0f * v2 - ic2[c];
                        w[i] = amt < 0.02f ? w[i] : (high ? w[i] - k * v1 - v2 : v2);
                    }
                break;
            }
            case SendFxType::Compressor:
            {
                const float drive = 1.0f + 15.0f * x;
                const float rel = std::exp (-1.0f / ((0.02f + 0.5f * (1.0f - y)) * (float) sampleRate));
                for (int i = 0; i < len; ++i)
                {
                    const float a = std::max (std::abs (wl[i]), std::abs (wr[i])) * drive;
                    env = a > env ? a : env * rel;
                    const float g = drive / (1.0f + env * 4.0f);
                    wl[i] *= g;
                    wr[i] *= g;
                }
                break;
            }
            case SendFxType::Off:
            case SendFxType::NumTypes:
                std::fill (wl, wl + len, 0.0f);
                std::fill (wr, wr + len, 0.0f);
                break;
        }
        for (int i = 0; i < len; ++i)
        {
            l[start + i] += wl[i];
            r[start + i] += wr[i];
        }
    }
}

//============================================================================== PunchFx
void PunchFx::prepare (double sr, int maxBlock)
{
    sampleRate = sr;
    hSize = juce::jmax (16, (int) (sr * 4.0));
    for (auto& h : hist)
        h.assign ((size_t) hSize, 0.0f);
    dryL.assign ((size_t) juce::jmax (16, maxBlock), 0.0f);
    dryR.assign ((size_t) juce::jmax (16, maxBlock), 0.0f);
    scatterShift.prepare (sr);
    dropShift.prepare (sr);
    reset();
}

void PunchFx::reset() noexcept
{
    for (auto& h : hist)
        std::fill (h.begin(), h.end(), 0.0f);
    hWrite = 0;
    gains.fill (0.0f);
    repActive = swapOn = false;
    tapeLag = 0.0;
    tapeSpeed = 1.0;
    for (auto& g : grains)
        g.on = false;
    std::memset (fic1, 0, sizeof (fic1));
    std::memset (fic2, 0, sizeof (fic2));
    scatterShift.reset();
    dropShift.reset();
    dropRatio = scatterRatio = 1.0f;
}

bool PunchFx::isActive() const noexcept
{
    for (int i = 0; i < kNumPunchFx; ++i)
        if (settings.held[(size_t) i] || gains[(size_t) i] > 1.0e-4f)
            return true;
    return false;
}

float PunchFx::readHistory (int ch, double ago) const noexcept
{
    ago = juce::jlimit (1.0, (double) hSize - 2.0, ago);
    double p = (double) hWrite - ago;
    if (p < 0.0)
        p += hSize;
    const int i0 = (int) p;
    const float f = (float) (p - i0);
    const auto& h = hist[(size_t) ch];
    return h[(size_t) i0] + (h[(size_t) ((i0 + 1) % hSize)] - h[(size_t) i0]) * f;
}

void PunchFx::svf (int which, float x[2], float cutoffHz, float q, float out[2], int mode) noexcept
{
    // mode 0 = low-pass, 1 = high-pass, 2 = band-pass
    const float g = std::tan (juce::MathConstants<float>::pi * juce::jlimit (20.0f, (float) sampleRate * 0.45f, cutoffHz) / (float) sampleRate);
    const float k = 1.0f / juce::jmax (0.5f, q);
    const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
    for (int c = 0; c < 2; ++c)
    {
        auto& s1 = fic1[which][c];
        auto& s2 = fic2[which][c];
        const float v3 = x[c] - s2;
        const float v1 = a1 * s1 + a2 * v3;
        const float v2 = s2 + a2 * s1 + a3 * v3;
        s1 = 2.0f * v1 - s1;
        s2 = 2.0f * v2 - s2;
        out[c] = mode == 0 ? v2 : (mode == 1 ? x[c] - k * v1 - v2 : v1);
    }
}

void PunchFx::process (float* l, float* r, int n, double ppqStart, double ppqPerSample) noexcept
{
    sendThrow = 0.0f;
    if (! isActive())
    {
        // keep the history running, so a press has material at once
        for (int i = 0; i < n; ++i)
        {
            hist[0][(size_t) hWrite] = l[i];
            hist[1][(size_t) hWrite] = r[i];
            if (++hWrite >= hSize) hWrite = 0;
        }
        tapeLag = 0.0;
        tapeSpeed = 1.0;
        repActive = false;
        return;
    }

    const float amount = juce::jlimit (0.0f, 1.0f, settings.amount);
    const float X = juce::jlimit (0.0f, 1.0f, settings.x), Y = juce::jlimit (0.0f, 1.0f, settings.y);
    const float ramp = 1.0f / (0.005f * (float) sampleRate);     // 5 ms fades
    const double samplesPerBeat = ppqPerSample > 0.0 ? 1.0 / ppqPerSample : sampleRate * 0.5;
    const auto& held = settings.held;

    for (int i = 0; i < n; ++i)
    {
        const double ppq = ppqStart + i * ppqPerSample;
        const float inL = l[i], inR = r[i];
        hist[0][(size_t) hWrite] = inL;
        hist[1][(size_t) hWrite] = inR;
        if (++hWrite >= hSize) hWrite = 0;

        for (int k = 0; k < kNumPunchFx; ++k)
        {
            auto& g = gains[(size_t) k];
            g = held[(size_t) k] ? juce::jmin (1.0f, g + ramp) : juce::jmax (0.0f, g - ramp);
        }
        float sL = inL, sR = inR;

        // ---- source replacers first: beat repeat, slice swap, grains, tape stop (they read the history)
        if (gains[3] > 0.0f)     // Beat Repeat: grid-locked repeat, harder = shorter
        {
            const double len = punchRepeatBeats (amount) * samplesPerBeat;
            if (! repActive)
            {
                repActive = true;
                repLen = juce::jlimit (32.0, (double) hSize * 0.5, len);
                repStartAgo = repLen;     // the last slice
                repPos = 0.0;
                repCount = 0;
            }
            const float gate = repPos < repLen * (0.3 + 0.7 * (1.0 - X)) ? 1.0f : 0.0f;
            const float decay = std::pow (1.0f - 0.6f * Y, (float) repCount);
            // the frozen slice is the repLen samples before the press: a constant distance into the
            // past plays it forward, one slice further back for every repeat
            const double readAgo = repStartAgo + repLen * (double) repCount;
            const float vL = readHistory (0, readAgo) * gate * decay;
            const float vR = readHistory (1, readAgo) * gate * decay;
            sL += (vL - sL) * gains[3];
            sR += (vR - sR) * gains[3];
            repPos += 1.0;
            if (repPos >= repLen)
            {
                repPos = 0.0;
                ++repCount;
                if (repLen * (repCount + 1) + repStartAgo > hSize - 4)
                    repCount = 0, repStartAgo = repLen;
            }
        }
        else
            repActive = false;

        if (gains[1] > 0.0f)     // Slice Swap: on each grid cell (maybe) play an earlier slice instead
        {
            static constexpr double sizes[] = { 0.25, 0.5, 1.0, 2.0 };
            const double sliceBeats = sizes[juce::jlimit (0, 3, (int) (X * 3.999f))];
            const double cell = std::floor (ppq / sliceBeats);
            if (cell != lastSwapCell)
            {
                lastSwapCell = cell;
                swapOn = rng.nextFloat() < 0.35f + 0.65f * amount;
                const int back = 1 + rng.nextInt (juce::jmax (1, (int) (Y * 6.0f) + 2));
                swapAgo = juce::jmin ((double) hSize * 0.8, back * sliceBeats * samplesPerBeat);
            }
            if (swapOn)
            {
                sL += (readHistory (0, swapAgo) - sL) * gains[1];
                sR += (readHistory (1, swapAgo) - sR) * gains[1];
            }
        }

        if (gains[2] > 0.0f)     // Grains: short windowed grains from the last second
        {
            const double grainLen = sampleRate * (0.02 + 0.18 * amount);
            const double every = grainLen / (1.0 + 3.0 * X);
            grainClock += 1.0;
            if (grainClock >= every)
            {
                grainClock = 0.0;
                for (auto& gr : grains)
                    if (! gr.on)
                    {
                        gr.on = true;
                        gr.len = grainLen;
                        gr.pos = 0.0;
                        gr.ago = grainLen + rng.nextFloat() * Y * sampleRate * 0.8;
                        break;
                    }
            }
            float gl = 0.0f, grr = 0.0f;
            for (auto& gr : grains)
                if (gr.on)
                {
                    const float w = 0.5f - 0.5f * std::cos (kTwoPi * (float) (gr.pos / gr.len));
                    gl += readHistory (0, gr.ago) * w;
                    grr += readHistory (1, gr.ago) * w;
                    gr.pos += 1.0;           // the read point stays fixed in the past (ago constant = moves with time)
                    if (gr.pos >= gr.len)
                        gr.on = false;
                }
            sL += (gl * 0.7f - sL) * gains[2];
            sR += (grr * 0.7f - sR) * gains[2];
        }

        if (gains[4] > 0.0f || tapeLag > 0.5)     // Tape Stop: the read head slows down, X = stop time, Y = restart time
        {
            if (held[4])
            {
                const double stopSamples = sampleRate * (0.1 + 1.9 * X);
                const double minSpeed = 1.0 - amount;
                tapeSpeed = juce::jmax (minSpeed, tapeSpeed - 1.0 / stopSamples);
            }
            else
            {
                const double restart = sampleRate * (0.05 + 0.95 * Y);
                tapeSpeed = juce::jmin (1.0, tapeSpeed + 1.0 / restart);
                if (tapeSpeed >= 1.0)
                    tapeLag = juce::jmax (0.0, tapeLag - 2.0);     // catch up quickly once at speed
            }
            tapeLag = juce::jlimit (0.0, (double) hSize * 0.9, tapeLag + (1.0 - tapeSpeed));
            const float vL = readHistory (0, 1.0 + tapeLag), vR = readHistory (1, 1.0 + tapeLag);
            const float w = juce::jmax (gains[4], (float) juce::jmin (1.0, tapeLag / 64.0));
            sL += (vL - sL) * w;
            sR += (vR - sR) * w;
        }

        // ---- pitch
        if (gains[0] > 0.0f)     // Pitch Scatter: a random pitch drop per grid cell
        {
            static constexpr double rates[] = { 1.0 / 8.0, 1.0 / 4.0, 1.0 / 2.0, 1.0 };
            const double cellBeats = rates[3 - juce::jlimit (0, 3, (int) (X * 3.999f))];
            const double cell = std::floor (ppq / cellBeats);
            if (cell != lastScatterCell)
            {
                lastScatterCell = cell;
                const bool jump = rng.nextFloat() < 0.2f + 0.8f * Y;
                const float semis = jump ? -(float) rng.nextInt (juce::jmax (1, (int) (amount * 12.0f) + 1)) : 0.0f;
                scatterRatio = std::pow (2.0f, semis / 12.0f);
            }
            const float pL = scatterShift.process (0, sL, scatterRatio), pR = scatterShift.process (1, sR, scatterRatio);
            sL += (pL - sL) * gains[0];
            sR += (pR - sR) * gains[0];
        }
        if (gains[10] > 0.0f)    // Octave Drop: glides toward -12 (X: -24) semitones
        {
            const float targetSemis = -(X > 0.5f ? 24.0f : 12.0f) * amount;
            const float target = std::pow (2.0f, targetSemis / 12.0f);
            const float glide = 1.0f / ((0.01f + 0.5f * Y) * (float) sampleRate);
            dropRatio += (target - dropRatio) * juce::jmin (1.0f, glide * 4.0f);
            const float pL = dropShift.process (0, sL, juce::jmax (0.25f, dropRatio));
            const float pR = dropShift.process (1, sR, juce::jmax (0.25f, dropRatio));
            sL += (pL - sL) * gains[10];
            sR += (pR - sR) * gains[10];
        }
        else
            dropRatio = 1.0f;

        // ---- filters
        if (gains[5] > 0.0f)     // Filter Wobble: synced-ish LFO on a band-pass / low-pass
        {
            wobblePhase += (0.5 + 15.5 * amount) / sampleRate;
            if (wobblePhase >= 1.0) wobblePhase -= 1.0;
            const float lfo = 0.5f + 0.5f * std::sin (kTwoPi * (float) wobblePhase);
            const float fc = 200.0f * std::pow (40.0f, 0.15f + 0.85f * (1.0f - X + X * lfo));
            float in2[2] { sL, sR }, out2[2];
            svf (0, in2, fc, 0.7f + 8.0f * Y, out2, 0);
            sL += (out2[0] - sL) * gains[5];
            sR += (out2[1] - sR) * gains[5];
        }
        if (gains[6] > 0.0f)     // Low-Pass: harder = lower cutoff
        {
            const float fc = 18000.0f * std::pow (0.006f, amount);
            float in2[2] { sL * (1.0f + 3.0f * Y), sR * (1.0f + 3.0f * Y) }, out2[2];
            svf (1, in2, fc, 0.7f + 6.0f * X, out2, 0);
            if (Y > 0.02f) { out2[0] = std::tanh (out2[0]) / (1.0f + Y); out2[1] = std::tanh (out2[1]) / (1.0f + Y); }
            sL += (out2[0] - sL) * gains[6];
            sR += (out2[1] - sR) * gains[6];
        }
        if (gains[7] > 0.0f)     // High-Pass: harder = higher cutoff (LP + HP together = band-pass)
        {
            const float fc = 30.0f * std::pow (200.0f, amount);
            float in2[2] { sL * (1.0f + 3.0f * Y), sR * (1.0f + 3.0f * Y) }, out2[2];
            svf (2, in2, fc, 0.7f + 6.0f * X, out2, 1);
            if (Y > 0.02f) { out2[0] = std::tanh (out2[0]) / (1.0f + Y); out2[1] = std::tanh (out2[1]) / (1.0f + Y); }
            sL += (out2[0] - sL) * gains[7];
            sR += (out2[1] - sR) * gains[7];
        }

        // ---- amplitude / lo-fi
        if (gains[9] > 0.0f)     // Tremolo: speed = amount, depth = X, shape sine .. square = Y
        {
            tremPhase += (1.0 + 19.0 * amount) / sampleRate;
            if (tremPhase >= 1.0) tremPhase -= 1.0;
            const float sine = 0.5f + 0.5f * std::sin (kTwoPi * (float) tremPhase);
            const float square = tremPhase < 0.5 ? 1.0f : 0.0f;
            const float shape = sine + (square - sine) * Y;
            const float gain = 1.0f - (0.3f + 0.7f * X) * (1.0f - shape);
            sL += (sL * gain - sL) * gains[9];
            sR += (sR * gain - sR) * gains[9];
        }
        if (gains[11] > 0.0f)    // Decimator: bits (X) and sample rate (Y), amount = how far
        {
            const float bits = 16.0f - (12.0f * amount) * (0.4f + 0.6f * X);
            const float step = std::pow (2.0f, juce::jmax (2.0f, bits) - 1.0f);
            const float hold = 1.0f + 40.0f * amount * Y;
            decCounter += 1.0f;
            if (decCounter >= hold)
            {
                decCounter -= hold;
                decHold[0] = std::round (sL * step) / step;
                decHold[1] = std::round (sR * step) / step;
            }
            sL += (decHold[0] - sL) * gains[11];
            sR += (decHold[1] - sR) * gains[11];
        }

        // Send Throw: more of this signal into the Send FX (the send itself is added by SendFx)
        if (gains[8] > 0.0f)
            sendThrow = juce::jmax (sendThrow, gains[8] * amount);

        l[i] = sL;
        r[i] = sR;
    }
}
} // namespace livelooper
