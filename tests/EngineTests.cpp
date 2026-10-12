// Tests of the looper engine: recording, quantize, overdub, undo/redo, stop modes,
// loop sync and transport handling. Runs at 48 kHz / 120 BPM, so
//   1 quarter note (ppq) = 24000 samples, 1 bar (4/4) = 96000 samples.

#include "engine/LooperEngine.h"

#include <juce_core/juce_core.h>

using namespace livelooper;
using Cmd = LooperEngine::Command;
using State = LoopTrack::State;

namespace
{
constexpr double kRate = 48000.0;
constexpr int kBlock = 480;
constexpr double kSpp = 24000.0;   // samples per quarter note at 120 BPM

struct Harness
{
    LooperEngine engine;
    GlobalSettings global;
    std::array<TrackSettings, kNumTracks> trackSettings;
    TimeInfo time;
    std::vector<float> inL, inR, outL, outR;
    long long samplePos = 0;      // absolute sample counter (drives the input generator)
    std::function<float (long long)> input = [] (long long) { return 0.0f; };

    explicit Harness (int maxBlock = kBlock, double seconds = 20.0)
    {
        engine.prepare (kRate, maxBlock, seconds);
        global.monitorLevel = 0.0f;           // only the loops in the output
        global.syncToHost = false;
        global.internalTempo = 120.0;
        global.quantize = Quantize::Off;
        global.recAction = RecAction::RecPlay;
        global.fadeMs = 100.0f;
        time.bpm = 120.0;
        apply();
    }

    void apply()
    {
        engine.setGlobalSettings (global);
        for (int t = 0; t < kNumTracks; ++t)
            engine.setTrackSettings (t, trackSettings[(size_t) t]);
    }

    /** Runs n blocks of size `block`. With hostPlaying the host ppq advances with the audio. */
    void run (int numBlocks, int block = kBlock)
    {
        inL.resize ((size_t) block); inR.resize ((size_t) block);
        outL.resize ((size_t) block); outR.resize ((size_t) block);
        for (int b = 0; b < numBlocks; ++b)
        {
            for (int i = 0; i < block; ++i)
                inL[(size_t) i] = inR[(size_t) i] = input (samplePos + i);
            apply();
            engine.process (inL.data(), inR.data(), outL.data(), outR.data(), block, time);
            samplePos += block;
            if (time.hostPlaying)
                time.ppq += block / kSpp;
        }
    }

    void press (Cmd c, int track = -1) { engine.queueCommand (c, track); }
    LoopTrack& track (int t) { return engine.getTrack (t); }

    std::vector<float> loopContent (int t)
    {
        auto& tr = track (t);
        std::vector<float> l ((size_t) tr.getLength()), r ((size_t) tr.getLength());
        tr.readLoop (l.data(), r.data());
        return l;
    }
};
} // namespace

//==============================================================================
class RecordingTests : public juce::UnitTest
{
public:
    RecordingTests() : juce::UnitTest ("Recording", "engine") {}

