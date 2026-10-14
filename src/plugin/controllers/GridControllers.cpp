#include "GridControllers.h"

#include "../AppSettings.h"
#include "../Localisation.h"
#include "../PluginProcessor.h"

namespace livelooper::controllers
{
using namespace params;

namespace
{
    bool isLaunchpad (TemplateId t) { return t == TemplateId::LaunchpadMiniMk3 || t == TemplateId::LaunchpadX; }
    bool isApc (TemplateId t)       { return t == TemplateId::ApcMiniMk2 || t == TemplateId::ApcMini; }
    bool isLaunchkey (TemplateId t) { return t == TemplateId::LaunchkeyMk3 || t == TemplateId::LaunchkeyMk4; }

    // Launchpad programmer mode: right column (scene launch) and top row CCs
    constexpr int kLpScene[8] = { 89, 79, 69, 59, 49, 39, 29, 19 };
    constexpr int kLpTopFirst = 91;
    // APC mini mk2: track buttons 0x64.., scene buttons 0x70.., shift 0x7A, faders CC 0x30..0x38
    constexpr int kApc2TrackFirst = 100, kApc2SceneFirst = 112, kApc2Shift = 122;
    // APC mini (first model): track buttons 64.., scene buttons 82.., shift 98, faders CC 48..56
    constexpr int kApc1TrackFirst = 64, kApc1SceneFirst = 82, kApc1Shift = 98;
    constexpr int kFaderFirst = 48;
    // Launchkey DAW mode: session pads 0x60.. (upper) / 0x70.. (lower), scene launch CC 0x68, stop/solo/mute CC 0x69
    constexpr int kLkUpper = 0x60, kLkLower = 0x70, kLkScene = 0x68, kLkStop = 0x69;

    // function buttons (Launchpad top row / APC track buttons), left to right
    constexpr Action::Kind kFunctions[8] = { Action::Kind::AllStartStop, Action::Kind::AllStop, Action::Kind::Capture, Action::Kind::Tap,
                                             Action::Kind::Rhythm, Action::Kind::Metronome, Action::Kind::Recorder, Action::Kind::Shift };

    int palette (Led c)
    {
        switch (c)
        {
            case Led::Off: return 0;        case Led::White: return 3;
            case Led::Red: return 5;        case Led::RedDim: return 7;
            case Led::Orange: return 9;     case Led::OrangeDim: return 11;
            case Led::Yellow: return 13;    case Led::YellowDim: return 15;
            case Led::Green: return 21;     case Led::GreenDim: return 23;
            case Led::Cyan: return 37;      case Led::Blue: return 45;
            case Led::Magenta: return 53;
        }
        return 0;
    }

