// Tests of the effect slots: every type stays stable, bypass is bit-exact, filters
// filter and the delay is tempo synced.

#include "engine/Effects.h"

using namespace livelooper;

class EffectsTests : public juce::UnitTest
{
public:
    EffectsTests() : juce::UnitTest ("Effects", "engine") {}

    static float rms (const std::vector<float>& v, size_t from = 0)
    {
        double s = 0.0;
        for (size_t i = from; i < v.size(); ++i)
            s += (double) v[i] * v[i];
        return (float) std::sqrt (s / (double) std::max<size_t> (1, v.size() - from));
    }

    void runTest() override
    {
        constexpr double rate = 48000.0;
        constexpr int n = 48000;
        const double ppqPerSample = 120.0 / (60.0 * rate);

        beginTest ("Every effect type produces finite, bounded output");
        for (int t = 0; t < (int) FxType::NumTypes; ++t)
        {
            for (float p : { 0.0f, 0.5f, 1.0f })
            {
                FxUnit fx;
                fx.prepare (rate);
                FxSettings s;
                s.enabled = true;
                s.type = (FxType) t;
                s.p1 = p;
                s.p2 = 1.0f - p;
                s.mix = 1.0f;
                fx.setSettings (s);

                juce::Random rnd (42);
                std::vector<float> l ((size_t) n), r ((size_t) n);
                for (int i = 0; i < n; ++i)
                    l[(size_t) i] = r[(size_t) i] = rnd.nextFloat() * 1.6f - 0.8f;

                for (int start = 0; start < n; start += 512)
                {
                    const int len = std::min (512, n - start);
                    fx.process (l.data() + start, r.data() + start, len, start * ppqPerSample, ppqPerSample, 120.0);
                }

                bool finite = true;
                float peak = 0.0f;
                for (int i = 0; i < n; ++i)
                {
                    finite = finite && std::isfinite (l[(size_t) i]) && std::isfinite (r[(size_t) i]);
                    peak = std::max (peak, std::max (std::abs (l[(size_t) i]), std::abs (r[(size_t) i])));
                }
                expect (finite, juce::String (getFxTypeName ((FxType) t)) + " output is finite");
                expectLessThan (peak, 8.0f, juce::String (getFxTypeName ((FxType) t)) + " output is bounded");
            }
        }

        beginTest ("A disabled slot is bit-exact bypass");
        {
            FxUnit fx;
            fx.prepare (rate);
            FxSettings s;
            s.enabled = false;
            s.type = FxType::Reverb;
            fx.setSettings (s);
            std::vector<float> l (512), r (512);
            for (int i = 0; i < 512; ++i) l[(size_t) i] = r[(size_t) i] = (float) std::sin (i * 0.1);
            auto refL = l;
            fx.process (l.data(), r.data(), 512, 0.0, ppqPerSample, 120.0);
            expect (l == refL);
        }

        beginTest ("Low pass attenuates high frequencies, high pass attenuates lows");
        {
            auto run = [&] (FxType type, float cutoff, double freq)
            {
                FxUnit fx;
                fx.prepare (rate);
                FxSettings s;
                s.enabled = true; s.type = type; s.p1 = cutoff; s.p2 = 0.0f; s.mix = 1.0f;
                fx.setSettings (s);
                std::vector<float> l ((size_t) n), r ((size_t) n);
                for (int i = 0; i < n; ++i)
                    l[(size_t) i] = r[(size_t) i] = (float) std::sin (juce::MathConstants<double>::twoPi * freq * i / rate);
                for (int start = 0; start < n; start += 512)
                    fx.process (l.data() + start, r.data() + start, std::min (512, n - start), 0.0, ppqPerSample, 120.0);
                return rms (l, 4800);
            };
            expectLessThan (run (FxType::LowPass, 0.3f, 8000.0), 0.05f);
            expectGreaterThan (run (FxType::LowPass, 0.3f, 50.0), 0.6f);
            expectLessThan (run (FxType::HighPass, 0.7f, 60.0), 0.05f);
        }

        beginTest ("Delay echoes after the tempo-synced time (1/8 at 120 BPM = 12000 samples)");
        {
            FxUnit fx;
            fx.prepare (rate);
            FxSettings s;
            s.enabled = true; s.type = FxType::Delay; s.p1 = 0.0f; s.p2 = 0.2f; s.mix = 1.0f;
            fx.setSettings (s);
            std::vector<float> l ((size_t) n, 0.0f), r ((size_t) n, 0.0f);
            l[100] = r[100] = 1.0f;
            for (int start = 0; start < n; start += 512)
                fx.process (l.data() + start, r.data() + start, std::min (512, n - start), 0.0, ppqPerSample, 120.0);
            expectWithinAbsoluteError (l[12100], 1.0f, 1.0e-4f);
            expectWithinAbsoluteError (l[12099], 0.0f, 1.0e-4f);
        }

        beginTest ("Slicer gates on 16th notes of the timeline");
        {
            FxUnit fx;
            fx.prepare (rate);
            FxSettings s;
            s.enabled = true; s.type = FxType::Slicer; s.p1 = 1.0f; s.p2 = 0.0f; s.mix = 1.0f; // straight 8ths pattern
            fx.setSettings (s);
            std::vector<float> l ((size_t) n, 1.0f), r ((size_t) n, 1.0f);
            fx.process (l.data(), r.data(), n, 0.0, ppqPerSample, 120.0);
            // 16th = 6000 samples. Step 0 open, step 1 closed (pattern bit0 = 1, bit1 = 0).
            expectGreaterThan (l[3000], 0.9f);
            expectLessThan (l[9000], 0.1f);
        }
    }
};

static EffectsTests effectsTests;
