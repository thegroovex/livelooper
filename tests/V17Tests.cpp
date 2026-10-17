// v1.7 (branch v1.7-wip): latency measurement on synthetic signals, latency compensation of first
// recordings, overdubs and captures, velocity curves (Push 1, K.O. II, generic MIDI), the generic MIDI
// feedback profile, Push 1 display bars, "Follow host transport" on by default, new host parameters.

#include "engine/LooperEngine.h"
#include "engine/LatencyMeasure.h"
#include "engine/Velocity.h"
#include "plugin/PluginProcessor.h"
#include "plugin/AppSettings.h"
#include "plugin/MidiFeedback.h"
#include "plugin/push/PushSurface.h"
#include "plugin/controllers/Ko2.h"

using namespace livelooper;
using namespace livelooper::params;
using Cmd17 = LooperEngine::Command;
using State17 = LoopTrack::State;

namespace
{
constexpr double kRate17 = 48000.0;
constexpr int kBlock17 = 480;

/** Hosts send a trigger parameter back to 0 after a press (a trigger fires on the rising edge). */
void releaseTriggers17 (LiveLooperProcessor& proc)
{
    for (auto* p : proc.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (isTrigger (r->getParameterID()) && r->getValue() >= 0.5f)
                r->setValueNotifyingHost (0.0f);
}

struct Harness17
{
    LooperEngine engine;
    GlobalSettings global;
    std::array<TrackSettings, kNumTracks> trackSettings;
    TimeInfo time;
    std::vector<float> inL, inR, outL, outR;
    long long samplePos = 0;
    std::function<float (long long)> input = [] (long long) { return 0.0f; };
    std::vector<float> outputLog;                 // whole output (left), when logging
    bool logOutput = false;

    Harness17()
    {
        engine.prepare (kRate17, kBlock17, 20.0);
        global.monitorLevel = 0.0f;
        global.syncToHost = false;
        global.internalTempo = 120.0;
        global.quantize = Quantize::Off;
        global.recAction = RecAction::RecPlay;
        global.fadeMs = 10.0f;
        time.bpm = 120.0;
        engine.setRhythm (false, 0, 0.0f);
    }
    void run (int blocks)
    {
        inL.resize (kBlock17); inR.resize (kBlock17); outL.resize (kBlock17); outR.resize (kBlock17);
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < kBlock17; ++i)
                inL[(size_t) i] = inR[(size_t) i] = input (samplePos + i);
            engine.setGlobalSettings (global);
            for (int t = 0; t < kNumTracks; ++t)
                engine.setTrackSettings (t, trackSettings[(size_t) t]);
            engine.process (inL.data(), inR.data(), outL.data(), outR.data(), kBlock17, time);
            if (logOutput)
                outputLog.insert (outputLog.end(), outL.begin(), outL.end());
            samplePos += kBlock17;
        }
    }
    void press (Cmd17 c, int t = -1) { engine.queueCommand (c, t); }
    LoopTrack& track (int t) { return engine.getTrack (t); }
    std::vector<float> loop (int t)
    {
        auto& tr = track (t);
        std::vector<float> l ((size_t) tr.getLength()), r ((size_t) tr.getLength());
        tr.readLoop (l.data(), r.data());
        return l;
    }
};

int argmaxAbs (const std::vector<float>& v, size_t from = 0, size_t to = 0)
{
    if (to == 0 || to > v.size()) to = v.size();
    int best = -1; float m = 0.0f;
    for (size_t i = from; i < to; ++i)
        if (std::abs (v[i]) > m) { m = std::abs (v[i]); best = (int) i; }
    return best;
}

