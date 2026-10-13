// Tests of the 1.4 performance features in the engine: loop cutting / stutter, mute, multiply,
// halve, copy, loop length for one recording, tape stop and the performance filter.

#include "plugin/PluginProcessor.h"

using namespace livelooper;
using Cmd14 = LooperEngine::Command;

namespace
{
constexpr double kRate14 = 48000.0;
constexpr int kBlock14 = 480;
constexpr int kBar14 = 96000;          // 120 BPM, 4/4, 48 kHz
constexpr int kSixteenth14 = 6000;

/** The recorded test loop: a ramp, so every loop position has its own value. */
float ramp14 (int loopPos) { return 0.1f + 0.8f * (float) loopPos / (float) kBar14; }

struct Rig14
{
    LooperEngine engine;
    GlobalSettings global;
    std::array<TrackSettings, kNumTracks> track {};
    TimeInfo time;
    std::vector<float> inL, inR, outL, outR;
    std::vector<float> out;            // everything played since the last clearOut()
    long long pos = 0;                 // samples since the first recording started
    bool rampInput = false;
    float tape = 0.0f, filter = 0.0f;

    Rig14()
    {
        engine.prepare (kRate14, kBlock14, 10.0);
        global.monitorLevel = 0.0f;
        global.syncToHost = false;
        global.quantize = Quantize::Off;
        global.recAction = RecAction::RecPlay;
        global.internalTempo = 120.0;
        for (auto* v : { &inL, &inR, &outL, &outR })
            v->assign ((size_t) kBlock14, 0.0f);
    }

    void run (int blocks)
    {
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < kBlock14; ++i)
                inL[(size_t) i] = inR[(size_t) i] = rampInput ? ramp14 ((int) ((pos + i) % kBar14)) : 0.0f;
            engine.setGlobalSettings (global);
            for (int t = 0; t < kNumTracks; ++t)
                engine.setTrackSettings (t, track[(size_t) t]);
            engine.setPerformance (tape, filter);
            engine.process (inL.data(), inR.data(), outL.data(), outR.data(), kBlock14, time);
            out.insert (out.end(), outL.begin(), outL.end());
            pos += kBlock14;
        }
    }

    /** Records exactly one bar of the ramp on track t (fixed length, quantize off) and plays it. */
    void recordRampBar (int t = 0)
    {
        track[(size_t) t].measures = 1;
        rampInput = true;
        engine.queueCommand (Cmd14::RecPlay, t);
        run (kBar14 / kBlock14);
        rampInput = false;
        track[(size_t) t].measures = 0;
    }

    /** sample played at absolute position p (since the recording started), p >= the first clearOut position */
    float at (long long p) const { return out[(size_t) (p - outStart)]; }
    void clearOut() { out.clear(); outStart = pos; }
    long long outStart = 0;

    float maxStep (long long from, long long to) const
    {
        float m = 0.0f;
        for (long long p = from + 1; p < to; ++p)
            m = std::max (m, std::abs (at (p) - at (p - 1)));
        return m;
    }
};
} // namespace

class Feature14EngineTests : public juce::UnitTest
{
public:
    Feature14EngineTests() : juce::UnitTest ("1.4 loop cutting, mute, loop moves, tape stop", "livelooper") {}

