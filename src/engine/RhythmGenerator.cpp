#include "RhythmGenerator.h"

#include <algorithm>

namespace livelooper
{
namespace
{
    constexpr double kTwoPi = 6.283185307179586;

    struct Pattern
    {
        const char* name;
        const char* kick;     // 16 steps, 'x' = hit
        const char* snare;    // 'x' snare, 'c' hand clap, 'b' both
        const char* hat;      // 'x' closed, 'o' open
    };

    // Index 0 is the metronome (handled separately).
    constexpr Pattern kPatterns[] = {
        { "Metronome",  "", "", "" },
        { "Rock",       "x.......x.x.....", "....x.......x...", "x.x.x.x.x.x.x.x." },
        { "Pop",        "x.....x.x.......", "....b.......b...", "x.x.x.x.x.x.x.xo" },
        { "Funk",       "x..x..x...x..x..", "....x..x.x..x..x", "xxxxxxxxxxxxxxxx" },
        { "Hip Hop",    "x......x..x.....", "....c.......c...", "x.x.x.x.x.x.x.x." },
        { "House",      "x...x...x...x...", "....c.......c...", "x.o.x.o.x.o.x.o." },
        { "Techno",     "x...x...x...x...", "....c.......c...", "xxoxxxoxxxoxxxox" },
        { "Breakbeat",  "x.x.......x.....", "....x..x.x..x...", "x.x.x.x.x.x.x.x." },
        { "Reggae",     "........x.......", "........x.......", "..x...x...x...x." },
        { "Bossa",      "x..x x..x..x x..", "..x..x..x...x..x", "x.x.x.x.x.x.x.x." },
        { "Shuffle",    "x.....x.x.....x.", "....x.......x...", "x.xx.xx.xx.xx.xx" },
        { "Half Time",  "x...............", "........x.......", "x.x.x.x.x.x.x.x." },
    };
    constexpr int kNumPatterns = (int) (sizeof (kPatterns) / sizeof (kPatterns[0]));

    inline char at (const char* s, int step) noexcept
    {
        for (int i = 0; i <= step; ++i)
            if (s[i] == 0)
                return '.';
        return s[step];
    }

    // 808-style metallic hi-hat: six detuned square oscillators
    constexpr float kHatFreqs[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };

    inline float softClip (float x) noexcept
    {
        // cheap tanh-like saturator
        if (x > 3.0f) return 1.0f;
        if (x < -3.0f) return -1.0f;
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }
}

int RhythmGenerator::getNumPatterns() noexcept { return kNumPatterns; }

const char* RhythmGenerator::getPatternName (int index) noexcept
{
    return kPatterns[std::clamp (index, 0, kNumPatterns - 1)].name;
}

void RhythmGenerator::Svf::set (double sr, double freq, double q) noexcept
{
    freq = std::clamp (freq, 20.0, sr * 0.45);
    g = (float) std::tan (3.141592653589793 * freq / sr);
    k = (float) (1.0 / std::max (0.1, q));
}

float RhythmGenerator::Svf::process (float x, float& lp, float& hp) noexcept
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

void RhythmGenerator::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    snFilter.set (sampleRate, 2200.0, 0.9);
    clapFilter.set (sampleRate, 1300.0, 1.6);
    hatFilter.set (sampleRate, 9500.0, 1.2);
    hatHighPass.set (sampleRate, 7000.0, 0.7);
    woodFilter.set (sampleRate, 1800.0, 9.0);
    reset();
}

void RhythmGenerator::reset() noexcept
{
    kickEnv = kickClick = snBodyEnv = snNoiseEnv = clapEnv = hatEnv = clickEnv = 0.0f;
    clapBurstLeft = 0;
    for (auto* f : { &snFilter, &clapFilter, &hatFilter, &hatHighPass, &woodFilter })
        f->reset();
    lastStep = -1;
}

float RhythmGenerator::decayFor (double seconds) const noexcept
{
    return (float) std::exp (-1.0 / (seconds * sampleRate));
}

float RhythmGenerator::noise() noexcept
{
    noiseSeed = noiseSeed * 1664525u + 1013904223u;
    return (float) (int32_t) noiseSeed / 2147483648.0f;
}

void RhythmGenerator::trigger (long long step, int beatsPerMeasure) noexcept
{
    const int p = std::clamp (pattern, 0, kNumPatterns - 1);

    if (p == 0)
    {
        if (((step % 4) + 4) % 4 != 0)
            return;
        const long long stepsPerMeasure = std::max (1, beatsPerMeasure) * 4;
        const bool accent = ((step % stepsPerMeasure) + stepsPerMeasure) % stepsPerMeasure == 0;
        clickEnv = 1.0f;
        clickPh1 = clickPh2 = 0.0f;
        if (clickSound == 1)          // woodblock: two inharmonic partials + a resonant knock
        {
            clickF1 = accent ? 1250.0f : 950.0f;
            clickF2 = clickF1 * 2.76f;
            clickDecay = decayFor (0.045);
            clickAmp = accent ? 1.0f : 0.7f;
            woodFilter.set (sampleRate, clickF1 * 1.5, 9.0);
        }
        else                          // classic beep
        {
            clickF1 = clickF2 = accent ? 1800.0f : 1200.0f;
            clickDecay = decayFor (0.03);
            clickAmp = accent ? 0.9f : 0.6f;
        }
        return;
    }

    const auto& pat = kPatterns[p];
    const int s = (int) positiveModulo ((double) step, 16.0);

    if (at (pat.kick, s) == 'x')
    {
        kickEnv = 1.0f;
        kickPitch = 1.0f;
        kickPitchDecay = decayFor (0.028);
        kickDecay = decayFor (0.32);
        kickClick = 1.0f;
        kickClickDecay = decayFor (0.004);
    }
    const char sn = at (pat.snare, s);
    if (sn == 'x' || sn == 'b')
    {
        snBodyEnv = 1.0f;
        snBodyDecay = decayFor (0.075);
        snNoiseEnv = 1.0f;
        snNoiseDecay = decayFor (0.17);
    }
    if (sn == 'c' || sn == 'b')
    {
        clapEnv = 1.0f;
        clapBurst = 0;
        clapBurstLeft = (int) (0.009 * sampleRate);
        clapDecay = decayFor (0.012);
    }
    const char h = at (pat.hat, s);
    if (h == 'x')
    {
        hatEnv = 0.9f;
        hatDecay = decayFor (0.04);
    }
    else if (h == 'o')
    {
        hatEnv = 0.75f;
        hatDecay = decayFor (0.28);
    }
}

