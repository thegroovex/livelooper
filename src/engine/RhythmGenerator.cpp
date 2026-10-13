#include "RhythmGenerator.h"

#include <algorithm>
#include <cmath>

namespace livelooper
{
namespace
{
    constexpr double kTwoPi = 6.283185307179586;

    /*  Pattern lanes, 16 steps each ('.' or ' ' = rest, shorter strings are padded with rests).
        Velocity: upper case = accent, lower case = normal, 'g' = ghost note.
        kick:  X x g
        snare: X x g = snare, C c = hand clap, B b = snare + clap
        hat:   X x g = closed, O o = open, R r = roll (2 x 32nd), T t = triplet roll (3 hits)
        perc:  R r = rimshot, S s = shaker, T t = tom                                           */
    struct Pattern
    {
        const char* name;
        const char* kick;
        const char* snare;
        const char* hat;
        const char* perc;
    };

    // Index 0 is the metronome (handled separately). Never reorder: the index is saved in projects.
    constexpr Pattern kPatterns[] = {
        { "Metronome",    "",                 "",                 "",                 "" },
        { "Rock",         "X.....x.X.x.....", "....X......gX...", "X.x.X.x.X.x.X.x.", "" },
        { "Pop",          "X.....x.X.....x.", "....B.......B...", "Xgx.Xgx.Xgx.Xgxo", "" },
        { "Funk",         "X..x..x...X..x..", "....X..g.g..X..g", "XgxgXgxgXgxgXgxg", "" },
        { "Hip Hop",      "X......x..X.....", "....B.......B..g", "x.x.X.x.x.x.X.x.", "" },
        { "House",        "X...X...X...X...", "....C.......C...", "g.O.g.O.g.O.g.O.", ".s.s.s.s.s.s.s.s" },
        { "Techno",       "X...X...X...X...", "....c.......c...", "gxOxgxOxgxOxgxOx", "r......r..r....." },
        { "Breakbeat",    "X.x.......X..x..", "....X..g.g..X..g", "x.x.X.x.x.x.X.x.", "" },
        { "Reggae",       "........X.......", "",                 "..x...x...x...x.", "........R......." },
        { "Bossa",        "X..xX..xX..xX..x", "",                 "xgxgxgxgxgxgxgxg", "R..R..R...R..R.." },
        { "Shuffle",      "X.....x.X.....x.", "....X.......X...", "X..xX..xX..xX..x", "" },
        { "Half Time",    "X.........x.....", "......g.X.....g.", "x.x.x.x.x.x.x.x.", "" },
        // since 1.3
        { "Trap",         "X......x..X.....", "........B.......", "x.x.x.xrx.x.t.x.", "" },
        { "Lo-Fi",        "X......xX.x.....", "....X.......X.g.", "xgx.xgx.xgx.xgxg", "" },
        { "Drum & Bass",  "X.........x.....", "....X..g....X...", "x.x.x.x.x.x.x.x.", "...s...s...s...s" },
        { "Afrobeat",     "X..x..X...X..x..", "....g..X...g..X.", "x.x.x.x.x.x.x.x.", "SsssSsssSsssSsss" },
        { "Dembow",       "X...X...X...X...", "...X..x....X..x.", "x.x.x.x.x.x.x.x.", "" },
        { "UK Garage",    "X......x..X.....", "....X.......X...", "..x...x...x...xo", ".s.s.s.s.s.s.s.s" },
        { "Neo Soul",     "X.....x...xX....", "....X..g.g..X.g.", "x.xgx.xgx.xgx.xg", "" },
        { "Disco",        "X...X...X...X...", "....X.......X...", "x.O.x.O.x.O.x.O.", ".s.s.s.s.s.s.s.s" },
        { "Afro House",   "X...X...X...X...", "....c.......c...", "..x...x...x...x.", "s.Ss..t.s.Ss.t.t" },
    };
    constexpr int kNumPatterns = (int) (sizeof (kPatterns) / sizeof (kPatterns[0]));

    struct Kit
    {
        const char* name;
        // kick
        float kickStart, kickEnd, kickPitchTime, kickDecay, kickClick, kickDrive;
        // snare
        float snTone1, snTone2, snToneDecay, snNoiseDecay, snBandFreq, snHighFreq, snToneMix;
        // clap, hats
        float clapTail, hatClosed, hatOpen, hatHighFreq, hatMetal;
        // bus
        float busLowPass, busDrive, room;
    };

