#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include <array>
#include <atomic>
#include <map>

namespace livelooper { class LiveLooperProcessor; }

/**
    v1.6 controller templates for grid controllers (besides Push 1, which has its own surface):
    Launchpad Mini MK3, Launchpad X, APC mini mk2 (and the first APC mini), Launchkey MK3 / MK4
    pads. livelooper finds the controller by its MIDI port name (or the template chosen in SETUP),
    switches it into its documented programmer / DAW mode, maps the pads and buttons and lights
    the LEDs as described in the manufacturers' public programmer's references.

    Untested on hardware (written from the public documentation) - please report problems.

    8 x 8 grid (Launchpad, APC mini), one column per track, rows from the top:
      1 REC/PLAY   2 STOP (SHIFT: CLEAR)   3 UNDO (SHIFT: REDO)   4 MUTE
      5 HALF SPEED 6 DOUBLE SPEED   7 REVERSE LOOP   8 LOOP x2 (SHIFT: LOOP / 2)
    Scene buttons: scenes 1 - 8 (SHIFT: store). Function buttons: ALL START/STOP, ALL STOP, CAPTURE,
    TAP, RHYTHM, METRONOME, performance recorder, SHIFT. APC mini faders: track levels 1 - 8, master.
    Launchkey pads (2 x 8): upper row REC/PLAY, lower row STOP; > = ALL START/STOP, the button
    below = ALL STOP.
*/
namespace livelooper::controllers
{
enum class TemplateId : int { LaunchpadMiniMk3 = 0, LaunchpadX, ApcMiniMk2, ApcMini, LaunchkeyMk3, LaunchkeyMk4, NumTemplates };
inline constexpr int kNumTemplates = (int) TemplateId::NumTemplates;
/** Automatic = by port name; Off = no template; First + n = template n. */
enum class Selection : int { Automatic = -2, Off = -1, First = 0 };

juce::String templateName (TemplateId t);
/** The template whose MIDI port this is (the port livelooper talks to), or -1. */
int detectTemplate (const juce::String& portName);
/** Same, with the naming rules of Windows (second USB interface = "MIDIIN2 (...)") or macOS / Linux. */
int detectTemplate (const juce::String& portName, bool windowsNames);

/** What a pad / button does. */
struct Action
{
    enum class Kind { None, RecPlay, Stop, Clear, Undo, Redo, Mute, HalfSpeed, DoubleSpeed, Reverse, LoopDouble, LoopHalve,
                      Scene, SceneStore, AllStartStop, AllStop, Capture, Tap, Rhythm, Metronome, Recorder, Shift, Level, MasterLevel };
    Kind kind = Kind::None;
    int index = -1;            // track / scene
    bool pressed = true;       // Shift: held state
    float value = 0.0f;        // Level / MasterLevel: 0 .. 1
};

/** LED colours (mapped to each controller's palette). */
enum class Led : int { Off = 0, Red, RedDim, Orange, OrangeDim, Yellow, YellowDim, Green, GreenDim, Cyan, Blue, Magenta, White };
enum class Blink : int { Solid = 0, Flash, Pulse };

/** What the LEDs should show (built from the processor state, any thread). */
struct SurfaceState
{
    struct Track { int state = 0; bool pending = false; bool muted = false; bool exists = true; };
    std::array<Track, 8> tracks {};
    std::array<int, 8> scenes {};      // 0 = empty, 1 = stored, 2 = last started
    bool shift = false, rhythm = false, metronome = false, recorder = false, running = false;
};

/** The mapping of one controller: pure functions, no I/O (tested without hardware). */
class Template
{
public:
    explicit Template (TemplateId id) : id (id) {}
    TemplateId getId() const noexcept { return id; }
    int  getRows() const noexcept;
    /** Messages that switch the controller into the mode livelooper uses / back. */
    std::vector<juce::MidiMessage> enterMessages() const;
    std::vector<juce::MidiMessage> exitMessages() const;
    /** Incoming message -> action (shift = SHIFT held). */
    Action translate (const juce::MidiMessage& m, bool shift) const;
    /** All LED messages for the state (the device sends only the ones that changed). */
    void render (const SurfaceState& s, std::vector<juce::MidiMessage>& out) const;
    /** Pad note for grid row (0 = top) / column. */
    int padNote (int row, int col) const noexcept;

private:
    juce::MidiMessage led (int note, Led colour, Blink blink, bool isCC = false) const;
    TemplateId id;
};

/** Talks to the controller: finds and opens its port, feeds actions to the processor, sends LEDs. */
class GridControllerDevice : private juce::Timer, private juce::AsyncUpdater, private juce::MidiInputCallback
{
public:
    explicit GridControllerDevice (LiveLooperProcessor& p);
    ~GridControllerDevice() override;

    Selection getSelection() const noexcept { return selection; }
    void setSelection (Selection s);
    bool isConnected() const noexcept { return input != nullptr && output != nullptr; }
    juce::String getStatusText() const;

    /** Applies an action to the processor (public for tests). */
    static void apply (LiveLooperProcessor& p, const Action& a);
    /** Builds the LED state from the processor (public for tests). */
    static SurfaceState makeState (LiveLooperProcessor& p, bool shift);

private:
    void timerCallback() override;
    void handleAsyncUpdate() override;
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m) override;
    void connect();
    void disconnect();
    void sendLeds (bool full);

    LiveLooperProcessor& proc;
    Selection selection = Selection::Automatic;
    std::unique_ptr<Template> tmpl;
    std::unique_ptr<juce::MidiInput> input;
    std::unique_ptr<juce::MidiOutput> output;
    juce::String inputId, deviceName;
    bool openFailed = false, shift = false;
    int ticks = 0;
    std::map<int, juce::MidiMessage> sent;          // last LED message per key (note / CC)
    static std::atomic<GridControllerDevice*> owner;

    static constexpr int kQueueSize = 256;
    juce::AbstractFifo fifo { kQueueSize };
    std::array<std::array<juce::uint8, 3>, kQueueSize> queue {};
};
} // namespace livelooper::controllers
