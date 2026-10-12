// Tests of the 1.2 features: cue output for the rhythm, metronome sounds, track / master
// meters, the waveform overview and looper sets.

#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

using namespace livelooper;
using Cmd = LooperEngine::Command;

namespace
{
constexpr double kRate = 48000.0;
constexpr int kBlock = 480;

struct EngineRig
{
    LooperEngine engine;
    GlobalSettings global;
    TimeInfo time;
    std::vector<float> inL, inR, outL, outR, cueL, cueR;
    float input = 0.0f;

    EngineRig()
    {
        engine.prepare (kRate, kBlock, 10.0);
        global.monitorLevel = 0.0f;
        global.syncToHost = false;
        global.quantize = Quantize::Off;
        global.recAction = RecAction::RecPlay;
        for (auto* v : { &inL, &inR, &outL, &outR, &cueL, &cueR })
            v->assign ((size_t) kBlock, 0.0f);
    }

    /** Runs blocks and returns the peaks of main and cue. */
    std::pair<float, float> run (int blocks, bool withCue)
    {
        float pm = 0.0f, pc = 0.0f;
        for (int b = 0; b < blocks; ++b)
        {
            std::fill (inL.begin(), inL.end(), input);
            std::fill (inR.begin(), inR.end(), input);
            engine.setGlobalSettings (global);
            engine.process (inL.data(), inR.data(), outL.data(), outR.data(),
                            withCue ? cueL.data() : nullptr, withCue ? cueR.data() : nullptr, kBlock, time);
            for (int i = 0; i < kBlock; ++i)
            {
                pm = std::max (pm, std::abs (outL[(size_t) i]));
                if (withCue)
                    pc = std::max (pc, std::abs (cueL[(size_t) i]));
            }
        }
        return { pm, pc };
    }
};

float peakOf (const juce::AudioBuffer<float>& b, int ch)
{
    return b.getMagnitude (ch, 0, b.getNumSamples());
}
} // namespace

class FeatureTests : public juce::UnitTest
{
public:
    FeatureTests() : juce::UnitTest ("Cue, meters, waveform, sets", "livelooper") {}

