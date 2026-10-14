// Tests of the v1.6 features: loop tricks, performance recorder, set container, demo set.

#include "plugin/PluginProcessor.h"
#include "plugin/AppSettings.h"

using namespace livelooper;
using Cmd16 = LooperEngine::Command;
using Trick = LoopTrack::Trick;

namespace
{
constexpr double kRate16 = 48000.0;
constexpr int kBlock16 = 480;
constexpr int kBar16 = 96000;          // 120 BPM, 4/4, 48 kHz

float ramp16 (int loopPos) { return 0.1f + 0.8f * (float) loopPos / (float) kBar16; }

struct Rig16
{
    LooperEngine engine;
    GlobalSettings global;
    std::array<TrackSettings, kNumTracks> track {};
    TimeInfo time;
    std::vector<float> inL, inR, outL, outR;
    long long pos = 0;
    bool rampInput = false;

    Rig16()
    {
        engine.prepare (kRate16, kBlock16, 10.0);
        global.monitorLevel = 0.0f;
        global.syncToHost = false;
        global.quantize = Quantize::Off;
        global.recAction = RecAction::RecPlay;
        global.internalTempo = 120.0;
        for (auto* v : { &inL, &inR, &outL, &outR })
            v->assign ((size_t) kBlock16, 0.0f);
    }
    void run (int blocks)
    {
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < kBlock16; ++i)
                inL[(size_t) i] = inR[(size_t) i] = rampInput ? ramp16 ((int) ((pos + i) % kBar16)) : 0.0f;
            engine.setGlobalSettings (global);
            for (int t = 0; t < kNumTracks; ++t)
                engine.setTrackSettings (t, track[(size_t) t]);
            engine.process (inL.data(), inR.data(), outL.data(), outR.data(), kBlock16, time);
            pos += kBlock16;
        }
    }
    void recordRampBar (int t = 0)
    {
        track[(size_t) t].measures = 1;
        rampInput = true;
        engine.queueCommand (Cmd16::RecPlay, t);
        run (kBar16 / kBlock16);
        rampInput = false;
        track[(size_t) t].measures = 0;
    }
    std::vector<float> loop (int t)
    {
        auto& tr = engine.getTrack (t);
        std::vector<float> l ((size_t) tr.getLength()), r ((size_t) tr.getLength());
        tr.readLoop (l.data(), r.data());
        return l;
    }
};

struct Proc16
{
    LiveLooperProcessor proc { false, false };
    juce::AudioBuffer<float> buffer { 2, kBlock16 };
    juce::MidiBuffer midi;
    float lastPeak = 0.0f;

    Proc16()
    {
        proc.prepareToPlay (kRate16, kBlock16);
        set ("quantize", 0.0f);
        set ("recaction", 1.0f);
        set ("sync", 0.0f);
        set ("monitor", 0.0f);
    }
    void set (const juce::String& id, float v) { proc.getState().getParameter (id)->setValueNotifyingHost (v); }
    float get (const juce::String& id)         { return proc.getState().getParameter (id)->getValue(); }
    void run (int blocks, float in = 0.0f)
    {
        lastPeak = 0.0f;
        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
                juce::FloatVectorOperations::fill (buffer.getWritePointer (ch), in, kBlock16);
            proc.processBlock (buffer, midi);
            midi.clear();
            lastPeak = std::max (lastPeak, buffer.getMagnitude (0, 0, kBlock16));
        }
    }
    void press (const juce::String& id, float in = 0.0f)
    {
        proc.pressTrigger (id);
        run (1, in);
        set (id, 0.0f);
    }
    void pump()
    {
        for (int i = 0; i < 5; ++i)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
    }
    void recordBar (int t)
    {
        set (params::trackId (t, params::kMeasure), 0.2f);       // "1"
        press (params::trackId (t, params::kRecPlay), 0.5f);
        run (kBar16 / kBlock16, 0.5f);
        set (params::trackId (t, params::kMeasure), 0.0f);
    }
    LoopTrack& track (int t) { return proc.getEngine().getTrack (t); }
};

struct TempFolder16
{
    juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("livelooper-tests-16-" + juce::String (juce::Random::getSystemRandom().nextInt64()));
    TempFolder16()  { dir.createDirectory(); appsettings::setStorageFolderForTests (dir); }
    ~TempFolder16() { appsettings::setStorageFolderForTests ({}); dir.deleteRecursively(); }
};
} // namespace