    constexpr Kit kKits[] = {
        //  name     kick: start  end  pitchT decay click drive  snare: t1   t2   toneD noiseD band  high  mix  clap  hatC   hatO  hatHP  metal  lp     drive room
        { "Studio",        165.f, 52.f, 0.034f, 0.42f, 0.55f, 1.6f, 195.f, 340.f, 0.09f, 0.19f, 4800.f, 1400.f, 0.45f, 0.18f, 0.045f, 0.34f, 7200.f, 0.30f, 0.0f,    1.25f, 0.13f },
        { "808",           105.f, 46.f, 0.055f, 1.10f, 0.22f, 2.0f, 238.f, 476.f, 0.07f, 0.15f, 6800.f, 2000.f, 0.55f, 0.20f, 0.035f, 0.42f, 8200.f, 0.75f, 0.0f,    1.20f, 0.07f },
        { "909",           230.f, 55.f, 0.024f, 0.34f, 0.90f, 2.6f, 180.f, 330.f, 0.06f, 0.24f, 4300.f, 1200.f, 0.35f, 0.22f, 0.050f, 0.46f, 7000.f, 0.50f, 0.0f,    1.35f, 0.09f },
        { "Lo-Fi",         125.f, 50.f, 0.040f, 0.30f, 0.20f, 2.0f, 190.f, 300.f, 0.08f, 0.16f, 3000.f,  900.f, 0.50f, 0.15f, 0.030f, 0.22f, 5200.f, 0.20f, 4200.0f, 1.80f, 0.20f },
    };
    constexpr int kNumKits = (int) (sizeof (kKits) / sizeof (kKits[0]));

    inline char at (const char* s, int step) noexcept
    {
        for (int i = 0; i <= step; ++i)
            if (s[i] == 0)
                return '.';
        return s[step] == ' ' ? '.' : s[step];
    }

    inline float velocityOf (char c) noexcept
    {
        if (c == 'g') return 0.38f;
        return (c >= 'A' && c <= 'Z') ? 1.0f : 0.78f;
    }

    inline char lower (char c) noexcept { return (c >= 'A' && c <= 'Z') ? (char) (c - 'A' + 'a') : c; }

    // metallic hi-hat partials (six detuned square oscillators), mixed with noise per kit
    constexpr float kHatFreqs[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };

    // room delay lengths at 44.1 kHz (small, bright room)
    constexpr int kCombLen[4] = { 557, 617, 683, 733 };
    constexpr int kApLen[2] = { 225, 341 };

