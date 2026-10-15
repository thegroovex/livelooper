// v1.6 K.O. II controller support, punch FX / send FX and the new parameters - without hardware.

#include "plugin/PluginProcessor.h"
#include "plugin/controllers/Ko2.h"
#include "plugin/AppSettings.h"
#include "engine/PunchFx.h"

using namespace livelooper;
using namespace livelooper::params;
using K = ko2::Action::Kind;

namespace
{
constexpr int kBlockK = 480;

struct KRig
{
    LiveLooperProcessor proc { false, false };
    juce::AudioBuffer<float> buffer { 2, kBlockK };
    juce::MidiBuffer midi;

    KRig()
    {
        proc.prepareToPlay (48000.0, kBlockK);
        set ("quantize", 0.0f);
        set ("recaction", 1.0f);
        set ("sync", 0.0f);
        set ("monitor", 0.0f);
    }
    void set (const juce::String& id, float v) { proc.getState().getParameter (id)->setValueNotifyingHost (v); }
    float get (const juce::String& id)         { return proc.getState().getParameter (id)->getValue(); }
    float run (int blocks, float in = 0.0f, bool sine = false)
    {
        float peak = 0.0f;
        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < kBlockK; ++i)
                    buffer.setSample (ch, i, sine ? in * std::sin ((float) i * 0.3f) : in);
            proc.processBlock (buffer, midi);
            peak = std::max (peak, buffer.getMagnitude (0, 0, kBlockK));
            for (auto* p : proc.getParameters())
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
                    if (isTrigger (r->getParameterID()) && r->getValue() >= 0.5f)
                        r->setValueNotifyingHost (0.0f);
        }
        return peak;
    }
};

std::vector<ko2::Action> note (ko2::Controller& c, int group, int offset, bool on, juce::uint32 ms, int vel = 100)
{
    std::vector<ko2::Action> out;
    c.handleNote (ko2::noteFor (group, offset), vel, on, ms, out);
    return out;
}
}

class Ko2Tests : public juce::UnitTest
{
public:
    Ko2Tests() : juce::UnitTest ("K.O. II + punch FX", "livelooper") {}

