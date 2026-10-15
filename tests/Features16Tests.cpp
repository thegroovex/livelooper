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
            expectEquals (h.get (gMonitor), 0.0f, "input monitor muted with the demo set (no feedback)");
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

//============================================================================== Push 1: SHIFT combos for the loop tricks
#include "plugin/push/PushSurface.h"

namespace
{
struct PushRig16
{
    LiveLooperProcessor proc { false, false };
    push::PushSurface surface { proc };
    juce::AudioBuffer<float> buffer { 2, kBlock16 };
    juce::MidiBuffer midi;
    juce::uint32 ms = 10000;

    PushRig16()
    {
        proc.prepareToPlay (kRate16, kBlock16);
        for (auto [id, v] : { std::pair<const char*, float> { "quantize", 0.0f }, { "recaction", 1.0f }, { "sync", 0.0f }, { "monitor", 0.0f } })
            proc.getState().getParameter (id)->setValueNotifyingHost (v);
        surface.setClock ([this] { return ms; });
    }
    void run (int blocks, float in = 0.0f)
    {
        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
                juce::FloatVectorOperations::fill (buffer.getWritePointer (ch), in, kBlock16);
            proc.processBlock (buffer, midi);
            ms += 10;
            for (auto* p : proc.getParameters())
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
                    if (params::isTrigger (r->getParameterID()) && r->getValue() >= 0.5f)
                        r->setValueNotifyingHost (0.0f);
        }
    }
    void button (int number, bool down) { surface.handleMidi (juce::MidiMessage::controllerEvent (1, number, down ? 127 : 0)); }
    void shiftPress (int number)
    {
        button (push::cc::shift, true);
        button (number, true);
        run (1);
        button (number, false);
        button (push::cc::shift, false);
    }
};
}

class Features16PushTests : public juce::UnitTest
{
public:
    Features16PushTests() : juce::UnitTest ("1.6 loop tricks on Push 1", "livelooper") {}

    void runTest() override
    {
        using namespace params;
        beginTest ("SHIFT+OCTAVE DOWN / UP = half / double speed, SHIFT+SCALES = reverse, OCTAVE alone still transposes");
        {
            PushRig16 r;
            r.proc.getState().getParameter (trackId (0, kMeasure))->setValueNotifyingHost (0.2f);
            r.proc.pressTrigger (trackId (0, kRecPlay));
            r.run (kBar16 / kBlock16 + 1, 0.5f);
            auto& tr = r.proc.getEngine().getTrack (0);
            expectEquals (tr.getLength(), kBar16);

            r.shiftPress (push::cc::octaveDown);
            expectEquals (tr.getLength(), 2 * kBar16, "half speed");
            r.shiftPress (push::cc::undo);          // SHIFT+UNDO = redo: nothing to redo yet
            r.button (push::cc::undo, true); r.run (1); r.button (push::cc::undo, false);
            expectEquals (tr.getLength(), kBar16, "UNDO");

            r.shiftPress (push::cc::octaveUp);
            expectEquals (tr.getLength(), kBar16, "double speed keeps the length");
            expect (tr.canUndo());
            r.button (push::cc::undo, true); r.run (1); r.button (push::cc::undo, false);

            // v1.6 merge: reverse moved from SHIFT+SCALES to SHIFT + bottom-row button (SCALES = STEP mode)
            r.shiftPress (push::kBottomRowFirst + 0);
            expect (tr.canUndo(), "reversed");
            expect (r.proc.getState().getParameter (trackId (0, kReverse))->getValue() < 0.5f,
                    "SHIFT + bottom row does not toggle reverse playback");

            auto* pitch = r.proc.getState().getParameter (trackId (0, kPitch));
            r.button (push::cc::octaveUp, true); r.run (1); r.button (push::cc::octaveUp, false);
            expectWithinAbsoluteError (pitch->convertFrom0to1 (pitch->getValue()), 12.0f, 1.0e-3f);
        }
    }
};

static Features16PushTests features16PushTests;

//============================================================================== clip export, guided tour, grid controller templates
#include "plugin/ClipExport.h"
#include "plugin/GuidedTour.h"
#include "plugin/WelcomeScreen.h"
#include "plugin/PluginEditor.h"
#include "plugin/controllers/GridControllers.h"
#include "plugin/Localisation.h"

class Features16UiTests : public juce::UnitTest
{
public:
    Features16UiTests() : juce::UnitTest ("1.6 clip export, tour, controller templates", "plugin") {}

