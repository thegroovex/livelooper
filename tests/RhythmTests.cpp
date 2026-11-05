// Tests of the 1.3 rhythm section: kits, swing, humanize, new patterns, metronome sounds.

#include <juce_core/juce_core.h>

#include "engine/RhythmGenerator.h"

#include <cmath>
#include <set>
#include <string>
#include <vector>

using namespace livelooper;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr double kPpqPerSample = 2.0 / kSr;          // 120 BPM
    constexpr int kStep = 6000;                          // samples per 16th at 120 BPM / 48 kHz

    std::vector<float> render (RhythmGenerator& r, int samples)
    {
        std::vector<float> l ((size_t) samples, 0.0f), rr ((size_t) samples, 0.0f);
        const int block = 256;
        for (int pos = 0; pos < samples; pos += block)
        {
            const int n = std::min (block, samples - pos);
            r.process (l.data() + pos, rr.data() + pos, n, pos * kPpqPerSample, kPpqPerSample, 4, true);
        }
        return l;
    }

    float peak (const std::vector<float>& v, int from, int to)
    {
        float p = 0.0f;
        for (int i = std::max (0, from); i < std::min ((int) v.size(), to); ++i)
            p = std::max (p, std::abs (v[(size_t) i]));
        return p;
    }

    /** First sample after 'from' where the high-passed signal jumps well above what was there before. */
    int onsetAfter (const std::vector<float>& v, int from)
    {
        float before = 0.0f;
        for (int i = from - 480; i < from; ++i)
            before = std::max (before, std::abs (v[(size_t) i] - v[(size_t) i - 1]));
        const float threshold = std::max (0.01f, before * 2.0f);
        for (int i = from; i < (int) v.size(); ++i)
            if (std::abs (v[(size_t) i] - v[(size_t) i - 1]) > threshold)
                return i;
        return -1;
    }
}

class RhythmTests : public juce::UnitTest
{
public:
    RhythmTests() : juce::UnitTest ("Rhythm", "livelooper") {}