    inline float softClip (float x) noexcept
    {
        if (x > 3.0f) return 1.0f;
        if (x < -3.0f) return -1.0f;
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    inline float sine (float phase) noexcept { return (float) std::sin (kTwoPi * phase); }

    inline void advance (float& phase, float inc) noexcept
    {
        phase += inc;
        if (phase >= 1.0f) phase -= 1.0f;
    }
}

int RhythmGenerator::getNumPatterns() noexcept { return kNumPatterns; }

const char* RhythmGenerator::getPatternName (int index) noexcept
{
    return kPatterns[std::clamp (index, 0, kNumPatterns - 1)].name;
}

int RhythmGenerator::getNumKits() noexcept { return kNumKits; }

const char* RhythmGenerator::getKitName (int index) noexcept
{
    return kKits[std::clamp (index, 0, kNumKits - 1)].name;
}

double RhythmGenerator::swingOffset (int step, float swingAmount) noexcept
{
    if ((step & 1) == 0)
        return 0.0;
    return (std::clamp ((double) swingAmount, 0.5, 0.75) - 0.5) * 2.0;   // 0.6667 -> 1/3 step (triplet)
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
    kickClickFilter.set (sampleRate, 3500.0, 0.7);
    clapFilter.set (sampleRate, 1250.0, 1.3);
    clapHigh.set (sampleRate, 700.0, 0.7);
    hatBand.set (sampleRate, 10000.0, 0.8);
    rimFilter.set (sampleRate, 2600.0, 3.0);
    shBand.set (sampleRate, 6500.0, 1.4);
    clickFilter.set (sampleRate, 2500.0, 0.7);

    const double scale = sampleRate / 44100.0;
    for (int c = 0; c < 4; ++c)
        combBuf[c].assign ((size_t) std::max (1, (int) (kCombLen[c] * scale)), 0.0f);
    for (int a = 0; a < 2; ++a)
        apBuf[a].assign ((size_t) std::max (1, (int) (kApLen[a] * scale)), 0.0f);
    reset();
}

void RhythmGenerator::reset() noexcept
{
    kickEnv = kickClick = snBodyEnv = snNoiseEnv = snSnap = clapEnv = hatEnv = clickEnv = 0.0f;
    rimEnv = shEnv = tomEnv = clickKnock = 0.0f;
    clapBurstLeft = 0;
    numEvents = 0;
    for (auto* f : { &kickClickFilter, &snBand, &snHigh, &clapFilter, &clapHigh, &hatBand, &hatHighPass,
                     &rimFilter, &shBand, &busLowPass, &clickFilter })
        f->reset();
    for (int c = 0; c < 4; ++c) { std::fill (combBuf[c].begin(), combBuf[c].end(), 0.0f); combPos[c] = 0; combStore[c] = 0.0f; }
    for (int a = 0; a < 2; ++a) { std::fill (apBuf[a].begin(), apBuf[a].end(), 0.0f); apPos[a] = 0; }
    lastStep = -1;
}

float RhythmGenerator::decayFor (double seconds) const noexcept
{
    return (float) std::exp (-1.0 / (std::max (1.0e-4, seconds) * sampleRate));
}

float RhythmGenerator::noise() noexcept
{
    noiseSeed = noiseSeed * 1664525u + 1013904223u;
    return (float) (int32_t) noiseSeed / 2147483648.0f;
}

float RhythmGenerator::random01() noexcept
{
    randSeed ^= randSeed << 13;
    randSeed ^= randSeed >> 17;
    randSeed ^= randSeed << 5;
    return (float) (randSeed & 0xffffff) / 16777216.0f;
}

void RhythmGenerator::addEvent (double delaySamples, Voice v, float velocity, uint8_t variant) noexcept
{
    if (numEvents >= (int) events.size())
        return;
    // humanize: a little late (never early - the step has already started) and a little softer / louder
    const float h = std::clamp (humanize, 0.0f, 1.0f);
    const double lateMax = (v == Kick ? 0.003 : 0.008) * h * sampleRate;
    delaySamples += random01() * lateMax;
    if (v != Click)
        velocity = std::clamp (velocity * (1.0f + (random01() - 0.6f) * 0.3f * h), 0.05f, 1.0f);
    events[(size_t) numEvents++] = { delaySamples, v, velocity, variant };
}

void RhythmGenerator::scheduleStep (long long step, int beatsPerMeasure) noexcept
{
    const int p = std::clamp (pattern, 0, kNumPatterns - 1);

    if (p == 0)
    {
        if (((step % 4) + 4) % 4 != 0)
            return;
        const long long stepsPerMeasure = std::max (1, beatsPerMeasure) * 4;
        const bool accent = ((step % stepsPerMeasure) + stepsPerMeasure) % stepsPerMeasure == 0;
        events[(size_t) std::min (numEvents, (int) events.size() - 1)] = { 0.0, Click, accent ? 1.0f : 0.7f, (uint8_t) (accent ? 1 : 0) };
        numEvents = std::min (numEvents + 1, (int) events.size());
        return;
    }

    const auto& pat = kPatterns[p];
    const int s = (int) positiveModulo ((double) step, 16.0);
    const double base = swingOffset (s, swing) * samplesPerStep;

    if (const char k = at (pat.kick, s); k != '.')
        addEvent (base, Kick, velocityOf (k));

    if (const char sn = at (pat.snare, s); sn != '.')
    {
        const char l = lower (sn);
        if (l == 'x' || l == 'g' || l == 'b')
            addEvent (base, Snare, velocityOf (sn));
        if (l == 'c' || l == 'b')
            addEvent (base, Clap, velocityOf (sn));
    }

    if (const char h = at (pat.hat, s); h != '.')
    {
        const char l = lower (h);
        const float v = velocityOf (h);
        if (l == 'o')
            addEvent (base, HatOpen, v);
        else if (l == 'r')
        {
            addEvent (base, HatClosed, v);
            addEvent (base + samplesPerStep * 0.5, HatClosed, v * 0.7f);
        }
        else if (l == 't')
        {
            for (int i = 0; i < 3; ++i)
                addEvent (base + samplesPerStep * i / 3.0, HatClosed, v * (1.0f - 0.18f * (float) i));
        }
        else
            addEvent (base, HatClosed, v);
    }

    if (const char pc = at (pat.perc, s); pc != '.')
    {
        const char l = lower (pc);
        const float v = velocityOf (pc);
        if (l == 'r')      addEvent (base, Rim, v);
        else if (l == 's') addEvent (base, Shaker, v);
        else if (l == 't') addEvent (base, Tom, v);
    }
}

void RhythmGenerator::fire (Voice v, float vel, uint8_t variant) noexcept
{
    const auto& k = kKits[std::clamp (kit, 0, kNumKits - 1)];
    switch (v)
    {
        case Kick:
            kickVel = vel;
            kickEnv = 1.0f;
            kickPitch = 1.0f;
            kickPitchDecay = decayFor (k.kickPitchTime);
            kickDecay = decayFor (k.kickDecay * (0.75f + 0.25f * vel));
            kickClick = k.kickClick;
            kickClickDecay = decayFor (0.0035);
            kickPhase = 0.0f;
            break;

        case Snare:
            snVel = vel;
            snBodyEnv = 1.0f;
            snPitch = 1.0f;
            snPitchDecay = decayFor (0.012);
            snBodyDecay = decayFor (k.snToneDecay);
            snNoiseEnv = 1.0f;
            snNoiseDecay = decayFor (k.snNoiseDecay * (0.6f + 0.4f * vel));   // ghost notes are shorter
            snSnap = 1.0f;
            snSnapDecay = decayFor (0.003);
            snBand.set (sampleRate, k.snBandFreq, 0.8);
            snHigh.set (sampleRate, k.snHighFreq, 0.7);
            break;

        case Clap:
            clapVel = vel;
            clapEnv = 1.0f;
            clapBurst = 0;
            clapBurstLeft = (int) (0.0085 * sampleRate);
            clapDecay = decayFor (0.006);
            break;

        case HatClosed:
        case HatOpen:
            // one envelope for both: a closed hat chokes a ringing open hat
            hatVel = vel;
            hatEnv = 1.0f;
            hatDecay = decayFor ((v == HatOpen ? k.hatOpen : k.hatClosed * (0.7f + 0.5f * vel))
                                 * (1.0f + (random01() - 0.5f) * 0.2f * humanize));
            hatHighPass.set (sampleRate, k.hatHighFreq, 0.7);
            break;

        case Rim:
            rimVel = vel;
            rimEnv = 1.0f;
            rimDecay = decayFor (0.028);
            rimPh1 = rimPh2 = 0.0f;
            break;

        case Shaker:
            shVel = vel;
            shEnv = 1.0f;
            shAttack = 0.0f;
            shAttackCoeff = 1.0f - decayFor (0.006);
            shDecay = decayFor (0.07);
            break;

        case Tom:
            tomVel = vel;
            tomEnv = 1.0f;
            tomPitch = 1.0f;
            tomPitchDecay = decayFor (0.05);
            tomDecay = decayFor (0.22);
            break;

        case Click:
        {
            const bool accent = variant == 1;
            activeClickSound = std::clamp (clickSound, 0, kNumClickSounds - 1);
            clickEnv = 1.0f;
            clickPh1 = clickPh2 = 0.0f;
            clickGlide = 0.0f;
            clickKnock = 0.0f;
            switch (activeClickSound)
            {
                case 1:   // woodblock: one hollow, pitched knock that dies quickly - no ringing partials
                    clickF1 = accent ? 1150.0f : 860.0f;
                    clickF2 = clickF1 * 1.52f;
                    clickGlide = 1.0f;
                    clickGlideDecay = decayFor (0.004);
                    clickDecay = decayFor (0.018);
                    clickKnock = 1.0f;
                    clickKnockDecay = decayFor (0.0015);
                    clickFilter.set (sampleRate, 2200.0, 0.7);
                    clickAmp = accent ? 1.1f : 0.8f;
                    break;
                case 2:   // click: dry high tick, no pitch
                    clickF1 = clickF2 = 0.0f;
                    clickDecay = decayFor (accent ? 0.006 : 0.004);
                    clickFilter.set (sampleRate, accent ? 3500.0 : 2500.0, 0.7);
                    clickAmp = accent ? 1.4f : 1.0f;
                    break;
                case 3:   // rimshot: uses the drum voice
                    clickEnv = 0.0f;
                    fire (Rim, accent ? 1.0f : 0.7f, 0);
                    break;
                default:  // classic beep
                    clickF1 = clickF2 = accent ? 1800.0f : 1200.0f;
                    clickDecay = decayFor (0.03);
                    clickAmp = accent ? 0.9f : 0.6f;
                    break;
            }
            break;
        }
    }
}

void RhythmGenerator::process (float* outL, float* outR, int numSamples, double ppqStart, double ppqPerSample,
                               int beatsPerMeasure, bool timelineRunning, float* kickOut) noexcept
{
    if (! timelineRunning)
    {
        lastStep = -1;
        numEvents = 0;
    }
    if (ppqPerSample > 0.0)
        samplesPerStep = 0.25 / ppqPerSample;

    const auto& k = kKits[std::clamp (kit, 0, kNumKits - 1)];
    const bool lowPass = k.busLowPass > 0.0f;
    if (lowPass)
        busLowPass.set (sampleRate, k.busLowPass, 0.6);
    const float kickNorm = 1.0f / softClip (k.kickDrive);
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
                    scheduleStep (step, beatsPerMeasure);
                lastStep = step;
            }
        }

