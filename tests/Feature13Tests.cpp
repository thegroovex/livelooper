// Tests of the 1.3 features: retrospective capture, auto record, ducking, feedback, direct
// outputs, resampling, time-stretch / pitch, ghost play head, scenes, 8 tracks and stems.

#include "plugin/PluginProcessor.h"

using namespace livelooper;
using Cmd = LooperEngine::Command;

namespace
{
constexpr double kRate13 = 48000.0;
constexpr int kBlock13 = 480;

struct Rig
{
    LooperEngine engine;
    GlobalSettings global;
    std::array<TrackSettings, kNumTracks> track {};
    TimeInfo time;
    std::vector<float> inL, inR, outL, outR;
    std::array<std::vector<float>, 2> direct;
    bool useDirect = false;
    float input = 0.0f;
    bool sine = false;
    double phase = 0.0;
    float peakMain = 0.0f, peakDirect = 0.0f;

    Rig()
    {
        engine.prepare (kRate13, kBlock13, 10.0);
        global.monitorLevel = 0.0f;
        global.syncToHost = false;
        global.quantize = Quantize::Off;
        global.recAction = RecAction::RecPlay;
        for (auto* v : { &inL, &inR, &outL, &outR, &direct[0], &direct[1] })
            v->assign ((size_t) kBlock13, 0.0f);
    }

    void run (int blocks)
    {
        peakMain = peakDirect = 0.0f;
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < kBlock13; ++i)
            {
                float v = input;
                if (sine)
                {
                    v = input * (float) std::sin (phase);
                    phase += 2.0 * juce::MathConstants<double>::pi * 220.0 / kRate13;
                }
                inL[(size_t) i] = inR[(size_t) i] = v;
            }
            std::fill (direct[0].begin(), direct[0].end(), 0.0f);
            std::fill (direct[1].begin(), direct[1].end(), 0.0f);
            engine.setGlobalSettings (global);
            for (int t = 0; t < kNumTracks; ++t)
                engine.setTrackSettings (t, track[(size_t) t]);
            LooperEngine::ExtraOutputs x;
            if (useDirect)
            {
                x.directL[0] = direct[0].data();
                x.directR[0] = direct[1].data();
            }
            engine.process (inL.data(), inR.data(), outL.data(), outR.data(), x, kBlock13, time);
            for (int i = 0; i < kBlock13; ++i)
            {
                peakMain = std::max (peakMain, std::abs (outL[(size_t) i]));
                peakDirect = std::max (peakDirect, std::abs (direct[0][(size_t) i]));
            }
        }
    }

    void recordLoop (int t, int blocks, float level)
    {
        input = level;
        engine.queueCommand (Cmd::RecPlay, t);
        run (blocks);
        engine.queueCommand (Cmd::RecPlay, t);
        input = 0.0f;
        run (1);
    }

    float loopPeak (int t)
    {
        auto& tr = engine.getTrack (t);
        std::vector<float> l ((size_t) tr.getLength()), r ((size_t) tr.getLength());
        tr.readLoop (l.data(), r.data());
        float p = 0.0f;
        for (auto v : l)
            p = std::max (p, std::abs (v));
        return p;
    }
};

struct Proc13
{
    LiveLooperProcessor proc { false, false };
    juce::AudioBuffer<float> buffer { 2, kBlock13 };
    juce::MidiBuffer midi;

    Proc13()
    {
        proc.prepareToPlay (kRate13, kBlock13);
        set ("quantize", 0.0f);
        set ("recaction", 1.0f);
        set ("sync", 0.0f);
    }
    void set (const juce::String& id, float v) { proc.getState().getParameter (id)->setValueNotifyingHost (v); }
    void run (int blocks, float in)
    {
        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
                juce::FloatVectorOperations::fill (buffer.getWritePointer (ch), in, kBlock13);
            proc.processBlock (buffer, midi);
        }
    }
    /** Presses a trigger, runs one block, releases it (the message-thread timer does that in the app). */
    void press (const juce::String& id, float in = 0.0f)
    {
        proc.pressTrigger (id);
        run (1, in);
        set (id, 0.0f);
    }
    void recordLoop (int t, int blocks)
    {
        press (params::trackId (t, params::kRecPlay), 0.5f);
        run (blocks, 0.5f);
        press (params::trackId (t, params::kRecPlay), 0.0f);
    }
};
} // namespace

