#include "Ko2.h"
#include "../PluginProcessor.h"
#include "../Localisation.h"

#include <utility>

namespace livelooper::ko2
{
using namespace params;

//============================================================================== names
bool matchesName (const juce::String& deviceName)
{
    const auto n = deviceName.toUpperCase().replace (juce::String::fromUTF8 ("\xe2\x80\x93"), "-");   // en dash
    if (n.contains ("EP-40") || n.contains ("EP-1320") || n.contains ("EP40") || n.contains ("EP1320"))
        return false;
    const auto compact = n.removeCharacters (" .-_");
    return n.contains ("EP-133") || compact.contains ("EP133") || compact.contains ("KOII") || compact.contains ("KO2");
}

int pickPort (const juce::StringArray& names)
{
    int found = -1;
    for (int i = 0; i < names.size(); ++i)
        if (matchesName (names[i]))
        {
            if (names[i].trim().endsWithIgnoreCase ("MIDI 1"))
                return i;
            if (found < 0)
                found = i;
        }
    return found;
}

//============================================================================== audio
AudioRoute chooseAudioRoute (const juce::StringArray& ins, const juce::StringArray& outs,
                             const juce::String& previousOutput, int defaultOutput, AudioOutput mode)
{
    AudioRoute r;
    const int in = pickPort (ins);
    if (in < 0)
        return r;
    r.ok = true;
    r.input = ins[in];
    if (mode == AudioOutput::Ko2)
        if (const int o = pickPort (outs); o >= 0)
        {
            r.output = outs[o];
            return r;
        }
    // the computer: the output used before, the system default, the first other output
    if (previousOutput.isNotEmpty() && ! matchesName (previousOutput) && outs.contains (previousOutput))
        r.output = previousOutput;
    else if (juce::isPositiveAndBelow (defaultOutput, outs.size()) && ! matchesName (outs[defaultOutput]))
        r.output = outs[defaultOutput];
    else
        for (const auto& o : outs)
            if (! matchesName (o))
            {
                r.output = o;
                break;
            }
    return r;
}

//============================================================================== hold ramp
double holdRampBeats (HoldRamp r, double measurePpq) noexcept
{
    switch (r)
    {
        case HoldRamp::OneBeat:  return 1.0;
        case HoldRamp::TwoBeats: return 2.0;
        case HoldRamp::OneBar:   return measurePpq > 0.0 ? measurePpq : 4.0;
        case HoldRamp::Off:      break;
    }
    return 0.0;
}

float holdRampAmount (float base, double heldMs, double bpm, double rampBeats) noexcept
{
    base = juce::jlimit (0.0f, 1.0f, base);
    if (rampBeats <= 0.0 || bpm <= 0.0 || heldMs <= 0.0)
        return base;
    const double beats = heldMs * bpm / 60000.0;
    const float t = (float) juce::jlimit (0.0, 1.0, beats / rampBeats);
    return base + (1.0f - base) * t;
}

//============================================================================== pads
juce::String padLabel (int offset)
{
    switch (offset)
    {
        case pad::dot:   return ".";
        case pad::zero:  return "0";
        case pad::enter: return "ENTER";
        default:         return juce::String (offset - 2);
    }
}

int padRow (int offset) noexcept
{
    if (offset <= pad::enter)
        return 3;
    return 2 - (offset - 3) / 3;           // 1 2 3 = row 2, 4 5 6 = row 1, 7 8 9 = row 0
}

int padColumn (int offset) noexcept
{
    return offset <= pad::enter ? offset : (offset - 3) % 3;
}

juce::String describe (Action::Kind kind, int index)
{
    using K = Action::Kind;
    const auto n = juce::String (index + 1);
    switch (kind)
    {
        case K::RecPlay:      return "T" + n + " REC";
        case K::Stop:         return index < 0 ? juce::String ("STOP") : "STOP " + n;
        case K::Clear:        return "CLEAR " + n;
        case K::Undo:         return index < 0 ? juce::String ("UNDO") : "UNDO " + n;
        case K::Redo:         return "REDO";
        case K::Mute:         return "MUTE " + n;
        case K::AllStartStop: return "ALL START/STOP";
        case K::AllStart:     return "ALL START";
        case K::AllStop:      return "ALL STOP";
        case K::Capture:      return "CAPTURE";
        case K::Tap:          return "TAP";
        case K::Metronome:    return "METRONOME";
        case K::Punch:        return getPunchFxName (index);
        case K::Scene:        return "Scene " + n;
        case K::SceneStore:   return "Store " + n;
        case K::Rhythm:       return "RHYTHM";
        case K::Fill:         return "FILL";
        case K::Recorder:     return "REC SET";
        case K::Resample:     return "RESAMPLE";
        case K::RoleSwitch:   return "ROLE";
        case K::None:         break;
    }
    return {};
}

//============================================================================== Controller
Action::Kind Controller::functionOf (int group, int offset, int& index) const noexcept
{
    using K = Action::Kind;
    index = -1;
    const bool number = offset >= pad::num1;
    const int num = offset - 2;              // 1 .. 9
    if (compact (group))
    {
        // Record K.O. II: group D is a compact looper
        if (number && num <= 8) { index = num - 1; return K::RecPlay; }
        if (num == 9)           return K::AllStartStop;
        if (offset == pad::zero) return K::Undo;
        if (offset == pad::dot)  return K::Stop;
        return K::Resample;                  // ENTER
    }
    switch (group)
    {
        case 0:   // A: loops
            if (number && num <= 8) { index = num - 1; return K::RecPlay; }
            if (num == 9)            return K::AllStartStop;
            if (offset == pad::dot)  return K::Stop;
            if (offset == pad::zero) return K::Undo;
            return K::Capture;
        case 1:   // B: track edit
            if (number && num <= 8) { index = num - 1; return K::Stop; }
            if (num == 9)            return K::AllStop;
            if (offset == pad::dot)  return K::None;       // SHIFT (held: pads = undo)
            if (offset == pad::zero) return K::Tap;
            return K::Metronome;
        case 2:   // C: punch FX, same positions as the device's own punch-in pads
            index = offset;
            return K::Punch;
        case 3:   // D: scenes and setup
            if (number && num <= 8) { index = num - 1; return K::Scene; }
            if (num == 9)            return K::Rhythm;
            if (offset == pad::zero) return K::Fill;
            if (offset == pad::enter) return K::Recorder;
            return K::None;                                // "." (+ ENTER held = role switch)
        default:
            return K::None;
    }
}

void Controller::handleNote (int note, int velocity, bool on, juce::uint32 now, std::vector<Action>& out)
{
    if (! isPadNote (note))
    {
        if (on)
            keysWarning = true;      // KEYS mode: chromatic notes, not the pad map
        return;
    }
    const int group = groupOf (note), offset = offsetOf (note);
    page = group;
    if (! isControlGroup (group))
        return;
    if (on && velocity > 0)
        press (group, offset, velocity, now, out);
    else
        release (group, offset, now, out);
}

void Controller::press (int group, int offset, int velocity, juce::uint32 now, std::vector<Action>& out)
{
    using K = Action::Kind;
    auto& p = h (group, offset);
    if (p.down)
    {
        // a new hit on a held punch pad (if a device ever sends one): new start amount, the ramp starts again
        int idx = -1;
        if (functionOf (group, offset, idx) == K::Punch)
        {
            p.velocity = velocity;
            punchPressed (velocity, now);
            out.push_back ({ K::Punch, idx, true, velocity });
        }
        return;
    }
    const auto lastTapAt = lastTap[(size_t) (group * kPadsPerGroup + offset)];
    p = { true, now, velocity, false, false };
    lastTap[(size_t) (group * kPadsPerGroup + offset)] = now;

    // role switch chord: "." and ENTER of group D pressed within 150 ms
    if (group == 3 && (offset == pad::dot || offset == pad::enter))
    {
        auto& other = h (3, offset == pad::dot ? pad::enter : pad::dot);
        if (other.down && now - other.since <= kChordMs)
        {
            p.chord = other.chord = true;
            chordDone = false;
            return;
        }
    }

    int index = -1;
    const auto kind = functionOf (group, offset, index);
    const bool shiftB = group == 1 && h (1, pad::dot).down;
    switch (kind)
    {
        case K::RecPlay:
            lastTrack = index;
            if (index < numTracks)
                out.push_back ({ K::RecPlay, index });
            return;
        case K::Stop:
            if (index < 0)
                return;                          // "." : on release (hold = clear)
            lastTrack = index;
            if (index >= numTracks)
                return;
            if (shiftB)
            {
                out.push_back ({ K::Undo, index });
                p.holdDone = true;
                return;
            }
            if (now - lastTapAt <= kDoubleTapMs && lastTapAt != 0)
            {
                out.push_back ({ K::Mute, index });    // double tap = mute on / off
                p.holdDone = true;
                return;
            }
            out.push_back ({ K::Stop, index });
            return;
        case K::Punch:
            ++punch.count;
            punchPressed (velocity, now);      // the latest hit sets the start amount
            out.push_back ({ K::Punch, index, true, velocity });
            return;
        case K::Fill:
            out.push_back ({ K::Fill, -1, true });
            return;
        case K::AllStartStop: case K::AllStop: case K::Capture: case K::Tap: case K::Metronome: case K::Rhythm:
            out.push_back ({ kind, -1 });
            return;
        case K::Undo: case K::Scene: case K::Recorder: case K::Resample: case K::None:
        default:
            return;                              // these act on release (their hold does something else)
    }
}

void Controller::release (int group, int offset, juce::uint32 now, std::vector<Action>& out)
{
    using K = Action::Kind;
    auto& p = h (group, offset);
    if (! p.down)
        return;
    const bool wasHoldDone = p.holdDone, wasChord = p.chord;
    p.down = false;
    if (wasChord)
    {
        p.chord = false;
        return;
    }
    int index = -1;
    const auto kind = functionOf (group, offset, index);
    if (kind == K::Punch)
    {
        // the last punch pad up: the amount goes back to the hit (the ramp / pressure moved it)
        punch.count = juce::jmax (0, punch.count - 1);
        int back = 0;
        if (punch.count == 0)
        {
            if (holdRamp != HoldRamp::Off || punch.pressureSeen)
                back = juce::jmax (1, punch.velocity);
            punch = {};
        }
        out.push_back ({ K::Punch, index, false, back });
        return;
    }
    if (kind == K::Fill)
    {
        out.push_back ({ K::Fill, -1, false });
        return;
    }
    if (wasHoldDone)
        return;
    juce::ignoreUnused (now);
    switch (kind)
    {
        case K::Stop:
            if (index < 0)
                out.push_back ({ K::Stop, lastTrack });          // "." = stop the last used track
            return;
        case K::Undo:
            out.push_back ({ K::Undo, lastTrack });
            return;
        case K::Scene:
            out.push_back ({ K::Scene, index });
            return;
        case K::Recorder:
        case K::Resample:
            out.push_back ({ kind, -1 });
            return;
        default:
            return;
    }
}

void Controller::punchPressed (int velocity, juce::uint32 now) noexcept
{
    punch.velocity = velocity;
    punch.since = now;
    punch.pressure = -1;
}

void Controller::handlePressure (int note, int value, juce::uint32 nowMs)
{
    juce::ignoreUnused (nowMs);
    if (punch.count <= 0)
        return;
    if (note >= 0)
    {
        // poly aftertouch: only for a held punch pad of a control group
        if (! isPadNote (note) || ! isHeld (note) || ! isControlGroup (groupOf (note)))
            return;
        int idx = -1;
        if (functionOf (groupOf (note), offsetOf (note), idx) != Action::Kind::Punch)
            return;
    }
    punch.pressure = juce::jlimit (0, 127, value);
    punch.pressureSeen = true;
}

float Controller::livePunchAmount (juce::uint32 nowMs, double bpm, double measurePpq) const noexcept
{
    if (punch.count <= 0)
        return -1.0f;
    const float base = punchAmountFromVelocity (punch.velocity);
    if (punch.pressureSeen)          // pressure drives it (no time ramp)
        return juce::jmax (base, punch.pressure < 0 ? 0.0f : (float) punch.pressure / 127.0f);
    return holdRampAmount (base, (double) (nowMs - punch.since), bpm, holdRampBeats (holdRamp, measurePpq));
}

void Controller::update (juce::uint32 now, std::vector<Action>& out)
{
    using K = Action::Kind;
    // the role switch chord: both held for a second
    auto& dot = h (3, pad::dot);
    auto& enter = h (3, pad::enter);
    if (dot.down && enter.down && dot.chord && enter.chord && ! chordDone
        && now - dot.since >= kHoldMs && now - enter.since >= kHoldMs)
    {
        chordDone = true;
        out.push_back ({ K::RoleSwitch, -1 });
    }

    for (int g = 0; g < kNumGroups; ++g)
        for (int o = 0; o < kPadsPerGroup; ++o)
        {
            auto& p = h (g, o);
            if (! p.down || p.holdDone || p.chord || now - p.since < kHoldMs || ! isControlGroup (g))
                continue;
            int index = -1;
            const auto kind = functionOf (g, o, index);
            switch (kind)
            {
                case K::Stop:          // hold = clear (track pad or "." = last track)
                    p.holdDone = true;
                    out.push_back ({ K::Clear, index >= 0 ? index : lastTrack });
                    break;
                case K::Undo:          // hold = redo
                    p.holdDone = true;
                    out.push_back ({ K::Redo, lastTrack });
                    break;
                case K::Scene:         // hold = store
                    p.holdDone = true;
                    out.push_back ({ K::SceneStore, index });
                    break;
                default:
                    break;
            }
        }
}

void Controller::releaseAll (juce::uint32 now, std::vector<Action>& out)
{
    for (int g = 0; g < kNumGroups; ++g)
        for (int o = 0; o < kPadsPerGroup; ++o)
            if (h (g, o).down)
            {
                int index = -1;
                const auto kind = functionOf (g, o, index);
                h (g, o).holdDone = true;          // no tap actions on a forced release
                if (kind == Action::Kind::Punch || kind == Action::Kind::Fill)
                    release (g, o, now, out);
                h (g, o).down = false;
            }
}

//============================================================================== Clock
void Clock::process (double ppq, bool running, double measurePpq, std::vector<juce::MidiMessage>& out)
{
    if (! running)
    {
        stop (out);
        return;
    }
    const double bar = measurePpq > 0.0 ? measurePpq : 4.0;
    if (! started)
    {
        // start on a bar line: song position 0, then start; the first clock follows right away
        const double inBar = std::fmod (std::max (0.0, ppq), bar);
        if (inBar > 1.0 / 24.0 && bar - inBar > 1.0e-6)
            return;
        out.push_back (juce::MidiMessage::songPositionPointer (0));
        out.push_back (juce::MidiMessage::midiStart());
        out.push_back (juce::MidiMessage::midiClock());
        started = true;
        lastTick = (long long) std::floor (ppq * 24.0 + 1.0e-6);
        return;
    }
    const long long tick = (long long) std::floor (ppq * 24.0 + 1.0e-6);
    if (tick < lastTick)
    {
        if (lastTick - tick > 24)
            lastTick = tick;       // the timeline jumped back: keep counting from here
        return;                    // (small steps back = the estimate ran ahead of the audio block: wait)
    }
    for (long long t = lastTick; t < tick && t - lastTick < 48; ++t)
        out.push_back (juce::MidiMessage::midiClock());
    lastTick = tick;
}

void Clock::stop (std::vector<juce::MidiMessage>& out)
{
    if (started)
        out.push_back (juce::MidiMessage::midiStop());
    started = false;
}

//============================================================================== apply
void apply (LiveLooperProcessor& p, const Action& a)
{
    using K = Action::Kind;
    auto& st = p.getState();
    auto set = [&st] (const juce::String& id, float v) {
        if (auto* prm = st.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (v);
            prm->endChangeGesture();
        }
    };
    auto toggle = [&st, &set] (const juce::String& id) {
        if (auto* prm = st.getParameter (id))
            set (id, prm->getValue() >= 0.5f ? 0.0f : 1.0f);
    };
    const bool trackOk = a.index >= 0 && a.index < p.getVisibleTracks();
    switch (a.kind)
    {
        case K::RecPlay: if (trackOk) p.pressTrigger (trackId (a.index, kRecPlay)); break;
        case K::Stop:    if (trackOk) p.pressTrigger (trackId (a.index, kStop)); break;
        case K::Clear:   if (trackOk) p.pressTrigger (trackId (a.index, kClear)); break;
        case K::Undo:    if (trackOk) p.pressTrigger (trackId (a.index, kUndo)); break;
        case K::Redo:    if (trackOk) p.pressTrigger (trackId (a.index, kRedo)); break;
        case K::Mute:    if (trackOk) toggle (trackId (a.index, kMute)); break;
        case K::AllStartStop: p.pressTrigger (gAllStartStop); break;
        case K::AllStart:     p.pressTrigger (gAllStart); break;
        case K::AllStop:      p.pressTrigger (gAllStop); break;
        case K::Capture:      p.pressTrigger (gCapture); break;
        case K::Tap:          p.pressTrigger (gTap); break;
        case K::Metronome:    toggle (gMetroOn); break;
        case K::Punch:
            if (a.index >= 0 && a.index < kNumPunchHolds)
            {
                if (a.on || a.velocity > 0)
                    set (gPfxAmount, punchAmountFromVelocity (a.velocity));     // velocity = amount (release: back to the hit)
                set (punchHoldId (a.index), a.on ? 1.0f : 0.0f);
            }
            break;
        case K::Scene:      if (a.index >= 0 && a.index < kNumScenes) p.pressTrigger (sceneId (a.index)); break;
        case K::SceneStore:
            if (a.index >= 0 && a.index < kNumScenes)
            {
                set (gSceneStore, 1.0f);
                p.pressTrigger (sceneId (a.index));
            }
            break;
        case K::Rhythm:     toggle (gRhythmOn); break;
        case K::Fill:       set (gRhythmFill, a.on ? 1.0f : 0.0f); break;
        case K::Recorder:   toggle (gPerfRec); break;
        case K::Resample:   p.pressTrigger (gKo2Resample); break;
        case K::RoleSwitch: toggle (gCtlAudioMode); break;
        case K::None:       break;
    }
}

//============================================================================== Device
std::atomic<Device*> Device::owner { nullptr };

Device::Device (LiveLooperProcessor& p) : proc (p)
{
    startTimerHz (50);
}

Device::~Device()
{
    juce::HighResolutionTimer::stopTimer();
    juce::Timer::stopTimer();
    cancelPendingUpdate();
    disconnect();
}

void Device::setSimulatedConnection (bool on)
{
    if (simulated == on)
        return;
    simulated = on;
    if (onConnectionChanged)
        onConnectionChanged (isConnected());
}

void Device::connect()
{
    Device* expected = nullptr;
    if (! owner.compare_exchange_strong (expected, this) && expected != this)
        return;                          // another livelooper instance in this process has it

    auto names = [] (const juce::Array<juce::MidiDeviceInfo>& list) {
        juce::StringArray s;
        for (const auto& d : list)
            s.add (d.name);
        return s;
    };
    const auto ins = juce::MidiInput::getAvailableDevices();
    const auto outs = juce::MidiOutput::getAvailableDevices();
    const int i = pickPort (names (ins));
    if (i < 0)
    {
        auto* self = this;
        owner.compare_exchange_strong (self, nullptr);
        return;
    }
    input = juce::MidiInput::openDevice (ins[i].identifier, this);
    if (input == nullptr)
    {
        auto* self = this;
        owner.compare_exchange_strong (self, nullptr);
        return;
    }
    if (const int o = pickPort (names (outs)); o >= 0)
    {
        const juce::ScopedLock sl (outputLock);
        output = juce::MidiOutput::openDevice (outs[o].identifier);
    }
    inputId = ins[i].identifier;
    deviceName = ins[i].name;
    confirmed.store (false);
    input->start();
    clock.reset();
    {
        // identity request: a reply with manufacturer 00 20 76 confirms the device
        const juce::ScopedLock sl (outputLock);
        if (output != nullptr)
        {
            const juce::uint8 req[] = { 0xf0, 0x7e, 0x7f, 0x06, 0x01, 0xf7 };
            output->sendMessageNow (juce::MidiMessage (req, (int) sizeof (req)));
        }
    }
    identitySentAt = juce::Time::getMillisecondCounter();
    juce::HighResolutionTimer::startTimer (1);
    if (onConnectionChanged)
        onConnectionChanged (true);
}

void Device::disconnect()
{
    const bool was = input != nullptr;
    juce::HighResolutionTimer::stopTimer();
    {
        const juce::ScopedLock sl (outputLock);
        if (output != nullptr)
        {
            std::vector<juce::MidiMessage> out;
            clock.stop (out);
            for (const auto& m : out)
                output->sendMessageNow (m);
        }
        output.reset();
    }
    if (input != nullptr)
        input->stop();
    input.reset();
    inputId.clear();
    auto* self = this;
    owner.compare_exchange_strong (self, nullptr);
    std::vector<Action> released;
    controller.releaseAll (juce::Time::getMillisecondCounter(), released);
    for (const auto& a : released)
        apply (proc, a);
    if (was && onConnectionChanged)
        onConnectionChanged (false);
}

void Device::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    if (m.isSysEx())
    {
        // identity reply: F0 7E <dev> 06 02 00 20 76 ...
        const auto* d = m.getSysExData();
        if (m.getSysExDataSize() >= 7 && d[0] == 0x7e && d[2] == 0x06 && d[3] == 0x02 && d[4] == 0x00 && d[5] == 0x20 && d[6] == 0x76)
            confirmed.store (true);
        return;
    }
    if (m.getRawDataSize() > 3 || m.isActiveSense() || m.isMidiClock())
        return;                          // the device's own clock is not used (livelooper is the master)
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

void Device::injectMessage (const juce::MidiMessage& m)
{
    handle (m);
}

void Device::processIncoming()
{
    const auto scope = fifo.read (fifo.getNumReady());
    auto run = [this] (int start, int size) {
        for (int i = 0; i < size; ++i)
        {
            const auto& d = queue[(size_t) (start + i)];
            const int len = juce::MidiMessage::getMessageLengthFromFirstByte (d[0]);
            handle (juce::MidiMessage (d.data(), juce::jlimit (1, 3, len)));
        }
    };
    run (scope.startIndex1, scope.blockSize1);
    run (scope.startIndex2, scope.blockSize2);
}

void Device::handle (const juce::MidiMessage& m)
{
    const auto now = juce::Time::getMillisecondCounter();
    std::vector<Action> out;
    controller.setNumTracks (proc.getVisibleTracks());
    controller.setHoldRamp ((HoldRamp) proc.getPunchHoldRamp());
    if (channel > 0 && m.getChannel() > 0 && m.getChannel() != channel)
        return;                                              // channel messages from other channels
    if (m.isNoteOnOrOff())
        controller.handleNote (m.getNoteNumber(), m.isNoteOn() ? m.getVelocity() : 0, m.isNoteOn(), now, out);
    else if (m.isAftertouch())                              // poly pressure (0xA0): not sent by OS 2.x, handled anyway
        controller.handlePressure (m.getNoteNumber(), m.getAfterTouchValue(), now);
    else if (m.isChannelPressure())                         // channel pressure (0xD0): likewise
        controller.handlePressure (-1, m.getChannelPressureValue(), now);
    else if ((m.isMidiStart() || m.isMidiContinue()) && ! clockOut.load())
        out.push_back ({ Action::Kind::AllStart, -1 });       // the device's PLAY (only while it is not clocked by us)
    else if (m.isMidiStop() && ! clockOut.load())
        out.push_back ({ Action::Kind::AllStop, -1 });
    for (const auto& a : out)
    {
        apply (proc, a);
        lastActionText = describe (a.kind, a.index);
    }
    updatePunchAmount (now);
}

void Device::updatePunchAmount (juce::uint32 now)
{
    // hold to intensify: the held punch pads ramp the amount (the K.O. II sends no pressure)
    controller.setHoldRamp ((HoldRamp) proc.getPunchHoldRamp());
    auto& e = proc.getEngine();
    const float a = controller.livePunchAmount (now, e.getCurrentBpm(), e.getMeasurePpq());
    liveAmount.store (a);
    if (a < 0.0f)
        return;
    if (auto* prm = proc.getState().getParameter (gPfxAmount))
        if (std::abs (prm->getValue() - a) > 0.004f)
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (a);
            prm->endChangeGesture();
        }
}

