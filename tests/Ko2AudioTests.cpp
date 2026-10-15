// v1.6 K.O. II hardware test fixes, without hardware:
//  - "Record K.O. II": the K.O. II is the audio INPUT only, the output stays on the computer (else
//    livelooper's output - metronome, loops - goes into the K.O. II and is recorded again), and the
//    metronome / other loops never end up in a recording made from the input;
//  - punch pads without pressure (the K.O. II sends no aftertouch): hold to intensify (ramp from the
//    hit velocity to 100 %), pressure messages handled should they ever arrive.

#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "plugin/SettingsPanel.h"
#include "plugin/AppSettings.h"
#include "plugin/Localisation.h"
#include "plugin/controllers/Ko2.h"
#include "engine/PunchFx.h"

using namespace livelooper;
using namespace livelooper::params;
using K = ko2::Action::Kind;

namespace
{
constexpr int kBlockA = 480;

struct ASettings
{
    juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("livelooper-tests-ko2audio-" + juce::String (juce::Random::getSystemRandom().nextInt64()));
    ASettings()  { dir.createDirectory(); appsettings::setStorageFolderForTests (dir); }
    ~ASettings() { appsettings::setStorageFolderForTests ({}); dir.deleteRecursively(); }
};

struct ARig
{
    LiveLooperProcessor proc { false, false };
    juce::AudioBuffer<float> buffer { 2, kBlockA };
    juce::MidiBuffer midi;

    ARig()
    {
        proc.prepareToPlay (48000.0, kBlockA);
        set ("quantize", 0.0f);
        set ("recaction", 1.0f);
        set ("sync", 0.0f);
        set ("monitor", 0.0f);
    }
    void set (const juce::String& id, float v) { proc.getState().getParameter (id)->setValueNotifyingHost (v); }
    float get (const juce::String& id)         { return proc.getState().getParameter (id)->getValue(); }
    /** Runs blocks with the given input; returns the output peak. */
    float run (int blocks, float in = 0.0f)
    {
        float peak = 0.0f;
        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < kBlockA; ++i)
                    buffer.setSample (ch, i, in * std::sin ((float) i * 0.3f));
            proc.processBlock (buffer, midi);
            peak = std::max (peak, buffer.getMagnitude (0, 0, kBlockA));
            for (auto* p : proc.getParameters())
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
                    if (isTrigger (r->getParameterID()) && r->getValue() >= 0.5f)
                        r->setValueNotifyingHost (0.0f);
        }
        return peak;
    }
    float loopPeak (int t)
    {
        auto& tr = proc.getEngine().getTrack (t);
        const int n = tr.getLength();
        if (n <= 0)
            return -1.0f;
        std::vector<float> l ((size_t) n), r ((size_t) n);
        proc.getEngine().lockTrack (t);
        tr.readLoop (l.data(), r.data());
        proc.getEngine().unlockTrack (t);
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
            peak = std::max ({ peak, std::abs (l[(size_t) i]), std::abs (r[(size_t) i]) });
        return peak;
    }
};

std::vector<ko2::Action> hit (ko2::Controller& c, int offset, bool on, juce::uint32 ms, int vel = 100)
{
    std::vector<ko2::Action> out;
    c.handleNote (ko2::noteFor (2, offset), vel, on, ms, out);      // group C = punch FX
    return out;
}
}

class Ko2AudioTests : public juce::UnitTest
{
public:
    Ko2AudioTests() : juce::UnitTest ("K.O. II audio routing + hold to intensify", "livelooper") {}