class Feature13Tests : public juce::UnitTest
{
public:
    Feature13Tests() : juce::UnitTest ("Capture, auto record, ducking, outputs, stretch, scenes", "livelooper") {}

    void runTest() override
    {
        beginTest ("Retrospective capture turns what was just played into a loop");
        {
            Rig r;
            r.global.captureBars = 4;          // 4 bars at 120 BPM = 8 s, only 3 s were played
            r.input = 0.5f;
            r.sine = true;
            r.run (300);
            expect (r.engine.getTrack (0).isEmpty());
            r.engine.queueCommand (Cmd::Capture, -1);
            r.input = 0.0f;
            r.run (1);
            auto& tr = r.engine.getTrack (0);
            expect (tr.getState() == LoopTrack::State::Playing, "captured loop plays at once");
            expectEquals (tr.getLength(), 96000, "falls back to the bars that were available (1 bar = 2 s)");
            expectGreaterThan (r.loopPeak (0), 0.3f, "the captured audio is in the loop");
            r.run (10);
            expectGreaterThan (r.peakMain, 0.1f, "and it is heard");
            r.engine.queueCommand (Cmd::Undo, 0);
            r.run (1);
            expect (r.engine.getTrack (0).isEmpty(), "UNDO removes a captured loop");
        }

        beginTest ("Capture fills the first empty track and leaves full tracks alone");
        {
            Rig r;
            r.recordLoop (0, 100, 0.3f);
            r.global.captureBars = 1;
            r.input = 0.4f;
            r.run (450);
            r.engine.queueCommand (Cmd::Capture, -1);
            r.run (1);
            expect (! r.engine.getTrack (1).isEmpty(), "track 2 got the capture");
            expect (r.engine.getTrack (1).getState() == LoopTrack::State::Playing);
            const int len0 = r.engine.getTrack (0).getLength();
            r.engine.queueCommand (Cmd::Capture, 0);
            r.run (1);
            expectEquals (r.engine.getTrack (0).getLength(), len0, "a track with a loop is not overwritten");
        }

        beginTest ("Auto record waits for the input to pass the threshold");
        {
            Rig r;
            r.global.autoRecord = true;
            r.global.autoRecThreshold = juce::Decibels::decibelsToGain (-30.0f);
            r.engine.queueCommand (Cmd::RecPlay, 0);
            r.input = 0.01f;                   // -40 dB: below the threshold
            r.run (20);
            expect (r.engine.getPending (0) == LooperEngine::Pending::AutoRecord, "armed, waiting");
            expect (r.engine.getTrack (0).isEmpty());
            r.input = 0.5f;
            r.run (2);
            expect (r.engine.getTrack (0).getState() == LoopTrack::State::Recording, "starts with the first loud sample");
            expect (r.engine.getPending (0) == LooperEngine::Pending::None);
        }

        beginTest ("Auto record can be cancelled with a second press");
        {
            Rig r;
            r.global.autoRecord = true;
            r.engine.queueCommand (Cmd::RecPlay, 0);
            r.run (2);
            r.engine.queueCommand (Cmd::RecPlay, 0);
            r.input = 0.5f;
            r.run (5);
            expect (r.engine.getTrack (0).isEmpty(), "cancelled before the signal came");
        }

        beginTest ("Feedback: every overdub pass keeps only part of the loop");
        {
            Rig r;
            r.recordLoop (0, 50, 0.8f);
            const float before = r.loopPeak (0);
            r.track[0].feedback = 0.5f;
            r.engine.queueCommand (Cmd::RecPlay, 0);    // overdub with silence
            r.run (50);
            r.engine.queueCommand (Cmd::RecPlay, 0);
            r.run (1);
            expectWithinAbsoluteError (r.loopPeak (0), before * 0.5f, 0.02f, "one pass at 50 % feedback halves the loop");
        }

        beginTest ("Ducking: track 1 lowers a ducked track");
        {
            Rig r;
            r.recordLoop (0, 50, 0.8f);
            r.recordLoop (1, 50, 0.8f);
            r.global.duckSource = DuckSource::Track1;
            r.track[1].duck = 1.0f;
            r.run (5);
            expectLessThan (r.engine.getTrackStatus (1).duckGain.load(), 0.3f, "ducked while track 1 plays");
            expectGreaterThan (r.engine.getTrackStatus (0).duckGain.load(), 0.99f, "the source itself is never ducked");
            r.engine.queueCommand (Cmd::Stop, 0);
            r.run (200);
            expectGreaterThan (r.engine.getTrackStatus (1).duckGain.load(), 0.95f, "comes back when the source stops");
        }

        beginTest ("Ducking from the rhythm's kick");
        {
            Rig r;
            r.recordLoop (0, 50, 0.5f);
            r.track[0].duck = 1.0f;
            r.engine.setRhythm (true, 1, 0.0f);    // a drum pattern at level 0: the kick still ducks
            float minGain = 1.0f;
            for (int i = 0; i < 100; ++i)
            {
                r.run (1);
                minGain = std::min (minGain, r.engine.getTrackStatus (0).duckGain.load());
            }
            expectLessThan (minGain, 0.5f);
        }

        beginTest ("Direct output takes the track off the main output");
        {
            Rig r;
            r.useDirect = true;
            r.recordLoop (0, 50, 0.5f);
            r.track[0].output = TrackOutput::Direct;
            r.run (5);
            expectLessThan (r.peakMain, 0.001f, "main silent");
            expectGreaterThan (r.peakDirect, 0.3f, "direct output has the track");
            r.track[0].output = TrackOutput::MainAndDirect;
            r.run (5);
            expectGreaterThan (r.peakMain, 0.3f);
            expectGreaterThan (r.peakDirect, 0.3f);
            r.useDirect = false;
            r.track[0].output = TrackOutput::Direct;
            r.run (5);
            expectGreaterThan (r.peakMain, 0.3f, "without a direct output the track stays on main");
        }

        beginTest ("Resample: a track records the mix of the other tracks");
        {
            Rig r;
            r.recordLoop (0, 50, 0.5f);
            r.track[1].input = InputRoute::Resample;
            r.engine.queueCommand (Cmd::RecPlay, 1);
            r.run (50);                          // the input is silent, only track 1 plays
            r.engine.queueCommand (Cmd::RecPlay, 1);
            r.run (1);
            expectGreaterThan (r.loopPeak (1), 0.3f, "the bounce holds track 1's audio");
        }

        beginTest ("Pitch shift and follow tempo (time-stretch)");
        {
            Rig r;
            r.sine = true;
            r.input = 0.5f;
            r.engine.queueCommand (Cmd::RecPlay, 0);
            r.run (200);
            r.engine.queueCommand (Cmd::RecPlay, 0);
            r.input = 0.0f;
            r.sine = false;
            r.run (1);
            r.track[0].pitch = 7.0f;
            r.run (30);
            expect (r.engine.getTrack (0).isStretching(), "pitch goes through the stretcher");
            expectGreaterThan (r.peakMain, 0.1f, "pitched output is audible");

            r.track[0].pitch = 0.0f;
            r.track[0].stretch = true;
            r.global.internalTempo = 60.0;      // half the tempo the loop was recorded at
            r.run (1);
            const int p0 = r.engine.getTrack (0).getPlayPosition();
            r.run (20);
            const int p1 = r.engine.getTrack (0).getPlayPosition();
            const int len = r.engine.getTrack (0).getLength();
            const int moved = ((p1 - p0) + len) % len;
            expectWithinAbsoluteError (moved, 20 * kBlock13 / 2, 4, "the loop runs at half speed");
            expectGreaterThan (r.peakMain, 0.1f);

            r.track[0].stretch = false;
            r.run (2);
            expect (! r.engine.getTrack (0).isStretching(), "back to normal playback");
        }

        beginTest ("Ghost play head for stopped tracks");
        {
            Rig r;
            r.recordLoop (0, 100, 0.5f);
            r.recordLoop (1, 100, 0.5f);
            r.engine.queueCommand (Cmd::Stop, 1);
            r.run (20);
            const float g1 = r.engine.getTrackStatus (1).ghostProgress.load();
            expect (g1 >= 0.0f && g1 <= 1.0f, "a stopped track shows where it would be");
            r.run (10);
            expectGreaterThan (r.engine.getTrackStatus (1).ghostProgress.load(), g1, "and it moves on");
            expectLessThan (r.engine.getTrackStatus (0).ghostProgress.load(), 0.0f, "a playing track has none");
        }

        beginTest ("Scenes store and recall which tracks play");
        {
            Proc13 h;
            h.recordLoop (0, 40);
            h.recordLoop (1, 40);
            expect (! h.proc.sceneIsStored (0));
            h.press ("scene1");                      // empty scene: stores
            expect (h.proc.sceneIsStored (0));
            expectEquals ((int) h.proc.getSceneMask (0), 3, "tracks 1 and 2 playing");

            h.press (params::trackId (0, params::kStop));
            h.run (2, 0.0f);
            h.set ("scene_store", 1.0f);
            h.press ("scene2");
            expectEquals ((int) h.proc.getSceneMask (1), 2, "only track 2");
            h.set ("scene_store", 0.0f);             // the message-thread timer does this in the app

            h.press ("scene1");
            expect (h.proc.getEngine().getPending (0) == LooperEngine::Pending::Play
                    || h.proc.getEngine().getTrack (0).getState() == LoopTrack::State::Playing, "scene 1 starts track 1 again");
            h.run (220, 0.0f);
            expect (h.proc.getEngine().getTrack (0).getState() == LoopTrack::State::Playing);
            expectEquals (h.proc.getLastScene(), 0);
            h.press ("scene2");
            h.run (60, 0.0f);
            expect (h.proc.getEngine().getTrack (0).getState() == LoopTrack::State::Stopped, "scene 2 stops track 1");
            expect (h.proc.getEngine().getTrack (1).getState() == LoopTrack::State::Playing, "and keeps track 2");

            juce::MemoryBlock state;
            h.proc.getStateInformation (state);
            LiveLooperProcessor other (false, false);
            other.setStateInformation (state.getData(), (int) state.getSize());
            expectEquals ((int) other.getSceneMask (1), 2, "scenes are saved with the project");
        }

        beginTest ("8 tracks: memory only for the tracks that are shown");
        {
            LiveLooperProcessor proc (false, false);
            proc.prepareToPlay (kRate13, kBlock13);
            expectEquals (proc.getVisibleTracks(), 5);
            expectLessThan (proc.getEngine().getTrack (7).getMaxLength(), 10000, "hidden track 8 holds no loop memory");
            proc.setVisibleTracks (8);
            expectEquals (proc.getEngine().getActiveTracks(), 8);
            expectGreaterThan (proc.getEngine().getTrack (7).getMaxLength(), (int) kRate13 * 10);
            juce::MemoryBlock state;
            proc.getStateInformation (state);
            LiveLooperProcessor other (false, false);
            other.setStateInformation (state.getData(), (int) state.getSize());
            expectEquals (other.getVisibleTracks(), 8, "the number of tracks is saved");
            proc.setVisibleTracks (5);
            expectLessThan (proc.getEngine().getTrack (7).getMaxLength(), 10000, "memory is given back");
        }

        beginTest ("Plug-in direct outputs: one optional stereo bus per track");
        {
            LiveLooperProcessor proc (false, true);
            expect (proc.hasTrackOutputBuses());
            expectEquals (proc.getBusCount (false), 1 + kNumTracks);
            expect (! proc.getBus (false, 1)->isEnabled(), "off until the host enables it");
            auto layout = proc.getBusesLayout();
            layout.outputBuses.getReference (1) = juce::AudioChannelSet::stereo();
            expect (proc.checkBusesLayoutSupported (layout));
            layout.outputBuses.getReference (1) = juce::AudioChannelSet::create5point1();
            expect (! proc.checkBusesLayoutSupported (layout));

            // enabled bus: the track plays there when its output is "Direct"
            layout.outputBuses.getReference (1) = juce::AudioChannelSet::stereo();
            expect (proc.setBusesLayout (layout));
            proc.prepareToPlay (kRate13, kBlock13);
            auto set = [&] (const juce::String& id, float v) { proc.getState().getParameter (id)->setValueNotifyingHost (v); };
            set ("quantize", 0.0f); set ("recaction", 1.0f); set ("sync", 0.0f); set ("monitor", 0.0f);
            juce::AudioBuffer<float> buffer (proc.getTotalNumOutputChannels(), kBlock13);
            juce::MidiBuffer midi;
            auto run = [&] (int blocks, float in) {
                for (int b = 0; b < blocks; ++b)
                {
                    buffer.clear();
                    for (int ch = 0; ch < 2; ++ch)
                        juce::FloatVectorOperations::fill (buffer.getWritePointer (ch), in, kBlock13);
                    proc.processBlock (buffer, midi);
                }
            };
            proc.pressTrigger ("t1_recplay"); run (1, 0.5f); set ("t1_recplay", 0.0f);
            run (40, 0.5f);
            proc.pressTrigger ("t1_recplay"); run (1, 0.0f); set ("t1_recplay", 0.0f);
            set ("t1_output", 0.5f);              // Direct
            run (5, 0.0f);
            expectLessThan (buffer.getMagnitude (0, 0, kBlock13), 0.001f, "main output silent");
            expectGreaterThan (buffer.getMagnitude (2, 0, kBlock13), 0.3f, "track 1 on its own bus");
        }

        beginTest ("Stem export writes one WAV per track with its tempo");
        {
            Proc13 h;
            h.recordLoop (0, 40);
            auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("livelooper-stems-test");
            dir.deleteRecursively();
            expectEquals (h.proc.exportStems (dir), 1);
            auto files = dir.findChildFiles (juce::File::findFiles, false, "*.wav");
            expectEquals (files.size(), 1);
            if (files.size() == 1)
            {
                expect (files[0].getFileName().contains ("track1") && files[0].getFileName().contains ("bpm"));
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (files[0].createInputStream().release(), true));
                expect (reader != nullptr);
                if (reader != nullptr)
                {
                    expectEquals ((int) reader->lengthInSamples, h.proc.getEngine().getTrack (0).getLength());
                    expect (reader->metadataValues.containsKey (juce::WavAudioFormat::acidTempo), "tempo for Live / DAWs");
                }
            }
            dir.deleteRecursively();
        }