struct TempSettings17
{
    juce::File dir { juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ll-v17-" + juce::String (juce::Random::getSystemRandom().nextInt())) };
    TempSettings17()  { dir.createDirectory(); appsettings::setStorageFolderForTests (dir); }
    ~TempSettings17() { appsettings::setStorageFolderForTests ({}); dir.deleteRecursively(); }
};

/** Runs a LatencyMeasure against a simulated loopback: output -> delay (samples) -> gain -> input + noise. */
LatencyMeasure::Status runLoopback (LatencyMeasure& m, std::function<int (int run)> delayForRun, float gain, float noise, int& result)
{
    m.start (kRate17, 5);
    std::vector<float> line (48000 * 2, 0.0f);   // output history
    long long t = 0;
    juce::Random rnd (42);
    std::vector<float> in (kBlock17), outL (kBlock17), outR (kBlock17);
    const int runLength = (int) std::lround (kRate17 * LatencyMeasure::kGapSeconds) + (int) std::lround (kRate17 * LatencyMeasure::kMaxLatencySeconds) + (int) m.getBurst().size();
    for (int b = 0; b < 2000 && m.isRunning(); ++b)
    {
        for (int i = 0; i < kBlock17; ++i)
        {
            const long long now = t + i;
            const int run = (int) (now / runLength);
            const int d = delayForRun (run);
            const long long src = now - d;
            const float v = src >= 0 ? line[(size_t) (src % (long long) line.size())] : 0.0f;
            in[(size_t) i] = v * gain + noise * (rnd.nextFloat() * 2.0f - 1.0f);
        }
        m.process (in.data(), in.data(), outL.data(), outR.data(), kBlock17);
        for (int i = 0; i < kBlock17; ++i)
            line[(size_t) ((t + i) % (long long) line.size())] = outL[(size_t) i];
        t += kBlock17;
        m.update();
    }
    result = m.getResultSamples();
    return m.getStatus();
}
} // namespace

//==============================================================================
class LatencyTests17 : public juce::UnitTest
{
public:
    LatencyTests17() : juce::UnitTest ("Latency measurement + compensation (1.7)", "livelooper") {}