    void runTest() override
    {
        beginTest ("Free recording (quantize off) has the exact length between the presses");
        {
            Harness h;
            h.input = [] (long long) { return 0.3f; };
            h.press (Cmd::RecPlay, 0);
            h.run (1);
            expect (h.track (0).getState() == State::Recording);
            h.run (99);
            h.press (Cmd::RecPlay, 0);
            h.run (1);
            expectEquals (h.track (0).getLength(), 100 * kBlock);
            expect (h.track (0).getState() == State::Playing, "Rec > Play goes to play");
        }

        beginTest ("Playback reproduces the recorded audio");
        {
            Harness h;
            h.input = [] (long long n) { return n < 48000 ? 0.5f * (float) std::sin (n * 0.01) : 0.0f; };
            h.press (Cmd::RecPlay, 0);
            h.run (100);                       // 48000 samples
            h.press (Cmd::RecPlay, 0);
            h.run (10);                        // first 4800 samples of the second pass

            float maxErr = 0.0f;
            const long long passStart = 48000;
            // the last block of the run is samples 4320..4799 of the loop
            for (int i = 0; i < kBlock; ++i)
            {
                const long long loopIndex = 4320 + i;
                const float expected = 0.5f * (float) std::sin (loopIndex * 0.01);
                maxErr = std::max (maxErr, std::abs (h.outL[(size_t) i] - expected));
            }
            juce::ignoreUnused (passStart);
            expectLessThan (maxErr, 1.0e-5f);
        }

        beginTest ("Rec > Dub goes into overdub after the first recording");
        {
            Harness h;
            h.global.recAction = RecAction::RecDub;
            h.press (Cmd::RecPlay, 0);
            h.run (50);
            h.press (Cmd::RecPlay, 0);
            h.run (1);
            expect (h.track (0).getState() == State::Overdubbing);
            h.press (Cmd::RecPlay, 0);
            h.run (1);
            expect (h.track (0).getState() == State::Playing);
            h.press (Cmd::RecPlay, 0);
            h.run (1);
            expect (h.track (0).getState() == State::Overdubbing, "play -> dub");
        }

        beginTest ("Quantized recording starts and ends on bar lines (host sync)");
        {
            Harness h;
            h.global.syncToHost = true;
            h.global.quantize = Quantize::Measure;
            h.time.hostPlaying = true;
            h.time.ppq = 0.3;                  // somewhere in bar 1

            h.press (Cmd::RecPlay, 0);
            h.run (1);
            expect (h.engine.getPending (0) == LooperEngine::Pending::Record, "waits for the next bar");
            expect (h.track (0).getState() == State::Empty);

            while (h.time.ppq < 6.5)           // into bar 2 (ppq 4..8)
                h.run (1);
            expect (h.track (0).getState() == State::Recording);
            expectWithinAbsoluteError (h.track (0).getAnchorPpq(), 4.0, 1.0e-3);

            h.press (Cmd::RecPlay, 0);
            h.run (1);
            expect (h.engine.getPending (0) == LooperEngine::Pending::FinishRecord, "waits for the bar end");
            while (h.time.ppq < 8.5)
                h.run (1);
            expect (h.track (0).getState() == State::Playing);
            expectEquals (h.track (0).getLength(), 96000, "exactly one bar");
        }

        beginTest ("Retroactive finish: pressing just after the bar line ends the loop there");
        {
            Harness h;
            h.global.syncToHost = true;
            h.global.quantize = Quantize::Measure;
            h.time.hostPlaying = true;
            h.time.ppq = 4.0;
            h.press (Cmd::RecPlay, 0);
            h.run (1);
            expect (h.track (0).getState() == State::Recording);
            while (h.time.ppq < 8.2)
                h.run (1);
            h.press (Cmd::RecPlay, 0);
            h.run (1);
            expect (h.track (0).getState() == State::Playing);
            expectEquals (h.track (0).getLength(), 96000);
            expect (h.engine.getPending (0) == LooperEngine::Pending::None);
        }

        beginTest ("Beat quantize rounds to quarter notes");
        {
            Harness h;
            h.global.syncToHost = true;
            h.global.quantize = Quantize::Beat;
            h.time.hostPlaying = true;
            h.time.ppq = 1.5;
            h.press (Cmd::RecPlay, 0);
            h.run (1);
            while (h.time.ppq < 4.5) h.run (1);
            h.press (Cmd::RecPlay, 0);
            while (h.time.ppq < 5.2) h.run (1);
            expectEquals (h.track (0).getLength(), 3 * 24000, "beats 2..5 = 3 quarter notes");
        }

        beginTest ("Fixed measure count finishes the recording by itself");
        {
            Harness h;
            h.global.quantize = Quantize::Measure;
            h.trackSettings[1].measures = 2;
            h.press (Cmd::RecPlay, 1);
            h.run (1);
            expect (h.track (1).getState() == State::Recording, "internal clock starts immediately");
            h.run (450);                        // 216000 samples > 2 bars
            expect (h.track (1).getState() == State::Playing);
            expectEquals (h.track (1).getLength(), 192000);
        }

        beginTest ("Second press while waiting cancels the pending recording");
        {
            Harness h;
            h.global.syncToHost = true;
            h.global.quantize = Quantize::Measure;
            h.time.hostPlaying = true;
            h.time.ppq = 0.5;
            h.press (Cmd::RecPlay, 0);
            h.run (1);
            h.press (Cmd::RecPlay, 0);
            h.run (1);
            expect (h.engine.getPending (0) == LooperEngine::Pending::None);
            while (h.time.ppq < 4.5) h.run (1);
            expect (h.track (0).getState() == State::Empty);
        }

        beginTest ("Host blocks larger than the prepared block size are split");
        {
            Harness h (512);
            h.press (Cmd::RecPlay, 0);
            h.run (10, 4096);
            h.press (Cmd::RecPlay, 0);
            h.run (1, 4096);
            expectEquals (h.track (0).getLength(), 40960);
        }

        beginTest ("Stereo input routing (left only)");
        {
            Harness h;
            h.trackSettings[0].input = InputRoute::Left;
            std::vector<float> l (kBlock, 0.4f), r (kBlock, -0.4f), oL (kBlock), oR (kBlock);
            h.apply();
            h.engine.queueCommand (Cmd::RecPlay, 0);
            for (int b = 0; b < 20; ++b)
                h.engine.process (l.data(), r.data(), oL.data(), oR.data(), kBlock, h.time);
            h.engine.queueCommand (Cmd::RecPlay, 0);
            h.engine.process (l.data(), r.data(), oL.data(), oR.data(), kBlock, h.time);
            auto& tr = h.track (0);
            std::vector<float> dl ((size_t) tr.getLength()), dr ((size_t) tr.getLength());
            tr.readLoop (dl.data(), dr.data());
            expectWithinAbsoluteError (dl[5000], 0.4f, 1.0e-6f);
            expectWithinAbsoluteError (dr[5000], 0.4f, 1.0e-6f);
        }
    }
};

