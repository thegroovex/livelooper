// v1.7 (branch v1.7-wip): SETUP > Audio > Latency, SETUP > Controllers > velocity curves,
// generic MIDI feedback (experimental), Push 1 display. Menu ids 1700 - 1899.
#include "PluginEditor.h"
#include "SettingsPanel.h"
#include "AppSettings.h"

namespace livelooper
{
using namespace params;
using i18n::tr;

namespace
{
    const int kOffsets[] = { -10, -5, -2, -1, 0, 1, 2, 5, 10, 20 };
    juce::String sourceName (int s)
    {
        switch (s)
        {
            case 0: return tr ("measured");
            case 1: return tr ("reported by the audio device");
            case 2: return tr ("K.O. II latency offset");
            default: return tr ("nothing measured");
        }
    }
}

void LiveLooperEditor::addLatencyMenu (juce::PopupMenu& menu, juce::Array<int>& toggles)
{
    auto& m = proc.getLatencyMeasure();
    const bool running = m.isRunning();
    const double sr = juce::jmax (1.0, proc.getSampleRate());
    juce::PopupMenu lat;
    lat.addItem (1700, running ? tr ("Measuring ...") : tr ("Measure latency ..."), ! running);
    const double ms = proc.getMeasuredLatencyMs();
    if (ms >= 0.0)
        lat.addItem (1790, tr ("Measured: %1 ms (%2 samples)", juce::String (ms, 1), juce::String (juce::roundToInt (ms * 0.001 * sr))), false);
    else
        lat.addItem (1790, tr ("Not measured yet for this audio device"), false);
    switch (m.getStatus())
    {
        case LatencyMeasure::Status::NoSignal:
            lat.addItem (1791, tr ("Last measurement: no ping detected - connect an output to an input with a cable, or hold a microphone to the speakers, and turn the input up"), false);
            break;
        case LatencyMeasure::Status::Unstable:
            lat.addItem (1791, tr ("Last measurement: the runs disagree (%1 ms apart) - use a cable instead of a microphone, close other audio apps",
                                   juce::String (m.getSpreadSamples() * 1000.0 / sr, 1)), false);
            break;
        case LatencyMeasure::Status::Running:
            lat.addItem (1791, tr ("Plays a few clicks on the output and listens on the input - keep the cable / microphone in place"), false);
            break;
        case LatencyMeasure::Status::Idle:
        case LatencyMeasure::Status::Done:
            break;
    }
    lat.addSeparator();
    lat.addItem (1701, tr ("Latency compensation (all recordings, overdubs and captures)"), true, proc.isLatencyCompensationOn());
    juce::PopupMenu off;
    const float cur = proc.getLatencyOffsetMs();
    for (int i = 0; i < (int) std::size (kOffsets); ++i)
        off.addItem (1710 + i, (kOffsets[i] > 0 ? "+" : "") + juce::String (kOffsets[i]) + " ms", true, std::abs (cur - (float) kOffsets[i]) < 0.05f);
    lat.addSubMenu (tr ("Manual offset"), off);
    lat.addItem (1702, tr ("Forget the measurement for this audio device"), ms >= 0.0);
    lat.addSeparator();
    const int used = proc.getLatencyCompensationSamples();
    lat.addItem (1792, tr ("In use: %1 ms (%2 samples) - %3", juce::String (used * 1000.0 / sr, 1), juce::String (used))
                           .replace ("%3", sourceName (proc.getLatencySource())), false);
    if (proc.wrapperType != juce::AudioProcessor::wrapperType_Standalone)
        lat.addItem (1793, tr ("Plug-in: hosts give plug-ins no audio device latency - measure inside the DAW (cable from an output to the track's input, input monitoring on, nothing else playing)"), false);
    lat.addItem (1794, tr ("Fine-tune with the manual offset or the host parameter \"Latency Offset\" (-50 .. +50 ms)"), false);
    menu.addSubMenu (tr ("Latency"), lat);
    toggles.add (1701);
    juce::ignoreUnused (cur);
}

void LiveLooperEditor::addController17Menu (juce::PopupMenu& menu, juce::Array<int>& toggles)
{
    // velocity curves per controller
    {
        juce::PopupMenu vel;
        const char* const names[] = { "Soft", "Normal", "Hard", "Fixed" };
        const std::pair<LiveLooperProcessor::VelocitySource, const char*> sources[] = {
            { LiveLooperProcessor::VelocitySource::Push, "Push 1" },
            { LiveLooperProcessor::VelocitySource::Ko2, "K.O. II" },
            { LiveLooperProcessor::VelocitySource::Midi, "MIDI (learned notes)" } };
        for (int s = 0; s < 3; ++s)
        {
            juce::PopupMenu c;
            const int curve = (int) proc.getVelocityCurve (sources[s].first);
            for (int i = 0; i < 4; ++i)
                c.addItem (1720 + s * 4 + i, tr (names[i]), true, curve == i);
            vel.addSubMenu (tr (sources[s].second), c);
        }
        vel.addSeparator();
        vel.addItem (1795, tr ("Soft: light hits sound louder. Hard: hit harder for full level. Fixed: every hit is full velocity."), false);
        menu.addSubMenu (tr ("Velocity curves"), vel);
    }
    // Push 1 display
    {
        juce::PopupMenu pd;
        pd.addItem (1735, tr ("Loop progress bars on the display"), true, proc.isPushProgressShown());
        pd.addItem (1796, tr ("Line 3: state letter and loop position per track. Punch FX / XY amount as a bar on line 4 while played."), false);
        menu.addSubMenu (tr ("Push 1 display"), pd);
        toggles.add (1735);
    }
    // generic MIDI feedback (experimental)
    {
        juce::PopupMenu fb;
        fb.addItem (1740, tr ("Send LED feedback for learned controls"), true, proc.isMidiFeedbackOn());
        juce::PopupMenu outs;
        fbDevices = juce::MidiOutput::getAvailableDevices();
        const auto curOut = proc.getMidiFeedbackOutput();
        outs.addItem (1750, tr ("None"), true, curOut.isEmpty());
        for (int i = 0; i < juce::jmin (30, fbDevices.size()); ++i)
            outs.addItem (1751 + i, fbDevices[i].name, true, fbDevices[i].identifier == curOut || fbDevices[i].name == curOut);
        fb.addSubMenu (tr ("MIDI output"), outs);
        const auto prof = proc.getMidiFeedbackProfile();
        juce::PopupMenu on, offm, blink;
        for (int i = 0; i < 4; ++i)
            on.addItem (1800 + i, juce::String (MidiFeedback::kOnValues[i]), true, prof.onValue == MidiFeedback::kOnValues[i]);
        for (int i = 0; i < 3; ++i)
            offm.addItem (1810 + i, juce::String (MidiFeedback::kOffValues[i]), true, prof.offValue == MidiFeedback::kOffValues[i]);
        for (int i = 0; i < 4; ++i)
        {
            const int v = MidiFeedback::kBlinkValues[i];
            blink.addItem (1820 + i, v < 0 ? tr ("Blink in time (livelooper switches on / off)") : juce::String (v),
                           true, v < 0 ? prof.softBlink : (! prof.softBlink && prof.blinkValue == v));
        }
        fb.addSubMenu (tr ("On value"), on);
        fb.addSubMenu (tr ("Off value"), offm);
        fb.addSubMenu (tr ("Blink value"), blink);
        fb.addSeparator();
        fb.addItem (1797, tr ("Sends a note-on / CC with the learned channel and number: on = playing / switched on, blink = recording / waiting, off. Knobs learned on a CC get their value."), false);
        fb.addItem (1798, tr (juce::String::fromUTF8 ("Experimental \xe2\x80\x93 please test with your controller")), false);
        menu.addSubMenu (tr ("MIDI feedback (experimental)"), fb);
        toggles.add (1740);
    }
}

bool LiveLooperEditor::handle17Menu (int r)
{
    if (r < 1700 || r > 1899)
        return false;
    auto setBool = [this] (const juce::String& id, bool on) {
        if (auto* p = proc.getState().getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (on ? 1.0f : 0.0f);
            p->endChangeGesture();
        }
    };
    if (r == 1700)
        proc.startLatencyMeasurement();
    else if (r == 1701)
        setBool (gLatComp, ! proc.isLatencyCompensationOn());
    else if (r == 1702)
        proc.clearMeasuredLatency();
    else if (r >= 1710 && r < 1710 + (int) std::size (kOffsets))
        proc.setLatencyOffsetMs ((float) kOffsets[r - 1710]);
    else if (r >= 1720 && r < 1732)
        proc.setVelocityCurve ((LiveLooperProcessor::VelocitySource) ((r - 1720) / 4), velocity::fromIndex ((r - 1720) % 4));
    else if (r == 1735)
        setBool (gPushProgress, ! proc.isPushProgressShown());
    else if (r == 1740)
        setBool (gMidiFeedback, ! proc.isMidiFeedbackOn());
    else if (r == 1750)
        proc.setMidiFeedbackOutput ({});
    else if (r > 1750 && r <= 1780 && r - 1751 < fbDevices.size())
        proc.setMidiFeedbackOutput (fbDevices[r - 1751].identifier);
    else if (r >= 1800 && r < 1830)
    {
        auto prof = proc.getMidiFeedbackProfile();
        if (r < 1804)
            prof.onValue = MidiFeedback::kOnValues[r - 1800];
        else if (r >= 1810 && r < 1813)
            prof.offValue = MidiFeedback::kOffValues[r - 1810];
        else if (r >= 1820 && r < 1824)
        {
            const int v = MidiFeedback::kBlinkValues[r - 1820];
            prof.softBlink = v < 0;
            if (v >= 0)
                prof.blinkValue = v;
        }
        proc.setMidiFeedbackProfile (prof);
    }
    proc.updateLatency();
    return true;
}

void LiveLooperEditor::timer17()
{
    // the SETUP window shows the measurement while it runs and its result afterwards
    const int st = (int) proc.getLatencyMeasure().getStatus();
    if (st != shownLatencyStatus)
    {
        shownLatencyStatus = st;
        if (settingsWindow != nullptr)
            if (auto* sp = dynamic_cast<SettingsPanel*> (settingsWindow->getContentComponent()))
                sp->rebuild();
    }
}
} // namespace livelooper