    void runTest() override
    {
        beginTest ("K.O. II is found by a loose name match (macOS / Windows / Linux spellings, en dash)");
        {
            for (auto n : { "EP-133 KO II MIDI 1", "EP-133", "K.O. II", "KO II", "KOII", "KO2 MIDI", "ep-133 k.o. ii" })
                expect (ko2::matchesName (n), n);
            expect (ko2::matchesName (juce::String::fromUTF8 ("EP\xe2\x80\x93" "133 K.O. II")), "en dash");
            for (auto n : { "EP-1320 Medieval", "EP-40 Riddim", "Ableton Push User Port", "Launchpad X", "KORG nanoKONTROL" })
                expect (! ko2::matchesName (n), n);
            expectEquals (ko2::pickPort ({ "Push", "EP-133 KO II MIDI 2", "EP-133 KO II MIDI 1" }), 2, "prefers MIDI 1");
            expectEquals (ko2::pickPort ({ "Push", "K.O. II" }), 1);
            expectEquals (ko2::pickPort ({ "Push", "Launchpad" }), -1);
        }

        beginTest ("Pad map: groups A - D = notes 36 - 83, '.' 0 ENTER at the bottom, 7 8 9 at the top");
        {
            expectEquals (ko2::noteFor (0, ko2::pad::dot), 36);
            expectEquals (ko2::noteFor (3, 11), 83);
            expectEquals (ko2::groupOf (60), 2);
            expectEquals (ko2::padLabel (ko2::pad::enter), juce::String ("ENTER"));
            expectEquals (ko2::padLabel (ko2::padNumberOffset (7)), juce::String ("7"));
            expectEquals (ko2::padRow (ko2::padNumberOffset (7)), 0);
            expectEquals (ko2::padColumn (ko2::padNumberOffset (9)), 2);
            expectEquals (ko2::padRow (ko2::pad::zero), 3);
            expectEquals (ko2::padRow (ko2::padNumberOffset (1)), 2);
        }

        beginTest ("Controller only: A = REC/PLAY 1-8, 9 = ALL START/STOP, ENTER = CAPTURE, '.' = stop / hold clear, 0 = undo / hold redo");
        {
            ko2::Controller c;
            auto a = note (c, 0, ko2::padNumberOffset (3), true, 1000);
            expect (a.size() == 1 && a[0].kind == K::RecPlay && a[0].index == 2);
            expect (note (c, 0, ko2::padNumberOffset (3), false, 1100).empty());
            a = note (c, 0, ko2::padNumberOffset (9), true, 1200);
            expect (a.size() == 1 && a[0].kind == K::AllStartStop);
            note (c, 0, ko2::padNumberOffset (9), false, 1250);
            a = note (c, 0, ko2::pad::enter, true, 1300);
            expect (a.size() == 1 && a[0].kind == K::Capture);
            note (c, 0, ko2::pad::enter, false, 1350);
            // '.' tap = stop the last used track (3) on release
            expect (note (c, 0, ko2::pad::dot, true, 2000).empty());
            a = note (c, 0, ko2::pad::dot, false, 2100);
            expect (a.size() == 1 && a[0].kind == K::Stop && a[0].index == 2);
            // '.' held 1 s = clear, no stop on release
            note (c, 0, ko2::pad::dot, true, 3000);
            std::vector<ko2::Action> u;
            c.update (3500, u);
            expect (u.empty());
            c.update (4001, u);
            expect (u.size() == 1 && u[0].kind == K::Clear && u[0].index == 2);
            expect (note (c, 0, ko2::pad::dot, false, 4100).empty());
            // 0: tap = undo, hold = redo
            note (c, 0, ko2::pad::zero, true, 5000);
            a = note (c, 0, ko2::pad::zero, false, 5100);
            expect (a.size() == 1 && a[0].kind == K::Undo && a[0].index == 2);
            note (c, 0, ko2::pad::zero, true, 6000);
            u.clear();
            c.update (7100, u);
            expect (u.size() == 1 && u[0].kind == K::Redo);
            expect (note (c, 0, ko2::pad::zero, false, 7200).empty());
            expectEquals (c.getPage(), 0);
        }

        beginTest ("B: STOP 1-8 (double tap = mute, hold = clear, '.' held = undo), C: punch FX with velocity, D: scenes");
        {
            ko2::Controller c;
            auto a = note (c, 1, ko2::padNumberOffset (2), true, 1000);
            expect (a.size() == 1 && a[0].kind == K::Stop && a[0].index == 1);
            note (c, 1, ko2::padNumberOffset (2), false, 1050);
            a = note (c, 1, ko2::padNumberOffset (2), true, 1200);
            expect (a.size() == 1 && a[0].kind == K::Mute && a[0].index == 1, "double tap = mute");
            note (c, 1, ko2::padNumberOffset (2), false, 1250);
            note (c, 1, ko2::pad::dot, true, 2000);
            a = note (c, 1, ko2::padNumberOffset (5), true, 2100);
            expect (a.size() == 1 && a[0].kind == K::Undo && a[0].index == 4, "SHIFT ('.') + pad = undo");
            note (c, 1, ko2::padNumberOffset (5), false, 2200);
            note (c, 1, ko2::pad::dot, false, 2300);
            a = note (c, 1, ko2::pad::zero, true, 2400);
            expect (a.size() == 1 && a[0].kind == K::Tap);
            note (c, 1, ko2::pad::zero, false, 2450);

            // C: pad n = punch FX n while held, velocity = amount
            a = note (c, 2, ko2::padNumberOffset (2), true, 3000, 64);     // pad "2" = punch 5 Tape Stop
            expect (a.size() == 1 && a[0].kind == K::Punch && a[0].index == 4 && a[0].on && a[0].velocity == 64);
            a = note (c, 2, ko2::padNumberOffset (2), false, 3200);
            expect (a.size() == 1 && a[0].kind == K::Punch && ! a[0].on);
            expectEquals (juce::String (getPunchFxName (4)), juce::String ("Tape Stop"));
            expectWithinAbsoluteError (punchAmountFromVelocity (127), 1.0f, 1.0e-6f);
            expectWithinAbsoluteError (punchAmountFromVelocity (0), 0.25f, 1.0e-6f);

            // D: scene on release, hold = store; 9 = rhythm; 0 = fill while held; ENTER = recorder
            expect (note (c, 3, ko2::padNumberOffset (4), true, 4000).empty());
            a = note (c, 3, ko2::padNumberOffset (4), false, 4100);
            expect (a.size() == 1 && a[0].kind == K::Scene && a[0].index == 3);
            note (c, 3, ko2::padNumberOffset (4), true, 5000);
            std::vector<ko2::Action> u;
            c.update (6050, u);
            expect (u.size() == 1 && u[0].kind == K::SceneStore && u[0].index == 3);
            note (c, 3, ko2::padNumberOffset (4), false, 6100);
            a = note (c, 3, ko2::pad::zero, true, 7000);
            expect (a.size() == 1 && a[0].kind == K::Fill && a[0].on);
            a = note (c, 3, ko2::pad::zero, false, 7100);
            expect (a.size() == 1 && a[0].kind == K::Fill && ! a[0].on);
            note (c, 3, ko2::pad::enter, true, 8000);
            a = note (c, 3, ko2::pad::enter, false, 8100);
            expect (a.size() == 1 && a[0].kind == K::Recorder);
        }

        beginTest ("Role switch: '.' + ENTER of group D held together for 1 s, no stray actions");
        {
            ko2::Controller c;
            note (c, 3, ko2::pad::dot, true, 1000);
            expect (note (c, 3, ko2::pad::enter, true, 1080).empty());
            std::vector<ko2::Action> u;
            c.update (1900, u);
            expect (u.empty());
            c.update (2090, u);
            expect (u.size() == 1 && u[0].kind == K::RoleSwitch);
            u.clear();
            c.update (2500, u);
            expect (u.empty(), "only once per chord");
            expect (note (c, 3, ko2::pad::dot, false, 2600).empty());
            expect (note (c, 3, ko2::pad::enter, false, 2610).empty(), "no recorder / stop on release");
            // pressed 300 ms apart = no chord: ENTER alone acts
            note (c, 3, ko2::pad::dot, true, 5000);
            note (c, 3, ko2::pad::enter, true, 5300);
            auto a = note (c, 3, ko2::pad::enter, false, 5400);
            expect (a.size() == 1 && a[0].kind == K::Recorder);
            note (c, 3, ko2::pad::dot, false, 5500);
        }

        beginTest ("Record K.O. II: only group D controls (compact looper), the other groups stay instruments");
        {
            ko2::Controller c;
            c.setRole (ko2::Role::Record);
            c.setGroups (ko2::defaultGroups (ko2::Role::Record));
            expect (note (c, 0, ko2::padNumberOffset (1), true, 1000).empty(), "group A ignored");
            expectEquals (c.getPage(), 0, "the page still follows the pad");
            auto a = note (c, 3, ko2::padNumberOffset (1), true, 1100);
            expect (a.size() == 1 && a[0].kind == K::RecPlay && a[0].index == 0);
            note (c, 3, ko2::padNumberOffset (1), false, 1150);
            note (c, 3, ko2::pad::enter, true, 1200);
            a = note (c, 3, ko2::pad::enter, false, 1300);
            expect (a.size() == 1 && a[0].kind == K::Resample);
            a = note (c, 3, ko2::padNumberOffset (9), true, 1400);
            expect (a.size() == 1 && a[0].kind == K::AllStartStop);
            // KEYS mode notes are ignored and reported once
            std::vector<ko2::Action> out;
            c.handleNote (90, 100, true, 1500, out);
            expect (out.empty() && c.takeKeysWarning() && ! c.takeKeysWarning());
            // tracks beyond the visible ones do nothing
            c.setNumTracks (5);
            expect (note (c, 3, ko2::padNumberOffset (7), true, 1600).empty());
        }

        beginTest ("Clock: start on a bar line after song position 0, 24 clocks per quarter, stop");
        {
            ko2::Clock clock;
            std::vector<juce::MidiMessage> out;
            clock.process (1.5, true, 4.0, out);
            expect (out.empty(), "waits for the next bar");
            clock.process (4.0, true, 4.0, out);
            expectEquals ((int) out.size(), 3);
            expect (out[0].isSongPositionPointer() && out[0].getSongPositionPointerMidiBeat() == 0);
            expect (out[1].isMidiStart() && out[2].isMidiClock());
            out.clear();
            for (int i = 1; i <= 240; ++i)
                clock.process (4.0 + i / 240.0, true, 4.0, out);    // one beat in fine steps
            int clocks = 0;
            for (auto& m : out)
                clocks += m.isMidiClock() ? 1 : 0;
            expectEquals (clocks, 24);
            out.clear();
            clock.process (5.0 - 0.01, true, 4.0, out);
            expect (out.empty(), "a small step back (estimate ahead of the audio) sends nothing twice");
            clock.process (5.0, false, 4.0, out);
            expect (out.size() == 1 && out[0].isMidiStop());
            expect (! clock.isStarted());
        }

        beginTest ("Actions reach the looper: punch FX hold + amount from velocity, resample trigger, role switch");
        {
            KRig r;
            ko2::apply (r.proc, { K::Punch, 6, true, 127 });
            expectEquals (r.get (punchHoldId (6)), 1.0f);
            expectWithinAbsoluteError (r.get (gPfxAmount), 1.0f, 1.0e-4f);
            ko2::apply (r.proc, { K::Punch, 6, false, 0 });
            expectEquals (r.get (punchHoldId (6)), 0.0f);
            ko2::apply (r.proc, { K::RoleSwitch, -1 });
            expectEquals (r.proc.getControllerAudioMode(), 1);
            ko2::apply (r.proc, { K::RoleSwitch, -1 });
            expectEquals (r.proc.getControllerAudioMode(), 0);
            ko2::apply (r.proc, { K::RecPlay, 1 });
            r.run (2, 0.5f);
            expect (r.proc.getEngine().getTrack (1).getState() == LoopTrack::State::Recording);
            ko2::apply (r.proc, { K::Metronome, -1 });
            expectEquals (r.get (gMetroOn), 1.0f);
        }

        beginTest ("Simulated K.O. II: connected, pad messages drive the looper, chip state");
        {
            KRig r;
            r.proc.simulateControllers (true, false);
            expect (r.proc.isKo2Connected());
            auto* d = r.proc.getKo2Device();
            expect (d != nullptr);
            d->injectMessage (juce::MidiMessage::noteOn (1, ko2::noteFor (0, ko2::padNumberOffset (2)), (juce::uint8) 100));
            r.run (2, 0.5f);
            expect (r.proc.getEngine().getTrack (1).getState() == LoopTrack::State::Recording);
            expectEquals (d->getLastActionText(), juce::String ("T2 REC"));
            // channel filter
            d->setChannel (16);
            d->injectMessage (juce::MidiMessage::noteOn (1, ko2::noteFor (0, ko2::padNumberOffset (3)), (juce::uint8) 100));
            r.run (1);
            expect (r.proc.getEngine().getTrack (2).isEmpty(), "other channels are ignored");
            d->setChannel (0);
            r.proc.simulateControllers (false, false);
            expect (! r.proc.isKo2Connected());
        }

        beginTest ("Resample: first empty track, capture length, rec length set; no free track = notice");
        {
            KRig r;
            r.set (gCaptureBars, 1.0f / 5.0f);      // 2 bars
            const int t = r.proc.performKo2Resample();
            expectEquals (t, 0);
            expectWithinAbsoluteError (r.get (trackId (0, kRecLen)), 2.0f / 6.0f, 1.0e-4f);
            r.run (2, 0.5f);
            expect (r.proc.getEngine().getTrack (0).getState() == LoopTrack::State::Recording);
            expectEquals (r.proc.takeControllerNotice(), juce::String ("Resample into track 1"));
            // fill all visible tracks
            for (int i = 1; i < r.proc.getVisibleTracks(); ++i)
                r.proc.pressTrigger (trackId (i, kRecPlay));
            r.run (2, 0.5f);
            expectEquals (r.proc.performKo2Resample(), -1);
            expectEquals (r.proc.takeControllerNotice(), juce::String ("No free track"));
        }

        beginTest ("Latency compensation moves a loop earlier against the timeline");
        {
            KRig r;
            r.proc.pressTrigger (trackId (0, kRecPlay));
            r.run (10, 0.5f);
            r.proc.pressTrigger (trackId (0, kRecPlay));
            r.run (2, 0.5f);
            auto& tr = r.proc.getEngine().getTrack (0);
            expect (tr.getState() == LoopTrack::State::Playing);
            const int before = tr.getPlayPosition();
            const double anchor = tr.getAnchorPpq();
            tr.nudgeAlignment (480);
            expectEquals (tr.getPlayPosition(), (before + 480) % tr.getLength());
            if (tr.hasAnchor())
                expect (tr.getAnchorPpq() < anchor);
        }

        beginTest ("Punch FX: low-pass held darkens, release returns to dry, latch toggles, send FX adds a tail");
        {
            KRig r;
            r.set ("monitor", 0.5f);              // the input is heard
            r.set (gPfxTarget, 2.0f / 3.0f);       // master (loops + monitor)
            const float dry = r.run (20, 0.5f, true);
            r.set (gPfxAmount, 1.0f);
            r.set (punchHoldId (6), 1.0f);         // punch 7 low-pass
            r.run (10, 0.5f, true);
            const float wet = r.run (10, 0.5f, true);
            expect (wet < dry * 0.6f, "low-pass at full amount cuts the bright test tone");
            expect (r.proc.getEngine().getPunchFx().isOn (6));
            r.set (punchHoldId (6), 0.0f);
            r.run (10, 0.5f, true);
            expectWithinAbsoluteError (r.run (10, 0.5f, true), dry, dry * 0.1f);
            expect (! r.proc.getEngine().getPunchFx().isActive());
            // latch: a press switches on, the next press off
            r.set (gPfxLatch, 1.0f);
            r.set (punchHoldId (9), 1.0f); r.run (2); r.set (punchHoldId (9), 0.0f); r.run (2);
            expect (r.proc.getEngine().getPunchFx().isOn (9), "latched on after release");
            r.set (punchHoldId (9), 1.0f); r.run (2); r.set (punchHoldId (9), 0.0f); r.run (20);
            expect (! r.proc.getEngine().getPunchFx().isOn (9), "second press = off");
            r.set (gPfxLatch, 0.0f);
        }

        beginTest ("Every punch FX and send FX type runs without blowing up (finite, bounded)");
        {
            PunchFx pf;
            pf.prepare (48000.0, 512);
            SendFx sf;
            sf.prepare (48000.0);
            std::vector<float> l (512), rr (512);
            double ppq = 0.0;
            for (int k = 0; k < kNumPunchFx; ++k)
            {
                PunchFxSettings s;
                s.held[(size_t) k] = true;
                s.amount = 0.9f;
                pf.setSettings (s);
                for (int b = 0; b < 40; ++b)
                {
                    for (int i = 0; i < 512; ++i)
                        l[(size_t) i] = rr[(size_t) i] = 0.5f * std::sin ((float) (b * 512 + i) * 0.05f);
                    pf.process (l.data(), rr.data(), 512, ppq, 2.0 / 48000.0);
                    ppq += 512 * 2.0 / 48000.0;
                    for (float v : l)
                        expect (std::isfinite (v) && std::abs (v) < 8.0f, getPunchFxName (k));
                }
            }
            for (int t = 1; t < (int) SendFxType::NumTypes; ++t)
            {
                sf.setSettings ({ (SendFxType) t, 0.7f, 0.6f, 0.8f });
                for (int b = 0; b < 40; ++b)
                {
                    for (int i = 0; i < 512; ++i)
                        l[(size_t) i] = rr[(size_t) i] = b < 10 ? 0.5f * std::sin ((float) i * 0.05f) : 0.0f;
                    sf.process (l.data(), rr.data(), 512, 0.0f);
                    for (float v : l)
                        expect (std::isfinite (v) && std::abs (v) < 8.0f, getSendFxName ((SendFxType) t));
                }
            }
            expectEquals (punchRepeatBeats (0.0f), 1.0);
            expectEquals (punchRepeatBeats (1.0f), 0.125);
        }

        beginTest ("New parameters: generic names (no device names for hosts), MIDI kinds, translations");
        {
            LiveLooperProcessor proc;
            for (int i = 465; i < proc.getParameters().size(); ++i)
            {
                auto* p = dynamic_cast<juce::RangedAudioParameter*> (proc.getParameters()[i]);
                const auto name = englishName (p->getParameterID());
                expect (! name.containsIgnoreCase ("K.O") && ! name.containsIgnoreCase ("EP-133") && name != p->getParameterID(), name);
            }
            expect (isMomentary (punchHoldId (0)) && isMomentary (punchHoldId (11)));
            expect (isTrigger (gKo2Resample));
            expectEquals (englishName (punchHoldId (4)), juce::String ("Punch FX 5 (Hold)"));
            expectEquals (proc.getMidiMap().getNumParams(), kNumParameters);
        }
    }
};

static Ko2Tests ko2Tests;