class Features16EngineTests : public juce::UnitTest
{
public:
    Features16EngineTests() : juce::UnitTest ("1.6 loop tricks (engine)", "livelooper") {}

    void runTest() override
    {
        beginTest ("Half speed: twice as long, every sample stretched, UNDO / REDO");
        {
            Rig16 r;
            r.recordRampBar();
            auto& tr = r.engine.getTrack (0);
            const auto before = r.loop (0);
            r.engine.queueCommand (Cmd16::LoopTrick, 0, (int) Trick::HalfSpeed);
            r.run (1);
            expectEquals (tr.getLength(), 2 * kBar16);
            const auto after = r.loop (0);
            expectWithinAbsoluteError (after[20000], before[10000], 1.0e-5f);
            expectWithinAbsoluteError (after[20001], 0.5f * (before[10000] + before[10001]), 1.0e-5f);
            expectWithinAbsoluteError (after[150000], before[75000], 1.0e-5f);
            expect (tr.getState() == LoopTrack::State::Playing, "keeps playing");
            expect (tr.canUndo());

            r.engine.queueCommand (Cmd16::Undo, 0);
            r.run (1);
            expectEquals (tr.getLength(), kBar16, "undo: length");
            const auto undone = r.loop (0);
            bool same = true;
            for (size_t i = 0; i < undone.size(); ++i)
                same = same && undone[i] == before[i];
            expect (same, "undo: the original audio is back");

            r.engine.queueCommand (Cmd16::Redo, 0);
            r.run (1);
            expectEquals (tr.getLength(), 2 * kBar16, "redo");
            expectWithinAbsoluteError (r.loop (0)[20000], before[10000], 1.0e-5f);
        }

        beginTest ("Double speed keeps the length and plays the loop twice; reverse flips the audio");
        {
            Rig16 r;
            r.recordRampBar();
            auto& tr = r.engine.getTrack (0);
            const auto before = r.loop (0);
            r.engine.queueCommand (Cmd16::LoopTrick, 0, (int) Trick::DoubleSpeed);
            r.run (1);
            expectEquals (tr.getLength(), kBar16);
            auto after = r.loop (0);
            expectWithinAbsoluteError (after[1000], 0.5f * (before[2000] + before[2001]), 1.0e-5f);
            expectWithinAbsoluteError (after[kBar16 / 2 + 1000], 0.5f * (before[2000] + before[2001]), 1.0e-5f, "second pass");
            r.engine.queueCommand (Cmd16::Undo, 0);
            r.run (1);

            r.engine.queueCommand (Cmd16::LoopTrick, 0, (int) Trick::Reverse);
            r.run (1);
            after = r.loop (0);
            expectEquals ((int) after.size(), kBar16);
            expectEquals (after[0], before[(size_t) kBar16 - 1]);
            expectEquals (after[12345], before[(size_t) (kBar16 - 1 - 12345)]);
            r.engine.queueCommand (Cmd16::Undo, 0);
            r.run (1);
            expectEquals (r.loop (0)[12345], before[12345], "undo reverse");
        }

        beginTest ("Halve / double length through the trick command");
        {
            Rig16 r;
            r.recordRampBar();
            r.engine.queueCommand (Cmd16::LoopTrick, 0, (int) Trick::DoubleLength);
            r.run (1);
            expectEquals (r.engine.getTrack (0).getLength(), 2 * kBar16);
            r.engine.queueCommand (Cmd16::LoopTrick, 0, (int) Trick::HalveLength);
            r.engine.queueCommand (Cmd16::LoopTrick, 0, (int) Trick::HalveLength);
            r.run (1);
            expectEquals (r.engine.getTrack (0).getLength(), kBar16 / 2);
        }

        beginTest ("Tricks are quantized to the next bar line");
        {
            Rig16 r;
            r.recordRampBar();
            r.global.quantize = Quantize::Measure;
            r.run (kBar16 / 4 / kBlock16);              // a quarter into the next bar
            r.engine.queueCommand (Cmd16::LoopTrick, 0, (int) Trick::HalfSpeed);
            r.run (1);
            expectEquals (r.engine.getTrack (0).getLength(), kBar16, "waits for the bar line");
            expectEquals (r.engine.getTrackStatus (0).pendingTrick.load(), (int) Trick::HalfSpeed);
            r.run ((kBar16 * 3 / 4) / kBlock16 + 1);
            expectEquals (r.engine.getTrack (0).getLength(), 2 * kBar16, "done at the bar line");
            expectEquals (r.engine.getTrackStatus (0).pendingTrick.load(), -1);
        }

        beginTest ("Empty and recording tracks are left alone, half speed without memory keeps the length");
        {
            Rig16 r;
            r.engine.queueCommand (Cmd16::LoopTrick, 1, (int) Trick::Reverse);
            r.run (1);
            expect (r.engine.getTrack (1).isEmpty());
            r.rampInput = true;
            r.engine.queueCommand (Cmd16::RecPlay, 1);
            r.run (10);
            r.engine.queueCommand (Cmd16::LoopTrick, 1, (int) Trick::HalfSpeed);
            r.run (1);
            expect (r.engine.getTrack (1).getState() == LoopTrack::State::Recording);
            r.rampInput = false;

            Rig16 big;
            big.track[0].measures = 4;              // 8 s of 10 s
            big.rampInput = true;
            big.engine.queueCommand (Cmd16::RecPlay, 0);
            big.run (4 * kBar16 / kBlock16);
            big.rampInput = false;
            const auto before = big.loop (0);
            big.engine.queueCommand (Cmd16::LoopTrick, 0, (int) Trick::HalfSpeed);
            big.run (1);
            expectEquals (big.engine.getTrack (0).getLength(), 4 * kBar16, "no memory for x2: the first half, stretched");
            expectWithinAbsoluteError (big.loop (0)[100000], before[50000], 1.0e-5f);
        }
    }
};