//==============================================================================
class OverdubUndoTests : public juce::UnitTest
{
public:
    OverdubUndoTests() : juce::UnitTest ("Overdub and Undo", "engine") {}

    static void recordConstant (Harness& h, float value, int blocks = 100)
    {
        h.input = [value] (long long) { return value; };
        h.press (Cmd::RecPlay, 0);
        h.run (blocks);
        h.press (Cmd::RecPlay, 0);
        h.input = [] (long long) { return 0.0f; };
        h.run (1);
    }

    void runTest() override
    {
        beginTest ("Overdub adds to the loop, undo and redo switch layers");
        {
            Harness h;
            recordConstant (h, 0.25f);
            h.press (Cmd::RecPlay, 0);          // play -> dub
            h.input = [] (long long) { return 0.25f; };
            h.run (101);                        // a full pass
            h.press (Cmd::RecPlay, 0);          // dub -> play
            h.input = [] (long long) { return 0.0f; };
            h.run (1);

            auto content = h.loopContent (0);
            expectWithinAbsoluteError (content[20000], 0.5f, 1.0e-5f);

            h.press (Cmd::Undo, 0);
            h.run (1);
            content = h.loopContent (0);
            expectWithinAbsoluteError (content[20000], 0.25f, 1.0e-5f);
            expect (h.track (0).canRedo());

            h.press (Cmd::Redo, 0);
            h.run (1);
            content = h.loopContent (0);
            expectWithinAbsoluteError (content[20000], 0.5f, 1.0e-5f);
        }

        beginTest ("A new overdub after undo discards the redo layer");
        {
            Harness h;
            recordConstant (h, 0.25f);
            h.press (Cmd::RecPlay, 0);
            h.input = [] (long long) { return 0.25f; };
            h.run (20);
            h.press (Cmd::RecPlay, 0);
            h.run (1);
            h.press (Cmd::Undo, 0);
            h.run (1);
            h.press (Cmd::RecPlay, 0);          // new dub
            h.run (1);
            expect (! h.track (0).canRedo());
        }

        beginTest ("Replace mode overwrites the loop");
        {
            Harness h;
            recordConstant (h, 0.25f);
            h.trackSettings[0].dubMode = DubMode::Replace;
            h.press (Cmd::RecPlay, 0);
            h.input = [] (long long) { return -0.1f; };
            h.run (101);
            h.press (Cmd::RecPlay, 0);
            h.run (1);
            auto content = h.loopContent (0);
            expectWithinAbsoluteError (content[20000], -0.1f, 1.0e-5f);
        }

        beginTest ("Undo of the first recording empties the track, redo brings it back");
        {
            Harness h;
            recordConstant (h, 0.25f);
            h.press (Cmd::Undo, 0);
            h.run (1);
            expect (h.track (0).getState() == State::Empty);
            h.press (Cmd::Redo, 0);
            h.run (1);
            expect (h.track (0).getState() == State::Stopped);
            expectEquals (h.track (0).getLength(), 48000);
        }

        beginTest ("Clear empties the track");
        {
            Harness h;
            recordConstant (h, 0.25f);
            h.press (Cmd::Clear, 0);
            h.run (1);
            expect (h.track (0).isEmpty());
            expect (! h.track (0).canUndo());
        }
    }
};