        // due events
        for (int e = 0; e < numEvents;)
        {
            if (events[(size_t) e].delay < 1.0)
            {
                const auto ev = events[(size_t) e];
                events[(size_t) e] = events[(size_t) --numEvents];
                fire (ev.voice, ev.velocity, ev.variant);
            }
            else
            {
                events[(size_t) e].delay -= 1.0;
                ++e;
            }
        }

        float drums = 0.0f, roomSend = 0.0f;
        const float n = noise();

        // kick: sine with exponential pitch drop, filtered click transient, driven
        if (kickEnv > 1.0e-4f)
        {
            const float f = k.kickEnd + (k.kickStart - k.kickEnd) * kickPitch;
            kickPitch *= kickPitchDecay;
            advance (kickPhase, f / sr);
            const float body = softClip (sine (kickPhase) * kickEnv * k.kickDrive) * kickNorm;
            float lp, hp;
            kickClickFilter.process (n, lp, hp);
            drums += (body * 0.95f + (lp * 0.8f + sine (kickPhase * 7.0f) * 0.2f) * kickClick) * kickVel;
            if (kickOut != nullptr)
                kickOut[i] = kickEnv * kickVel;     // sidechain source: the kick's envelope (independent of the level)
            kickEnv *= kickDecay;
            kickClick *= kickClickDecay;
        }