static Features16EngineTests features16EngineTests;

class Features16ProcessorTests : public juce::UnitTest
{
public:
    Features16ProcessorTests() : juce::UnitTest ("1.6 features (processor)", "plugin") {}

    void runTest() override
    {
        using namespace params;

        beginTest ("Loop trick parameters: triggers, appended after the 1.5 parameters, MIDI-learnable");
        {
            Proc16 h;
            for (int t = 0; t < kNumTracks; ++t)
                for (auto* n : { kHalfSpeed, kDblSpeed, kRevLoop })
                {
                    expect (h.proc.getState().getParameter (trackId (t, n)) != nullptr);
                    expect (isTrigger (trackId (t, n)));
                }
            expect (isTrigger (gClipExport) && ! isTrigger (gPerfRec));
            const int lastOld = h.proc.getParamIndex (gRhythmFills);
            expect (h.proc.getParamIndex (trackId (0, kHalfSpeed)) > lastOld, "appended");
            expectEquals (h.proc.getParamIndex (gClipExport), h.proc.getParamIndex (trackId (0, kHalfSpeed)) + kNumTracks * 3 + 1);
            expectEquals (englishName (trackId (2, kHalfSpeed)), juce::String ("T3 Half Speed"));
            expectEquals (englishName (gPerfRec), juce::String ("Performance Recorder"));

            h.recordBar (0);
            h.press (trackId (0, kHalfSpeed));
            expectEquals (h.track (0).getLength(), 2 * kBar16);
            h.press (trackId (0, kUndo));
            expectEquals (h.track (0).getLength(), kBar16);
            h.press (trackId (0, kRevLoop));
            expect (h.track (0).canUndo());

            h.proc.startMidiLearn (trackId (0, kDblSpeed));
            h.proc.handleMidiMessage (juce::MidiMessage::noteOn (1, 40, (juce::uint8) 100));
            expect (h.proc.describeMidiMapping (trackId (0, kDblSpeed)).isNotEmpty(), "MIDI learn");
        }

        beginTest ("Performance recorder writes the master mix as WAV and shows the time");
        {
            TempFolder16 tmp;
            PerformanceRecorder::setFolder (tmp.dir.getChildFile ("rec"));
            Proc16 h;
            h.recordBar (0);
            h.set (gPerfRec, 1.0f);
            h.pump();
            expect (h.proc.isPerformanceRecording());
            h.run (100);                                   // 1 s of the loop
            expectWithinAbsoluteError (h.proc.getPerformanceRecordingSeconds(), 1.0, 0.02);
            const auto file = h.proc.stopPerformanceRecording();
            expect (! h.proc.isPerformanceRecording());
            expectEquals (h.get (gPerfRec), 0.0f);
            expect (file.existsAsFile(), "file written");
            expect (file.getParentDirectory() == tmp.dir.getChildFile ("rec"));
            expect (file.getFileName().startsWith ("livelooper-") && file.hasFileExtension (".wav"));

            juce::AudioFormatManager fm;
            fm.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
            expect (reader != nullptr);
            if (reader != nullptr)
            {
                expectEquals ((int) reader->numChannels, 2);
                expectEquals ((int) reader->sampleRate, 48000);
                expectEquals ((int) reader->bitsPerSample, 24);
                expectEquals ((int) reader->lengthInSamples, 100 * kBlock16);
                juce::AudioBuffer<float> b (2, (int) reader->lengthInSamples);
                reader->read (&b, 0, b.getNumSamples(), 0, true, true);
                expectWithinAbsoluteError (b.getMagnitude (0, 0, b.getNumSamples()), 0.5f, 0.01f, "the loop is in the file");
            }
            expectEquals (h.proc.getRecorder().getHistory (0.5).getNumSamples(), 24000, "history for the clip export");
            expect (PerformanceRecorder::makeFileName (juce::Time (2026, 9, 14, 21, 30, 5)) == "livelooper-2026-10-14_21-30-05.wav");
            PerformanceRecorder::setFolder ({});
        }

        beginTest ("Set container: one .livelooper file with all audio, opened again");
        {
            TempFolder16 tmp;
            Proc16 h;
            h.recordBar (0);
            h.recordBar (2);
            h.set (gTempo, h.proc.getState().getParameter (gTempo)->convertTo0to1 (97.0f));
            const auto file = tmp.dir.getChildFile ("my set");
            expect (h.proc.saveSetContainer (file));
            const auto saved = file.withFileExtension (".livelooper");
            expect (saved.existsAsFile());
            {
                juce::ZipFile zip (saved);
                expect (zip.getIndexOfFileName ("set.xml") >= 0);
                expect (zip.getIndexOfFileName ("info.json") >= 0);
                expect (zip.getIndexOfFileName ("audio/track1.flac") >= 0);
                expect (zip.getIndexOfFileName ("audio/track3.flac") >= 0);
                expect (zip.getIndexOfFileName ("audio/track2.flac") < 0);
            }

            Proc16 other;
            juce::String error;
            expect (other.proc.loadSet (saved, error), error);
            other.pump();
            expectEquals (other.track (0).getLength(), kBar16);
            expectEquals (other.track (2).getLength(), kBar16);
            expect (other.track (1).isEmpty());
            expectWithinAbsoluteError (other.proc.getState().getParameter (gTempo)->convertFrom0to1 (other.get (gTempo)), 97.0f, 0.05f);

            // not a set
            const auto bogus = tmp.dir.getChildFile ("x.livelooper");
            bogus.replaceWithText ("hello");
            expect (! other.proc.loadSet (bogus, error));
            expect (error.isNotEmpty());
        }

        beginTest ("Demo set: groove + three loops ready to play, empty set clears everything");
        {
            Proc16 h;
            expect (h.proc.loadDemoSet());
            for (int t = 0; t < 3; ++t)
            {
                expect (h.track (t).getState() == LoopTrack::State::Stopped, "loop " + juce::String (t + 1) + " loaded");
                expectWithinAbsoluteError (h.track (t).getLength(), 230400, 2);     // 2 bars at 100 BPM, 48 kHz
            }
            expect (h.track (3).isEmpty());
            expect (h.get (gRhythmOn) >= 0.5f);
            expectWithinAbsoluteError (h.proc.getState().getParameter (gTempo)->convertFrom0to1 (h.get (gTempo)), 100.0f, 0.05f);
            expect (h.proc.sceneIsStored (0) && h.proc.sceneIsStored (1));
            h.press (gAllStartStop);
            h.run (200);
            expect (h.lastPeak > 0.05f, "sound within seconds");
            expect (h.track (0).getState() == LoopTrack::State::Playing);

            if (auto path = juce::SystemStats::getEnvironmentVariable ("LIVELOOPER_WRITE_DEMO_SET", {}); path.isNotEmpty())
                h.proc.saveSetContainer (juce::File (path));

            h.proc.loadEmptySet();
            for (int t = 0; t < kNumTracks; ++t)
                expect (h.track (t).isEmpty());
            expect (h.get (gRhythmOn) < 0.5f);
            expect (! h.proc.sceneIsStored (0));
        }
    }
};

static Features16ProcessorTests features16ProcessorTests;
