#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include <array>
#include <atomic>
#include <functional>
#include <utility>
#include <vector>

namespace livelooper { class LiveLooperProcessor; }

/**
    v1.6 controller support for the teenage engineering EP-133 K.O. II (OS 2.5+), EXPERIMENTAL -
    written from the public documentation, not yet tested on hardware.

    Over MIDI the K.O. II sends only its pad notes (groups A - D = notes 36 - 83, velocity when
    pad velocity is switched on), clock and transport - no knobs, fader or pad pressure, and no
    host can light its LEDs. So the four pad groups are four pages of livelooper functions, and
    the feedback is on screen (header chip, Controller GUI).

    Roles: "Controller only" (pads control livelooper, audio unchanged) or "Record K.O. II"
    (standalone: audio input switched to the K.O. II's USB stereo pair, output stays on the computer,
    only group D controls livelooper by default). livelooper is the clock master: it sends MIDI clock, start (on a bar)
    and stop to the K.O. II (set it to clock in, system setting 101).

    Everything except Ko2Device is pure (no I/O) and tested without hardware.
*/
namespace livelooper::ko2
{
//============================================================================== names
/** True when a MIDI / audio device name looks like a K.O. II ("EP-133 KO II MIDI 1", "K.O. II", "KO2" ...). */
bool matchesName (const juce::String& deviceName);
/** Index of the port to use among the names (prefers "... MIDI 1"), -1 when none matches. */
int pickPort (const juce::StringArray& names);

//============================================================================== audio (standalone, Record K.O. II)
/** Where livelooper's output goes while the K.O. II is recorded. */
enum class AudioOutput : int { Computer = 0, Ko2 = 1 };

/** Input and output device names for "Record K.O. II" within one audio device type. */
struct AudioRoute
{
    juce::String input, output;     // output empty = no output device (none other than the K.O. II exists)
    bool ok = false;                // false: no K.O. II input in this device type
};

/**
    Picks the devices for "Record K.O. II": the input is always the K.O. II, the output stays on the
    computer (the output used before, else the system default, else the first output that is not
    the K.O. II) - otherwise livelooper's output (metronome, loops) would go into the K.O. II and come
    back on its USB input, i.e. be recorded again. Only with AudioOutput::Ko2 the output is the K.O. II.
    Pure (tested): ins / outs = the device names of one device type, defaultOutput = its default index.
*/
AudioRoute chooseAudioRoute (const juce::StringArray& ins, const juce::StringArray& outs,
                             const juce::String& previousOutput, int defaultOutput, AudioOutput mode);

//============================================================================== hold to intensify
/**
    The K.O. II sends no pad pressure (no poly / channel aftertouch, only note on + velocity). So a
    held punch pad ramps the amount from the hit velocity up to 100 % (Off / 1 beat / 2 beats / 1 bar);
    a note off lets it go. Pressure messages, should a future firmware send them, drive the amount instead.
*/
enum class HoldRamp : int { Off = 0, OneBeat = 1, TwoBeats = 2, OneBar = 3 };
/** Ramp length in beats (quarter notes); 0 = off. */
double holdRampBeats (HoldRamp r, double measurePpq) noexcept;
/** Amount after holding for heldMs at bpm: from base up to 1 over rampBeats (0 = stays at base). */
float holdRampAmount (float base, double heldMs, double bpm, double rampBeats) noexcept;

//============================================================================== pads
inline constexpr int kFirstNote = 36, kLastNote = 83, kPadsPerGroup = 12, kNumGroups = 4;
/** Pad offsets inside a group (official note map, bottom-left to top-right). */
namespace pad
{
    inline constexpr int dot = 0, zero = 1, enter = 2;      // bottom row: "."  "0"  ENTER
    inline constexpr int num1 = 3;                           // "1" .. "9" = offsets 3 .. 11
}
inline constexpr int padNumberOffset (int number) noexcept { return number + 2; }   // "1" -> 3 ... "9" -> 11
inline constexpr int noteFor (int group, int offset) noexcept { return kFirstNote + group * kPadsPerGroup + offset; }
inline constexpr int groupOf (int note) noexcept { return (note - kFirstNote) / kPadsPerGroup; }
inline constexpr int offsetOf (int note) noexcept { return (note - kFirstNote) % kPadsPerGroup; }
inline constexpr bool isPadNote (int note) noexcept { return note >= kFirstNote && note <= kLastNote; }
/** Label printed on the pad: ".", "0", "ENTER", "1" .. "9". */
juce::String padLabel (int offset);
/** Grid position of a pad (row 0 = top, column 0 = left) as on the device: 7 8 9 / 4 5 6 / 1 2 3 / . 0 ENTER. */
int padRow (int offset) noexcept;
int padColumn (int offset) noexcept;

enum class Role : int { Controller = 0, Record = 1 };

/** What a pad does. */
struct Action
{
    enum class Kind { None, RecPlay, Stop, Clear, Undo, Redo, Mute, AllStartStop, AllStart, AllStop, Capture, Tap, Metronome,
                      Punch, Scene, SceneStore, Rhythm, Fill, Recorder, Resample, RoleSwitch };
    Kind kind = Kind::None;
    int index = -1;         // track / scene / punch FX (0 based)
    bool on = true;         // Punch / Fill: pressed or released
    int velocity = 100;     // Punch: amount from the velocity
    bool operator== (const Action& o) const noexcept { return kind == o.kind && index == o.index && on == o.on; }
};

/** Short text of an action for the pad labels / display ("T1 REC", "STOP 3", "Tape Stop", "Scene 2"); English. */
juce::String describe (Action::Kind kind, int index);

/** Groups that are livelooper controls, as bits (A = 1, B = 2, C = 4, D = 8). */
inline constexpr int kAllGroups = 15, kGroupD = 8;
inline constexpr int defaultGroups (Role r) noexcept { return r == Role::Record ? kGroupD : kAllGroups; }

/**
    Notes -> actions, with the gestures (hold 1 s, double tap, the role switch chord "." + ENTER
    of group D held 1 s). Pure: the caller passes the time in milliseconds.
*/
class Controller
{
public:
    static constexpr juce::uint32 kHoldMs = 1000, kDoubleTapMs = 400, kChordMs = 150;