        // snare: two tuned bodies with a short pitch drop, band + high passed noise, snap
        if (snBodyEnv > 1.0e-4f || snNoiseEnv > 1.0e-4f)
        {
            const float bend = 1.0f + 0.3f * snPitch;
            snPitch *= snPitchDecay;
            advance (snPh1, k.snTone1 * bend / sr);
            advance (snPh2, k.snTone2 * bend / sr);
            const float tone = (sine (snPh1) * 0.65f + sine (snPh2) * 0.35f) * snBodyEnv;
            float lp, hp, lp2, hp2;
            const float bp = snBand.process (n, lp, hp);
            snHigh.process (n, lp2, hp2);
            const float rattle = (bp * 0.8f + hp2 * 0.55f) * snNoiseEnv;
            const float s = (tone * k.snToneMix + rattle * (1.0f - k.snToneMix) * 1.4f + n * snSnap * 0.45f) * snVel;
            drums += s;
            roomSend += s;
            snBodyEnv *= snBodyDecay;
            snNoiseEnv *= snNoiseDecay;
            snSnap *= snSnapDecay;
        }

        // hand clap: four short noise bursts, then a tail, band-passed
        if (clapEnv > 1.0e-4f || clapBurstLeft > 0)
        {
            if (clapBurstLeft > 0 && --clapBurstLeft == 0 && clapBurst < 3)
            {
                ++clapBurst;
                clapEnv = 1.0f;
                clapBurstLeft = (int) ((clapBurst == 1 ? 0.0105 : 0.0092) * sampleRate);
                clapDecay = clapBurst == 3 ? decayFor (k.clapTail) : decayFor (0.006);
            }
            float lp, hp, lp2, hp2;
            const float bp = clapFilter.process (n, lp, hp);
            clapHigh.process (bp, lp2, hp2);
            const float c = hp2 * clapEnv * 1.9f * clapVel;
            drums += c;
            roomSend += c * 1.3f;
            clapEnv *= clapDecay;
        }

