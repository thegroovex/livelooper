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
        int fillStyle = -1;     // v1.6: RhythmGenerator::FillStyle, -1 = the kit decides
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
        // ---- since 1.6 (branch v1.6-rhythm): grooves for the new style kits, each with a matching fill
        { "Deep House",      "X...x...X...x...", "....C.......C...", "..o...o...o...o.", "..s...s...s.s.s.", 6 },
        { "Tech House",      "X...x...X...x...", "....c.......c...", "g.Og.xOgg.Og.xOx", "r.....r.....r...", 6 },
        { "Peak Techno",     "X...x...X...x...", "....c.......c...", "..O...O...O...O.", "...r..r....r..r.", 2 },
        { "Hypnotic Techno", "X...x...X...x...", "",                 "xgxgxgxgxgxgxgxg", "t.....t...r.....", 2 },
        { "Boom Bap",        "X......xX.x.....", "....X.......X..g", "x.x.x.x.x.x.x.x.", "",                 1 },
        { "Boom Bap Swing",  "X.....x.x.X.....", "....X..g....X...", "x.xgx.x.x.xgx.x.", "",                 1 },
        { "Trap Rolls",      "X......x..X...x.", "........B.......", "x.x.xrx.x.x.t.xr", "",                 3 },
        { "Trap Half Time",  "X.....x...x.....", "........B......g", "xgxgxgxgtgxgrgxg", "",                 3 },
        { "Jungle",          "X.x.......xx....", "....X..g.X.gX...", "x.x.x.x.x.x.x.x.", "",                 4 },
        { "Two Step",        "X.........X.....", "....X.......X...", "x.x.x.x.x.x.x.x.", "..s...s...s...s.", 4 },
        { "Big Beat",        "X..x..x.X.......", "....X.......X...", "xgxgxgxgxgxgxgxg", "",                 4 },
        { "Rock Drive",      "X.......X.x.....", "....X.......X...", "X.x.X.x.X.x.X.x.", "",                 0 },
        { "Rock Half Time",  "X.....x.....X...", "........X.......", "X.x.X.x.X.x.X.x.", "",                 0 },
        { "Disco Hustle",    "X...x...X...x...", "....X.......X...", "xgOgxgOgxgOgxgOg", ".s.s.s.s.s.s.s.s", 7 },
        { "Funk Groove",     "X..x...x..X.x...", "....X..g.g..X.gg", "XgxgXgxgXgxgXgxg", "",                 1 },
        { "Son Clave",       "X...x...X...x...", "C..C..C...C.C...", "xxxxxxxxxxxxxxxx", "R.t.R.tTR.t.R.tT", 5 },
        { "Afro Groove",     "X..x..X...X..x..", "....g..X...g..X.", "x.x.x.x.x.x.x.x.", "T.tTt.T.t.tTt.T.", 5 },
        { "Samba",           "X..xX..xX..xX..x", "",                 "xgxgxgxgxgxgxgxg", "R.R.RR.RR.R.RR.R", 5 },
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
        // v1.6 style kits (synthesised fallback; the plug-in plays their samples)
        { "House",         150.f, 48.f, 0.030f, 0.45f, 0.45f, 1.8f, 200.f, 350.f, 0.07f, 0.20f, 5200.f, 1600.f, 0.40f, 0.24f, 0.040f, 0.38f, 7600.f, 0.40f, 0.0f,    1.30f, 0.10f },
        { "Techno",        190.f, 44.f, 0.028f, 0.50f, 0.70f, 3.0f, 185.f, 320.f, 0.05f, 0.18f, 4600.f, 1500.f, 0.30f, 0.20f, 0.035f, 0.30f, 8400.f, 0.55f, 0.0f,    1.50f, 0.12f },
        { "Boom Bap",      120.f, 50.f, 0.045f, 0.36f, 0.35f, 2.2f, 210.f, 320.f, 0.10f, 0.20f, 3400.f, 1100.f, 0.55f, 0.18f, 0.035f, 0.26f, 6000.f, 0.25f, 6500.0f, 1.70f, 0.16f },
        { "Trap",           90.f, 42.f, 0.070f, 1.40f, 0.25f, 2.4f, 230.f, 460.f, 0.06f, 0.14f, 6000.f, 2200.f, 0.45f, 0.26f, 0.030f, 0.40f, 8800.f, 0.60f, 0.0f,    1.25f, 0.08f },
        { "Breaks",        140.f, 55.f, 0.032f, 0.32f, 0.60f, 2.2f, 220.f, 360.f, 0.08f, 0.22f, 4200.f, 1300.f, 0.50f, 0.20f, 0.040f, 0.32f, 7000.f, 0.45f, 9000.0f, 1.60f, 0.14f },
        { "Rock",          130.f, 50.f, 0.036f, 0.40f, 0.65f, 1.8f, 180.f, 300.f, 0.11f, 0.24f, 4000.f, 1200.f, 0.50f, 0.22f, 0.055f, 0.45f, 6800.f, 0.25f, 0.0f,    1.40f, 0.18f },
        { "Disco",         140.f, 52.f, 0.034f, 0.38f, 0.40f, 1.7f, 200.f, 340.f, 0.08f, 0.17f, 5000.f, 1500.f, 0.45f, 0.20f, 0.045f, 0.36f, 7400.f, 0.35f, 0.0f,    1.30f, 0.12f },
        { "Afro Latin",    110.f, 48.f, 0.040f, 0.50f, 0.30f, 1.6f, 260.f, 420.f, 0.12f, 0.10f, 3600.f, 1000.f, 0.75f, 0.14f, 0.030f, 0.20f, 6400.f, 0.20f, 0.0f,    1.20f, 0.14f },
    };
    constexpr int kNumKits = (int) (sizeof (kKits) / sizeof (kKits[0]));

    // ---- v1.5 grooves: micro-timing in ms per voice (kick, snare, clap, closed hat, open hat, rim, shaker, tom)
    //      plus extra for hats on the off-beat 8ths / 16ths, and an accent curve over the 16 steps
    struct Groove
    {
        const char* name;
        float voiceMs[8];
        float hatOffbeatMs;     // added on steps 2, 6, 10, 14
        float hat16thMs;        // added on odd steps
        float accent[16];
        float kickDrunkMs[16];  // per-step kick offsets ("drunk" feel), 0 for most
    };
    constexpr Groove kGrooves[] = {
        { "Straight",  { 0, 0, 0, 0, 0, 0, 0, 0 }, 0.0f, 0.0f,
          { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, {} },
        { "Natural",   { 0, 4, 3, 3, 3, 4, 5, 2 }, 2.0f, 3.0f,
          { 1.0f, .80f, .90f, .78f, .97f, .80f, .90f, .78f, 1.0f, .80f, .90f, .78f, .97f, .80f, .90f, .80f }, {} },
        { "Laid Back", { 2, 12, 10, 9, 8, 10, 10, 6 }, 3.0f, 4.0f,
          { 1.0f, .76f, .88f, .74f, .95f, .76f, .88f, .74f, 1.0f, .76f, .88f, .74f, .95f, .76f, .88f, .78f }, {} },
        { "Pushed",    { 0, -3, -2, -6, -5, -3, -6, 0 }, -2.0f, -2.0f,
          { 1.0f, .82f, .92f, .80f, 1.0f, .82f, .92f, .80f, 1.0f, .82f, .92f, .80f, 1.0f, .82f, .92f, .84f }, {} },
        { "Drunk",     { 0, 15, 12, 6, 6, 12, 8, 6 }, 8.0f, 11.0f,
          { 1.0f, .70f, .86f, .72f, .94f, .70f, .84f, .74f, 1.0f, .72f, .86f, .70f, .92f, .72f, .86f, .76f },
          { 0, 18, -7, 12, 0, 16, -6, 10, 0, 18, -7, 12, 0, 14, -5, 9 } },
        { "Drive",     { 0, 1, 2, -2, -4, 0, -3, 0 }, -4.0f, 0.0f,
          { 1.0f, .74f, 1.0f, .74f, 1.0f, .74f, 1.0f, .74f, 1.0f, .74f, 1.0f, .74f, 1.0f, .74f, 1.0f, .78f }, {} },
    };
    constexpr int kNumGrooves = (int) (sizeof (kGrooves) / sizeof (kGrooves[0]));

    // level trims per kit and voice for the samples (kick, snare, clap, closed hat, open hat, rim, shaker, tom)
    constexpr float kSampleTrim[][8] = {
        { 1.00f, 0.80f, 0.55f, 0.42f, 0.38f, 0.55f, 0.40f, 0.65f },   // Studio (acoustic)
        { 1.00f, 0.70f, 0.60f, 0.42f, 0.38f, 0.50f, 0.40f, 0.70f },   // 808
        { 1.00f, 0.72f, 0.62f, 0.45f, 0.40f, 0.50f, 0.40f, 0.65f },   // 909 style
        { 0.95f, 0.75f, 0.60f, 0.45f, 0.40f, 0.55f, 0.40f, 0.65f },   // Lo-Fi / vintage
        // v1.6
        { 1.00f, 0.72f, 0.62f, 0.40f, 0.36f, 0.50f, 0.38f, 0.62f },   // House
        { 1.00f, 0.66f, 0.58f, 0.38f, 0.34f, 0.48f, 0.36f, 0.60f },   // Techno
        { 1.00f, 0.80f, 0.60f, 0.42f, 0.38f, 0.55f, 0.40f, 0.62f },   // Boom Bap
        { 0.95f, 0.70f, 0.62f, 0.40f, 0.36f, 0.50f, 0.38f, 0.62f },   // Trap
        { 1.00f, 0.78f, 0.60f, 0.42f, 0.38f, 0.52f, 0.40f, 0.62f },   // Breaks
        { 1.00f, 0.80f, 0.55f, 0.42f, 0.40f, 0.55f, 0.40f, 0.68f },   // Rock
        { 1.00f, 0.74f, 0.60f, 0.42f, 0.38f, 0.52f, 0.40f, 0.62f },   // Disco
        { 0.95f, 0.72f, 0.60f, 0.42f, 0.42f, 0.55f, 0.45f, 0.70f },   // Afro Latin
    };
    constexpr float kSampleRoom[] = { 0.10f, 0.05f, 0.06f, 0.12f, 0.06f, 0.07f, 0.10f, 0.04f, 0.09f, 0.14f, 0.08f, 0.10f };
    constexpr int kKitFill[] = { 0, 0, 0, 0, 6, 2, 1, 3, 4, 0, 7, 5 };

    constexpr const char* kVoiceNames[8] = { "Kick", "Snare", "Clap", "Closed Hat", "Open Hat", "Rim", "Shaker", "Tom" };
    // Afro Latin: the voices are hand percussion
    constexpr const char* kAfroVoiceNames[8] = { "Kick", "Bongo", "Claves", "Shaker", "Tambourine", "Cowbell", "Cabasa", "Conga" };

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
int RhythmGenerator::getNumGrooves() noexcept { return kNumGrooves; }

const char* RhythmGenerator::getGrooveName (int index) noexcept
{
    return kGrooves[std::clamp (index, 0, kNumGrooves - 1)].name;
}

double RhythmGenerator::grooveOffsetMs (int g, int voice, int step) noexcept
{
    const auto& gr = kGrooves[std::clamp (g, 0, kNumGrooves - 1)];
    if (voice < 0 || voice > 7)
        return 0.0;
    const int s = ((step % 16) + 16) % 16;
    double ms = gr.voiceMs[voice];
    if (voice == HatClosed || voice == HatOpen || voice == Shaker)
    {
        if (s % 4 == 2) ms += gr.hatOffbeatMs;
        if (s % 2 == 1) ms += gr.hat16thMs;
    }
    if (voice == Kick)
        ms += gr.kickDrunkMs[s];
    return ms;
}

float RhythmGenerator::grooveAccent (int g, int step) noexcept
{
    return kGrooves[std::clamp (g, 0, kNumGrooves - 1)].accent[((step % 16) + 16) % 16];
}

const DrumBank::Kit* RhythmGenerator::currentKit() const noexcept
{
    const auto* b = bankPtr.load (std::memory_order_acquire);
    return b != nullptr ? b->getKit (kit) : nullptr;
}

bool RhythmGenerator::isUsingSamples() const noexcept
{
    return currentKit() != nullptr;
}

const char* RhythmGenerator::getKitName (int index) noexcept
{
    // v1.5: the kits are recorded samples; the names describe the sound (index kept for saved projects)
    // v1.6: style kits appended (never reorder)
    static const char* const names[] = { "Studio", "808", "909 Style", "Lo-Fi",
                                         "House", "Techno", "Boom Bap", "Trap", "Breaks", "Rock", "Disco", "Afro Latin" };
    static_assert (sizeof (names) / sizeof (names[0]) == kNumKits);
    return names[std::clamp (index, 0, kNumKits - 1)];
}

float RhythmGenerator::builtInTrim (int k, int voice) noexcept
{
    return kSampleTrim[std::clamp (k, 0, kNumKits - 1)][std::clamp (voice, 0, 7)];
}

float RhythmGenerator::builtInRoom (int k) noexcept { return kSampleRoom[std::clamp (k, 0, kNumKits - 1)]; }

const char* RhythmGenerator::getVoiceName (int voice) noexcept { return kVoiceNames[std::clamp (voice, 0, 7)]; }

const char* RhythmGenerator::getKitVoiceName (int k, int voice) noexcept
{
    return k == 11 ? kAfroVoiceNames[std::clamp (voice, 0, 7)] : kVoiceNames[std::clamp (voice, 0, 7)];
}

int RhythmGenerator::kitFillStyle (int k) noexcept
{
    return k >= 0 && k < kNumKits ? kKitFill[k] : (int) FillToms;
}

int RhythmGenerator::patternFillStyle (int p) noexcept
{
    return p > 0 && p < kNumPatterns ? kPatterns[p].fillStyle : -1;
}

float RhythmGenerator::kickAccent (int step, long long bar, float var) noexcept
{
    const int s = ((step % 16) + 16) % 16;
    float f = s == 0 ? 1.0f : s == 8 ? 0.95f : (s % 4 == 0 ? 0.9f : (s % 2 == 0 ? 0.84f : 0.8f));
    if (s == 0 && ((bar % 4) + 4) % 4 != 0)
        f = 0.97f;                       // the first bar of a 4-bar phrase gets the strongest downbeat
    return 1.0f - std::clamp (var, 0.0f, 1.0f) * (1.0f - f);
}

BeatPattern RhythmGenerator::presetToPattern (int p)
{
    BeatPattern bp;
    bp.name = getPatternName (p);
    if (p <= 0 || p >= kNumPatterns)
        return bp;
    const auto& pat = kPatterns[p];
    auto vel = [] (char c) { return c == 'g' ? 40 : ((c >= 'A' && c <= 'Z') ? 127 : 96); };
    for (int st = 0; st < 16; ++st)
    {
        if (const char k = at (pat.kick, st); k != '.') bp.set (Kick, st, vel (k));
        if (const char sn = at (pat.snare, st); sn != '.')
        {
            const char l = lower (sn);
            if (l == 'x' || l == 'g' || l == 'b') bp.set (Snare, st, vel (sn));
            if (l == 'c' || l == 'b')             bp.set (Clap, st, vel (sn));
        }
        if (const char h = at (pat.hat, st); h != '.')
            bp.set (lower (h) == 'o' ? HatOpen : HatClosed, st, vel (h));
        if (const char pc = at (pat.perc, st); pc != '.')
        {
            const char l = lower (pc);
            bp.set (l == 'r' ? Rim : l == 's' ? Shaker : Tom, st, vel (pc));
        }
    }
    return bp;
}

//============================================================================== v1.6 BEATS patterns
void BeatPattern::resize (int newStepsPerBar, int newBars) noexcept
{
    newStepsPerBar = newStepsPerBar >= 32 ? 32 : 16;
    newBars = std::clamp (newBars, 1, kMaxBars);
    if (newStepsPerBar != stepsPerBar)
    {
        auto old = cells;
        for (auto& r : cells) r.fill (0);
        for (int row = 0; row < kRows; ++row)
            for (int st = 0; st < stepsPerBar * bars; ++st)
            {
                const int to = newStepsPerBar > stepsPerBar ? st * 2 : (st % 2 == 0 ? st / 2 : -1);
                if (to >= 0 && to < kMaxSteps && old[(size_t) row][(size_t) st] != 0)
                    cells[(size_t) row][(size_t) to] = std::max (cells[(size_t) row][(size_t) to], old[(size_t) row][(size_t) st]);
            }
        stepsPerBar = newStepsPerBar;
    }
    // shorter: clear the steps that fell off; longer: new bars repeat the first bar
    const int oldBars = bars;
    bars = newBars;
    for (int row = 0; row < kRows; ++row)
        for (int st = stepsPerBar * bars; st < kMaxSteps; ++st)
            cells[(size_t) row][(size_t) st] = 0;
    if (newBars > oldBars)
        for (int row = 0; row < kRows; ++row)
            for (int st = stepsPerBar * oldBars; st < stepsPerBar * newBars; ++st)
                cells[(size_t) row][(size_t) st] = cells[(size_t) row][(size_t) (st % (stepsPerBar * oldBars))];
}

BeatPatternBank::BeatPatternBank()
{
    for (auto& sl : slots)
        for (auto& r : sl.cells)
            for (auto& c : r)
                c.store (0, std::memory_order_relaxed);
}

void BeatPatternBank::store (int slot, const BeatPattern& p) noexcept
{
    if (slot < 0 || slot >= kNumSlots) return;
    auto& sl = slots[(size_t) slot];
    sl.stepsPerBar.store (p.stepsPerBar >= 32 ? 32 : 16);
    sl.bars.store (std::clamp (p.bars, 1, BeatPattern::kMaxBars));
    for (int r = 0; r < BeatPattern::kRows; ++r)
        for (int st = 0; st < BeatPattern::kMaxSteps; ++st)
            sl.cells[(size_t) r][(size_t) st].store (p.cells[(size_t) r][(size_t) st], std::memory_order_relaxed);
    version.fetch_add (1);
}

BeatPattern BeatPatternBank::load (int slot) const noexcept
{
    BeatPattern p;
    if (slot < 0 || slot >= kNumSlots) return p;
    const auto& sl = slots[(size_t) slot];
    p.stepsPerBar = sl.stepsPerBar.load();
    p.bars = sl.bars.load();
    for (int r = 0; r < BeatPattern::kRows; ++r)
        for (int st = 0; st < BeatPattern::kMaxSteps; ++st)
            p.cells[(size_t) r][(size_t) st] = sl.cells[(size_t) r][(size_t) st].load (std::memory_order_relaxed);
    return p;
}

void BeatPatternBank::setCell (int slot, int row, int step, int velocity) noexcept
{
    if (slot < 0 || slot >= kNumSlots || row < 0 || row >= BeatPattern::kRows || step < 0 || step >= BeatPattern::kMaxSteps) return;
    slots[(size_t) slot].cells[(size_t) row][(size_t) step].store ((uint8_t) std::clamp (velocity, 0, 127), std::memory_order_relaxed);
    version.fetch_add (1);
}

int BeatPatternBank::getCell (int slot, int row, int step) const noexcept
{
    if (slot < 0 || slot >= kNumSlots || row < 0 || row >= BeatPattern::kRows || step < 0 || step >= BeatPattern::kMaxSteps) return 0;
    return slots[(size_t) slot].cells[(size_t) row][(size_t) step].load (std::memory_order_relaxed);
}

int BeatPatternBank::getStepsPerBar (int slot) const noexcept { return slot >= 0 && slot < kNumSlots ? slots[(size_t) slot].stepsPerBar.load() : 16; }
int BeatPatternBank::getBars (int slot) const noexcept        { return slot >= 0 && slot < kNumSlots ? slots[(size_t) slot].bars.load() : 1; }

bool BeatPatternBank::isEmpty (int slot) const noexcept
{
    if (slot < 0 || slot >= kNumSlots) return true;
    for (auto& r : slots[(size_t) slot].cells)
        for (auto& c : r)
            if (c.load (std::memory_order_relaxed) != 0) return false;
    return true;
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
    for (auto& p : players) p.active = false;
    kickFollow = 0.0f;
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

void RhythmGenerator::addEvent (double delaySamples, Voice v, float velocity, uint8_t variant, int step, bool allowEarly) noexcept
{
    if (numEvents >= (int) events.size())
        return;
    if (v != Click && voiceMuted[(size_t) v])
        return;                                   // v1.6: row muted / not soloed
    if (v != Click)
    {
        // groove: per-instrument micro-timing and the accent curve of the template
        delaySamples += grooveOffsetMs (groove, (int) v, step) * 0.001 * sampleRate;
        velocity *= grooveAccent (groove, step);
        // humanize: random timing early / late (the kick stays tighter) and velocity
        const float h = std::clamp (humanize, 0.0f, 1.0f);
        const double spread = (v == Kick ? 0.0025 : 0.007) * h * sampleRate;
        delaySamples += (random01() * 2.0f - 1.0f) * spread;
        velocity = std::clamp (velocity * (1.0f + (random01() - 0.5f) * 0.32f * h), 0.05f, 1.0f);
    }
    if (! allowEarly || delaySamples < 0.0)
        delaySamples = std::max (0.0, delaySamples);
    events[(size_t) numEvents++] = { delaySamples, v, velocity, variant };
}

void RhythmGenerator::scheduleFill (int style, int f, double base, int s, bool allowEarly) noexcept
{
    // one beat of fill, f = 16th inside the beat (0 .. 3)
    auto add = [&] (double d, Voice v, float vel) { addEvent (d, v, vel, 0, s, allowEarly); };
    const float rise = 0.62f + 0.12f * (float) f;
    const double half = samplesPerStep * 0.5;
    switch (style)
    {
        case FillSnareRoll:
            if (f == 0) add (base, Kick, 0.9f);
            add (base, Snare, rise);
            add (base + half, Snare, rise * 0.75f);
            break;
        case FillKickRoll:
            add (base, Kick, 0.6f + 0.12f * (float) f);
            if (f >= 2) add (base + half, Rim, 0.5f + 0.1f * (float) f);
            if (f == 3) add (base + half, Clap, 0.8f);
            break;
        case FillHatRoll:
            if (f == 0) add (base, Kick, 0.95f);
            for (int i = 0; i < (f == 3 ? 4 : 3); ++i)
                add (base + samplesPerStep * i / (f == 3 ? 4.0 : 3.0), HatClosed, rise * (0.7f + 0.1f * (float) i));
            if (f == 2) add (base, Snare, 0.7f);
            break;
        case FillBreaks:
        {
            static constexpr Voice a[4] = { Kick, Snare, Snare, Snare };
            add (base, a[f], f == 0 ? 0.95f : rise);
            if (f >= 2) add (base + half, Snare, rise * 0.7f);
            if (f == 3) add (base, Tom, 0.8f);
            break;
        }
        case FillConga:
            if (f == 0) add (base, Kick, 0.85f);
            add (base, f == 2 ? Rim : Tom, rise);
            if (f != 2) add (base + half, Tom, rise * 0.7f);
            break;
        case FillClapRoll:
            if (f == 0) add (base, Kick, 0.9f);
            add (base, Clap, rise);
            add (base + half, Clap, rise * 0.7f);
            if (f == 3) add (base, HatOpen, 0.8f);
            break;
        case FillDisco:
            if (f == 0) add (base, Kick, 0.9f);
            add (base, Snare, rise);
            if (f == 2) add (base, HatOpen, 0.85f);
            if (f == 3) add (base + half, Snare, rise * 0.8f);
            break;
        case FillToms:
        default:
            if (f == 0)
                add (base, Kick, 0.9f);
            add (base, f < 2 ? Tom : Snare, rise);
            add (base + half, f < 2 ? Tom : Snare, rise * 0.8f);   // 32nds
            break;
    }
}

void RhythmGenerator::scheduleUserStep (int slot, long long step, double base, bool allowEarly) noexcept
{
    if (userBank == nullptr)
        return;
    const int spb = userBank->getStepsPerBar (slot);
    const int bars = userBank->getBars (slot);
    const long long len16 = 16LL * bars;
    const int s16 = (int) (((step % len16) + len16) % len16);
    const int s = s16 % 16;
    const int sub = spb >= 32 ? 2 : 1;
    for (int k = 0; k < sub; ++k)
    {
        const int cell = s16 * sub + k;
        const double d = base + samplesPerStep * (double) k / (double) sub;
        for (int row = 0; row < BeatPattern::kRows; ++row)
            if (const int v = userBank->getCell (slot, row, cell); v > 0)
            {
                float vel = (float) v / 127.0f;
                if (row == Kick)
                    vel *= kickAccent (s, scheduleBar, variation);
                addEvent (d, (Voice) row, std::clamp (vel, 0.05f, 1.0f), 0, s, allowEarly);
                if (row == HatClosed || row == HatOpen)
                {
                    // the next open / closed hat chokes - nothing to do here, fireSample handles it
                }
            }
    }
}

void RhythmGenerator::scheduleStep (long long step, int beatsPerMeasure, double baseDelay, bool allowEarly) noexcept
{
    const long long stepsPerMeasure = (long long) std::max (1, beatsPerMeasure) * 4;
    const long long inBar = ((step % stepsPerMeasure) + stepsPerMeasure) % stepsPerMeasure;
    scheduleBar = (step - inBar) / stepsPerMeasure;

    // v1.6: quantised pattern switching (immediately / next beat / next bar)
    if (requestedPattern != pattern)
    {
        const bool boundary = switchQuantize == 0 || ! allowEarly
                           || (switchQuantize == 1 && ((step % 4) + 4) % 4 == 0)
                           || (switchQuantize == 2 && inBar == 0);
        if (boundary)
            pattern = requestedPattern;
    }
    activePattern.store (pattern, std::memory_order_relaxed);
    const bool user = pattern >= kNumPatterns && pattern < getNumPatternChoices();
    const int p = user ? 1 : std::clamp (pattern, 0, kNumPatterns - 1);

    if (p == 0)
    {
        if (((step % 4) + 4) % 4 != 0)
            return;
        const bool accent = clickAccent && inBar == 0;
        events[(size_t) std::min (numEvents, (int) events.size() - 1)] = { baseDelay, Click, accent ? 1.0f : 0.7f, (uint8_t) (accent ? 1 : 0) };
        numEvents = std::min (numEvents + 1, (int) events.size());
        return;
    }

    const auto& pat = kPatterns[p];
    const int s = (int) positiveModulo ((double) step, 16.0);
    const double base = baseDelay + swingOffset (s, swing) * samplesPerStep;
    auto add = [&] (double d, Voice v, float vel) { addEvent (d, v, vel, 0, s, allowEarly); };

    int style = user ? -1 : pat.fillStyle;
    if (style < 0)
    {
        const auto* kt = currentKit();
        style = kt != nullptr && kt->fillStyle >= 0 ? kt->fillStyle : kitFillStyle (kit);
    }

    // v1.6 fill on demand: plays while FILL is held, lands on the next downbeat after release
    if (fillPressed)
    {
        fillPressed = false;
        if (! fillActive)
            fillSteps = 0;
        fillActive = true;
    }
    if (fillActive)
    {
        if (! fillHeld && inBar == 0 && fillSteps > 0)
        {
            fillActive = false;
            fillPlaying.store (false, std::memory_order_relaxed);
            add (base, HatOpen, 1.0f);            // the landing: accented downbeat
            add (base, Kick, 1.0f);
            if (user)
                scheduleUserStep (pattern - kNumPatterns, step, base, allowEarly);
            else
            {
                if (const char sn = at (pat.snare, s); sn != '.')
                    add (base, lower (sn) == 'c' ? Clap : Snare, velocityOf (sn));
            }
            return;
        }
        fillPlaying.store (true, std::memory_order_relaxed);
        ++fillSteps;
        scheduleFill (style, (int) (inBar % 4), base, s, allowEarly);
        return;
    }
    fillPlaying.store (false, std::memory_order_relaxed);

    // v1.5 fills: the last beat of every 4th / 8th bar
    if (fills > 0)
    {
        const long long bar = scheduleBar;
        const int every = fills == 1 ? 4 : 8;
        if (inBar >= stepsPerMeasure - 4 && ((bar % every) + every) % every == every - 1)
        {
            scheduleFill (style, (int) (inBar - (stepsPerMeasure - 4)), base, s, allowEarly);
            return;
        }
    }

    if (user)
    {
        scheduleUserStep (pattern - kNumPatterns, step, base, allowEarly);
        return;
    }

    if (const char k = at (pat.kick, s); k != '.')
        add (base, Kick, velocityOf (k) * kickAccent (s, scheduleBar, variation));

    if (const char sn = at (pat.snare, s); sn != '.')
    {
        const char l = lower (sn);
        if (l == 'x' || l == 'g' || l == 'b')
            add (base, Snare, velocityOf (sn));
        if (l == 'c' || l == 'b')
            add (base, Clap, velocityOf (sn));
    }

    if (const char h = at (pat.hat, s); h != '.')
    {
        const char l = lower (h);
        const float v = velocityOf (h);
        if (l == 'o')
            add (base, HatOpen, v);
        else if (l == 'r')
        {
            add (base, HatClosed, v);
            add (base + samplesPerStep * 0.5, HatClosed, v * 0.7f);
        }
        else if (l == 't')
        {
            for (int i = 0; i < 3; ++i)
                add (base + samplesPerStep * i / 3.0, HatClosed, v * (1.0f - 0.18f * (float) i));
        }
        else
            add (base, HatClosed, v);
    }

    if (const char pc = at (pat.perc, s); pc != '.')
    {
        const char l = lower (pc);
        const float v = velocityOf (pc);
        if (l == 'r')      add (base, Rim, v);
        else if (l == 's') add (base, Shaker, v);
        else if (l == 't') add (base, Tom, v);
    }
}

RhythmGenerator::Player* RhythmGenerator::startPlayer (const DrumBank::Sample& sample, Voice v, float gain, double inc,
                                                         double fadeAt, uint32_t age) noexcept
{
    Player* slot = nullptr;
    for (auto& p : players)
        if (! p.active) { slot = &p; break; }
    if (slot == nullptr)
    {
        slot = &players[0];
        for (auto& p : players)
            if (p.age < slot->age)
                slot = &p;
    }
    slot->sample = &sample;
    slot->pos = 0.0;
    slot->inc = inc;
    slot->gain = gain;
    slot->fade = 1.0f;
    slot->fadeStep = 0.0f;
    slot->fadeAt = fadeAt;
    slot->fadeLen = 0.03 * sampleRate;
    slot->voice = v;
    slot->room = (v == Snare || v == Clap || v == Rim) ? 1.0f : (v == HatClosed || v == HatOpen || v == Tom ? 0.3f : 0.0f);
    slot->active = true;
    slot->age = age;
    return slot;
}

bool RhythmGenerator::fireSample (Voice v, float vel) noexcept
{
    if (v == Click)
        return false;
    const auto* kitData = currentKit();
    if (kitData == nullptr)
        return false;
    const auto& list = kitData->voices[(size_t) v];
    if (list.empty())
        return false;
    const float var = variation;

    // v1.6 velocity layers: the nearest layer, sometimes (variation) the neighbouring one, so a run
    // of equal hits never repeats the same few samples
    float layers[16];
    int numLayers = 0;
    for (const auto& s : list)
    {
        bool known = false;
        for (int i = 0; i < numLayers; ++i)
            known = known || std::abs (layers[i] - s.layer) < 1.0e-4f;
        if (! known && numLayers < 16)
            layers[numLayers++] = s.layer;
    }
    std::sort (layers, layers + numLayers);
    int li = 0;
    for (int i = 1; i < numLayers; ++i)
        if (std::abs (layers[i] - vel) < std::abs (layers[li] - vel))
            li = i;
    if (numLayers > 1 && random01() < 0.35f * var)
    {
        const bool up = vel > layers[li] ? true : (vel < layers[li] ? false : random01() < 0.5f);
        li = std::clamp (li + (up ? 1 : -1), 0, numLayers - 1);
    }
    int candidates[32];
    int numCandidates = 0;
    for (int i = 0; i < (int) list.size() && numCandidates < 32; ++i)
        if (std::abs (list[(size_t) i].layer - layers[li]) < 1.0e-4f)
            candidates[numCandidates++] = i;
    if (numCandidates == 0)
        return false;

    // round robin: random among the layer's samples, never the one played last
    auto& rr = roundRobin[(size_t) v];
    int pick = (rr + 1) % numCandidates;
    if (numCandidates > 2)
    {
        pick = (int) (random01() * (float) numCandidates) % numCandidates;
        if (candidates[pick] == lastSampleIndex[(size_t) v])
            pick = (pick + 1) % numCandidates;
    }
    rr = pick;
    const int index = candidates[pick];
    const auto& sample = list[(size_t) index];
    lastSampleIndex[(size_t) v] = index;

    // choke: a closed hat stops a ringing open hat, a new open hat the previous one
    if (v == HatClosed || v == HatOpen)
        for (auto& p : players)
            if (p.active && p.voice == HatOpen)
                p.fadeStep = 1.0f / (float) (0.004 * sampleRate);

    // the oldest hit of the same voice is released when it already rings twice
    int same = 0;
    Player* oldest = nullptr;
    for (auto& p : players)
        if (p.active && p.voice == v && p.fadeStep == 0.0f)
        {
            if (oldest == nullptr || p.age < oldest->age)
                oldest = &p;
            ++same;
        }
    if (oldest != nullptr)
    {
        int hits = 0;
        uint32_t lastAge = 0;
        for (auto& p : players)
            if (p.active && p.voice == v && p.fadeStep == 0.0f && p.age != lastAge)
                { ++hits; lastAge = p.age; }
        if (hits >= 2 || same >= 4)
        {
            const auto age = oldest->age;
            for (auto& p : players)
                if (p.active && p.voice == v && p.age == age)
                    p.fadeStep = 1.0f / (float) (0.008 * sampleRate);
        }
    }

    // v1.6 humanised sound: pitch (harder = a touch higher), decay and level vary per hit
    const float h = std::clamp (humanize, 0.0f, 1.0f);
    const bool tonal = v == Kick || v == Snare || v == Tom;
    const float pitchRange = v == Kick ? 14.0f : v == Snare ? 10.0f : (v == HatClosed || v == HatOpen) ? 18.0f : 8.0f;
    double cents = (random01() * 2.0f - 1.0f) * (7.0f * h + pitchRange * var);
    if (tonal)
        cents += (vel - 0.7f) * 14.0f * var;
    cents += voicePitch[(size_t) v] * 100.0;
    const double inc = sample.sampleRate / sampleRate * std::pow (2.0, cents / 1200.0);
    const float layerGain = std::clamp (vel / std::max (0.1f, sample.layer), 0.45f, 1.35f);
    const float gain = kitData->trim[(size_t) v] * layerGain * voiceGain[(size_t) v]
                     * (1.0f + (random01() - 0.5f) * (0.1f * h + 0.12f * var));
    const double len = (double) sample.data.size();
    double fadeAt = 1.0e18;
    if (v == Kick || v == Snare || v == Tom || v == HatOpen)
        fadeAt = len * (1.0 - 0.4 * var * random01()) * (0.75 + 0.25 * vel);   // softer = shorter

    const auto age = ++playerAge;
    // v1.6 blend: a second take from the same layer underneath, in a random amount
    const int partnerPick = numCandidates > 1 ? (pick + 1 + (int) (random01() * (float) (numCandidates - 1))) % numCandidates : -1;
    const bool blend = var > 0.05f && partnerPick >= 0 && candidates[partnerPick] != index
                    && (v == Kick || v == Snare || v == HatClosed || v == Clap);
    const float b = blend ? (0.15f + 0.3f * random01()) * var : 0.0f;
    startPlayer (sample, v, gain * (1.0f - b), inc, fadeAt, age);
    if (blend)
        startPlayer (list[(size_t) candidates[partnerPick]], v, gain * b, inc, fadeAt, age);
    return true;
}

void RhythmGenerator::fire (Voice v, float vel, uint8_t variant) noexcept
{
    if (v != Click)
    {
        lastVelocity[(size_t) v] = vel;
        ++hitCount[(size_t) v];
    }
    if (fireSample (v, vel))
        return;
    if (v != Click)
        vel *= std::min (1.5f, voiceGain[(size_t) v]);
    const auto& k = kKits[kit >= 0 && kit < kNumKits ? kit : 0];
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
        fillActive = fillPressed = false;
        fillPlaying.store (false, std::memory_order_relaxed);
        currentStep.store (-1, std::memory_order_relaxed);
        pattern = requestedPattern;
        activePattern.store (pattern, std::memory_order_relaxed);
    }
    if (ppqPerSample > 0.0)
        samplesPerStep = 0.25 / ppqPerSample;

    const auto& k = kKits[kit >= 0 && kit < kNumKits ? kit : 0];
    const bool lowPass = k.busLowPass > 0.0f;
    if (lowPass)
        busLowPass.set (sampleRate, isUsingSamples() ? 7800.0f : k.busLowPass, 0.6);
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
                // v1.5: steps are scheduled one 16th ahead, so groove / humanize can also play early
                if (lastStep < 0 || step != lastStep + 1)
                {
                    numEvents = 0;
                    pattern = requestedPattern;       // v1.6: a jump / start switches at once
                    if (std::abs (ppq * 4.0 - (double) step) < ppqPerSample * 4.0 + 1.0e-9)
                        scheduleStep (step, beatsPerMeasure, 0.0, false);
                }
                const double toNext = ((double) (step + 1) * 0.25 - ppq) / ppqPerSample;
                scheduleStep (step + 1, beatsPerMeasure, toNext, true);
                lastStep = step;
                currentStep.store (step, std::memory_order_relaxed);
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

        // v1.5: recorded samples
        float kickNow = 0.0f;
        for (auto& p : players)
        {
            if (! p.active)
                continue;
            const auto& d = p.sample->data;
            const int i0 = (int) p.pos;
            if (i0 + 1 >= (int) d.size() || p.fade <= 0.0f)
            {
                p.active = false;
                continue;
            }
            const float frac = (float) (p.pos - i0);
            const float x = (d[(size_t) i0] + (d[(size_t) i0 + 1] - d[(size_t) i0]) * frac) * p.gain * p.fade;
            drums += x;
            roomSend += x * p.room;
            if (p.voice == Kick)
                kickNow = std::max (kickNow, std::abs (x));
            p.pos += p.inc;
            if (p.fadeStep == 0.0f && p.pos >= p.fadeAt)
                p.fadeStep = 1.0f / (float) std::max (1.0, p.fadeLen);
            if (p.fadeStep > 0.0f)
                p.fade -= p.fadeStep;
        }
        if (kickOut != nullptr && isUsingSamples())
        {
            kickFollow = std::max (kickNow * 1.4f, kickFollow * 0.9993f);
            kickOut[i] = std::min (1.0f, kickFollow);
        }

        // small room on snare / clap / rim / a bit of hat
        const auto* roomKit = currentKit();
        const float roomAmount = roomKit != nullptr ? roomKit->room : k.room;
        if (roomAmount > 0.0f && ! combBuf[0].empty())
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
            drums += wet * roomAmount * 2.0f;
        }

        if (lowPass)
        {
            float lp, hp;
            busLowPass.process (drums, lp, hp);
            drums = lp;
        }

        float out = isUsingSamples() ? softClip (drums * 0.9f) * 1.05f : softClip (drums * k.busDrive * 0.5f) * 1.25f;

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