    void runTest() override
    {
        beginTest ("Loop cutting: slice jump lands on the next 16th, crossfaded, stays (tap) and returns in sync");
        {
            Rig14 r;
            r.recordRampBar();
            auto& tr = r.engine.getTrack (0);
            expect (tr.getState() == LoopTrack::State::Playing);
            expectEquals (tr.getLength(), kBar14);
            expectEquals (r.engine.sliceStart (0, 3), kBar14 / 2, "slice 4 of 6 snapped to the 16th grid (beat 3)");
            expectEquals (r.engine.sliceStart (0, 1), 3 * kSixteenth14, "1/6 bar = 2.67 16ths -> 3 16ths");
            expectEquals (r.engine.sliceStart (0, 1) % kSixteenth14, 0, "slices start on 16th notes");

            r.global.quantize = Quantize::Beat;
            r.run (21);                                  // loop position 10080
            r.clearOut();
            const long long cmdPos = r.pos;
            r.engine.queueCommand (Cmd14::Cut, 0, 3);
            r.run (20);
            const long long jump = (cmdPos / kSixteenth14 + 1) * kSixteenth14;       // next 16th
            expectWithinAbsoluteError (r.at (jump - 10), ramp14 ((int) ((jump - 10) % kBar14)), 1.0e-4f, "before the 16th: running position");
            expectWithinAbsoluteError (r.at (jump + 300), ramp14 (kBar14 / 2 + 300), 1.0e-4f, "after the jump: slice 4");
            expect (r.maxStep (jump - 50, jump + 400) < 0.01f, "crossfaded, no click");
            expect (r.engine.getTrackStatus (0).cut.load());

            // a tap stays at the new position (mlr cut)
            const long long later = jump + 7000;
            expectWithinAbsoluteError (r.at (later), ramp14 ((int) ((kBar14 / 2 + 7000) % kBar14)), 1.0e-4f);

            // back in sync: the running position as if it never jumped
            r.clearOut();
            const long long offPos = r.pos;
            r.engine.queueCommand (Cmd14::CutOff, 0);
            r.run (4);
            expectWithinAbsoluteError (r.at (offPos + 400), ramp14 ((int) ((offPos + 400) % kBar14)), 1.0e-4f, "in sync again");
            expect (r.maxStep (offPos, offPos + 400) < 0.01f, "return crossfaded");
            expect (! tr.isCut());
        }

        beginTest ("Stutter: the slice repeats while held (an 8th here), release returns in sync");
        {
            Rig14 r;
            r.recordRampBar();
            r.global.quantize = Quantize::Beat;
            const int rep = r.engine.stutterLength (0);
            expectEquals (rep, 12000, "1/6 of a bar is shorter than a beat: 8th note repeat");
            r.run (10);
            r.clearOut();
            const long long cmdPos = r.pos;
            r.engine.queueCommand (Cmd14::Cut, 0, 1);
            r.engine.queueCommand (Cmd14::StutterOn, 0);
            r.run (100);
            const long long jump = (cmdPos / kSixteenth14 + 1) * kSixteenth14;
            const int s1 = r.engine.sliceStart (0, 1);
            expectWithinAbsoluteError (r.at (jump + 1000), ramp14 (s1 + 1000), 1.0e-4f);
            expectWithinAbsoluteError (r.at (jump + rep + 1000), ramp14 (s1 + 1000), 1.0e-4f, "repeats");
            expectWithinAbsoluteError (r.at (jump + 3 * rep + 1000), ramp14 (s1 + 1000), 1.0e-4f, "still repeating");
            expect (r.maxStep (jump + rep - 300, jump + rep + 300) < 0.01f, "the repeat wrap is crossfaded");
            expect (r.engine.getTrack (0).isStuttering());

            r.clearOut();
            const long long offPos = r.pos;
            r.engine.queueCommand (Cmd14::CutOff, 0);
            r.run (4);
            expectWithinAbsoluteError (r.at (offPos + 500), ramp14 ((int) ((offPos + 500) % kBar14)), 1.0e-4f, "back in sync");
            expect (! r.engine.getTrack (0).isCut());
        }

        beginTest ("Cutting is ignored on stopped tracks, stop cancels a pending cut");
        {
            Rig14 r;
            r.recordRampBar();
            r.engine.queueCommand (Cmd14::Stop, 0);
            r.run (2);
            r.engine.queueCommand (Cmd14::Cut, 0, 2);
            r.run (2);
            expect (! r.engine.getTrack (0).isCut());
            expect (! r.engine.getTrackStatus (0).cut.load());
        }

        beginTest ("Mute: silent at once (declicked), keeps running, unmute is in sync");
        {
            Rig14 r;
            r.recordRampBar();
            r.run (5);
            r.track[0].muted = true;
            r.clearOut();
            r.run (10);
            float peak = 0.0f;
            for (long long p = r.outStart + 400; p < r.pos; ++p)
                peak = std::max (peak, std::abs (r.at (p)));
            expectEquals (peak, 0.0f, "muted");
            expect (r.maxStep (r.outStart, r.outStart + 400) < 0.01f, "fades in 5 ms");
            expect (r.engine.getTrack (0).getState() == LoopTrack::State::Playing, "still running");
            r.track[0].muted = false;
            r.clearOut();
            r.run (3);
            expectWithinAbsoluteError (r.at (r.outStart + 600), ramp14 ((int) ((r.outStart + 600) % kBar14)), 1.0e-4f);
        }

        beginTest ("Multiply x2 duplicates the loop, halve keeps the first half, both undoable");
        {
            Rig14 r;
            r.recordRampBar();
            auto& tr = r.engine.getTrack (0);
            r.engine.queueCommand (Cmd14::Multiply, 0);
            r.run (1);
            expectEquals (tr.getLength(), 2 * kBar14);
            std::vector<float> l ((size_t) tr.getLength()), rr ((size_t) tr.getLength());
            tr.readLoop (l.data(), rr.data());
            bool same = true;
            for (int i = 0; i < kBar14; ++i)
                same = same && l[(size_t) i] == l[(size_t) (i + kBar14)];
            expect (same, "second half = first half");
            expect (tr.getState() == LoopTrack::State::Playing, "keeps playing");
            expectWithinAbsoluteError ((float) r.engine.getTrackStatus (0).lengthPpq.load(), 8.0f, 1.0e-3f);

            r.engine.queueCommand (Cmd14::Undo, 0);
            r.run (1);
            expectEquals (tr.getLength(), kBar14, "undo");
            r.engine.queueCommand (Cmd14::Redo, 0);
            r.run (1);
            expectEquals (tr.getLength(), 2 * kBar14, "redo");

            r.engine.queueCommand (Cmd14::Halve, 0);
            r.engine.queueCommand (Cmd14::Halve, 0);
            r.run (1);
            expectEquals (tr.getLength(), kBar14 / 2, "halved twice");
            r.clearOut();
            r.run (300);
            float maxv = 0.0f;
            for (auto v : r.out)
                maxv = std::max (maxv, v);
            expect (maxv < ramp14 (kBar14 / 2) + 1.0e-3f, "only the first half is heard");
            r.engine.queueCommand (Cmd14::Undo, 0);
            r.run (1);
            expectEquals (tr.getLength(), kBar14);
        }

        beginTest ("Multiply needs memory, recording tracks are left alone");
        {
            Rig14 r;
            r.track[0].measures = 4;          // 8 s of a 10 s maximum
            r.rampInput = true;
            r.engine.queueCommand (Cmd14::RecPlay, 0);
            r.run (4 * kBar14 / kBlock14);
            r.engine.queueCommand (Cmd14::Multiply, 0);
            r.run (1);
            expectEquals (r.engine.getTrack (0).getLength(), 4 * kBar14, "16 s would not fit");

            r.track[1].measures = 0;
            r.engine.queueCommand (Cmd14::RecPlay, 1);
            r.run (10);
            r.engine.queueCommand (Cmd14::Multiply, 1);
            r.engine.queueCommand (Cmd14::Halve, 1);
            r.run (1);
            expect (r.engine.getTrack (1).getState() == LoopTrack::State::Recording);
        }

        beginTest ("Copy: the loop goes to an empty track, stopped, and starts in sync");
        {
            Rig14 r;
            r.recordRampBar();
            r.engine.queueCommand (Cmd14::Copy, 0, -1);
            r.run (1);
            auto& dst = r.engine.getTrack (1);
            expect (dst.getState() == LoopTrack::State::Stopped);
            expectEquals (dst.getLength(), kBar14);
            std::vector<float> a ((size_t) kBar14), b ((size_t) kBar14), x ((size_t) kBar14), y ((size_t) kBar14);
            r.engine.getTrack (0).readLoop (a.data(), b.data());
            dst.readLoop (x.data(), y.data());
            expect (a == x && b == y, "same audio");

            r.engine.queueCommand (Cmd14::Start, 1);
            r.run (2);
            r.clearOut();
            r.run (4);
            const long long p = r.outStart + 1000;
            expectWithinAbsoluteError (r.at (p), 2.0f * ramp14 ((int) (p % kBar14)), 1.0e-3f, "both tracks in phase");

            // explicit destination, but never onto a track with a loop
            r.engine.queueCommand (Cmd14::Copy, 1, 0);
            r.engine.queueCommand (Cmd14::Copy, 0, 3);
            r.run (1);
            expectEquals (r.engine.getTrack (3).getLength(), kBar14);
            expect (r.engine.getTrack (2).isEmpty());
            r.engine.queueCommand (Cmd14::Undo, 3);
            r.run (1);
            expect (r.engine.getTrack (3).isEmpty(), "undo removes the copy");
        }

        beginTest ("Loop length for one recording: running and pending recordings");
        {
            Rig14 r;
            r.engine.queueCommand (Cmd14::RecPlay, 0);
            r.rampInput = true;
            r.run (10);
            r.engine.queueCommand (Cmd14::SetRecLength, 0, 1);
            r.run (1);
            expect (r.engine.getTrackStatus (0).recordingSpan.load(), "fixed length shows a progress bar");
            expect (r.engine.getTrackStatus (0).progress.load() > 0.0f && r.engine.getTrackStatus (0).progress.load() < 0.1f);
            r.run (kBar14 / kBlock14);
            expect (r.engine.getTrack (0).getState() == LoopTrack::State::Playing, "finished by itself");
            expectEquals (r.engine.getTrack (0).getLength(), kBar14);

            // pending (quantized) recording: the length is used when it starts, then forgotten
            r.global.quantize = Quantize::Measure;
            r.run (7);
            r.engine.queueCommand (Cmd14::RecPlay, 1);
            r.engine.queueCommand (Cmd14::SetRecLength, 1, 2);
            r.run (2 * kBar14 / kBlock14 + kBar14 / kBlock14 + 2);
            expect (r.engine.getTrack (1).getState() == LoopTrack::State::Playing);
            expectEquals (r.engine.getTrack (1).getLength(), 2 * kBar14);
            r.engine.queueCommand (Cmd14::RecPlay, 2);
            r.run (kBar14 / kBlock14 * 3);
            expect (r.engine.getTrack (2).getState() == LoopTrack::State::Recording, "the next recording is free again");
        }

        beginTest ("Tape stop: slows to silence, spins back into sync, bit-exact when idle");
        {
            TapeStop ts;
            ts.prepare (kRate14);
            std::vector<float> l (4800), rr (4800), dl (4800);
            double ph = 0.0;
            auto fill = [&] {
                for (size_t i = 0; i < l.size(); ++i)
                {
                    l[i] = rr[i] = dl[i] = 0.5f * (float) std::sin (ph);
                    ph += 2.0 * juce::MathConstants<double>::pi * 440.0 / kRate14;
                }
            };
            fill();
            ts.process (l.data(), rr.data(), 4800);
            expect (l == dl, "idle = bit-exact");
            ts.setDepth (1.0f);
            float tail = 0.0f;
            for (int b = 0; b < 10; ++b)
            {
                fill();
                ts.process (l.data(), rr.data(), 4800);
                if (b == 9)
                    for (auto v : l) tail = std::max (tail, std::abs (v));
            }
            expect (ts.isActive());
            expect (tail < 1.0e-3f, "stopped tape is silent (no DC)");
            ts.setDepth (0.0f);
            for (int b = 0; b < 3; ++b)
            {
                fill();
                ts.process (l.data(), rr.data(), 4800);
            }
            fill();
            ts.process (l.data(), rr.data(), 4800);
            expect (! ts.isActive(), "back to normal");
            expect (l == dl, "in sync again: the live signal passes untouched");

            // in the engine: the loops stop, the track keeps running underneath
            Rig14 r;
            r.recordRampBar();
            r.tape = 1.0f;
            r.run (100);
            r.clearOut();
            r.run (5);
            float peak = 0.0f;
            for (auto v : r.out) peak = std::max (peak, std::abs (v));
            expect (peak < 1.0e-3f);
            expect (r.engine.getTrack (0).getState() == LoopTrack::State::Playing);
            r.tape = 0.0f;
            r.run (20);
            r.clearOut();
            r.run (2);
            expectWithinAbsoluteError (r.at (r.outStart + 100), ramp14 ((int) ((r.outStart + 100) % kBar14)), 1.0e-5f);
        }

        beginTest ("Performance filter: low-pass / high-pass sweep, off = bit-exact");
        {
            auto rms = [] (float amount, double freq) {
                PerformanceFilter f;
                f.prepare (kRate14);
                f.setAmount (amount);
                std::vector<float> l (9600), r (9600);
                for (size_t i = 0; i < l.size(); ++i)
                    l[i] = r[i] = (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * (double) i / kRate14);
                const auto dry = l;
                f.process (l.data(), r.data(), (int) l.size());
                if (amount == 0.0f)
                    return l == dry ? 1.0 : -1.0;
                double s = 0.0;
                for (size_t i = 4800; i < l.size(); ++i)
                    s += (double) l[i] * l[i];
                return std::sqrt (s / 4800.0);
            };
            expectEquals (rms (0.0f, 1000.0), 1.0, "off: untouched");
            expect (rms (-1.0f, 8000.0) < 0.707 * 0.05, "low-pass closed: highs gone");
            expect (rms (-1.0f, 60.0) > 0.5, "low-pass keeps the bass");
            expect (rms (1.0f, 60.0) < 0.707 * 0.05, "high-pass open: bass gone");
            expect (rms (1.0f, 12000.0) > 0.5, "high-pass keeps the highs");
        }
    }
};

static Feature14EngineTests feature14EngineTests;