    int apc1Colour (Led c, Blink b)
    {
        if (c == Led::Off)
            return 0;
        int base = 1;                                                  // green
        if (c == Led::Red || c == Led::RedDim || c == Led::Magenta) base = 3;
        else if (c == Led::Orange || c == Led::OrangeDim || c == Led::Yellow || c == Led::YellowDim) base = 5;
        return base + (b != Blink::Solid ? 1 : 0);
    }
}

juce::String templateName (TemplateId t)
{
    switch (t)
    {
        case TemplateId::LaunchpadMiniMk3: return "Launchpad Mini MK3";
        case TemplateId::LaunchpadX:       return "Launchpad X";
        case TemplateId::ApcMiniMk2:       return "APC mini mk2";
        case TemplateId::ApcMini:          return "APC mini";
        case TemplateId::LaunchkeyMk3:     return "Launchkey MK3 (pads)";
        case TemplateId::LaunchkeyMk4:     return "Launchkey MK4 (pads)";
        case TemplateId::NumTemplates:     break;
    }
    return {};
}

int detectTemplate (const juce::String& portName)
{
    const auto n = portName.toLowerCase();
    const bool dawPort = n.contains ("daw") || n.startsWith ("midiin2") || n.startsWith ("midiout2");
    if ((n.contains ("lpminimk3") || n.contains ("launchpad mini mk3")) && ! dawPort) return (int) TemplateId::LaunchpadMiniMk3;
    if ((n.contains ("lpx") || n.contains ("launchpad x")) && ! dawPort)               return (int) TemplateId::LaunchpadX;
    if (n.contains ("apc mini mk2"))                                                    return n.contains ("notes") ? -1 : (int) TemplateId::ApcMiniMk2;
    if (n.contains ("apc mini"))                                                        return (int) TemplateId::ApcMini;
    if ((n.contains ("lkmk3") || n.contains ("launchkey mk3")) && dawPort)              return (int) TemplateId::LaunchkeyMk3;
    if ((n.contains ("lkmk4") || n.contains ("launchkey mk4")) && dawPort)              return (int) TemplateId::LaunchkeyMk4;
    return -1;
}

//============================================================================== Template
int Template::getRows() const noexcept { return isLaunchkey (id) ? 2 : 8; }

int Template::padNote (int row, int col) const noexcept
{
    if (isLaunchpad (id)) return (8 - row) * 10 + col + 1;
    if (isApc (id))       return (7 - row) * 8 + col;
    return (row == 0 ? kLkUpper : kLkLower) + col;
}

std::vector<juce::MidiMessage> Template::enterMessages() const
{
    if (isLaunchpad (id))
    {
        const juce::uint8 dev = id == TemplateId::LaunchpadMiniMk3 ? 0x0d : 0x0c;
        const juce::uint8 sx[] = { 0x00, 0x20, 0x29, 0x02, dev, 0x0e, 0x01 };     // programmer mode
        return { juce::MidiMessage::createSysExMessage (sx, (int) sizeof (sx)) };
    }
    if (isLaunchkey (id))
        return { juce::MidiMessage::noteOn (16, 0x0c, (juce::uint8) 127),         // DAW mode on
                 juce::MidiMessage::controllerEvent (16, 0x03, 2) };              // pads: session layout
    return {};
}

std::vector<juce::MidiMessage> Template::exitMessages() const
{
    if (isLaunchpad (id))
    {
        const juce::uint8 dev = id == TemplateId::LaunchpadMiniMk3 ? 0x0d : 0x0c;
        const juce::uint8 sx[] = { 0x00, 0x20, 0x29, 0x02, dev, 0x0e, 0x00 };     // back to live mode
        return { juce::MidiMessage::createSysExMessage (sx, (int) sizeof (sx)) };
    }
    if (isLaunchkey (id))
        return { juce::MidiMessage::noteOn (16, 0x0c, (juce::uint8) 0) };         // DAW mode off
    return {};
}

Action Template::translate (const juce::MidiMessage& m, bool shift) const
{
    Action a;
    const bool note = m.isNoteOnOrOff();
    const bool cc = m.isController();
    if (! note && ! cc)
        return a;
    const int num = note ? m.getNoteNumber() : m.getControllerNumber();
    const bool pressed = note ? (m.isNoteOn() && m.getVelocity() > 0) : m.getControllerValue() > 0;
    a.pressed = pressed;

    auto shiftAction = [&a, pressed] { a.kind = Action::Kind::Shift; a.pressed = pressed; return a; };

    // ---- function buttons, shift, faders
    if (isLaunchpad (id) && cc && num >= kLpTopFirst && num < kLpTopFirst + 8)
    {
        const auto k = kFunctions[num - kLpTopFirst];
        if (k == Action::Kind::Shift) return shiftAction();
        if (pressed) a.kind = k;
        return a;
    }
    if (isApc (id))
    {
        const int trackFirst = id == TemplateId::ApcMiniMk2 ? kApc2TrackFirst : kApc1TrackFirst;
        const int shiftNote = id == TemplateId::ApcMiniMk2 ? kApc2Shift : kApc1Shift;
        if (cc && num >= kFaderFirst && num <= kFaderFirst + 8)
        {
            a.kind = num == kFaderFirst + 8 ? Action::Kind::MasterLevel : Action::Kind::Level;
            a.index = num - kFaderFirst;
            a.value = (float) m.getControllerValue() / 127.0f;
            a.pressed = true;
            return a;
        }
        if (note && num == shiftNote) return shiftAction();
        if (note && num >= trackFirst && num < trackFirst + 7)
        {
            if (pressed) a.kind = kFunctions[num - trackFirst];
            return a;
        }
    }
    if (isLaunchkey (id) && cc && (num == kLkScene || num == kLkStop))
    {
        if (pressed) a.kind = num == kLkScene ? Action::Kind::AllStartStop : Action::Kind::AllStop;
        return a;
    }
    if (! pressed)
        return a;

    // ---- scene buttons
    for (int s = 0; s < 8; ++s)
    {
        const bool hit = (isLaunchpad (id) && cc && num == kLpScene[s])
                      || (id == TemplateId::ApcMiniMk2 && note && num == kApc2SceneFirst + s)
                      || (id == TemplateId::ApcMini && note && num == kApc1SceneFirst + s);
        if (hit)
        {
            a.kind = shift ? Action::Kind::SceneStore : Action::Kind::Scene;
            a.index = s;
            return a;
        }
    }

    // ---- grid pads
    if (! note)
        return a;
    for (int row = 0; row < getRows(); ++row)
        for (int col = 0; col < 8; ++col)
            if (padNote (row, col) == num)
            {
                using K = Action::Kind;
                static constexpr K normal[8] = { K::RecPlay, K::Stop, K::Undo, K::Mute, K::HalfSpeed, K::DoubleSpeed, K::Reverse, K::LoopDouble };
                static constexpr K shifted[8] = { K::RecPlay, K::Clear, K::Redo, K::Mute, K::HalfSpeed, K::DoubleSpeed, K::Reverse, K::LoopHalve };
                a.kind = (shift ? shifted : normal)[row];
                a.index = col;
                return a;
            }
    return a;
}

juce::MidiMessage Template::led (int number, Led colour, Blink blink, bool isCC) const
{
    if (isApc (id))
    {
        const bool pad = ! isCC && number < 64;
        if (! pad)                     // single-colour buttons: 0 off, 1 on, 2 blink
            return juce::MidiMessage::noteOn (1, number, (juce::uint8) (colour == Led::Off ? 0 : (blink != Blink::Solid ? 2 : 1)));
        if (id == TemplateId::ApcMini)
            return juce::MidiMessage::noteOn (1, number, (juce::uint8) apc1Colour (colour, blink));
        // mk2 pads: the channel sets brightness / behaviour (7 = 100 %, 10 = pulse 1/4, 14 = blink 1/8)
        const int ch = blink == Blink::Solid ? 7 : (blink == Blink::Pulse ? 10 : 14);
        return juce::MidiMessage::noteOn (ch, number, (juce::uint8) palette (colour));
    }
    // Launchpad / Launchkey: channel 1 static, 2 flashing, 3 pulsing; velocity = palette colour
    const int ch = blink == Blink::Solid ? 1 : (blink == Blink::Flash ? 2 : 3);
    return isCC ? juce::MidiMessage::controllerEvent (ch, number, palette (colour))
                : juce::MidiMessage::noteOn (ch, number, (juce::uint8) palette (colour));
}

void Template::render (const SurfaceState& s, std::vector<juce::MidiMessage>& out) const
{
    using S = LoopTrack::State;
    for (int col = 0; col < 8; ++col)
    {
        const auto& t = s.tracks[(size_t) col];
        const bool hasLoop = t.exists && t.state != (int) S::Empty;
        for (int row = 0; row < getRows(); ++row)
        {
            Led c = Led::Off;
            Blink b = Blink::Solid;
            if (t.exists)
                switch (row)
                {
                    case 0:
                        if (t.pending) { c = Led::Yellow; b = Blink::Flash; }
                        else if (t.state == (int) S::Recording)   { c = Led::Red; b = Blink::Pulse; }
                        else if (t.state == (int) S::Overdubbing) { c = Led::Orange; b = Blink::Pulse; }
                        else if (t.state == (int) S::Playing)     c = t.muted ? Led::GreenDim : Led::Green;
                        else if (t.state == (int) S::Stopped)     c = Led::YellowDim;
                        break;
                    case 1: c = hasLoop ? (s.shift ? Led::Red : Led::RedDim) : Led::Off; break;
                    case 2: c = hasLoop ? Led::Blue : Led::Off; break;
                    case 3: c = hasLoop ? (t.muted ? Led::Yellow : Led::GreenDim) : Led::Off; break;
                    case 4: case 5: c = hasLoop ? Led::Magenta : Led::Off; break;
                    case 6: c = hasLoop ? Led::Cyan : Led::Off; break;
                    case 7: c = hasLoop ? Led::White : Led::Off; break;
                    default: break;
                }
            out.push_back (led (padNote (row, col), c, b));
        }
    }

    auto functionLed = [&s] (Action::Kind k, Led& c, Blink& b)
    {
        b = Blink::Solid;
        switch (k)
        {
            case Action::Kind::AllStartStop: c = s.running ? Led::Green : Led::GreenDim; break;
            case Action::Kind::AllStop:      c = Led::RedDim; break;
            case Action::Kind::Capture:      c = Led::RedDim; break;
            case Action::Kind::Tap:          c = Led::YellowDim; break;
            case Action::Kind::Rhythm:       c = s.rhythm ? Led::Orange : Led::OrangeDim; break;
            case Action::Kind::Metronome:    c = s.metronome ? Led::Yellow : Led::YellowDim; break;
            case Action::Kind::Recorder:     c = s.recorder ? Led::Red : Led::RedDim; b = s.recorder ? Blink::Pulse : Blink::Solid; break;
            case Action::Kind::Shift:        c = s.shift ? Led::White : Led::Off; break;
            default:                         c = Led::Off; break;
        }
    };
    auto sceneLed = [&s] (int i) { return s.scenes[(size_t) i] == 2 ? Led::Green : (s.scenes[(size_t) i] == 1 ? Led::GreenDim : (s.shift ? Led::RedDim : Led::Off)); };

    if (isLaunchpad (id))
    {
        for (int i = 0; i < 8; ++i)
        {
            Led c; Blink b;
            functionLed (kFunctions[i], c, b);
            out.push_back (led (kLpTopFirst + i, c, b, true));
            out.push_back (led (kLpScene[i], sceneLed (i), Blink::Solid, true));
        }
    }
    else if (isApc (id))
    {
        const int trackFirst = id == TemplateId::ApcMiniMk2 ? kApc2TrackFirst : kApc1TrackFirst;
        const int sceneFirst = id == TemplateId::ApcMiniMk2 ? kApc2SceneFirst : kApc1SceneFirst;
        for (int i = 0; i < 7; ++i)
        {
            Led c; Blink b;
            functionLed (kFunctions[i], c, b);
            // single LEDs: only "on" states light up
            const bool on = c == Led::Green || c == Led::Orange || c == Led::Yellow || c == Led::Red;
            out.push_back (led (trackFirst + i, on ? c : Led::Off, b));
        }
        for (int i = 0; i < 8; ++i)
            out.push_back (led (sceneFirst + i, s.scenes[(size_t) i] > 0 ? Led::Green : Led::Off,
                                s.scenes[(size_t) i] == 2 ? Blink::Pulse : Blink::Solid));
    }
}

//============================================================================== device
std::atomic<GridControllerDevice*> GridControllerDevice::owner { nullptr };

GridControllerDevice::GridControllerDevice (LiveLooperProcessor& p) : proc (p)
{
    const auto v = appsettings::getValue ("controllerTemplate", "auto");
    selection = v == "auto" ? Selection::Automatic : (v == "off" ? Selection::Off
              : (Selection) juce::jlimit ((int) Selection::First, kNumTemplates - 1, v.getIntValue()));
    startTimerHz (30);
}

GridControllerDevice::~GridControllerDevice()
{
    stopTimer();
    cancelPendingUpdate();
    disconnect();
}

void GridControllerDevice::setSelection (Selection s)
{
    if (s == selection)
        return;
    selection = s;
    appsettings::setValue ("controllerTemplate", s == Selection::Automatic ? juce::String ("auto")
                                                 : (s == Selection::Off ? juce::String ("off") : juce::String ((int) s)));
    disconnect();
    ticks = 0;
}

juce::String GridControllerDevice::getStatusText() const
{
    if (selection == Selection::Off)
        return i18n::tr ("Off");
    if (isConnected() && tmpl != nullptr)
        return i18n::tr ("Connected: %1", deviceName + " (" + templateName (tmpl->getId()) + ")");
    if (openFailed)
        return i18n::tr ("Controller found, but its MIDI port is in use (Windows: switch it off in your DAW's MIDI settings)");
    return i18n::tr ("No supported controller found");
}

void GridControllerDevice::connect()
{
    if (selection == Selection::Off)
        return;
    GridControllerDevice* expected = nullptr;
    if (! owner.compare_exchange_strong (expected, this) && expected != this)
        return;                                          // another livelooper instance drives it

    const int wanted = selection == Selection::Automatic ? -1 : (int) selection;
    auto pick = [wanted] (const juce::Array<juce::MidiDeviceInfo>& list) -> std::pair<juce::MidiDeviceInfo, int>
    {
        std::pair<juce::MidiDeviceInfo, int> best { {}, -1 };
        for (const auto& d : list)
        {
            const int t = detectTemplate (d.name);
            if (t < 0)
                continue;
            if (wanted < 0 || t == wanted)
                return { d, t };
            if (best.second < 0)
                best = { d, t };
        }
        return best;
    };
    auto in = pick (juce::MidiInput::getAvailableDevices());
    auto out = pick (juce::MidiOutput::getAvailableDevices());
    if (in.second < 0 || out.second < 0)
    {
        auto* self = this;
        owner.compare_exchange_strong (self, nullptr);
        return;
    }
    input = juce::MidiInput::openDevice (in.first.identifier, this);
    output = juce::MidiOutput::openDevice (out.first.identifier);
    if (input == nullptr || output == nullptr)
    {
        input.reset();
        output.reset();
        openFailed = true;
        auto* self = this;
        owner.compare_exchange_strong (self, nullptr);
        return;
    }
    openFailed = false;
    tmpl = std::make_unique<Template> ((TemplateId) (wanted >= 0 ? wanted : in.second));
    inputId = in.first.identifier;
    deviceName = in.first.name;
    input->start();
    for (const auto& m : tmpl->enterMessages())
        output->sendMessageNow (m);
    sent.clear();
    sendLeds (true);
}

void GridControllerDevice::disconnect()
{
    if (input != nullptr)
        input->stop();
    if (output != nullptr && tmpl != nullptr)
    {
        std::vector<juce::MidiMessage> leds;
        tmpl->render (SurfaceState {}, leds);
        for (auto& m : leds)
            output->sendMessageNow (m);
        for (const auto& m : tmpl->exitMessages())
            output->sendMessageNow (m);
    }
    input.reset();
    output.reset();
    tmpl.reset();
    inputId.clear();
    sent.clear();
    shift = false;
    auto* self = this;
    owner.compare_exchange_strong (self, nullptr);
}

void GridControllerDevice::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    if (m.isSysEx() || m.getRawDataSize() > 3)
        return;
    const auto scope = fifo.write (1);
    auto store = [&] (int index) {
        auto& slot = queue[(size_t) index];
        slot.fill (0);
        std::memcpy (slot.data(), m.getRawData(), (size_t) m.getRawDataSize());
    };
    if (scope.blockSize1 > 0) store (scope.startIndex1);
    else if (scope.blockSize2 > 0) store (scope.startIndex2);
    triggerAsyncUpdate();
}