//==============================================================================
class PlaybackTests : public juce::UnitTest
{
public:
    PlaybackTests() : juce::UnitTest ("Playback", "engine") {}

    void runTest() override
    {
        auto recordRamp = [] (Harness& h, int t)
        {
            h.input = [] (long long n) { return (float) (n % 48000) / 48000.0f; };
            h.press (Cmd::RecPlay, t);
            h.run (100);
            h.press (Cmd::RecPlay, t);
            h.input = [] (long long) { return 0.0f; };
            h.run (1);
        };

        beginTest ("Reverse plays the loop backwards");
        {
            Harness h;
            recordRamp (h, 0);
            h.trackSettings[0].reverse = true;
            h.run (20);
            int falling = 0;
            for (int i = 1; i < kBlock; ++i)
                if (h.outL[(size_t) i] < h.outL[(size_t) i - 1])
                    ++falling;
            expectGreaterThan (falling, kBlock - 5);
        }

        beginTest ("One shot stops at the end of the loop");
        {
            Harness h;
            recordRamp (h, 0);
            h.trackSettings[0].oneShot = true;
            h.run (110);
            expect (h.track (0).getState() == State::Stopped);
        }

        beginTest ("Stop mode 'Loop End' waits for the end of the loop");
        {
            Harness h;
            recordRamp (h, 0);
            h.trackSettings[0].stopMode = StopMode::LoopEnd;
            h.run (10);
            h.press (Cmd::Stop, 0);
            h.run (50);
            expect (h.track (0).getState() == State::Playing, "still playing mid loop");
            h.run (50);
            expect (h.track (0).getState() == State::Stopped);
        }

        beginTest ("Stop mode 'Fade Out' fades over the fade time");
        {
            Harness h;
            recordRamp (h, 0);
            h.global.fadeMs = 200.0f;          // 9600 samples = 20 blocks
            h.trackSettings[0].stopMode = StopMode::FadeOut;
            h.run (2);
            h.press (Cmd::Stop, 0);
            h.run (10);
            expect (h.track (0).getState() == State::Playing && h.track (0).isStopping());
            h.run (12);
            expect (h.track (0).getState() == State::Stopped);
        }

        beginTest ("Level and pan");
        {
            Harness h;
            h.input = [] (long long) { return 0.5f; };
            h.press (Cmd::RecPlay, 0);
            h.run (100);
            h.press (Cmd::RecPlay, 0);
            h.input = [] (long long) { return 0.0f; };
            h.trackSettings[0].level = 2.0f;
            h.trackSettings[0].pan = 1.0f;       // hard right
            h.run (20);
            expectWithinAbsoluteError (h.outR[100], 1.0f, 1.0e-5f);
            expectWithinAbsoluteError (h.outL[100], 0.0f, 1.0e-5f);
        }

        beginTest ("All Start/Stop toggles every track");
        {
            Harness h;
            recordRamp (h, 0);
            recordRamp (h, 2);
            h.press (Cmd::AllStartStop);
            h.run (1);
            expect (h.track (0).getState() == State::Stopped && h.track (2).getState() == State::Stopped);
            h.press (Cmd::AllStartStop);
            h.run (1);
            expect (h.track (0).getState() == State::Playing && h.track (2).getState() == State::Playing);
            expect (h.track (1).getState() == State::Empty);
        }

        beginTest ("Clear All");
        {
            Harness h;
            recordRamp (h, 0);
            recordRamp (h, 4);
            h.press (Cmd::ClearAll);
            h.run (1);
            for (int t = 0; t < kNumTracks; ++t)
                expect (h.track (t).isEmpty());
        }

        beginTest ("Track FX routing sends the track to the FX bus");
        {
            Harness h;
            h.input = [] (long long) { return 0.5f; };
            h.press (Cmd::RecPlay, 0);
            h.run (100);
            h.press (Cmd::RecPlay, 0);
            h.input = [] (long long) { return 0.0f; };
            h.trackSettings[0].trackFx = true;
            FxSettings lpf;
            lpf.enabled = true;
            lpf.type = FxType::Tremolo;
            lpf.p1 = 1.0f;
            lpf.p2 = 0.0f;
            lpf.mix = 1.0f;
            h.engine.setTrackFx (0, lpf);
            h.run (40);
            float minV = 1.0f, maxV = 0.0f;
            for (auto v : h.outL) { minV = std::min (minV, v); maxV = std::max (maxV, v); }
            expectLessThan (maxV, 0.5f + 1.0e-4f);
            expectLessThan (minV, maxV, "tremolo modulates the track");
        }
    }
};