    void setRole (Role r) noexcept { role = r; }
    Role getRole() const noexcept { return role; }
    void setGroups (int bits) noexcept { groups = bits & kAllGroups; }
    int  getGroups() const noexcept { return groups; }
    bool isControlGroup (int group) const noexcept { return (groups >> group) & 1; }
    void setNumTracks (int n) noexcept { numTracks = juce::jlimit (1, 8, n); }
    void setHoldRamp (HoldRamp r) noexcept { holdRamp = r; }
    HoldRamp getHoldRamp() const noexcept { return holdRamp; }

    /** A pad note (any channel). Appends what to do now. */
    void handleNote (int note, int velocity, bool on, juce::uint32 nowMs, std::vector<Action>& out);
    /** Poly aftertouch (note >= 0) or channel pressure (note = -1), value 0 - 127: drives the held punch FX amount. */
    void handlePressure (int note, int value, juce::uint32 nowMs);
    /** Live amount (0 - 1) of the held punch pads (hold ramp / pressure), -1 while no punch pad is held. */
    float livePunchAmount (juce::uint32 nowMs, double bpm, double measurePpq) const noexcept;
    bool isPunchHeld() const noexcept { return punch.count > 0; }
    /** Timed gestures (holds); call regularly. */
    void update (juce::uint32 nowMs, std::vector<Action>& out);
    /** All pads up (disconnect). */
    void releaseAll (juce::uint32 nowMs, std::vector<Action>& out);