void GridControllerDevice::handleAsyncUpdate()
{
    if (tmpl == nullptr)
        return;
    const auto scope = fifo.read (fifo.getNumReady());
    auto handle = [this] (int start, int size) {
        for (int i = 0; i < size; ++i)
        {
            const auto& d = queue[(size_t) (start + i)];
            const int len = juce::MidiMessage::getMessageLengthFromFirstByte (d[0]);
            const auto a = tmpl->translate (juce::MidiMessage (d.data(), juce::jlimit (1, 3, len)), shift);
            if (a.kind == Action::Kind::Shift)
                shift = a.pressed;
            else
                apply (proc, a);
        }
    };
    handle (scope.startIndex1, scope.blockSize1);
    handle (scope.startIndex2, scope.blockSize2);
    sendLeds (false);
}

void GridControllerDevice::sendLeds (bool full)
{
    if (output == nullptr || tmpl == nullptr)
        return;
    std::vector<juce::MidiMessage> leds;
    tmpl->render (makeState (proc, shift), leds);
    for (const auto& m : leds)
    {
        const int key = (m.isController() ? 1000 : 0) + (m.isController() ? m.getControllerNumber() : m.getNoteNumber());
        auto it = sent.find (key);
        const bool same = it != sent.end() && it->second.getRawDataSize() == m.getRawDataSize()
                          && std::memcmp (it->second.getRawData(), m.getRawData(), (size_t) m.getRawDataSize()) == 0;
        if (full || ! same)
        {
            output->sendMessageNow (m);
            sent[key] = m;
        }
    }
}