void Device::handleAsyncUpdate()
{
    processIncoming();
}

void Device::timerCallback()
{
    ++ticks;
    if (simulated)
    {
        std::vector<Action> out;
        controller.update (juce::Time::getMillisecondCounter(), out);
        for (const auto& a : out)
            apply (proc, a);
        updatePunchAmount (juce::Time::getMillisecondCounter());
        return;
    }
    if (input == nullptr)
    {
        if (ticks % 100 == 1)                  // every two seconds
            connect();
        return;
    }
    if (ticks % 100 == 0)
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
    processIncoming();
    std::vector<Action> out;
    controller.update (juce::Time::getMillisecondCounter(), out);
    for (const auto& a : out)
    {
        apply (proc, a);
        lastActionText = describe (a.kind, a.index);
    }
    updatePunchAmount (juce::Time::getMillisecondCounter());
}

void Device::hiResTimerCallback()
{
    // clock out: the timeline position, extrapolated between audio blocks
    auto& engine = proc.getEngine();
    const bool running = engine.isTimelineRunning() && clockOut.load();
    double ppq = engine.getTimelinePpq();
    const auto now = juce::Time::getMillisecondCounter();
    if (std::abs (ppq - lastPpq) > 1.0e-9)
    {
        lastPpq = ppq;
        lastChange = now;
    }
    else if (running)
        ppq += (double) (now - lastChange) * engine.getCurrentBpm() / 60000.0;

    std::vector<juce::MidiMessage> out;
    clock.process (ppq, running, engine.getMeasurePpq(), out);
    if (out.empty())
        return;
    const juce::ScopedLock sl (outputLock);
    if (output != nullptr)
        for (const auto& m : out)
            output->sendMessageNow (m);
}
} // namespace livelooper::ko2