    void runTest() override
    {
        beginTest ("detect(): finds the burst in noise at any gain / polarity, rejects silence and noise");
        {
            LatencyMeasure m;
            m.start (kRate17, 1);
            const auto& burst = m.getBurst();
            expect (burst.size() > 100, "a few ms burst");
            juce::Random rnd (7);
            for (const int at : { 300, 1234, 5000, 20000 })
                for (const float gain : { 1.0f, -0.25f, 0.05f })
                {
                    std::vector<float> rec (30000);
                    for (auto& v : rec) v = 0.003f * (rnd.nextFloat() * 2.0f - 1.0f);
                    for (size_t j = 0; j < burst.size(); ++j)
                        rec[(size_t) at + j] += gain * burst[j];
                    expectEquals (LatencyMeasure::detect (rec, burst), at, "offset " + juce::String (at) + " gain " + juce::String (gain));
                }
            std::vector<float> silent (30000, 0.0f);
            expectEquals (LatencyMeasure::detect (silent, burst), -1, "silence: nothing");
            std::vector<float> noise (30000);
            for (auto& v : noise) v = 0.2f * (rnd.nextFloat() * 2.0f - 1.0f);
            expectEquals (LatencyMeasure::detect (noise, burst), -1, "noise only: nothing");
            expectEquals (LatencyMeasure::median ({ 5, 1, 9 }), 5);
            expectEquals (LatencyMeasure::median ({}), -1);
            m.cancel();
        }

        beginTest ("Loopback ping: median of the runs = the round trip; no cable = no signal; jitter = unstable");
        {
            LatencyMeasure m;
            int result = -1;
            auto st = runLoopback (m, [] (int) { return 557; }, 0.5f, 0.002f, result);
            expect (st == LatencyMeasure::Status::Done, "detected");
            expectWithinAbsoluteError (result, 557, 1);
            expectWithinAbsoluteError (m.getResultMs(), 557.0 / 48.0, 0.05);
            expectEquals (m.getDetectedRuns(), 5);
            // one disturbed run does not move the median
            st = runLoopback (m, [] (int run) { return run == 2 ? 3000 : 1024; }, 0.4f, 0.002f, result);
            expect (st == LatencyMeasure::Status::Done, "one outlier: still done");
            expectWithinAbsoluteError (result, 1024, 1);
            // nothing comes back (no cable / microphone)
            st = runLoopback (m, [] (int) { return 500; }, 0.0f, 0.002f, result);
            expect (st == LatencyMeasure::Status::NoSignal, "no signal");
            expectEquals (result, -1);
            // runs that disagree (e.g. a microphone in a noisy room / a drifting device)
            st = runLoopback (m, [] (int run) { return 400 + run * 900; }, 0.5f, 0.002f, result);
            expect (st == LatencyMeasure::Status::Unstable, "unstable");
        }

        beginTest ("Compensation: a first recording is moved by the round trip (heard on the beat)");
        {
            static constexpr int L = 600;
            Harness17 h;
            h.engine.setInputLatency (L);
            long long s0 = 0;
            // the player hits on beat 2 of the loop - the interface delivers it L samples later
            h.input = [&s0] (long long n) { return n == s0 + 24000 + L ? 1.0f : 0.0f; };
            h.run (1);
            s0 = 0;
            h.press (Cmd17::RecPlay, 0);
            h.run (1);
            s0 = h.samplePos - kBlock17;       // recording started at the start of that block
            h.run (199);
            h.press (Cmd17::RecPlay, 0);       // finish: one bar (96000 samples)
            h.logOutput = true;
            const long long logStart = h.samplePos;
            h.run (1);
            expect (h.track (0).getState() == State17::Playing);
            h.run (220);
            const int at = argmaxAbs (h.outputLog);
            expect (at >= 0, "the hit is played back");
            const long long abs = logStart + at;
            expectWithinAbsoluteError ((int) ((abs - s0) % 96000), 24000, 2, "on beat 2 - not L samples late");
        }

        beginTest ("Compensation: overdubs write the input where it was heard");
        {
            static constexpr int L = 777;
            Harness17 h;
            h.press (Cmd17::RecPlay, 0);
            h.run (1);
            const long long s0 = h.samplePos - kBlock17;
            h.run (199);
            h.press (Cmd17::RecPlay, 0);       // silent 1-bar loop, playing
            h.run (1);
            h.engine.setInputLatency (L);
            // overdub during the third pass, the hit belongs to loop position 48000
            const long long hit = s0 + 2 * 96000 + 48000 + L;
            h.input = [hit] (long long n) { return n == hit ? 1.0f : 0.0f; };
            while (h.samplePos < s0 + 2 * 96000 - kBlock17) h.run (1);
            h.press (Cmd17::RecPlay, 0);       // play -> overdub
            h.run (1);
            expect (h.track (0).getState() == State17::Overdubbing);
            while (h.samplePos < s0 + 3 * 96000 - kBlock17) h.run (1);
            h.press (Cmd17::RecPlay, 0);       // overdub -> play
            h.run (2);
            const auto content = h.loop (0);
            expectWithinAbsoluteError (argmaxAbs (content), 48000, 1, "written L samples earlier");
            expectWithinAbsoluteError (content[48000], 1.0f, 1.0e-4f);
            // without compensation the same hit lands L samples late
            Harness17 h2;
            h2.press (Cmd17::RecPlay, 0); h2.run (1);
            const long long t0 = h2.samplePos - kBlock17;
            h2.run (199); h2.press (Cmd17::RecPlay, 0); h2.run (1);
            const long long hit2 = t0 + 2 * 96000 + 48000 + L;
            h2.input = [hit2] (long long n) { return n == hit2 ? 1.0f : 0.0f; };
            while (h2.samplePos < t0 + 2 * 96000 - kBlock17) h2.run (1);
            h2.press (Cmd17::RecPlay, 0); h2.run (1);
            while (h2.samplePos < t0 + 3 * 96000 - kBlock17) h2.run (1);
            h2.press (Cmd17::RecPlay, 0); h2.run (2);
            expectWithinAbsoluteError (argmaxAbs (h2.loop (0)), 48000 + L, 1, "uncompensated reference");
        }

        beginTest ("Compensation: a capture plays the captured input earlier by the round trip");
        {
            static constexpr int L = 512;
            Harness17 a, b;
            b.engine.setInputLatency (L);
            for (auto* h : { &a, &b })
            {
                h->input = [] (long long n) { return 0.2f * std::sin ((float) n * 0.01f); };
                h->global.captureBars = 1;
                // a playing loop on track 2 keeps the timeline running
                h->press (Cmd17::RecPlay, 1); h->run (200); h->press (Cmd17::RecPlay, 1); h->run (230);
                h->press (Cmd17::Capture, 0);
                h->run (1);
            }
            expect (! a.track (0).isEmpty() && ! b.track (0).isEmpty(), "both captured");
            const int len = a.track (0).getLength();
            expectEquals (b.track (0).getLength(), len);
            const int diff = ((b.track (0).getPlayPosition() - a.track (0).getPlayPosition()) % len + len) % len;
            expectEquals (diff, L, "the captured loop runs L samples ahead");
        }

        beginTest ("Processor: measured value per audio device + manual offset; off = 0; reported value as fallback");
        {
            TempSettings17 settings;
            LiveLooperProcessor proc { false, false };
            proc.prepareToPlay (48000.0, kBlock17);
            proc.setAudioDeviceInfo ("standalone: Test | In A | Out A | 48000 | 480", 1500);
            expectEquals (proc.getLatencySource(), 1, "nothing measured: the reported round trip");
            expectEquals (proc.getLatencyCompensationSamples(), 1500);
            proc.setMeasuredLatencyMs (10.0);
            expectEquals (proc.getLatencySource(), 0);
            expectEquals (proc.getLatencyCompensationSamples(), 480);
            proc.setLatencyOffsetMs (2.5f);
            expectEquals (proc.getLatencyCompensationSamples(), 600, "measured + offset");
            proc.setAudioDeviceInfo ("standalone: Test | In B | Out B | 48000 | 480", 0);
            expectEquals (proc.getLatencySource(), 3, "another device: its own (missing) measurement");
            expectEquals (proc.getLatencyCompensationSamples(), 120, "only the offset");
            proc.setAudioDeviceInfo ("standalone: Test | In A | Out A | 48000 | 480", 1500);
            expectWithinAbsoluteError (proc.getMeasuredLatencyMs(), 10.0, 1.0e-6, "stored per device");
            proc.getState().getParameter (gLatComp)->setValueNotifyingHost (0.0f);
            expectEquals (proc.getLatencyCompensationSamples(), 0, "compensation off");
            proc.getState().getParameter (gLatComp)->setValueNotifyingHost (1.0f);
            proc.clearMeasuredLatency();
            expectEquals (proc.getLatencySource(), 1);
            // the engine gets it with the next update
            proc.updateLatency();
            juce::AudioBuffer<float> buf (2, kBlock17);
            juce::MidiBuffer midi;
            buf.clear();
            proc.processBlock (buf, midi);
            expectEquals (proc.getEngine().getInputLatency(), 1500 + 120);
            expectEquals (proc.getEngine().getTrack (0).getInputLatency(), 1500 + 120, "every track");
        }

        beginTest ("Processor: lat_measure starts the ping; loopback through processBlock stores the result");
        {
            TempSettings17 settings;
            LiveLooperProcessor proc { false, false };
            proc.prepareToPlay (48000.0, kBlock17);
            proc.getState().getParameter (gMonitor)->setValueNotifyingHost (0.0f);
            proc.setAudioDeviceInfo ("standalone: Loop | X | Y | 48000 | 480", 0);
            proc.pressTrigger (gLatMeasure);
            juce::AudioBuffer<float> buf (2, kBlock17);
            juce::MidiBuffer midi;
            static constexpr int D = 1100;
            std::vector<float> line (96000, 0.0f);
            long long t = 0;
            bool started = false;
            for (int b = 0; b < 1500; ++b)
            {
                for (int i = 0; i < kBlock17; ++i)
                {
                    const long long src = t + i - D;
                    const float v = src >= 0 ? 0.5f * line[(size_t) (src % 96000)] : 0.0f;
                    buf.setSample (0, i, v);
                    buf.setSample (1, i, v);
                }
                proc.processBlock (buf, midi);
                for (int i = 0; i < kBlock17; ++i)
                    line[(size_t) ((t + i) % 96000)] = buf.getSample (0, i);
                t += kBlock17;
                proc.updateLatency();
                started = started || proc.getLatencyMeasure().isRunning();
                if (started && ! proc.getLatencyMeasure().isRunning())
                    break;
            }
            expect (started, "the trigger started a measurement");
            expect (proc.getLatencyMeasure().getStatus() == LatencyMeasure::Status::Done, "done");
            expectWithinAbsoluteError (proc.getMeasuredLatencyMs(), D / 48.0, 0.05, "stored for the device");
            expectEquals (proc.getLatencyCompensationSamples(), D);
        }

        beginTest ("New host parameters 490 - 497 are appended (ids never change)");
        {
            LiveLooperProcessor proc { false, false };
            const auto& ps = proc.getParameters();
            expectEquals (ps.size(), kNumParameters);
            expectEquals (kNumParameters, 501);
            const juce::StringArray ids { "lat_comp", "lat_offset", "lat_measure", "vel_push", "vel_ko2", "vel_midi", "midi_fb", "push_progress" };
            for (int i = 0; i < ids.size(); ++i)
                if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (ps[489 + i]))
                    expectEquals (p->getParameterID(), ids[i], "index " + juce::String (489 + i));
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (ps[488]))
                expectEquals (p->getParameterID(), juce::String ("pfx_hold_ramp"), "the last v1.6 parameter stays in place");
            expect (isTrigger (gLatMeasure));
            expectEquals (englishName (gLatOffset), juce::String ("Latency Offset"));
            expectEquals (englishName (gVelKo2), juce::String ("Sampler Pad Velocity Curve"));
        }
    }
};

