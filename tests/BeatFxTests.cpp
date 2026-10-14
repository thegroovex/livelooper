// Tests of the v1.5 beat FX, colour FX and beat repeat / stutter (engine level).

#include "engine/BeatFx.h"

using namespace livelooper;

class BeatFxTests : public juce::UnitTest
{
public:
    BeatFxTests() : juce::UnitTest ("Beat FX", "engine") {}

    static constexpr double rate = 48000.0;
    static constexpr double ppqPerSample = 120.0 / (60.0 * rate);   // 120 BPM: one beat = 24000 samples

    template <typename Fn>
    static void run (std::vector<float>& l, std::vector<float>& r, int block, Fn&& fn)
    {
        const int n = (int) l.size();
        for (int start = 0; start < n; start += block)
            fn (l.data() + start, r.data() + start, std::min (block, n - start), start);
    }

    void runTest() override
    {
        beginTest ("Every beat FX type is finite and bounded at all depths and fractions");
        for (int t = 0; t < (int) BeatFxType::NumTypes; ++t)
            for (float depth : { 0.0f, 0.5f, 1.0f })
                for (int f : { 0, 3, 6, 10 })
                {
                    BeatFx fx;
                    fx.prepare (rate);
                    fx.setSettings ({ true, (BeatFxType) t, f, depth });
                    juce::Random rnd (7);
                    std::vector<float> l (48000), r (48000);
                    for (size_t i = 0; i < l.size(); ++i)
                        l[i] = r[i] = rnd.nextFloat() * 1.6f - 0.8f;
                    run (l, r, 512, [&] (float* a, float* b, int m, int s) { fx.process (a, b, m, s * ppqPerSample, ppqPerSample); });
                    bool ok = true;
                    float peak = 0.0f;
                    for (size_t i = 0; i < l.size(); ++i)
                    {
                        ok = ok && std::isfinite (l[i]) && std::isfinite (r[i]);
                        peak = std::max (peak, std::max (std::abs (l[i]), std::abs (r[i])));
                    }
                    expect (ok, juce::String (getBeatFxName ((BeatFxType) t)) + " finite");
                    expectLessThan (peak, 6.0f, juce::String (getBeatFxName ((BeatFxType) t)) + " bounded");
                }

        beginTest ("Beat FX off is bit-exact, colour FX centred is bit-exact");
        {
            BeatFx fx;
            fx.prepare (rate);
            fx.setSettings ({ false, BeatFxType::Echo, 6, 1.0f });
            ColorFx cfx;
            cfx.prepare (rate);
            juce::Random rnd (3);
            std::vector<float> l (8192), r (8192), l0, r0;
            for (size_t i = 0; i < l.size(); ++i)
                l[i] = r[i] = rnd.nextFloat() - 0.5f;
            l0 = l; r0 = r;
            run (l, r, 256, [&] (float* a, float* b, int m, int s) {
                fx.process (a, b, m, s * ppqPerSample, ppqPerSample);
                cfx.process (a, b, m, ColorFxType::Filter, 0.0f, ppqPerSample);
            });
            expect (l == l0 && r == r0);
        }

        beginTest ("Delay repeats exactly one beat fraction later");
        {
            for (int f : { 3, 6, 7 })
            {
                BeatFx fx;
                fx.prepare (rate);
                fx.setSettings ({ true, BeatFxType::Delay, f, 1.0f });
                std::vector<float> l (120000, 0.0f), r (120000, 0.0f);
                // let the wet level settle, then an impulse
                const int at = 20000;
                l[(size_t) at] = r[(size_t) at] = 1.0f;
                run (l, r, 480, [&] (float* a, float* b, int m, int s) { fx.process (a, b, m, s * ppqPerSample, ppqPerSample); });
                const int expected = at + (int) std::llround (kBeatFractions[(size_t) f] * 24000.0);
                int peakAt = -1;
                float peak = 0.0f;
                for (int i = at + 10; i < (int) l.size(); ++i)
                    if (std::abs (l[(size_t) i]) > peak) { peak = std::abs (l[(size_t) i]); peakAt = i; }
                expectWithinAbsoluteError (peakAt, expected, 2, "fraction " + juce::String (f));
            }
        }

        beginTest ("Colour FX: every type stays finite, filter LPF removes highs");
        for (int t = 0; t < (int) ColorFxType::NumTypes; ++t)
            for (float a : { -1.0f, -0.5f, 0.5f, 1.0f })
            {
                ColorFx c;
                c.prepare (rate);
                juce::Random rnd (11);
                std::vector<float> l (24000), r (24000);
                for (size_t i = 0; i < l.size(); ++i)
                    l[i] = r[i] = rnd.nextFloat() * 1.6f - 0.8f;
                run (l, r, 512, [&] (float* x, float* y, int m, int) { c.process (x, y, m, (ColorFxType) t, a, ppqPerSample); });
                bool ok = true;
                for (auto v : l)
                    ok = ok && std::isfinite (v) && std::abs (v) < 8.0f;
                expect (ok, juce::String (getColorFxName ((ColorFxType) t)) + " " + juce::String (a));
            }
        {
            ColorFx c;
            c.prepare (rate);
            std::vector<float> l (24000), r (24000);
            for (size_t i = 0; i < l.size(); ++i)
                l[i] = r[i] = (i & 1) ? 0.5f : -0.5f;      // Nyquist tone
            run (l, r, 512, [&] (float* x, float* y, int m, int) { c.process (x, y, m, ColorFxType::Filter, -0.9f, ppqPerSample); });
            float peak = 0.0f;
            for (size_t i = 12000; i < l.size(); ++i)
                peak = std::max (peak, std::abs (l[i]));
            expectLessThan (peak, 0.05f);
        }

        beginTest ("Beat repeat starts on the grid line and repeats exactly one timing");
        for (int rIdx = 0; rIdx < (int) kRepeatPpq.size(); ++rIdx)
        {
            BeatRepeat br;
            br.prepare (rate);
            const int len = (int) std::llround (kRepeatPpq[(size_t) rIdx] * 24000.0);
            const int n = 6 * 24000 + 4 * len;
            std::vector<float> l ((size_t) n), r ((size_t) n), in;
            for (int i = 0; i < n; ++i)
                l[(size_t) i] = r[(size_t) i] = (float) ((i % 9973) / 9973.0) - 0.5f;   // slow ramp: position is readable
            in = l;
            // pressed at sample 1000 (+ a timeline offset of 1/3 sample-free ppq)
            const int press = 1000;
            run (l, r, 64, [&] (float* x, float* y, int m, int s) {
                br.set (s + m > press, rIdx, false);
                br.process (x, y, m, s * ppqPerSample, ppqPerSample);
            });
            // the first grid line after the press
            const int grid = ((press / len) + 1) * len;
            expectEquals ((int) br.getStartSample(), grid, "start on the grid, timing " + juce::String (rIdx));
            expectWithinAbsoluteError ((int) std::llround (br.getSliceLength()), len, 1);
            // second pass: same as the first pass (middle of the slice, away from the seams)
            for (int rep = 1; rep < 3; ++rep)
            {
                const int k = grid + rep * len + len / 2;
                if (k < n)
                    expectWithinAbsoluteError (l[(size_t) k], in[(size_t) (grid + len / 2)], 1.0e-5f);
            }
        }

        beginTest ("Stutter chops every repeat, release returns to the live signal");
        {
            BeatRepeat br;
            br.prepare (rate);
            const int len = 6000;   // 1/16 at 120 BPM
            const int n = 48000;
            std::vector<float> l ((size_t) n, 0.5f), r ((size_t) n, 0.5f);
            const int release = 30000;
            run (l, r, 64, [&] (float* x, float* y, int m, int s) {
                br.set (s < release, 3, true);
                br.process (x, y, m, s * ppqPerSample, ppqPerSample);
            });
            expectWithinAbsoluteError (l[(size_t) (len + len / 4)], 0.5f, 1.0e-3f, "open half");
            expectWithinAbsoluteError (l[(size_t) (len + 3 * len / 4)], 0.0f, 1.0e-3f, "closed half");
            expectWithinAbsoluteError (l[(size_t) (release + 2000)], 0.5f, 1.0e-3f, "live again");
        }

        beginTest ("Roll repeats the fraction from the next grid line");
        {
            BeatFx fx;
            fx.prepare (rate);
            const int n = 4 * 24000;
            std::vector<float> l ((size_t) n), r ((size_t) n), in;
            for (int i = 0; i < n; ++i)
                l[(size_t) i] = r[(size_t) i] = (float) ((i % 7919) / 7919.0) - 0.5f;
            in = l;
            fx.setSettings ({ true, BeatFxType::Roll, 4, 1.0f });   // 1/2 beat = 12000 samples
            run (l, r, 128, [&] (float* a, float* b, int m, int s) { fx.process (a, b, m, s * ppqPerSample, ppqPerSample); });
            // pressed at 0: starts on the first grid line (0 or 12000), the slice 0 .. 12000 repeats: 30000 plays 6000
            expectWithinAbsoluteError (l[(size_t) 30000], in[(size_t) 6000], 1.0e-4f);
        }
    }
};

static BeatFxTests beatFxTests;