void GridControllerDevice::timerCallback()
{
    ++ticks;
    if (selection == Selection::Off)
        return;
    if (! isConnected())
    {
        if (ticks % 60 == 1)              // every two seconds
            connect();
        return;
    }
    if (ticks % 60 == 0)
    {
        bool present = false;
        for (const auto& d : juce::MidiInput::getAvailableDevices())
            present = present || d.identifier == inputId;
        if (! present)
        {
            disconnect();
            return;
        }
    }
    sendLeds (ticks % 30 == 0);           // everything again once a second
}

//============================================================================== processor glue
void GridControllerDevice::apply (LiveLooperProcessor& p, const Action& a)
{
    using K = Action::Kind;
    auto toggle = [&p] (const juce::String& id) {
        if (auto* prm = p.getState().getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->getValue() >= 0.5f ? 0.0f : 1.0f);
            prm->endChangeGesture();
        }
    };
    auto set = [&p] (const juce::String& id, float plain) {
        if (auto* prm = p.getState().getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
            prm->endChangeGesture();
        }
    };
    const bool trackOk = a.index >= 0 && a.index < p.getVisibleTracks();
    switch (a.kind)
    {
        case K::RecPlay:     if (trackOk) p.pressTrigger (trackId (a.index, kRecPlay)); break;
        case K::Stop:        if (trackOk) p.pressTrigger (trackId (a.index, kStop)); break;
        case K::Clear:       if (trackOk) p.pressTrigger (trackId (a.index, kClear)); break;
        case K::Undo:        if (trackOk) p.pressTrigger (trackId (a.index, kUndo)); break;
        case K::Redo:        if (trackOk) p.pressTrigger (trackId (a.index, kRedo)); break;
        case K::Mute:        if (trackOk) toggle (trackId (a.index, kMute)); break;
        case K::HalfSpeed:   if (trackOk) p.pressTrigger (trackId (a.index, kHalfSpeed)); break;
        case K::DoubleSpeed: if (trackOk) p.pressTrigger (trackId (a.index, kDblSpeed)); break;
        case K::Reverse:     if (trackOk) p.pressTrigger (trackId (a.index, kRevLoop)); break;
        case K::LoopDouble:  if (trackOk) p.pressTrigger (trackId (a.index, kDouble)); break;
        case K::LoopHalve:   if (trackOk) p.pressTrigger (trackId (a.index, kHalve)); break;
        case K::Scene:       p.pressTrigger (sceneId (a.index)); break;
        case K::SceneStore:  set (gSceneStore, 1.0f); p.pressTrigger (sceneId (a.index)); break;
        case K::AllStartStop: p.pressTrigger (gAllStartStop); break;
        case K::AllStop:     p.pressTrigger (gAllStop); break;
        case K::Capture:     p.pressTrigger (gCapture); break;
        case K::Tap:         p.pressTrigger (gTap); break;
        case K::Rhythm:      toggle (gRhythmOn); break;
        case K::Metronome:   toggle (gMetroOn); break;
        case K::Recorder:    toggle (gPerfRec); break;
        case K::Level:       if (trackOk) set (trackId (a.index, kLevel), a.value); break;     // fader top = 100 %
        case K::MasterLevel: set (gMaster, a.value); break;
        case K::Shift: case K::None: break;
    }
}

SurfaceState GridControllerDevice::makeState (LiveLooperProcessor& p, bool shiftHeld)
{
    SurfaceState s;
    auto& e = p.getEngine();
    for (int t = 0; t < 8; ++t)
    {
        auto& tr = s.tracks[(size_t) t];
        tr.exists = t < p.getVisibleTracks();
        tr.state = e.getTrackStatus (t).state.load();
        tr.pending = e.getTrackStatus (t).pending.load() != 0 || e.getTrackStatus (t).pendingTrick.load() >= 0;
        tr.muted = p.getState().getRawParameterValue (trackId (t, kMute))->load() >= 0.5f;
    }
    for (int i = 0; i < 8; ++i)
        s.scenes[(size_t) i] = p.getLastScene() == i ? 2 : (p.sceneIsStored (i) ? 1 : 0);
    s.shift = shiftHeld;
    s.rhythm = p.getState().getRawParameterValue (gRhythmOn)->load() >= 0.5f;
    s.metronome = p.getState().getRawParameterValue (gMetroOn)->load() >= 0.5f;
    s.recorder = p.isPerformanceRecording();
    s.running = e.isTimelineRunning();
    return s;
}
} // namespace livelooper::controllers