        // hi-hat: noise and metallic squares, band-pass + kit high-pass
        if (hatEnv > 1.0e-4f)
        {
            float metal = 0.0f;
            for (int o = 0; o < 6; ++o)
            {
                advance (hatPhase[o], kHatFreqs[o] * 1.7f / sr);
                metal += hatPhase[o] < 0.5f ? 1.0f : -1.0f;
            }
            const float src = metal * (1.0f / 6.0f) * k.hatMetal + n * (1.0f - k.hatMetal);
            float lp, hp, lp2, hp2;
            const float bp = hatBand.process (src, lp, hp);
            hatHighPass.process (bp + hp * 0.6f, lp2, hp2);
            const float h = hp2 * hatEnv * 0.7f * hatVel;
            drums += h;
            roomSend += h * 0.25f;
            hatEnv *= hatDecay;
        }

        // rimshot: two short partials plus a band-passed crack
        if (rimEnv > 1.0e-4f)
        {
            advance (rimPh1, 1720.0f / sr);
            advance (rimPh2, 820.0f / sr);
            float lp, hp;
            const float crack = rimFilter.process (n, lp, hp);
            const float r = ((sine (rimPh1) * 0.5f + sine (rimPh2) * 0.35f) * rimEnv + crack * rimEnv * rimEnv * 1.5f) * rimVel * 0.8f;
            drums += r;
            roomSend += r;
            rimEnv *= rimDecay;
        }

        // shaker: band-passed noise with a soft attack
        if (shEnv > 1.0e-4f)
        {
            shAttack += (1.0f - shAttack) * shAttackCoeff;
            float lp, hp;
            const float bp = shBand.process (n, lp, hp);
            drums += bp * shAttack * shEnv * 0.55f * shVel;
            shEnv *= shDecay;
        }

        // tom: sine with a pitch drop
        if (tomEnv > 1.0e-4f)
        {
            advance (tomPhase, (95.0f + 70.0f * tomPitch) / sr);
            tomPitch *= tomPitchDecay;
            drums += softClip (sine (tomPhase) * tomEnv * 1.5f) * 0.7f * tomVel;
            tomEnv *= tomDecay;
        }

        // small room on snare / clap / rim / a bit of hat
        if (k.room > 0.0f && ! combBuf[0].empty())
        {
            float wet = 0.0f;
            for (int c = 0; c < 4; ++c)
            {
                auto& buf = combBuf[c];
                const float y = buf[(size_t) combPos[c]];
                combStore[c] = y * 0.6f + combStore[c] * 0.4f;            // damping
                buf[(size_t) combPos[c]] = roomSend + combStore[c] * 0.72f;
                if (++combPos[c] >= (int) buf.size()) combPos[c] = 0;
                wet += y;
            }
            wet *= 0.25f;
            for (int a = 0; a < 2; ++a)
            {
                auto& buf = apBuf[a];
                const float b = buf[(size_t) apPos[a]];
                const float y = -wet + b;
                buf[(size_t) apPos[a]] = wet + b * 0.5f;
                if (++apPos[a] >= (int) buf.size()) apPos[a] = 0;
                wet = y;
            }
            drums += wet * k.room * 2.0f;
        }

        if (lowPass)
        {
            float lp, hp;
            busLowPass.process (drums, lp, hp);
            drums = lp;
        }

        float out = softClip (drums * k.busDrive) * 0.85f;

        // metronome (never swung)
        if (clickEnv > 1.0e-4f)
        {
            float c = 0.0f;
            switch (activeClickSound)
            {
                case 1:
                {
                    const float f = clickF1 * (1.0f + 0.06f * clickGlide);
                    clickGlide *= clickGlideDecay;
                    advance (clickPh1, f / sr);
                    advance (clickPh2, clickF2 / sr);
                    float lp, hp;
                    clickFilter.process (n, lp, hp);
                    c = sine (clickPh1) * clickEnv + sine (clickPh2) * 0.12f * clickEnv * clickEnv + lp * clickKnock * 0.9f;
                    clickKnock *= clickKnockDecay;
                    break;
                }
                case 2:
                {
                    float lp, hp;
                    const float bp = clickFilter.process (n, lp, hp);
                    c = (bp * 1.6f + hp * 0.4f) * clickEnv;
                    break;
                }
                default:
                    advance (clickPh1, clickF1 / sr);
                    c = sine (clickPh1) * clickEnv;
                    break;
            }
            out += c * clickAmp;
            clickEnv *= clickDecay;
        }

        out *= level * 0.6f;
        outL[i] += out;
        outR[i] += out;
    }
}
} // namespace livelooper
