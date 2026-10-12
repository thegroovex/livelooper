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
        const char* snare;
        const char* hat;      // 'x' closed, 'o' open
    };

    // Index 0 is the metronome (handled separately).
    constexpr Pattern kPatterns[] = {
        { "Metronome",  "", "", "" },
        { "Rock",       "x.......x.x.....", "....x.......x...", "x.x.x.x.x.x.x.x." },
        { "Pop",        "x.....x.x.......", "....x.......x...", "x.x.x.x.x.x.x.xo" },
        { "Funk",       "x..x..x...x..x..", "....x..x.x..x..x", "xxxxxxxxxxxxxxxx" },
        { "Hip Hop",    "x......x..x.....", "....x.......x...", "x.x.x.x.x.x.x.x." },
        { "House",      "x...x...x...x...", "....x.......x...", "..o...o...o...o." },
        { "Techno",     "x...x...x...x...", "................", "xxoxxxoxxxoxxxox" },
        { "Breakbeat",  "x.x.......x.....", "....x..x.x..x...", "x.x.x.x.x.x.x.x." },
        { "Reggae",     "........x.......", "........x.......", "..x...x...x...x." },
        { "Bossa",      "x..x x..x..x x..", "..x..x..x...x..x", "x.x.x.x.x.x.x.x." },
        { "Shuffle",    "x.....x.x.....x.", "....x.......x...", "x.xx.xx.xx.xx.xx" },
        { "Half Time",  "x...............", "........x.......", "x.x.x.x.x.x.x.x." },
    };
    constexpr int kNumPatterns = (int) (sizeof (kPatterns) / sizeof (kPatterns[0]));

    inline bool hit (const char* s, int step) noexcept
    {
        for (int i = 0; i <= step; ++i)
            if (s[i] == 0)
                return false;
        return s[step] == 'x';
    }

    inline bool openHat (const char* s, int step) noexcept
    {
        for (int i = 0; i <= step; ++i)
            if (s[i] == 0)
                return false;
        return s[step] == 'o';
    }
}

int RhythmGenerator::getNumPatterns() noexcept { return kNumPatterns; }

const char* RhythmGenerator::getPatternName (int index) noexcept
{
    return kPatterns[std::clamp (index, 0, kNumPatterns - 1)].name;
}

void RhythmGenerator::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    reset();
}

void RhythmGenerator::reset() noexcept
{
    kick = snareTone = click = Voice {};
    snareNoiseEnv = hatEnv = 0.0f;
    hatHp = hatPrev = snareBp1 = snareBp2 = 0.0f;
    lastStep = -1;
}

void RhythmGenerator::trigger (long long step, int beatsPerMeasure) noexcept
{
    const auto decayFor = [this] (double seconds) { return (float) std::exp (-1.0 / (seconds * sampleRate)); };
    const int p = std::clamp (pattern, 0, kNumPatterns - 1);

    if (p == 0)
    {
        if (((step % 4) + 4) % 4 != 0)
            return;
        const long long stepsPerMeasure = std::max (1, beatsPerMeasure) * 4;
        const bool accent = ((step % stepsPerMeasure) + stepsPerMeasure) % stepsPerMeasure == 0;
        click.env = 1.0f;
        click.freq = click.freqEnd = accent ? 1800.0f : 1200.0f;
        click.decay = decayFor (0.03);
        click.sweep = 1.0f;
        click.amp = accent ? 0.9f : 0.6f;
        return;
    }

    const auto& pat = kPatterns[p];
    const int s = (int) positiveModulo ((double) step, 16.0);

    if (hit (pat.kick, s))
    {
        kick.env = 1.0f;
        kick.freq = 160.0f;
        kick.freqEnd = 48.0f;
        kick.sweep = decayFor (0.035);
        kick.decay = decayFor (0.28);
        kick.amp = 1.0f;
    }
    if (hit (pat.snare, s))
    {
        snareTone.env = 1.0f;
        snareTone.freq = snareTone.freqEnd = 185.0f;
        snareTone.sweep = 1.0f;
        snareTone.decay = decayFor (0.09);
        snareTone.amp = 0.45f;
        snareNoiseEnv = 1.0f;
        snareNoiseDecay = decayFor (0.16);
    }
    if (hit (pat.hat, s))
    {
        hatEnv = 0.55f;
        hatDecay = decayFor (0.035);
    }
    else if (openHat (pat.hat, s))
    {
        hatEnv = 0.5f;
        hatDecay = decayFor (0.22);
    }
}

void RhythmGenerator::process (float* outL, float* outR, int numSamples, double ppqStart, double ppqPerSample,
                               int beatsPerMeasure, bool timelineRunning) noexcept
{
    if (! timelineRunning)
        lastStep = -1;

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

        float out = 0.0f;

        // kick: sine with pitch sweep
        if (kick.env > 1.0e-4f)
        {
            kick.freq = kick.freqEnd + (kick.freq - kick.freqEnd) * kick.sweep;
            kick.phase += (float) (kick.freq / sampleRate);
            if (kick.phase >= 1.0f) kick.phase -= 1.0f;
            out += (float) std::sin (kTwoPi * kick.phase) * kick.env * kick.amp;
            kick.env *= kick.decay;
        }

        // noise source
        noiseSeed = noiseSeed * 1664525u + 1013904223u;
        const float noise = (float) (int32_t) noiseSeed / 2147483648.0f;

        // snare: tone + band-limited noise
        if (snareTone.env > 1.0e-4f || snareNoiseEnv > 1.0e-4f)
        {
            snareTone.phase += (float) (snareTone.freq / sampleRate);
            if (snareTone.phase >= 1.0f) snareTone.phase -= 1.0f;
            out += (float) std::sin (kTwoPi * snareTone.phase) * snareTone.env * snareTone.amp;
            snareTone.env *= snareTone.decay;

            snareBp1 += (noise - snareBp1) * 0.45f;
            snareBp2 += (snareBp1 - snareBp2) * 0.08f;
            out += (snareBp1 - snareBp2) * snareNoiseEnv * 0.8f;
            snareNoiseEnv *= snareNoiseDecay;
        }

        // hi-hat: high-passed noise
        if (hatEnv > 1.0e-4f)
        {
            const float hp = noise - hatPrev + 0.6f * hatHp;
            hatPrev = noise;
            hatHp = hp;
            out += hp * hatEnv * 0.35f;
            hatEnv *= hatDecay;
        }

        // metronome click
        if (click.env > 1.0e-4f)
        {
            click.phase += (float) (click.freq / sampleRate);
            if (click.phase >= 1.0f) click.phase -= 1.0f;
            out += (float) std::sin (kTwoPi * click.phase) * click.env * click.amp;
            click.env *= click.decay;
        }

        out *= level * 0.6f;
        outL[i] += out;
        outR[i] += out;
    }
}
} // namespace livelooper