    void runTest() override
    {
        beginTest ("Pattern list keeps the saved indices and adds the new grooves");
        {
            expectEquals (juce::String (RhythmGenerator::getPatternName (0)), juce::String ("Metronome"));
            expectEquals (juce::String (RhythmGenerator::getPatternName (1)), juce::String ("Rock"));
            expectEquals (juce::String (RhythmGenerator::getPatternName (11)), juce::String ("Half Time"));
            expectGreaterOrEqual (RhythmGenerator::getNumPatterns(), 20);
            std::set<std::string> names;
            for (int p = 0; p < RhythmGenerator::getNumPatterns(); ++p)
                names.insert (RhythmGenerator::getPatternName (p));
            expectEquals ((int) names.size(), RhythmGenerator::getNumPatterns(), "pattern names are unique");
            expectEquals (RhythmGenerator::getNumKits(), 12);    // v1.6: + 8 style kits
            expectEquals (juce::String (RhythmGenerator::getKitName (3)), juce::String ("Lo-Fi"));
        }

        beginTest ("Swing offset: straight, triplet, hard");
        {
            expectEquals (RhythmGenerator::swingOffset (0, 0.75f), 0.0);
            expectEquals (RhythmGenerator::swingOffset (1, 0.5f), 0.0);
            expectWithinAbsoluteError (RhythmGenerator::swingOffset (1, 0.6667f), 1.0 / 3.0, 1.0e-3);
            expectWithinAbsoluteError (RhythmGenerator::swingOffset (3, 0.75f), 0.5, 1.0e-9);
            expectEquals (RhythmGenerator::swingOffset (2, 0.75f), 0.0);
        }

        beginTest ("Swing delays the off-beat 16th notes, humanize 0 stays on the grid");
        {
            for (float sw : { 0.5f, 0.75f })
            {
                RhythmGenerator r;
                r.prepare (kSr);
                r.setPattern (3);          // Funk: 16th hats, step 1 has only a hat
                r.setLevel (1.0f);
                r.setHumanize (0.0f);
                r.setGroove (0);
                r.setSwing (sw);
                const auto out = render (r, kStep * 4);
                const int on = onsetAfter (out, kStep);
                expect (on > 0, "hat onset found");
                const double late = (double) (on - kStep) / kStep;
                if (sw == 0.5f)
                    expectLessThan (late, 0.05, "straight: on the grid");
                else
                    expectWithinAbsoluteError (late, 0.5, 0.05, "swing 75 %: half a 16th late");
            }
        }

        beginTest ("Humanize moves hits only a few milliseconds, early or late (v1.5)");
        {
            for (int seed = 0; seed < 6; ++seed)
            {
                RhythmGenerator r;
                r.prepare (kSr);
                r.setPattern (3);
                r.setLevel (1.0f);
                r.setHumanize (1.0f);
                r.setGroove (0);
                r.setSwing (0.5f);
                for (int i = 0; i < seed; ++i)
                    render (r, 64);           // shifts the random sequence
                RhythmGenerator r2;
                r2.prepare (kSr);
                r2.setPattern (3);
                r2.setLevel (1.0f);
                r2.setHumanize (1.0f);
                r2.setGroove (0);
                const auto out = render (r2, kStep * 4);
                const int on = onsetAfter (out, kStep - (int) (0.008 * kSr));
                expect (on > 0);
                expectLessThan (std::abs (on - kStep), (int) (0.0085 * kSr), "within about 7 ms of the grid");
            }
        }

        beginTest ("Groove templates: hats laid back, snare late, pushed is early, accents shape the bar");
        {
            const int laid = 2, pushed = 3;
            expectEquals (juce::String (RhythmGenerator::getGrooveName (0)), juce::String ("Straight"));
            expectGreaterOrEqual (RhythmGenerator::getNumGrooves(), 6);
            expectGreaterThan (RhythmGenerator::grooveOffsetMs (laid, RhythmGenerator::HatClosed, 2), 5.0);
            expectGreaterThan (RhythmGenerator::grooveOffsetMs (laid, RhythmGenerator::Snare, 4), 5.0);
            expectEquals (RhythmGenerator::grooveOffsetMs (0, RhythmGenerator::Snare, 4), 0.0);
            expectLessThan (RhythmGenerator::grooveOffsetMs (pushed, RhythmGenerator::HatClosed, 2), 0.0);
            expectGreaterThan (RhythmGenerator::grooveAccent (1, 0), RhythmGenerator::grooveAccent (1, 1));

            // measured: Funk hat on step 2 (8th off-beat) comes later with "Laid Back" than with "Straight"
            int onsets[2] {};
            for (int g = 0; g < 2; ++g)
            {
                RhythmGenerator r;
                r.prepare (kSr);
                r.setPattern (3);
                r.setLevel (1.0f);
                r.setHumanize (0.0f);
                r.setGroove (g == 0 ? 0 : laid);
                const auto out = render (r, kStep * 4);
                onsets[g] = onsetAfter (out, 2 * kStep - 200);     // step 2: only a hat
            }
            const double ms = (onsets[1] - onsets[0]) / kSr * 1000.0;
            expectWithinAbsoluteError (ms, RhythmGenerator::grooveOffsetMs (laid, RhythmGenerator::HatClosed, 2), 1.0);
        }

        beginTest ("Fills every 4 bars add hits in the last beat of bar 4 only");
        {
            auto lastBeatEnergy = [this] (int fillsMode, int bar) {
                RhythmGenerator r;
                r.prepare (kSr);
                r.setPattern (11);           // Half Time: last beat is sparse
                r.setLevel (1.0f);
                r.setHumanize (0.0f);
                r.setGroove (0);
                r.setFills (fillsMode);
                const auto out = render (r, kStep * 16 * 4 + kStep);
                float e = 0.0f;
                for (int i = kStep * (16 * bar + 12); i < kStep * (16 * bar + 16); ++i)
                    e += out[(size_t) i] * out[(size_t) i];
                return e;
            };
            expectGreaterThan (lastBeatEnergy (1, 3), lastBeatEnergy (0, 3) * 1.5f + 1.0f, "fill in bar 4");
            expectWithinAbsoluteError (lastBeatEnergy (1, 1), lastBeatEnergy (0, 1), 1.0e-3f, "no fill in bar 2");
        }

        beginTest ("Every kit plays every pattern within range, and the kits sound different");
        {
            std::vector<std::vector<float>> rock;
            for (int k = 0; k < RhythmGenerator::getNumKits(); ++k)
            {
                for (int p = 1; p < RhythmGenerator::getNumPatterns(); ++p)
                {
                    RhythmGenerator r;
                    r.prepare (kSr);
                    r.setPattern (p);
                    r.setKit (k);
                    r.setLevel (1.0f);
                    r.setSwing (0.62f);
                    const auto out = render (r, kStep * 16);    // one bar
                    const float pk = peak (out, 0, (int) out.size());
                    const auto what = juce::String (RhythmGenerator::getKitName (k)) + " / " + RhythmGenerator::getPatternName (p);
                    expectGreaterThan (pk, 0.05f, what);
                    expectLessThan (pk, 1.0f, what);
                    if (p == 1)
                        rock.push_back (out);
                }
            }
            for (size_t a = 1; a < rock.size(); ++a)
            {
                float diff = 0.0f;
                for (size_t i = 0; i < rock[0].size(); ++i)
                    diff = std::max (diff, std::abs (rock[0][i] - rock[a][i]));
                expectGreaterThan (diff, 0.05f, "kit differs from Studio");
            }
        }

        beginTest ("Metronome: all four sounds are audible, woodblock and click do not ring");
        {
            for (int s = 0; s < RhythmGenerator::kNumClickSounds; ++s)
            {
                RhythmGenerator r;
                r.prepare (kSr);
                r.setPattern (0);
                r.setLevel (1.0f);
                r.setClickSound (s);
                const auto out = render (r, kStep * 4);
                const float pk = peak (out, 0, 2400);
                expectGreaterThan (pk, 0.05f, "click sound " + juce::String (s));
                expectLessThan (pk, 1.0f);
                if (s == 1 || s == 2)
                    expectLessThan (peak (out, (int) (0.08 * kSr), (int) (0.2 * kSr)), pk * 0.02f,
                                    "no ringing tail, sound " + juce::String (s));
            }
        }

        beginTest ("The metronome is never swung");
        {
            RhythmGenerator r;
            r.prepare (kSr);
            r.setPattern (0);
            r.setLevel (1.0f);
            r.setSwing (0.75f);
            r.setHumanize (1.0f);
            const auto out = render (r, kStep * 8);
            const int on = onsetAfter (out, kStep * 4);
            expect (on >= kStep * 4 && on < kStep * 4 + 10, "beat 2 exactly on time");
        }
    }
};

static RhythmTests rhythmTests;