    static juce::AudioBuffer<float> tone (double rate, double seconds)
    {
        juce::AudioBuffer<float> b (2, (int) (rate * seconds));
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float env = (i % (int) (rate / 2)) < (int) (rate / 8) ? 1.0f : 0.2f;     // beats, so the meter moves
            const float v = 0.5f * env * std::sin ((float) i * 440.0f * juce::MathConstants<float>::twoPi / (float) rate);
            b.setSample (0, i, v);
            b.setSample (1, i, v);
        }
        return b;
    }

    void runTest() override
    {
        using namespace params;

        beginTest ("Clip audio: last 20 s, resampled to 48 kHz");
        {
            auto a = clip::prepareAudio (tone (44100.0, 25.0), 44100.0, 20.0, false);
            expectEquals (a.getNumChannels(), 2);
            expectWithinAbsoluteError (a.getNumSamples(), 960000, 2);
            auto b = clip::prepareAudio (tone (48000.0, 3.0), 48000.0, 20.0, true);
            expectEquals (b.getNumSamples(), 144000);
        }

        beginTest ("Clip frames: 9:16, branded, waveform and progress move");
        {
            const auto audio = clip::prepareAudio (tone (48000.0, 4.0), 48000.0, 20.0, false);
            clip::Options opt;
            opt.subtitle = "test";
            clip::Renderer r (audio, 48000.0, opt);
            expectWithinAbsoluteError (r.getDuration(), 4.0, 1.0e-6);
            const auto f0 = r.renderFrame (0.0), f1 = r.renderFrame (3.9);
            expectEquals (f0.getWidth(), 1080);
            expectEquals (f0.getHeight(), 1920);
            // the left part of the waveform is in the brand colours once played
            const auto before = f0.getPixelAt (120, (int) (1920 * 0.47f));
            const auto after = f1.getPixelAt (120, (int) (1920 * 0.47f));
            expect (after.getRed() > before.getRed() + 40, "played part is coloured");
            // title area is not empty
            bool bright = false;
            for (int x = 300; x < 780 && ! bright; x += 4)
                bright = f0.getPixelAt (x, 230).getBrightness() > 0.8f;
            expect (bright, "livelooper title drawn");
            if (auto path = juce::SystemStats::getEnvironmentVariable ("LIVELOOPER_CLIP_PREVIEW", {}); path.isNotEmpty())
            {
                const juce::File file (path);
                file.deleteFile();
                juce::FileOutputStream out (file);
                juce::PNGImageFormat().writeImageToStream (r.renderFrame (2.3), out);
            }
        }

        beginTest ("Clip export writes a video (system encoder) or PNG frames + WAV");
        {
            TempFolder16 tmp;
            const auto audio = clip::prepareAudio (tone (48000.0, 1.5), 48000.0, 20.0, false);
            clip::Options opt;
            opt.width = 216; opt.height = 384; opt.fps = 10;
            const auto target = tmp.dir.getChildFile ("clip.mp4");
            float lastProgress = 0.0f;
            auto res = clip::exportClip (audio, target, opt, [&lastProgress] (float p) { lastProgress = p; return true; });
            expect (res.ok(), res.error);
            expect (res.cover.existsAsFile(), "cover image");
            expectWithinAbsoluteError (lastProgress, 1.0f, 1.0e-4f);
            logMessage ("clip encoder: " + res.encoder + (res.error.isNotEmpty() ? " (" + res.error + ")" : juce::String()));
           #if JUCE_MAC
            expect (res.kind == clip::Result::Kind::Video && res.encoder == "AVFoundation", "macOS: AVFoundation");
           #endif
            if (res.kind == clip::Result::Kind::Video)
                expect (res.file.getSize() > 1000, "mp4 written");

            // forced fallback: frames + WAV + instructions
            opt.allowNative = false;
            opt.allowFfmpeg = false;
            auto fb = clip::exportClip (audio, tmp.dir.getChildFile ("fallback.mp4"), opt);
            expect (fb.kind == clip::Result::Kind::Frames);
            expect (fb.file.getChildFile ("audio.wav").existsAsFile());
            expect (fb.file.getChildFile ("frame_00001.png").existsAsFile());
            expect (fb.file.getChildFile ("frame_00015.png").existsAsFile());
            expect (! fb.file.getChildFile ("frame_00016.png").existsAsFile());
            expect (fb.file.getChildFile ("cover.png").existsAsFile());
            expect (fb.file.getChildFile ("make-video.txt").loadFileAsString().contains ("ffmpeg"));

            auto tooShort = clip::exportClip (juce::AudioBuffer<float> (2, 100), tmp.dir.getChildFile ("x.mp4"), opt);
            expect (! tooShort.ok());
        }

        beginTest ("Clip export parameter is a MIDI-mappable trigger that reaches the editor");
        {
            Proc16 h;
            expect (! h.proc.takeClipExportRequest());
            h.press (gClipExport);
            expect (h.proc.takeClipExportRequest());
            expect (! h.proc.takeClipExportRequest(), "only once");
        }

        beginTest ("Record button: round, under the header buttons, bound to perf_rec, blinks with the beat");
        {
            TempFolder16 tmp;
            PerformanceRecorder::setFolder (tmp.dir.getChildFile ("rec"));
            Proc16 h;
            std::unique_ptr<LiveLooperEditor> ed (dynamic_cast<LiveLooperEditor*> (h.proc.createEditor()));
            expect (ed != nullptr);
            for (int tracks : { kDefaultVisibleTracks, kNumTracks })
            {
                h.proc.setVisibleTracks (tracks);      // 6 - 8 tracks: the header buttons move right
                h.pump();
                auto& rec = ed->getPerfRecButton();
                expectEquals (getParamId (rec), juce::String (gPerfRec), "MIDI-mappable via perf_rec");
                expect (rec.isVisible());
                auto* parent = rec.getParentComponent();
                const auto rb = rec.getBounds();
                const juce::Button* learn = nullptr;
                const juce::Button* help = nullptr;
                for (auto* c : parent->getChildren())
                    if (auto* b = dynamic_cast<juce::Button*> (c))
                    {
                        if (b->getButtonText() == "MIDI LEARN") learn = b;
                        if (b->getButtonText() == "?") help = b;
                    }
                expect (learn != nullptr && help != nullptr);
                if (learn == nullptr || help == nullptr)
                    continue;
                expect (rb.getY() >= help->getBottom() && rb.getY() - help->getBottom() <= 8, "directly below the header buttons");
                expectEquals (rb.getRight(), help->getRight(), "right-aligned with ?");
                expect (rb.getX() >= learn->getX(), "within the header button block");
                for (auto* c : parent->getChildren())
                    if (c != &rec && c->isVisible() && c->getWidth() < parent->getWidth() - 10 && c->getHeight() < parent->getHeight() - 10)
                        expect (! c->getBounds().intersects (rb), "no overlap: " + c->getName() + " " + c->getBounds().toString());
            }
            auto& rec = ed->getPerfRecButton();
            expect (! rec.getToggleState());
            rec.triggerClick();
            h.pump();
            expect (h.proc.isPerformanceRecording(), "click starts the recorder");
            expectEquals (h.get (gPerfRec), 1.0f);
            rec.setToggleState (true, juce::dontSendNotification);
            rec.setBeatPhase (4.2);
            expect (rec.isLit(), "first half of the beat: on");
            rec.setBeatPhase (4.7);
            expect (! rec.isLit(), "second half: off");
            rec.setElapsedSeconds (75.4);
            expectEquals (rec.getTimeText(), juce::String ("01:15"));
            expectEquals (RecordButton::formatTime (3725.0), juce::String ("1:02:05"));
            expectEquals (RecordButton::formatTime (std::numeric_limits<double>::infinity()), juce::String ("00:00"));
            rec.triggerClick();
            h.pump();
            expect (! h.proc.isPerformanceRecording(), "click stops it");
            ed.reset();
            PerformanceRecorder::setFolder ({});
        }

        beginTest ("Guided tour: five steps, next / back / close, first start loads the demo set");
        {
            TempFolder16 tmp;
            Proc16 h;
            std::unique_ptr<LiveLooperEditor> ed (dynamic_cast<LiveLooperEditor*> (h.proc.createEditor()));
            expect (ed != nullptr);
            ed->handleFirstRun();                         // tests: nothing happens without force
            expect (! ed->isTourVisible());
            expect (h.track (0).isEmpty());
            ed->handleFirstRun (true);
            expect (ed->isWelcomeVisible(), "first start: the welcome screen with the choice");
            expect (! ed->isTourVisible(), "the tour waits for the choice");
            expect (h.track (0).isEmpty(), "nothing replaced before the choice");
            expect (ed->getWelcome()->isFirstStart());
            {
                auto* w = ed->getWelcome();
                const auto card = w->getCardBounds();
                expect (card.getWidth() > 400 && w->getDemoButton().isVisible() && w->getEmptyButton().isVisible());
                expect (card.contains (w->getDemoButton().getBounds()) && card.contains (w->getEmptyButton().getBounds())
                        && card.contains (w->getCloseButton().getBounds()), "buttons inside the card");
                expect (! w->getDemoButton().getBounds().intersects (w->getEmptyButton().getBounds()));
            }
            if (auto path = juce::SystemStats::getEnvironmentVariable ("LIVELOOPER_UI_PREVIEW", {}); path.isNotEmpty())
            {
                const juce::File file (path + "welcome.png");
                file.deleteFile();
                juce::FileOutputStream out (file);
                juce::PNGImageFormat().writeImageToStream (ed->createComponentSnapshot (ed->getLocalBounds()), out);
            }
            ed->chooseDemoSet (false);
            h.pump();
            expect (! ed->isWelcomeVisible());
            expect (ed->isTourVisible(), "tour after the choice on the first start");
            expect (h.track (0).getState() == LoopTrack::State::Stopped, "demo set loaded");
            expectEquals (h.get (gMonitor), 0.0f, "first start + demo set: microphone / input monitor muted");
            expectEquals (appsettings::getValue ("firstRunDone"), juce::String ("1"));
            auto* tour = ed->getTour();
            expectEquals (tour->getNumSteps(), 5);
            tour->back();
            expectEquals (tour->getStep(), 0);
            auto snap = [&ed] (const juce::String& name) {
                if (auto path = juce::SystemStats::getEnvironmentVariable ("LIVELOOPER_UI_PREVIEW", {}); path.isNotEmpty())
                {
                    const juce::File file (path + name + ".png");
                    file.deleteFile();
                    juce::FileOutputStream out (file);
                    juce::PNGImageFormat().writeImageToStream (ed->createComponentSnapshot (ed->getLocalBounds()), out);
                }
            };
            snap ("tour1");
            for (int i = 1; i < 5; ++i)
            {
                if (i == 3) snap ("tour3");
                if (i == 4) snap ("tour4");
                tour->next();
                expectEquals (tour->getStep(), i);
                expect (tour->getCardBounds().getBottom() <= tour->getHeight() && tour->getCardBounds().getY() >= 0, "card on screen");
            }
            tour->next();                                  // DONE
            h.pump();
            expect (! ed->isTourVisible());
            ed->startTour();
            expect (ed->isTourVisible(), "from the help menu any time");
            ed->getTour()->close();
            h.pump();
            expect (! ed->isTourVisible());

            // SETUP > Welcome / demo set ...: the same choice any time, no tour afterwards
            expect (ed->currentSetHasLoops());
            ed->showWelcome (false);
            expect (ed->isWelcomeVisible() && ! ed->getWelcome()->isFirstStart());
            ed->chooseEmptySet (false);
            h.pump();
            expect (! ed->isWelcomeVisible());
            expect (! ed->isTourVisible(), "no tour from the menu");
            expect (! ed->currentSetHasLoops(), "empty set: all loops cleared");
            ed->showWelcome (false);
            ed->getWelcome()->getCloseButton().triggerClick();
            h.pump();
            expect (! ed->isWelcomeVisible(), "CLOSE leaves the set as it is");

            // screenshots for the visual check (LIVELOOPER_UI_PREVIEW): BEATS page and welcome in German
            ed->showBeatsPage (true);
            h.pump();
            expect (ed->isBeatsPageVisible());
            snap ("beats");
            const auto lang = i18n::getSetting();
            i18n::setSetting (i18n::Language::German, false);
            h.pump();
            snap ("beats_de");
            ed->showBeatsPage (false);
            ed->showWelcome (true);
            h.pump();
            snap ("welcome_de");
            ed->getWelcome()->getCloseButton().triggerClick();
            h.pump();
            i18n::setSetting (lang, false);
            ed.reset();
        }

        beginTest ("Every new 1.6 text has a German translation");
        {
            for (auto* s : { "Half Speed", "Double Speed", "Reverse Loop Audio", "Performance Recorder", "Export 9:16 Clip",
                             "NEXT", "BACK", "SKIP TOUR", "DONE", "RECORD", "OVERDUB", "SCENE", "STEP %1 / %2", "Load demo set",
                             "Empty set (clear everything)", "Guided tour (60 seconds)", "Performance recorder",
                             "Controller templates (grid controllers)", "Clip export 9:16 (Reels, TikTok, Shorts)",
                             "T%1 half speed", "T%1 double speed", "T%1 loop reversed", "live loop performance",
                             "Not enough audio for a clip yet.", "Save set to share (.livelooper, one file with all audio) ..." })
                expect (i18n::germanFor (s).isNotEmpty(), juce::String ("missing German text for: ") + s);
            for (int t = 0; t < kNumTracks; ++t)
                for (auto* n : { kHalfSpeed, kDblSpeed, kRevLoop })
                    expect (i18n::germanFor (englishName (trackId (t, n)).fromFirstOccurrenceOf (" ", false, false)).isNotEmpty());
        }

        beginTest ("Controller templates: detection by MIDI port name");
        {
            using controllers::detectTemplate;
            using controllers::TemplateId;
            // macOS / Linux names (false), Windows names (true) - interfaces as in the programmer's references
            expectEquals (detectTemplate ("Launchpad Mini MK3 LPMiniMK3 MIDI", false), (int) TemplateId::LaunchpadMiniMk3);
            expectEquals (detectTemplate ("LPMiniMK3 MIDI", false), (int) TemplateId::LaunchpadMiniMk3);
            expectEquals (detectTemplate ("Launchpad Mini MK3 LPMiniMK3 DAW", false), -1, "the DAW port is not used");
            expectEquals (detectTemplate ("MIDIIN2 (LPMiniMK3 MIDI)", true), (int) TemplateId::LaunchpadMiniMk3, "Windows: 2nd interface = MIDI");
            expectEquals (detectTemplate ("MIDIOUT2 (LPMiniMK3 MIDI)", true), (int) TemplateId::LaunchpadMiniMk3);
            expectEquals (detectTemplate ("LPMiniMK3 MIDI", true), -1, "Windows: 1st interface = DAW");
            expectEquals (detectTemplate ("Launchpad X LPX MIDI", false), (int) TemplateId::LaunchpadX);
            expectEquals (detectTemplate ("MIDIIN2 (LPX MIDI)", true), (int) TemplateId::LaunchpadX);
            expectEquals (detectTemplate ("LPX MIDI", true), -1);
            expectEquals (detectTemplate ("APC mini mk2", true), (int) TemplateId::ApcMiniMk2);
            expectEquals (detectTemplate ("MIDIIN2 (APC mini mk2)", true), -1, "APC mini mk2: port 0 only");
            expectEquals (detectTemplate ("APC mini mk2 Control", false), (int) TemplateId::ApcMiniMk2);
            expectEquals (detectTemplate ("APC mini mk2 Notes", false), -1);
            expectEquals (detectTemplate ("APC MINI", true), (int) TemplateId::ApcMini);
            expectEquals (detectTemplate ("Launchkey MK3 49 LKMK3 DAW Out", false), (int) TemplateId::LaunchkeyMk3);
            expectEquals (detectTemplate ("MIDIIN2 (LKMK3 MIDI)", true), (int) TemplateId::LaunchkeyMk3);
            expectEquals (detectTemplate ("LKMK3 MIDI", true), -1, "Windows: 1st interface = keys");
            expectEquals (detectTemplate ("Launchkey MK3 49 LKMK3 MIDI Out", false), -1, "the keys port is left to the DAW");
            expectEquals (detectTemplate ("Launchkey MK4 37 DAW In", false), (int) TemplateId::LaunchkeyMk4);
            expectEquals (detectTemplate ("MIDIIN2 (Launchkey MK4 37 MIDI)", true), (int) TemplateId::LaunchkeyMk4);
            expectEquals (detectTemplate ("Ableton Push User Port", false), -1);
            expectEquals (detectTemplate ("IAC Driver Bus 1", false), -1);
        }

        beginTest ("Controller templates: pads and buttons do the looper functions");
        {
            using namespace controllers;
            const Template lp (TemplateId::LaunchpadMiniMk3);
            expectEquals (lp.padNote (0, 0), 81, "top left in programmer mode");
            expectEquals (lp.padNote (7, 7), 18);
            auto a = lp.translate (juce::MidiMessage::noteOn (1, 81, (juce::uint8) 100), false);
            expect (a.kind == Action::Kind::RecPlay && a.index == 0);
            a = lp.translate (juce::MidiMessage::noteOn (1, 73, (juce::uint8) 100), false);
            expect (a.kind == Action::Kind::Stop && a.index == 2);
            a = lp.translate (juce::MidiMessage::noteOn (1, 73, (juce::uint8) 100), true);
            expect (a.kind == Action::Kind::Clear, "SHIFT + STOP = clear");
            a = lp.translate (juce::MidiMessage::noteOn (1, 41, (juce::uint8) 100), false);
            expect (a.kind == Action::Kind::HalfSpeed && a.index == 0);
            a = lp.translate (juce::MidiMessage::controllerEvent (1, 89, 127), false);
            expect (a.kind == Action::Kind::Scene && a.index == 0);
            a = lp.translate (juce::MidiMessage::controllerEvent (1, 19, 127), true);
            expect (a.kind == Action::Kind::SceneStore && a.index == 7);
            a = lp.translate (juce::MidiMessage::controllerEvent (1, 98, 127), false);
            expect (a.kind == Action::Kind::Shift && a.pressed);
            a = lp.translate (juce::MidiMessage::controllerEvent (1, 98, 0), false);
            expect (a.kind == Action::Kind::Shift && ! a.pressed);
            a = lp.translate (juce::MidiMessage::noteOff (1, 81), false);
            expect (a.kind == Action::Kind::None, "release does nothing");
            const auto enter = lp.enterMessages();
            expect (enter.size() == 1 && enter[0].isSysEx() && enter[0].getSysExData()[4] == 0x0d && enter[0].getSysExData()[6] == 1);

            const Template apc (TemplateId::ApcMiniMk2);
            expectEquals (apc.padNote (0, 0), 56, "APC: top left");
            a = apc.translate (juce::MidiMessage::noteOn (1, 56 + 3, (juce::uint8) 127), false);
            expect (a.kind == Action::Kind::RecPlay && a.index == 3);
            a = apc.translate (juce::MidiMessage::controllerEvent (1, 49, 64), false);
            expect (a.kind == Action::Kind::Level && a.index == 1 && std::abs (a.value - 64.0f / 127.0f) < 1.0e-5f);
            a = apc.translate (juce::MidiMessage::controllerEvent (1, 56, 127), false);
            expect (a.kind == Action::Kind::MasterLevel);
            a = apc.translate (juce::MidiMessage::noteOn (1, 112, (juce::uint8) 127), false);
            expect (a.kind == Action::Kind::Scene && a.index == 0);
            a = apc.translate (juce::MidiMessage::noteOn (1, 100, (juce::uint8) 127), false);
            expect (a.kind == Action::Kind::AllStartStop);

            const Template lk (TemplateId::LaunchkeyMk3);
            expectEquals (lk.getRows(), 2);
            a = lk.translate (juce::MidiMessage::noteOn (1, 0x60 + 4, (juce::uint8) 127), false);
            expect (a.kind == Action::Kind::RecPlay && a.index == 4);
            a = lk.translate (juce::MidiMessage::noteOn (1, 0x70 + 1, (juce::uint8) 127), false);
            expect (a.kind == Action::Kind::Stop && a.index == 1);
            a = lk.translate (juce::MidiMessage::controllerEvent (1, 0x68, 127), false);
            expect (a.kind == Action::Kind::AllStartStop);
            expect (lk.enterMessages().front().isNoteOn() && lk.enterMessages().front().getChannel() == 16);
            {
                const auto e3 = lk.enterMessages();
                expect (e3.size() == 2 && e3[1].isController() && e3[1].getChannel() == 16 && e3[1].getControllerNumber() == 3
                        && e3[1].getControllerValue() == 2, "MK3: BF 03 02 = Session layout");
                const auto e4 = Template (TemplateId::LaunchkeyMk4).enterMessages();
                expect (e4.size() == 2 && e4[0].getRawData()[0] == 0x9f && e4[0].getNoteNumber() == 0x0c && e4[0].getVelocity() == 127);
                expect (e4[1].isController() && e4[1].getChannel() == 7 && e4[1].getControllerNumber() == 0x1d
                        && e4[1].getControllerValue() == 2, "MK4: B6 1D 02 = DAW pad layout");
                a = lk.translate (juce::MidiMessage::controllerEvent (16, 0x68, 127), false);
                expect (a.kind == Action::Kind::None, "CC 104 on channel 16 = Scene Up, not ALL START / STOP");
                const auto lx = Template (TemplateId::LaunchpadX).enterMessages();
                expect (lx.size() == 1 && lx[0].getSysExData()[4] == 0x0c && lx[0].getSysExData()[5] == 0x0e && lx[0].getSysExData()[6] == 1,
                        "Launchpad X: F0 00 20 29 02 0C 0E 01 F7");
                // APC mini mk2 pad LED: channel = behaviour (index 6 = solid 100 %), velocity = palette
                std::vector<juce::MidiMessage> leds;
                SurfaceState st;
                st.tracks[0].state = (int) LoopTrack::State::Playing;
                apc.render (st, leds);
                expect (leds[0].getNoteNumber() == 56 && leds[0].getChannel() == 7 && leds[0].getVelocity() == 21, "APC mk2: 96 38 15 = green");
                const Template apc1 (TemplateId::ApcMini);
                leds.clear();
                apc1.render (st, leds);
                expect (leds[0].getNoteNumber() == 56 && leds[0].getChannel() == 1 && leds[0].getVelocity() == 1, "APC mini: 90 38 01 = green");
            }

            // actions reach the looper
            Proc16 h;
            GridControllerDevice::apply (h.proc, { Action::Kind::RecPlay, 0 });
            h.run (100, 0.5f);
            h.set (trackId (0, kRecPlay), 0.0f);           // what the timer does
            h.run (1, 0.5f);
            expect (h.track (0).getState() == LoopTrack::State::Recording, "recording");
            GridControllerDevice::apply (h.proc, { Action::Kind::RecPlay, 0 });
            h.run (2);
            h.set (trackId (0, kRecPlay), 0.0f);
            expect (h.track (0).getState() == LoopTrack::State::Playing, "playing");
            GridControllerDevice::apply (h.proc, { Action::Kind::Mute, 0 });
            expect (h.get (trackId (0, kMute)) >= 0.5f);
            Action lvl { Action::Kind::Level, 1 };
            lvl.value = 0.5f;
            GridControllerDevice::apply (h.proc, lvl);
            expectWithinAbsoluteError (h.proc.getState().getParameter (trackId (1, kLevel))->convertFrom0to1 (h.get (trackId (1, kLevel))), 0.5f, 1.0e-3f);
            GridControllerDevice::apply (h.proc, { Action::Kind::RecPlay, 7 });      // hidden track (5 tracks shown)
            h.run (2);
            expect (h.track (7).isEmpty());
        }

        beginTest ("Controller templates: LED feedback follows the track states");
        {
            using namespace controllers;
            Proc16 h;
            h.recordBar (0);
            auto state = GridControllerDevice::makeState (h.proc, false);
            expectEquals (state.tracks[0].state, (int) LoopTrack::State::Playing);
            expect (! state.tracks[5].exists);

            const Template lp (TemplateId::LaunchpadMiniMk3);
            std::vector<juce::MidiMessage> out;
            lp.render (state, out);
            auto find = [&out] (int note) -> const juce::MidiMessage* {
                for (auto& m : out)
                    if (m.isNoteOn (true) && m.getNoteNumber() == note)
                        return &m;
                return nullptr;
            };
            const auto* recPad = find (81);
            expect (recPad != nullptr && recPad->getVelocity() == 21 && recPad->getChannel() == 1, "playing = green");
            expect (find (82) != nullptr && find (82)->getVelocity() == 0, "empty track dark");
            state.tracks[1].state = (int) LoopTrack::State::Recording;
            out.clear();
            lp.render (state, out);
            expect (find (82)->getVelocity() == 5 && find (82)->getChannel() == 3, "recording = red, pulsing");

            const Template apc (TemplateId::ApcMiniMk2);
            out.clear();
            apc.render (state, out);
            const auto* apcPad = find (56);
            expect (apcPad != nullptr && apcPad->getChannel() == 7 && apcPad->getVelocity() == 21, "APC mk2: green at 100 %");
            const Template apc1 (TemplateId::ApcMini);
            out.clear();
            apc1.render (state, out);
            expect (find (56) != nullptr && find (56)->getVelocity() == 1, "first APC mini: green");
            expect (find (57) != nullptr && find (57)->getVelocity() == 4, "first APC mini: red blinking");
        }
    }
};

static Features16UiTests features16UiTests;