    void runTest() override
    {
        ASettings settings;
        using ko2::AudioOutput;

        beginTest ("Record K.O. II: input = K.O. II, output stays on the computer (previous, default, first other)");
        {
            const juce::StringArray ins { "MacBook Pro Microphone", "EP-133 K.O. II" };
            const juce::StringArray outs { "MacBook Pro Speakers", "EP-133 K.O. II", "External Headphones" };
            auto r = ko2::chooseAudioRoute (ins, outs, "External Headphones", 0, AudioOutput::Computer);
            expect (r.ok);
            expectEquals (r.input, juce::String ("EP-133 K.O. II"));
            expectEquals (r.output, juce::String ("External Headphones"), "the output used before");
            r = ko2::chooseAudioRoute (ins, outs, "", 0, AudioOutput::Computer);
            expectEquals (r.output, juce::String ("MacBook Pro Speakers"), "none before: the system default");
            r = ko2::chooseAudioRoute (ins, outs, "EP-133 K.O. II", 1, AudioOutput::Computer);
            expectEquals (r.output, juce::String ("MacBook Pro Speakers"), "never the K.O. II, even when it was the output / default");
            r = ko2::chooseAudioRoute (ins, outs, "Focusrite USB ASIO", -1, AudioOutput::Computer);
            expectEquals (r.output, juce::String ("MacBook Pro Speakers"), "previous output of another device type: first other output");
            r = ko2::chooseAudioRoute (ins, outs, "External Headphones", 0, AudioOutput::Ko2);
            expectEquals (r.output, juce::String ("EP-133 K.O. II"), "only when chosen: output to the K.O. II");
            r = ko2::chooseAudioRoute (ins, { "MacBook Pro Speakers" }, "", 0, AudioOutput::Ko2);
            expectEquals (r.output, juce::String ("MacBook Pro Speakers"), "K.O. II chosen but no K.O. II output: the computer");
            r = ko2::chooseAudioRoute (ins, { "EP-133 K.O. II" }, "", 0, AudioOutput::Computer);
            expect (r.ok && r.output.isEmpty(), "no other output: none (never the K.O. II)");
            r = ko2::chooseAudioRoute ({ "MacBook Pro Microphone" }, outs, "", 0, AudioOutput::Computer);
            expect (! r.ok, "no K.O. II input in this device type");
        }

        beginTest ("K.O. II audio output setting: computer by default, SETUP > Controllers switches it, notes EN/DE");
        {
            LiveLooperProcessor proc;
            expectEquals (proc.getKo2AudioOutput(), 0);
            std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
            auto& e = *dynamic_cast<LiveLooperEditor*> (ed.get());
            juce::Array<int> toggles;
            auto menu = e.buildSetupMenu (toggles);
            std::function<const juce::PopupMenu::Item* (const juce::PopupMenu&, int)> find = [&] (const juce::PopupMenu& m, int id) -> const juce::PopupMenu::Item* {
                for (juce::PopupMenu::MenuItemIterator it (m); it.next();)
                {
                    if (it.getItem().itemID == id) return &it.getItem();
                    if (it.getItem().subMenu != nullptr)
                        if (auto* f = find (*it.getItem().subMenu, id)) return f;
                }
                return nullptr;
            };
            expect (find (menu, 761) != nullptr && find (menu, 761)->isTicked, "Computer (default) ticked");
            expect (find (menu, 762) != nullptr && ! find (menu, 762)->isTicked);
            expect (find (menu, 793) != nullptr && find (menu, 793)->text.contains ("recorded again"), "warning note");
            for (int id = 763; id <= 766; ++id)
                expect (find (menu, id) != nullptr, "hold ramp choice " + juce::String (id));
            expect (find (menu, 765)->isTicked, "2 beats by default");
            e.handleSetupResult (762);
            expectEquals (proc.getKo2AudioOutput(), 1);
            expectEquals (appsettings::getValue ("ko2.audioOut"), juce::String ("ko2"));
            e.handleSetupResult (761);
            expectEquals (proc.getKo2AudioOutput(), 0);
            e.handleSetupResult (763);
            expectEquals (proc.getPunchHoldRamp(), 0);
            e.handleSetupResult (766);
            expectEquals (proc.getPunchHoldRamp(), 3);
            e.handleSetupResult (765);
            auto panel = e.createSettingsPanel ("Controllers");
            auto* sp = dynamic_cast<SettingsPanel*> (panel.get());
            expect (sp != nullptr && sp->getNumRows() > 0);
            // German
            const auto prev = i18n::getSetting();
            i18n::setSetting (i18n::Language::German, false);
            for (auto s : { "K.O. II audio output", "Computer (default)", "Punch pads: hold to intensify", "1 beat", "2 beats", "HOLD", "Role",
                            "K.O. II: input from the K.O. II, output on %1" })
                expect (i18n::germanFor (s).isNotEmpty(), juce::String ("German for ") + s);
            toggles.clear();
            menu = e.buildSetupMenu (toggles);
            expect (find (menu, 793)->text.contains ("aufgenommen"), find (menu, 793)->text);
            i18n::setSetting (prev, false);
        }

        beginTest ("Switching the output while recording the K.O. II asks the app to move it; not while only a controller");
        {
            LiveLooperProcessor proc;
            int calls = 0;
            bool lastRecord = false;
            proc.onControllerAudioChanged = [&] (bool rec) { ++calls; lastRecord = rec; };
            proc.setKo2AudioOutput (1);
            expectEquals (calls, 0, "not recording the K.O. II: nothing to move");
            proc.simulateControllers (true, false);
            proc.setControllerAudioMode (1);
            expect (calls >= 1 && lastRecord, "record role + connected: the app switches the input");
            const int before = calls;
            proc.setKo2AudioOutput (0);
            expectEquals (calls, before + 1);
            expect (lastRecord);
            proc.simulateControllers (false, false);
            proc.setControllerAudioMode (0);
            expect (! lastRecord, "unplugged: back to the previous input / output");
            proc.onControllerAudioChanged = nullptr;
        }

        beginTest ("Metronome and playing loops are never recorded: a recording holds only the input");
        {
            ARig r;
            // a loop on track 2 that plays while track 1 records
            r.proc.pressTrigger (trackId (1, kRecPlay));
            r.run (20, 0.5f);
            r.proc.pressTrigger (trackId (1, kRecPlay));
            r.run (2, 0.0f);
            expect (r.proc.getEngine().getTrack (1).getState() == LoopTrack::State::Playing);
            r.set (gMetroOn, 1.0f);
            r.set (gMetroLevel, 1.0f);
            // track 1 records a silent input while the metronome clicks and track 2 plays
            r.proc.pressTrigger (trackId (0, kRecPlay));
            const float outPeak = r.run (60, 0.0f);
            r.proc.pressTrigger (trackId (0, kRecPlay));
            r.run (2, 0.0f);
            expect (outPeak > 0.05f, "the output has the loop and the clicks (" + juce::String (outPeak) + ")");
            expect (r.proc.getEngine().getTrack (0).getLength() > 0);
            expectWithinAbsoluteError (r.loopPeak (0), 0.0f, 1.0e-6f);
            // and an input recording holds just the input
            r.proc.pressTrigger (trackId (2, kRecPlay));
            r.run (20, 0.25f);
            r.proc.pressTrigger (trackId (2, kRecPlay));
            r.run (2, 0.0f);
            expect (r.loopPeak (2) <= 0.26f && r.loopPeak (2) > 0.1f, "only the input level (" + juce::String (r.loopPeak (2)) + ")");
        }

        beginTest ("Hold ramp: amount from the hit velocity up to 100 % over 1 beat / 2 beats / 1 bar, off = stays");
        {
            expectEquals (ko2::holdRampBeats (ko2::HoldRamp::Off, 4.0), 0.0);
            expectEquals (ko2::holdRampBeats (ko2::HoldRamp::OneBeat, 4.0), 1.0);
            expectEquals (ko2::holdRampBeats (ko2::HoldRamp::TwoBeats, 4.0), 2.0);
            expectEquals (ko2::holdRampBeats (ko2::HoldRamp::OneBar, 3.0), 3.0);
            expectWithinAbsoluteError (ko2::holdRampAmount (0.4f, 0.0, 120.0, 2.0), 0.4f, 1.0e-5f);
            expectWithinAbsoluteError (ko2::holdRampAmount (0.4f, 500.0, 120.0, 2.0), 0.7f, 1.0e-5f);   // 1 of 2 beats
            expectWithinAbsoluteError (ko2::holdRampAmount (0.4f, 5000.0, 120.0, 2.0), 1.0f, 1.0e-5f);
            expectWithinAbsoluteError (ko2::holdRampAmount (0.4f, 5000.0, 120.0, 0.0), 0.4f, 1.0e-5f);

            ko2::Controller c;
            expectEquals (c.getHoldRamp(), ko2::HoldRamp::TwoBeats, "2 beats by default");
            expectEquals (c.livePunchAmount (0, 120.0, 4.0), -1.0f, "nothing held");
            auto out = hit (c, 4, true, 1000, 1);
            expect (out.size() == 1 && out[0].kind == K::Punch && out[0].on);
            const float base = punchAmountFromVelocity (1);
            expectWithinAbsoluteError (c.livePunchAmount (1000, 120.0, 4.0), base, 1.0e-5f);
            expectWithinAbsoluteError (c.livePunchAmount (1500, 120.0, 4.0), base + (1.0f - base) * 0.5f, 1.0e-4f);
            expectWithinAbsoluteError (c.livePunchAmount (2000, 120.0, 4.0), 1.0f, 1.0e-5f);
            expectWithinAbsoluteError (c.livePunchAmount (9000, 120.0, 4.0), 1.0f, 1.0e-5f);
            out = hit (c, 4, false, 2100);
            expect (out.size() == 1 && ! out[0].on && out[0].velocity == 1, "release: back to the hit");
            expectEquals (c.livePunchAmount (2100, 120.0, 4.0), -1.0f);

            // 1 bar at 90 bpm, 4/4 = 2667 ms
            c.setHoldRamp (ko2::HoldRamp::OneBar);
            hit (c, 0, true, 0, 127);
            expectWithinAbsoluteError (c.livePunchAmount (2000, 90.0, 4.0), 1.0f, 1.0e-5f, "velocity 127 starts at 100 %");
            hit (c, 0, false, 2500);
            hit (c, 0, true, 10000, 64);
            const float b64 = punchAmountFromVelocity (64);
            expectWithinAbsoluteError (c.livePunchAmount (10000 + 1333, 90.0, 4.0), b64 + (1.0f - b64) * 0.5f, 2.0e-3f);
            // a re-hit while held (if a device sends one): new start amount, the ramp starts again
            out = hit (c, 0, true, 12000, 20);
            expect (out.size() == 1 && out[0].on && out[0].velocity == 20);
            expectWithinAbsoluteError (c.livePunchAmount (12000, 90.0, 4.0), punchAmountFromVelocity (20), 1.0e-5f);
            // two pads: the ramp lasts until the last one is up
            hit (c, 1, true, 12100, 50);
            out = hit (c, 0, false, 12200);
            expect (out.size() == 1 && out[0].velocity == 0, "another punch pad still held: amount untouched");
            expect (c.livePunchAmount (12200, 90.0, 4.0) >= 0.0f);
            out = hit (c, 1, false, 12300);
            expect (out.size() == 1 && out[0].velocity == 50);
            expectEquals (c.livePunchAmount (12300, 90.0, 4.0), -1.0f);

            // off: amount stays at the hit, release leaves it
            c.setHoldRamp (ko2::HoldRamp::Off);
            hit (c, 2, true, 20000, 40);
            expectWithinAbsoluteError (c.livePunchAmount (25000, 120.0, 4.0), punchAmountFromVelocity (40), 1.0e-5f);
            out = hit (c, 2, false, 25000);
            expect (out.size() == 1 && out[0].velocity == 0);
        }

        beginTest ("Pressure: poly aftertouch (0xA0) / channel pressure (0xD0) drive the held amount, ignored otherwise");
        {
            ko2::Controller c;
            c.setHoldRamp (ko2::HoldRamp::Off);
            c.handlePressure (-1, 127, 0);
            expectEquals (c.livePunchAmount (0, 120.0, 4.0), -1.0f, "no pad held: ignored");
            hit (c, 5, true, 0, 30);
            const float base = punchAmountFromVelocity (30);
            c.handlePressure (ko2::noteFor (2, 6), 127, 10);
            expectWithinAbsoluteError (c.livePunchAmount (10, 120.0, 4.0), base, 1.0e-5f, "aftertouch of a pad not held: ignored");
            c.handlePressure (ko2::noteFor (2, 5), 127, 20);
            expectWithinAbsoluteError (c.livePunchAmount (20, 120.0, 4.0), 1.0f, 1.0e-5f, "poly aftertouch of the held pad");
            c.handlePressure (ko2::noteFor (2, 5), 0, 30);
            expectWithinAbsoluteError (c.livePunchAmount (30, 120.0, 4.0), base, 1.0e-5f, "never below the hit");
            c.handlePressure (-1, 100, 40);
            expectWithinAbsoluteError (c.livePunchAmount (40, 120.0, 4.0), juce::jmax (base, 100.0f / 127.0f), 1.0e-5f, "channel pressure");
            auto out = hit (c, 5, false, 50);
            expect (out.size() == 1 && out[0].velocity == 30, "pressure moved it: release goes back to the hit even with the ramp off");
            // pressure replaces the time ramp
            c.setHoldRamp (ko2::HoldRamp::OneBeat);
            hit (c, 5, true, 1000, 30);
            c.handlePressure (ko2::noteFor (2, 5), 64, 1001);
            expectWithinAbsoluteError (c.livePunchAmount (9000, 120.0, 4.0), juce::jmax (base, 64.0f / 127.0f), 1.0e-5f);
            hit (c, 5, false, 9000);
        }

        beginTest ("Simulated K.O. II: hit sets the amount, aftertouch / hold raise it, note off puts it back; live amount shown");
        {
            ARig r;
            r.proc.simulateControllers (true, false);
            auto* d = r.proc.getKo2Device();
            expect (d != nullptr);
            expectEquals (r.proc.getKo2LivePunchAmount(), -1.0f);
            const int n = ko2::noteFor (2, 3);
            d->injectMessage (juce::MidiMessage::noteOn (1, n, (juce::uint8) 40));
            const float base = punchAmountFromVelocity (40);
            expectEquals (r.get (punchHoldId (3)), 1.0f);
            expectWithinAbsoluteError (r.get (gPfxAmount), base, 0.01f);
            expect (r.proc.getKo2LivePunchAmount() >= base - 0.01f, "live amount while held");
            d->injectMessage (juce::MidiMessage::aftertouchChange (1, n, 127));
            expectWithinAbsoluteError (r.get (gPfxAmount), 1.0f, 0.01f, "poly aftertouch reaches the amount");
            d->injectMessage (juce::MidiMessage::channelPressureChange (1, 0));
            expectWithinAbsoluteError (r.get (gPfxAmount), base, 0.01f, "channel pressure 0: back to the hit");
            d->injectMessage (juce::MidiMessage::noteOff (1, n));
            expectEquals (r.get (punchHoldId (3)), 0.0f);
            expectWithinAbsoluteError (r.get (gPfxAmount), base, 0.01f, "note off: the hit amount");
            expectEquals (r.proc.getKo2LivePunchAmount(), -1.0f);
            // the ramp setting is a host parameter (MIDI-mappable): off / 1 beat / 2 beats / 1 bar
            r.proc.setPunchHoldRamp (0);
            expectEquals (r.proc.getPunchHoldRamp(), 0);
            r.proc.setPunchHoldRamp (3);
            expectEquals (r.proc.getPunchHoldRamp(), 3);
            expectEquals (englishName (gPfxHoldRamp), juce::String ("Punch FX Hold Ramp"));
            auto* prm = dynamic_cast<juce::AudioParameterChoice*> (r.proc.getState().getParameter (gPfxHoldRamp));
            expect (prm != nullptr && prm->choices.size() == 4);
            r.proc.simulateControllers (false, false);
        }
    }
};

static Ko2AudioTests ko2AudioTests;