//==============================================================================
class Controller17Tests : public juce::UnitTest
{
public:
    Controller17Tests() : juce::UnitTest ("Velocity curves, MIDI feedback, Push 1 bars (1.7)", "livelooper") {}

    void runTest() override
    {
        using velocity::Curve;
        beginTest ("Velocity curves: soft lifts, hard lowers, fixed = 127, normal unchanged, 0 stays a note off");
        {
            for (int v = 1; v <= 127; ++v)
            {
                expectEquals (velocity::apply (Curve::Normal, v), v);
                expectEquals (velocity::apply (Curve::Fixed, v), 127);
                expect (velocity::apply (Curve::Soft, v) >= v);
                expect (velocity::apply (Curve::Hard, v) <= v);
                expect (velocity::apply (Curve::Hard, v) >= 1, "never a note off");
                if (v > 1)
                {
                    expect (velocity::apply (Curve::Soft, v) >= velocity::apply (Curve::Soft, v - 1), "monotonic");
                    expect (velocity::apply (Curve::Hard, v) >= velocity::apply (Curve::Hard, v - 1), "monotonic");
                }
            }
            expectEquals (velocity::apply (Curve::Soft, 32), 64);
            expectEquals (velocity::apply (Curve::Hard, 64), 32);
            for (auto c : { Curve::Soft, Curve::Normal, Curve::Hard, Curve::Fixed })
                expectEquals (velocity::apply (c, 0), 0);
            expectEquals (velocity::apply (Curve::Soft, 127), 127);
            expectEquals (velocity::apply (Curve::Hard, 127), 127);
        }

        beginTest ("Generic MIDI: a learned note sets a continuous parameter through the curve (vel_midi)");
        {
            LiveLooperProcessor proc { false, false };
            proc.prepareToPlay (48000.0, kBlock17);
            const int idx = proc.getParamIndex (gMaster);
            proc.getMidiMap().assign ({ 0, false, 60 }, idx);
            auto send = [&proc] (int vel) {
                juce::AudioBuffer<float> buf (2, kBlock17); buf.clear();
                juce::MidiBuffer midi;
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) vel), 0);
                proc.processBlock (buf, midi);
                return proc.getState().getParameter (gMaster)->getValue();
            };
            expectWithinAbsoluteError (send (64), 64.0f / 127.0f, 0.01f, "normal");
            proc.setVelocityCurve (LiveLooperProcessor::VelocitySource::Midi, Curve::Hard);
            expect (proc.getVelocityCurve (LiveLooperProcessor::VelocitySource::Midi) == Curve::Hard);
            expectWithinAbsoluteError (send (64), 32.0f / 127.0f, 0.01f, "hard");
            proc.setVelocityCurve (LiveLooperProcessor::VelocitySource::Midi, Curve::Fixed);
            expectWithinAbsoluteError (send (20), 1.0f, 0.01f, "fixed");
            proc.setVelocityCurve (LiveLooperProcessor::VelocitySource::Midi, Curve::Soft);
            expectWithinAbsoluteError (send (32), 64.0f / 127.0f, 0.01f, "soft");
        }

        beginTest ("K.O. II: the punch amount follows its velocity curve (vel_ko2)");
        {
            LiveLooperProcessor proc { false, false };
            proc.prepareToPlay (48000.0, kBlock17);
            proc.simulateControllers (true, false);
            auto* d = proc.getKo2Device();
            expect (d != nullptr);
            const int n = ko2::noteFor (2, 3);
            auto hit = [&] (int vel) {
                d->injectMessage (juce::MidiMessage::noteOn (1, n, (juce::uint8) vel));
                const float a = proc.getState().getParameter (gPfxAmount)->getValue();
                d->injectMessage (juce::MidiMessage::noteOff (1, n));
                return a;
            };
            expectWithinAbsoluteError (hit (40), punchAmountFromVelocity (40), 0.01f, "normal");
            proc.setVelocityCurve (LiveLooperProcessor::VelocitySource::Ko2, Curve::Fixed);
            expectWithinAbsoluteError (hit (40), punchAmountFromVelocity (127), 0.01f, "fixed: full amount");
            proc.setVelocityCurve (LiveLooperProcessor::VelocitySource::Ko2, Curve::Soft);
            expectWithinAbsoluteError (hit (32), punchAmountFromVelocity (64), 0.01f, "soft");
            proc.simulateControllers (false, false);
        }

        beginTest ("Push 1: pad velocity goes through vel_push (step velocity in STEP mode)");
        {
            LiveLooperProcessor proc { false, false };
            proc.prepareToPlay (48000.0, kBlock17);
            push::PushSurface surface (proc);
            surface.handleMidi (juce::MidiMessage::controllerEvent (1, push::cc::scales, 127));   // STEP mode
            surface.handleMidi (juce::MidiMessage::controllerEvent (1, push::cc::scales, 0));
            expect (surface.getMode() == push::PushSurface::Mode::Step);
            proc.setVelocityCurve (LiveLooperProcessor::VelocitySource::Push, Curve::Fixed);
            surface.handleMidi (juce::MidiMessage::noteOn (1, push::padNote (0, 2), (juce::uint8) 30));
            surface.handleMidi (juce::MidiMessage::noteOff (1, push::padNote (0, 2)));
            // the kick row (row 0), step 3 was set with velocity 127 (fixed), not 30
            bool found127 = false;
            for (int slot = 0; slot < BeatPatternBank::kNumSlots && ! found127; ++slot)
                found127 = proc.getUserPatterns().getCell (slot, 0, 2) == 127;
            expect (found127, "step velocity = fixed 127");
        }

        beginTest ("MIDI feedback profile: on / off / blink values, only changes are sent, soft blink, CC values");
        {
            MidiFeedback fb;
            MidiFeedback::Profile p;
            p.onValue = 100; p.offValue = 1; p.blinkValue = 3; p.softBlink = false;
            fb.setProfile (p);
            std::vector<MidiFeedback::Item> items (3);
            items[0].key = { 0, false, 36 }; items[0].led = MidiFeedback::Led::On;
            items[1].key = { 1, true, 20 };  items[1].led = MidiFeedback::Led::Blink;
            items[2].key = { 0, true, 7 };   items[2].value = 90;
            std::vector<juce::MidiMessage> out;
            fb.render (items, true, out, false);
            expectEquals ((int) out.size(), 3);
            expect (out[0].isNoteOn() && out[0].getNoteNumber() == 36 && out[0].getVelocity() == 100 && out[0].getChannel() == 1);
            expect (out[1].isController() && out[1].getControllerNumber() == 20 && out[1].getControllerValue() == 3 && out[1].getChannel() == 2);
            expect (out[2].isController() && out[2].getControllerValue() == 90);
            out.clear();
            fb.render (items, false, out, false);
            expectEquals ((int) out.size(), 0, "nothing changed");
            items[0].led = MidiFeedback::Led::Off;
            fb.render (items, true, out, false);
            expectEquals ((int) out.size(), 1);
            expectEquals ((int) out[0].getVelocity(), 1, "off value");
            out.clear();
            fb.render (items, true, out, true);
            expectEquals ((int) out.size(), 3, "full resend");
            // soft blink: on / off with the phase
            p.softBlink = true; fb.setProfile (p);
            out.clear();
            fb.render (items, true, out, false);
            expectEquals (fb.valueFor (items[1], true), 100);
            expectEquals (fb.valueFor (items[1], false), 1);
            out.clear();
            fb.render (items, false, out, false);
            expectEquals ((int) out.size(), 1, "only the blinking LED changes");
            expectEquals (out[0].getControllerValue(), 1);
        }

        beginTest ("Processor feedback: learned REC/PLAY blinks while recording, lights while playing; toggles; CC knobs");
        {
            TempSettings17 settings;
            LiveLooperProcessor proc { false, false };
            proc.prepareToPlay (48000.0, kBlock17);
            proc.getState().getParameter (gQuantize)->setValueNotifyingHost (0.0f);
            proc.getState().getParameter (gRecAction)->setValueNotifyingHost (1.0f);
            proc.getMidiMap().assign ({ 0, false, 36 }, proc.getParamIndex (trackId (0, kRecPlay)));
            proc.getMidiMap().assign ({ 0, true, 20 }, proc.getParamIndex (trackId (0, kReverse)));
            proc.getMidiMap().assign ({ 0, true, 7 }, proc.getParamIndex (gMaster));
            MidiFeedback::Profile p; p.onValue = 127; p.offValue = 0; p.blinkValue = 2; p.softBlink = false;
            proc.setMidiFeedbackProfile (p);
            auto stateOf = [&proc] (int number, bool cc) {
                for (const auto& it : proc.buildMidiFeedbackItems())
                    if (it.key.number == number && it.key.isCC == cc)
                        return it;
                return MidiFeedback::Item {};
            };
            expect (stateOf (36, false).led == MidiFeedback::Led::Off, "empty track: off");
            juce::AudioBuffer<float> buf (2, kBlock17); juce::MidiBuffer midi;
            auto run = [&] (int n) { for (int i = 0; i < n; ++i) { buf.clear(); proc.processBlock (buf, midi); releaseTriggers17 (proc); } };
            proc.pressTrigger (trackId (0, kRecPlay)); run (2);
            expect (stateOf (36, false).led == MidiFeedback::Led::Blink, "recording: blink");
            run (100);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (30);
            proc.pressTrigger (trackId (0, kRecPlay)); run (2);
            expect (stateOf (36, false).led == MidiFeedback::Led::On, "playing: on");
            proc.getState().getParameter (trackId (0, kReverse))->setValueNotifyingHost (1.0f);
            expect (stateOf (20, true).led == MidiFeedback::Led::On, "toggle on");
            proc.getState().getParameter (gMaster)->setValueNotifyingHost (0.5f);
            expectEquals (stateOf (7, true).value, 64, "knob on a CC sends its value");
            std::vector<juce::MidiMessage> out;
            proc.renderMidiFeedback (out, true);
            expectEquals ((int) out.size(), 3);
            bool rec127 = false;
            for (auto& m : out)
                rec127 = rec127 || (m.isNoteOn() && m.getNoteNumber() == 36 && m.getVelocity() == 127);
            expect (rec127, "note-on 36 with the on value");
            expect (! proc.isMidiFeedbackOn(), "off by default (experimental)");
        }

        beginTest ("Push 1 display: progress bars per track (default), state words when off; punch amount bar on line 4");
        {
            LiveLooperProcessor proc { false, false };
            proc.prepareToPlay (48000.0, kBlock17);
            proc.getState().getParameter (gQuantize)->setValueNotifyingHost (0.0f);
            proc.getState().getParameter (gRecAction)->setValueNotifyingHost (1.0f);
            proc.getState().getParameter (gMonitor)->setValueNotifyingHost (0.0f);
            push::PushSurface surface (proc);
            juce::AudioBuffer<float> buf (2, kBlock17); juce::MidiBuffer midi;
            auto run = [&] (int n) { for (int i = 0; i < n; ++i) { buf.clear(); proc.processBlock (buf, midi); releaseTriggers17 (proc); } };
            proc.pressTrigger (trackId (0, kRecPlay)); run (100);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (30);
            proc.pressTrigger (trackId (0, kRecPlay)); run (50);    // playing, half way
            expect (proc.isPushProgressShown(), "on by default");
            auto line3 = surface.buildDisplay()[2];
            expectEquals (line3.length(), push::kDisplayChars);
            expect (line3.startsWith (">P"), "selection mark + state letter");
            expect (line3.containsChar (push::glyph::block), "filled bar cells");
            expect (line3.containsChar (push::glyph::dash), "empty bar cells");
            const auto col = surface.progressColumn (0, 8);
            expectEquals (col.length(), 8);
            proc.getState().getParameter (gPushProgress)->setValueNotifyingHost (0.0f);
            line3 = surface.buildDisplay()[2];
            expect (line3.contains ("PLAY"), "state words when the bars are off");
            // a held punch FX: its amount as a bar on the status line
            proc.getState().getParameter (gPfxAmount)->setValueNotifyingHost (0.5f);
            proc.getState().getParameter (punchHoldId (4))->setValueNotifyingHost (1.0f);
            juce::String label;
            expectWithinAbsoluteError (surface.activeAmount (label), 0.5f, 0.01f);
            const auto status = surface.buildDisplay()[3];
            expect (status.containsChar (push::glyph::block), "bar on line 4");
            expect (status.contains ("50%"), "percent");
            proc.getState().getParameter (punchHoldId (4))->setValueNotifyingHost (0.0f);
            expect (surface.activeAmount (label) < 0.0f, "nothing held: no bar");
        }

        beginTest ("Follow host transport: on by default, a DAW stop stops the loops; can be switched off");
        {
            LiveLooperProcessor proc { false, false };
            auto* follow = dynamic_cast<juce::AudioParameterBool*> (proc.getState().getParameter (gFollow));
            expect (follow != nullptr && follow->get(), "default on");
            expectEquals (static_cast<juce::AudioProcessorParameter*> (follow)->getDefaultValue(), 1.0f);
            LooperEngine engine;
            expect (engine.getFollowHostTransport(), "engine default on");

            struct Head : juce::AudioPlayHead
            {
                bool playing = true; double ppq = 0.0;
                juce::Optional<PositionInfo> getPosition() const override
                {
                    PositionInfo p; p.setBpm (120.0); p.setTimeSignature (TimeSignature { 4, 4 });
                    p.setPpqPosition (ppq); p.setIsPlaying (playing);
                    return p;
                }
            } head;
            proc.setPlayHead (&head);
            proc.prepareToPlay (48000.0, kBlock17);
            proc.getState().getParameter (gQuantize)->setValueNotifyingHost (0.0f);
            proc.getState().getParameter (gRecAction)->setValueNotifyingHost (1.0f);
            juce::AudioBuffer<float> buf (2, kBlock17); juce::MidiBuffer midi;
            auto run = [&] (int n) { for (int i = 0; i < n; ++i) { buf.clear(); proc.processBlock (buf, midi); releaseTriggers17 (proc); if (head.playing) head.ppq += kBlock17 / 24000.0; } };
            proc.pressTrigger (trackId (0, kRecPlay)); run (100);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (30);
            proc.pressTrigger (trackId (0, kRecPlay)); run (5);
            expect (proc.getEngine().getTrack (0).getState() == State17::Playing);
            head.playing = false;
            run (2);
            expect (proc.getEngine().getTrack (0).getState() == State17::Stopped, "DAW stop stops livelooper");
            // switched off: the loops keep running on the internal clock
            follow->setValueNotifyingHost (0.0f);
            head.playing = true; run (3);
            proc.pressTrigger (trackId (0, kRecPlay)); run (3);
            expect (proc.getEngine().getTrack (0).isRunning(), "playing again");
            head.playing = false; run (3);
            expect (proc.getEngine().getTrack (0).isRunning(), "follow off: keeps playing");
            proc.setPlayHead (nullptr);
        }
    }
};

static LatencyTests17 latencyTests17;
static Controller17Tests controller17Tests;