        beginTest ("New parameters reach the engine and are MIDI-mappable");
        {
            Proc13 h;
            h.set ("t2_feedback", 0.25f);
            h.set ("t2_duck", 0.5f);
            h.set ("t2_pitch", h.proc.getState().getParameter ("t2_pitch")->convertTo0to1 (5.0f));
            h.set ("t2_stretch", 1.0f);
            h.set ("t2_output", 1.0f);
            h.set ("autorec", 1.0f);
            h.set ("duck_source", 1.0f);
            h.run (1, 0.0f);
            const auto& s = h.proc.getEngine().getTrack (1).getSettings();
            expectWithinAbsoluteError (s.feedback, 0.25f, 0.001f);
            expectWithinAbsoluteError (s.duck, 0.5f, 0.001f);
            expectWithinAbsoluteError (s.pitch, 5.0f, 0.001f);
            expect (s.stretch);
            expect (s.output == TrackOutput::MainAndDirect);
            for (auto* id : { "scene1", "scene8", "scene_store", "capture", "t8_capture", "t3_pitch", "duck_source" })
                expectGreaterOrEqual (h.proc.getParamIndex (id), 0, juce::String ("parameter exists: ") + id);
            expect (params::isTrigger ("scene3") && params::isTrigger ("capture") && params::isTrigger ("t1_capture"));
            expect (! params::isTrigger ("scene_store"));
        }
    }
};

static Feature13Tests feature13Tests;