    void runTest() override
    {
        beginTest ("Rhythm goes to the cue output, the main output, or both");
        {
            EngineRig r;
            r.engine.setRhythm (true, 0, 1.0f);
            r.global.clickRoute = ClickRoute::Cue;
            auto [m1, c1] = r.run (1, true);
            expectEquals (m1, 0.0f, "cue only: main output silent");
            expectGreaterThan (c1, 0.05f, "cue only: click on the cue output");

            EngineRig r2;
            r2.engine.setRhythm (true, 0, 1.0f);
            r2.global.clickRoute = ClickRoute::Main;
            auto [m2, c2] = r2.run (1, true);
            expectGreaterThan (m2, 0.05f);
            expectEquals (c2, 0.0f);

            EngineRig r3;
            r3.engine.setRhythm (true, 0, 1.0f);
            r3.global.clickRoute = ClickRoute::MainAndCue;
            auto [m3, c3] = r3.run (1, true);
            expectGreaterThan (m3, 0.05f);
            expectWithinAbsoluteError (c3, m3, 1.0e-4f);

            EngineRig r4;                      // no cue output connected: never lose the click
            r4.engine.setRhythm (true, 0, 1.0f);
            r4.global.clickRoute = ClickRoute::Cue;
            auto [m4, c4] = r4.run (1, false);
            juce::ignoreUnused (c4);
            expectGreaterThan (m4, 0.05f);
        }

        beginTest ("Both metronome sounds are audible and sound different");
        {
            std::vector<float> beep, wood;
            for (int sound = 0; sound < RhythmGenerator::kNumClickSounds; ++sound)
            {
                EngineRig r;
                r.engine.setRhythm (true, 0, 1.0f);
                r.engine.setClickSound (sound);
                r.run (1, false);
                (sound == 0 ? beep : wood) = r.outL;
            }
            float diff = 0.0f, pw = 0.0f;
            for (size_t i = 0; i < beep.size(); ++i)
            {
                diff = std::max (diff, std::abs (beep[i] - wood[i]));
                pw = std::max (pw, std::abs (wood[i]));
            }
            expectGreaterThan (pw, 0.05f, "woodblock audible");
            expectGreaterThan (diff, 0.05f, "woodblock differs from the beep");
        }

        beginTest ("Every drum pattern plays and stays within range");
        {
            for (int p = 1; p < RhythmGenerator::getNumPatterns(); ++p)
            {
                EngineRig r;
                r.engine.setRhythm (true, p, 1.0f);
                auto [m, c] = r.run (200, false);       // two seconds = one bar at 120 BPM
                juce::ignoreUnused (c);
                expectGreaterThan (m, 0.05f, RhythmGenerator::getPatternName (p));
                expectLessThan (m, 1.0f, RhythmGenerator::getPatternName (p));
            }
        }

        beginTest ("Track meter shows the input while recording and the output while playing");
        {
            EngineRig r;
            r.input = 0.5f;
            r.engine.queueCommand (Cmd::RecPlay, 0);
            r.run (20, false);
            auto& st = r.engine.getTrackStatus (0);
            expect (st.meterIsInput.load());
            expectWithinAbsoluteError (st.meterL.exchange (0.0f), 0.5f, 0.01f);

            r.engine.queueCommand (Cmd::RecPlay, 0);    // rec > play
            r.input = 0.0f;
            r.run (20, false);
            expect (! st.meterIsInput.load());
            expectGreaterThan (st.meterL.exchange (0.0f), 0.3f, "loop playback level");

            float ml, mr;
            r.engine.takeMasterMeter (ml, mr);
            expectGreaterThan (ml, 0.3f, "master meter");
            r.engine.takeMasterMeter (ml, mr);
            expectEquals (ml, 0.0f, "master meter resets when read");
        }

        beginTest ("Waveform overview follows the recording and the overdubs");
        {
            EngineRig r;
            std::array<float, 64> bins {};
            expect (! r.engine.getTrack (0).getWaveform (bins.data(), 64), "empty track has no waveform");

            r.input = 0.25f;
            r.engine.queueCommand (Cmd::RecPlay, 0);
            r.run (100, false);
            expect (r.engine.getTrack (0).getWaveform (bins.data(), 64), "waveform while recording");
            expectWithinAbsoluteError (bins[10], 0.25f, 0.01f);

            r.engine.queueCommand (Cmd::RecPlay, 0);    // rec > play
            r.input = 0.0f;
            r.run (5, false);
            expect (r.engine.getTrack (0).getWaveform (bins.data(), 64));
            float mx = 0.0f;
            for (auto v : bins) mx = std::max (mx, v);
            expectWithinAbsoluteError (mx, 0.25f, 0.01f);

            // overdub the whole loop with +0.25, then undo
            r.engine.queueCommand (Cmd::RecPlay, 0);    // play > dub
            r.input = 0.25f;
            r.run (110, false);
            r.engine.queueCommand (Cmd::RecPlay, 0);    // dub > play
            r.input = 0.0f;
            r.run (1, false);
            r.engine.getTrack (0).getWaveform (bins.data(), 64);
            expectWithinAbsoluteError (bins[32], 0.5f, 0.02f, "overdub shows in the waveform");
            r.engine.queueCommand (Cmd::Undo, 0);
            r.run (1, false);
            r.engine.getTrack (0).getWaveform (bins.data(), 64);
            expectWithinAbsoluteError (bins[32], 0.25f, 0.02f, "undo restores the waveform");
        }

        beginTest ("Standalone cue bus: chosen output pair, everything else silent");
        {
            LiveLooperProcessor proc (true);
            expect (proc.hasCueOutput());
            expectEquals (proc.getBusCount (false), 2);
            proc.prepareToPlay (48000.0, 512);
            proc.getState().getParameter ("rhythm_on")->setValueNotifyingHost (1.0f);
            auto* pattern = proc.getState().getParameter ("rhythm_pattern");
            pattern->setValueNotifyingHost (pattern->convertTo0to1 (0.0f));
            proc.setCuePair (2);                          // outputs 5 / 6
            proc.setClickRoute (ClickRoute::Cue);

            juce::AudioBuffer<float> buffer (2 + LiveLooperProcessor::kCueBusChannels, 512);
            buffer.clear();
            juce::MidiBuffer midi;
            proc.processBlock (buffer, midi);
            expectEquals (peakOf (buffer, 0), 0.0f, "main output without click");
            expectEquals (peakOf (buffer, 2), 0.0f, "outputs 3/4 silent");
            expectGreaterThan (peakOf (buffer, 4), 0.02f, "click on output 5");
            expectGreaterThan (peakOf (buffer, 5), 0.02f, "click on output 6");
            expectEquals (peakOf (buffer, 6), 0.0f);

            LiveLooperProcessor plugin (false);
            expectEquals (plugin.getBusCount (false), 1, "plug-ins keep a stereo effect layout");
            expect (LiveLooperProcessor::getCuePairName (1).contains ("3"));
        }

        beginTest ("Looper sets: save and load loops and settings, keep the app's mappings");
        {
            const auto file = juce::File::createTempFile ("llset");
            std::vector<float> original;
            {
                LiveLooperProcessor proc;
                proc.prepareToPlay (48000.0, 512);
                auto set = [&proc] (const juce::String& id, float v) {
                    auto* p = proc.getState().getParameter (id);
                    p->setValueNotifyingHost (p->convertTo0to1 (v));
                };
                set ("quantize", 0.0f);
                set ("recaction", 1.0f);
                set ("t2_level", 0.7f);
                proc.setSaveLoopsWithProject (false);     // a set always holds its loops
                juce::AudioBuffer<float> buffer (2, 512);
                juce::MidiBuffer midi;
                proc.getEngine().queueCommand (Cmd::RecPlay, 1);
                for (int b = 0; b < 30; ++b)
                {
                    for (int i = 0; i < 512; ++i)
                        buffer.setSample (0, i, 0.3f * (float) std::sin ((b * 512 + i) * 0.01)),
                        buffer.setSample (1, i, 0.3f * (float) std::sin ((b * 512 + i) * 0.01));
                    proc.processBlock (buffer, midi);
                }
                proc.getEngine().queueCommand (Cmd::RecPlay, 1);
                buffer.clear();
                proc.processBlock (buffer, midi);
                auto& tr = proc.getEngine().getTrack (1);
                original.resize ((size_t) tr.getLength());
                std::vector<float> r ((size_t) tr.getLength());
                tr.readLoop (original.data(), r.data());
                expect (proc.saveSet (file));
            }
            expect (file.loadFileAsString().contains ("LIVELOOPER_SET"));
            expect (! file.loadFileAsString().contains ("KEYMAP"), "a set has no keyboard mapping");

            LiveLooperProcessor other;
            other.prepareToPlay (48000.0, 512);
            other.getKeyMap().clear();                    // the app's own mapping must survive the load
            other.loadDefaultMidiMap();
            juce::String error;
            expect (other.loadSet (file, error), error);
            auto& tr = other.getEngine().getTrack (1);
            expectEquals (tr.getLength(), (int) original.size());
            expectWithinAbsoluteError (other.getState().getRawParameterValue ("t2_level")->load(), 0.7f, 1.0e-4f);
            expect (other.getKeyMap().getActionFor (juce::KeyPress ('1')).isEmpty(), "keyboard mapping kept");
            expect (other.describeMidiMapping ("t1_recplay").isNotEmpty(), "MIDI mapping kept");
            expect (other.getSaveLoopsWithProject(), "loading a set keeps the app's save-loops preference");

            const auto bogus = juce::File::createTempFile ("txt");
            bogus.replaceWithText ("hello");
            expect (! other.loadSet (bogus, error));
            expect (error.isNotEmpty());
            file.deleteFile();
            bogus.deleteFile();
        }

        beginTest ("STOP button returns to STOP after a long-press clear");
        {
            LiveLooperProcessor proc;
            proc.prepareToPlay (48000.0, 512);
            StopButton b (proc, 0);
            b.setSize (100, 34);
            b.simulateLongPressForTests();
            expect (b.isShowingCleared());
            b.mouseUp (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), {}, {}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                         &b, &b, juce::Time::getCurrentTime(), {}, juce::Time::getCurrentTime(), 1, false));
            expect (! b.isShowingCleared(), "released: back to STOP");
        }
    }
};

static FeatureTests featureTests;
