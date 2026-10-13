#pragma once

#include "PushProtocol.h"

#include <array>
#include <functional>
#include <vector>

namespace livelooper
{
class LiveLooperProcessor;

namespace push
{
/**
    What livelooper does with an Ableton Push (1) in User mode, without any MIDI device:
    handleMidi() turns pads, buttons, encoders, touch strip and footswitches into looper
    actions, update() runs the timed gestures and the meters, render() produces the LED and
    display messages that changed since the last call.

    LOOP mode (SESSION button)          FX PAD mode (NOTE button)
      column = track 1 .. 8               the 8 x 8 pads are an XY pad for one effect:
      row 1: REC / PLAY / DUB             X = parameter 1, Y = parameter 2, pad pressure
      row 2: STOP                         = more mix. Releasing the pads snaps the effect
      rows 3 - 8: loop position            back (hold SHIFT while releasing to keep it).
        playing track: tap = jump to       Top buttons 1 - 8 pick the effect:
        that sixth (next 16th), hold =       input FX A - D, track FX A - D
        stutter, back in sync on release
        stopped / empty track: select
        REC pad held: length 1 .. 32 bars
      top buttons: track FX on / off
      lower buttons: reverse
    Both: scene buttons = scenes (SHIFT = store), encoders = level / pan / pitch / feedback /
    FX (VOLUME, PAN & SEND, TRACK, CLIP, DEVICE), master encoder = master level, tempo encoder
    = tempo, swing encoder = swing. PLAY = all start / stop, RECORD = capture, METRONOME =
    rhythm, TAP = tap tempo, UNDO = undo (SHIFT = redo) of the selected track, DELETE + pad =
    clear, MUTE + pad = tap: stop / hold: mute while held, SOLO + pad = solo while held,
    SELECT + pad = select, SHIFT + position pad = back in sync, DUPLICATE + pads = copy,
    DOUBLE = loop x 2 (SHIFT = half), OCTAVE = pitch +- 12, MASTER = level meters,
    touch strip = tape stop (SHIFT: filter), footswitch 2 = REC / PLAY, double tap STOP,
    hold CLEAR, footswitch 1 = next track, AUTOMATION = auto record, STOP = stop all,
    QUANTIZE = next quantize value.
*/
class PushSurface
{
public:
    enum class Mode { Loop, FxPad };
    enum class EncoderMode { Volume, Pan, Pitch, Feedback, Fx };
    enum class StripMode { TapeStop, Filter };

    /** Timing of the gestures (milliseconds). */
    static constexpr juce::uint32 kHoldMs = 250;           // MUTE + pad: longer = momentary mute
    static constexpr juce::uint32 kDoubleTapMs = 400;      // footswitch double tap
    static constexpr juce::uint32 kFootHoldMs = 1000;      // footswitch hold = clear
    static constexpr juce::uint32 kNoticeMs = 1500;        // short messages on display line 4
    static constexpr juce::uint32 kMeterIntervalMs = 40;   // meter lines at most 25 times per second

    explicit PushSurface (LiveLooperProcessor& p);

    /** Message thread. */
    void handleMidi (const juce::MidiMessage& m);
    /** Message thread, called regularly (about 50 times per second): held gestures, meters. */
    void update();
    /** Appends the LED / display messages that changed. full = resend everything. */
    void render (std::vector<juce::MidiMessage>& out, bool full);
    /** Forget what Push shows (after reconnecting), so the next render() sends everything. */
    void invalidate();

    Mode getMode() const noexcept                { return mode; }
    EncoderMode getEncoderMode() const noexcept  { return encoderMode; }
    int getSelectedTrack() const noexcept        { return selectedTrack; }
    int getFxTarget() const noexcept             { return fxTarget; }
    bool areMetersShown() const noexcept         { return metersShown; }
    StripMode getStripMode() const noexcept      { return stripMode; }
    bool isStripTouched() const noexcept         { return stripTouched; }
    int getPadColour (int row, int column) const noexcept { return padState[(size_t) (row * kGridSize + column)]; }
    juce::String getDisplayLine (int line) const { return displayState[(size_t) juce::jlimit (0, kDisplayLines - 1, line)]; }
    juce::String getNotice() const;

    /** Text of the four display lines as they should be now. */
    std::array<juce::String, kDisplayLines> buildDisplay() const;

    /** Tests: replaces the millisecond clock. */
    void setClock (std::function<juce::uint32()> c) { clock = std::move (c); }

private:
    void handlePad (int row, int column, int velocity);
    void handleButton (int number, bool pressed);
    void handleEncoder (int number, int delta);
    void handleStrip (bool touched, int value);
    void handlePressure (int value);
    void handleFootswitch (int number, bool pressed);
    void nudge (const juce::String& paramId, int steps, float stepSize);
    void setParam (const juce::String& paramId, float normalised);
    float getParam (const juce::String& paramId) const;
    void toggleParam (const juce::String& paramId);
    void fxPadTouch (int row, int column);
    void fxPadReleaseAll();
    juce::String fxParamId (const char* name) const;
    void selectTrack (int t);
    void showNotice (const juce::String& text);
    juce::uint32 now() const { return clock ? clock() : juce::Time::getMillisecondCounter(); }
    bool isTrackMuted (int t) const;
    juce::String meterColumn (int t, int width) const;

    int trackPadColour (int track, int row, bool blinkOn) const;
    /** first encoder of the master section (master level, tempo), -1 when the encoders are all in use */
    int masterColumn() const;
    int fxPadColour (int row, int column) const;

    LiveLooperProcessor& proc;
    std::function<juce::uint32()> clock;
    Mode mode = Mode::Loop;
    EncoderMode encoderMode = EncoderMode::Volume;
    int selectedTrack = 0;
    int fxTarget = 0;               // 0 .. 3 input FX A - D, 4 .. 7 track FX A - D
    bool shift = false, deleteHeld = false, muteHeld = false;
    bool selectHeld = false, soloHeld = false, duplicateHeld = false;

    struct FxSnapshot { bool active = false; float on = 0, p1 = 0, p2 = 0, mix = 0; };
    FxSnapshot fxSnapshot;
    int fxTouches = 0;
    int fxTouchRow = -1, fxTouchColumn = -1;
    std::array<float, 8> encoderRemainder {};

    // v1.4 gestures
    std::array<bool, 8> recPadHeld {};
    std::array<std::array<bool, 6>, 8> slicePadDown {};
    struct HeldPad { bool active = false; juce::uint32 since = 0; float previous = 0.0f; };
    std::array<HeldPad, 8> muteHold {}, soloHold {};
    int dupSource = -1;
    bool dupDone = false;
    bool fs2Down = false, fs2HoldDone = false;
    juce::uint32 fs2DownAt = 0, fs2LastTap = 0;
    bool fs2HasTap = false;
    bool stripTouched = false;
    StripMode stripMode = StripMode::TapeStop;
    bool metersShown = false;
    std::array<float, 9> meterLevel {};          // 8 tracks + master, 0 .. 1 (-48 .. 0 dB)
    std::array<juce::uint32, 9> clipUntil {};
    juce::uint32 lastMeterUpdate = 0;
    std::array<juce::uint32, kDisplayLines> lastLineSent {};
    juce::String notice;
    juce::uint32 noticeUntil = 0;

    std::array<int, 64> padState {};
    std::array<int, 128> ccState {};
    std::array<juce::String, kDisplayLines> displayState;
    bool sentOnce = false;
    juce::uint32 startTime = juce::Time::getMillisecondCounter();
};
} // namespace push
} // namespace livelooper