//==============================================================================
class TimelineTests : public juce::UnitTest
{
public:
    TimelineTests() : juce::UnitTest ("Timeline and Sync", "engine") {}

    void runTest() override
    {
        beginTest ("Loop sync realigns playback after a host jump");
        {
            Harness h;
            h.global.syncToHost = true;
            h.global.quantize = Quantize::Measure;
            h.time.hostPlaying = true;
            h.time.ppq = 0.0;
            h.press (Cmd::RecPlay, 0);
            h.run (1);
            while (h.time.ppq < 3.9) h.run (1);
            h.press (Cmd::RecPlay, 0);
            while (h.time.ppq < 4.5) h.run (1);
            expectEquals (h.track (0).getLength(), 96000);

            h.time.ppq = 10.5;                   // user relocates the host to bar 3, beat 3.5
            h.run (1);
            // position after the block: (10.5 - 0) * 24000 mod 96000 + block
            const int expected = (int) std::fmod (10.5 * kSpp, 96000.0) + kBlock;
            expectEquals (h.track (0).getPlayPosition(), expected);
        }

        beginTest ("Play waits for the next bar and starts in phase");
        {
            Harness h;
            h.global.syncToHost = true;
            h.global.quantize = Quantize::Measure;
            h.time.hostPlaying = true;
            h.time.ppq = 0.0;
            h.press (Cmd::RecPlay, 0);
            h.run (1);
            while (h.time.ppq < 7.95) h.run (1);
            h.press (Cmd::RecPlay, 0);           // finishes at 8.0 -> 2 bars
            while (h.time.ppq < 8.2) h.run (1);
            expectEquals (h.track (0).getLength(), 192000);
            h.press (Cmd::Stop, 0);
            h.run (1);
            expect (h.track (0).getState() == State::Stopped);
            while (h.time.ppq < 13.0) h.run (1);
            h.press (Cmd::RecPlay, 0);
            h.run (1);
            expect (h.engine.getPending (0) == LooperEngine::Pending::Play);
            while (h.time.ppq < 16.1) h.run (1);
            expect (h.track (0).getState() == State::Playing);
            // bar 5 (ppq 16) is the start of the 2-bar loop again
            const int expected = (int) std::fmod ((h.time.ppq) * kSpp, 192000.0);
            expectWithinAbsoluteError (h.track (0).getPlayPosition(), expected, kBlock);
        }

        beginTest ("Follow host transport starts and stops the loops with the host");
        {
            Harness h;
            h.global.syncToHost = true;
            h.engine.setFollowHostTransport (true);
            h.time.hostPlaying = true;
            h.time.ppq = 0.0;
            h.press (Cmd::RecPlay, 0);
            h.run (100);
            h.press (Cmd::RecPlay, 0);
            h.run (1);
            expect (h.track (0).getState() == State::Playing);
            h.time.hostPlaying = false;
            h.run (2);
            expect (h.track (0).getState() == State::Stopped);
            h.time.hostPlaying = true;
            h.time.ppq = 0.0;
            h.run (1);
            expect (h.track (0).getState() == State::Playing);
        }

        beginTest ("Internal clock runs while loops play and stops when idle");
        {
            Harness h;
            expect (! h.engine.isTimelineRunning());
            h.press (Cmd::RecPlay, 0);
            h.run (10);
            expect (h.engine.isTimelineRunning());
            expectWithinAbsoluteError (h.engine.getTimelinePpq(), 10.0 * kBlock / kSpp, 1.0e-6);
            h.press (Cmd::Clear, 0);
            h.run (2);
            expect (! h.engine.isTimelineRunning());
        }

        beginTest ("Rhythm is silent when off and plays on the beat when on");
        {
            Harness h;
            h.run (20);
            float peak = 0.0f;
            for (auto v : h.outL) peak = std::max (peak, std::abs (v));
            expectEquals (peak, 0.0f);

            h.engine.setRhythm (true, 0, 1.0f);   // metronome
            h.run (1);
            peak = 0.0f;
            for (auto v : h.outL) peak = std::max (peak, std::abs (v));
            expectGreaterThan (peak, 0.05f, "click on beat 1");
        }
    }
};

static RecordingTests recordingTests;
static OverdubUndoTests overdubUndoTests;
static PlaybackTests playbackTests;
static TimelineTests timelineTests;