void RhythmGenerator::process (float* outL, float* outR, int numSamples, double ppqStart, double ppqPerSample,
                               int beatsPerMeasure, bool timelineRunning) noexcept
{
    if (! timelineRunning)
        lastStep = -1;

    const float sr = (float) sampleRate;
    for (int i = 0; i < numSamples; ++i)
    {
        if (timelineRunning)
        {
            const double ppq = ppqStart + ppqPerSample * i;
            const auto step = (long long) std::floor (ppq * 4.0 + 1.0e-9);
            if (step != lastStep)
            {
                if (lastStep >= 0 || std::abs (ppq * 4.0 - (double) step) < ppqPerSample * 4.0 + 1.0e-9)
                    trigger (step, beatsPerMeasure);
                lastStep = step;
            }
        }

        float drums = 0.0f;
        const float n = noise();

        // kick: sine with fast exponential pitch drop (190 -> 48 Hz), click transient, driven
        if (kickEnv > 1.0e-4f)
        {
            const float f = 48.0f + 142.0f * kickPitch;
            kickPitch *= kickPitchDecay;
            kickPhase += f / sr;
            if (kickPhase >= 1.0f) kickPhase -= 1.0f;
            const float body = (float) std::sin (kTwoPi * kickPhase) * kickEnv;
            drums += softClip (body * 1.8f) * 0.95f + n * kickClick * 0.35f;
            kickEnv *= kickDecay;
            kickClick *= kickClickDecay;
        }

        // snare: two tuned bodies + band-passed / high-passed noise
        if (snBodyEnv > 1.0e-4f || snNoiseEnv > 1.0e-4f)
        {
            snPh1 += 185.0f / sr; if (snPh1 >= 1.0f) snPh1 -= 1.0f;
            snPh2 += 330.0f / sr; if (snPh2 >= 1.0f) snPh2 -= 1.0f;
            drums += ((float) std::sin (kTwoPi * snPh1) * 0.6f + (float) std::sin (kTwoPi * snPh2) * 0.35f) * snBodyEnv * 0.5f;
            float lp, hp;
            const float bp = snFilter.process (n, lp, hp);
            drums += (bp * 0.9f + hp * 0.35f) * snNoiseEnv * 0.75f;
            snBodyEnv *= snBodyDecay;
            snNoiseEnv *= snNoiseDecay;
        }

        // hand clap: three short noise bursts, then a longer tail, band-passed
        if (clapEnv > 1.0e-4f || clapBurstLeft > 0)
        {
            if (clapBurstLeft > 0 && --clapBurstLeft == 0 && clapBurst < 2)
            {
                ++clapBurst;
                clapEnv = 1.0f;
                clapBurstLeft = (int) (0.009 * sampleRate);
                clapDecay = clapBurst == 2 ? decayFor (0.16) : decayFor (0.012);
            }
            float lp, hp;
            const float bp = clapFilter.process (n, lp, hp);
            drums += bp * clapEnv * 1.6f;
            clapEnv *= clapDecay;
        }

        // hi-hat: six square oscillators, band-pass + high-pass
        if (hatEnv > 1.0e-4f)
        {
            float metal = 0.0f;
            for (int o = 0; o < 6; ++o)
            {
                hatPhase[o] += kHatFreqs[o] * 1.6f / sr;
                if (hatPhase[o] >= 1.0f) hatPhase[o] -= 1.0f;
                metal += hatPhase[o] < 0.5f ? 1.0f : -1.0f;
            }
            metal = metal * (1.0f / 6.0f) * 0.7f + n * 0.3f;
            float lp, hp, lp2, hp2;
            const float bp = hatFilter.process (metal, lp, hp);
            hatHighPass.process (bp + hp * 0.5f, lp2, hp2);
            drums += hp2 * hatEnv * 0.6f;
            hatEnv *= hatDecay;
        }

        float out = softClip (drums * 1.1f) * 0.85f;

        // metronome
        if (clickEnv > 1.0e-4f)
        {
            clickPh1 += clickF1 / sr; if (clickPh1 >= 1.0f) clickPh1 -= 1.0f;
            float c = (float) std::sin (kTwoPi * clickPh1);
            if (clickSound == 1)
            {
                clickPh2 += clickF2 / sr; if (clickPh2 >= 1.0f) clickPh2 -= 1.0f;
                float lp, hp;
                const float knock = woodFilter.process (n * clickEnv * clickEnv, lp, hp);
                c = c * 0.7f + (float) std::sin (kTwoPi * clickPh2) * 0.25f * clickEnv + knock * 2.0f;
            }
            out += c * clickEnv * clickAmp;
            clickEnv *= clickDecay;
        }

        out *= level * 0.6f;
        outL[i] += out;
        outR[i] += out;
    }
}
} // namespace livelooper