    /** The function of a pad on a page (for drawings and docs). */
    Action::Kind functionOf (int group, int offset, int& index) const noexcept;
    /** Page (group) of the last pad note, -1 before the first. */
    int getPage() const noexcept { return page; }
    int getLastTrack() const noexcept { return lastTrack; }
    bool isHeld (int note) const noexcept { return isPadNote (note) && held[(size_t) (note - kFirstNote)].down; }
    int  heldVelocity (int note) const noexcept { return isPadNote (note) ? held[(size_t) (note - kFirstNote)].velocity : 0; }
    /** True after a note outside 36 - 83 arrived (KEYS mode on the device), until taken. */
    bool takeKeysWarning() noexcept { return std::exchange (keysWarning, false); }

private:
    struct Held { bool down = false; juce::uint32 since = 0; int velocity = 0; bool holdDone = false, chord = false; };
    std::array<Held, kNumGroups * kPadsPerGroup> held {};
    std::array<juce::uint32, kNumGroups * kPadsPerGroup> lastTap {};
    Role role = Role::Controller;
    int groups = kAllGroups;
    int numTracks = 8;
    int page = -1, lastTrack = 0;
    bool keysWarning = false, chordDone = false;
    HoldRamp holdRamp = HoldRamp::TwoBeats;
    struct PunchHold { int count = 0, velocity = 0, pressure = -1; juce::uint32 since = 0; bool pressureSeen = false; };
    PunchHold punch;
    void punchPressed (int velocity, juce::uint32 now) noexcept;
    bool compact (int group) const noexcept { return role == Role::Record && group == 3; }
    void press (int group, int offset, int velocity, juce::uint32 now, std::vector<Action>& out);
    void release (int group, int offset, juce::uint32 now, std::vector<Action>& out);
    Held& h (int group, int offset) noexcept { return held[(size_t) (group * kPadsPerGroup + offset)]; }
};

/**
    MIDI clock out (24 ppqn) from the looper's timeline: Start on a bar line (after Song Position 0),
    Stop when the timeline stops. Pure: feed it the timeline regularly (every millisecond or so).
*/
class Clock
{
public:
    /** ppq: timeline position, running: timeline runs, measurePpq: length of a bar in quarter notes. */
    void process (double ppq, bool running, double measurePpq, std::vector<juce::MidiMessage>& out);
    void reset() noexcept { started = false; lastTick = 0; }
    bool isStarted() const noexcept { return started; }
    /** Stops (when the device goes away / the clock is switched off). */
    void stop (std::vector<juce::MidiMessage>& out);

private:
    bool started = false;
    long long lastTick = 0;
};

/** Applies a K.O. II action to the looper (message thread). */
void apply (LiveLooperProcessor& p, const Action& a);

/**
    The K.O. II on its USB MIDI port: finds it (name match, every 2 s), opens input and output,
    turns pads into actions on the message thread, sends the clock from a 1 ms timer.
*/
class Device : private juce::Timer, private juce::AsyncUpdater, private juce::MidiInputCallback, private juce::HighResolutionTimer
{
public:
    explicit Device (LiveLooperProcessor& p);
    ~Device() override;

    bool isConnected() const noexcept { return simulated || (input != nullptr); }
    juce::String getDeviceName() const { return simulated ? juce::String ("EP-133 KO II MIDI 1") : deviceName; }
    /** True when the device answered the identity request (manufacturer 00 20 76). */
    bool isConfirmed() const noexcept { return confirmed.load(); }
    bool isClockRunning() const noexcept { return clock.isStarted(); }
    Controller& getController() noexcept { return controller; }
    /** Live punch FX amount from the held pads (hold ramp / pressure), -1 while none is held. */
    float getLivePunchAmount() const noexcept { return liveAmount.load(); }

    void setClockOut (bool on) noexcept { clockOut.store (on); }
    bool getClockOut() const noexcept { return clockOut.load(); }
    void setChannel (int ch) noexcept { channel = juce::jlimit (0, 16, ch); }   // 0 = any
    int  getChannel() const noexcept { return channel; }

    /** Tests / screenshots: behave as if a K.O. II were plugged in. */
    void setSimulatedConnection (bool on);
    /** Feeds a message as if it came from the device (tests, the clickable Controller GUI). */
    void injectMessage (const juce::MidiMessage& m);
    /** Called on the message thread when the device comes or goes. */
    std::function<void (bool connected)> onConnectionChanged;
    /** Last action (for the on-screen mirror), English, and when. */
    juce::String getLastActionText() const { return lastActionText; }

private:
    void timerCallback() override;
    void handleAsyncUpdate() override;
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m) override;
    void hiResTimerCallback() override;
    void connect();
    void disconnect();
    void processIncoming();
    void handle (const juce::MidiMessage& m);
    void updatePunchAmount (juce::uint32 now);

    LiveLooperProcessor& proc;
    Controller controller;
    Clock clock;
    std::unique_ptr<juce::MidiInput> input;
    std::unique_ptr<juce::MidiOutput> output;
    juce::CriticalSection outputLock;
    juce::String inputId, deviceName, lastActionText;
    std::atomic<bool> confirmed { false }, clockOut { true };
    std::atomic<float> liveAmount { -1.0f };
    int channel = 0;
    bool simulated = false;
    int ticks = 0;
    juce::uint32 identitySentAt = 0;
    double lastPpq = -1.0;              // clock thread: timeline position seen last, and when it changed
    juce::uint32 lastChange = 0;
    static std::atomic<Device*> owner;

    static constexpr int kQueueSize = 512;
    juce::AbstractFifo fifo { kQueueSize };
    std::array<std::array<juce::uint8, 3>, kQueueSize> queue {};
};
} // namespace livelooper::ko2
