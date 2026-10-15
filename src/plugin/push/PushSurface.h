#pragma once

#include "PushProtocol.h"
#include "engine/RhythmGenerator.h"

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

    Live performance FX (1.5): BROWSE = encoders for beat FX type / beat / level / channel,
    colour FX, repeat rate, stutter rate, metronome level. ADD EFFECT = beat FX on / off,
    ADD TRACK = beat FX while held, LEFT / RIGHT = beat fraction, REPEAT = beat repeat while
    held (SHIFT: latch), ACCENT = stutter while held (SHIFT: latch), REPEAT or ACCENT held +
    touch strip = repeat / stutter rate 1/2 (bottom) .. 1/32 (top), FIXED LENGTH = capture
    length 1 .. 32 bars (SHIFT: shorter), SHIFT + METRONOME = Ableton Link on / off,
    SHIFT + TAP TEMPO = metronome click on / off.

    v1.6 rhythm: SCALES = STEP mode (step sequencer for the drums): the pad rows are the eight
    voices of the kit (bottom = kick), the columns 8 steps; LEFT / RIGHT = previous / next 8
    steps, UP / DOWN = previous / next pattern (switches quantised), pad = step on / off (pad
    velocity = step velocity, SHIFT + pad = accent), DELETE + pad = clear the row, MUTE / SOLO +
    pad or the top / lower button row = row mute / solo, encoders = row level (SHIFT: pitch).
    Editing a built-in groove copies it into a free user slot first. In every mode: IN = FILL
    while held (back to the groove on the next downbeat), OUT = drums through the beat FX on / off
    (SHIFT + OUT: drums through beat repeat / stutter).
*/
class PushSurface
{
public:
    enum class Mode { Loop, FxPad, Step };
    enum class EncoderMode { Volume, Pan, Pitch, Feedback, Fx, BeatFx, Colour };   // Colour: v1.6 track colours
    /** v1.6 what the XY pad (FX PAD mode) plays: an FX slot (top buttons) or, with SHIFT + top buttons 1 - 8,
        beat FX, colour FX, beat repeat, stutter or one of the punch FX (buttons 5 - 8 = pages of three,
        press again for the next one of the page). */
    enum class XyKind { Slot, BeatFx, ColorFx, Repeat, Stutter, Punch };
    enum class StripMode { TapeStop, Filter, RepeatRate, StutterRate };

    /** Timing of the gestures (milliseconds). */
    static constexpr juce::uint32 kHoldMs = 250;           // MUTE + pad: longer = momentary mute
    static constexpr juce::uint32 kDoubleTapMs = 400;      // footswitch double tap
    static constexpr juce::uint32 kFootHoldMs = 1000;      // footswitch hold = clear
    static constexpr juce::uint32 kNoticeMs = 1500;        // short messages on display line 4
    static constexpr juce::uint32 kMeterIntervalMs = 40;   // meter lines at most 25 times per second
    static constexpr juce::uint32 kEncoderDoubleTapMs = 400; // v1.6: two touches of a track encoder = next page
    static constexpr int kChopThreshold = 16;               // v1.6: pad pressure that starts the chop
    /** v1.6 free pads (5 tracks): top right = beat flash, the one below = set recording. */
    static constexpr int kBeatPadRow = 7, kRecPadRow = 6, kFreePadColumn = 7;

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
    XyKind getXyKind() const noexcept            { return xyKind; }
    int getXyPunch() const noexcept              { return xyPunch; }
    /** v1.6 the track whose loop is chopped by pad pressure (-1 = none) and the rate index (1/4 .. 1/32). */
    int getChopTrack() const noexcept            { return chopTrack; }
    /** v1.6: true during the first half of every beat while the timeline runs (LED blinking in time). */
    bool beatPhaseOn (double duty = 0.5) const;
    /** v1.6 the 8 encoder pages a double touch cycles through (Volume, Pan, Pitch, Feedback, Colour). */
    static EncoderMode nextTrackPage (EncoderMode m) noexcept;
    /** Text of the XY selection for the display ("Punch: Tape Stop"). */
    juce::String xySelectionText() const;
    /** v1.6: CC value of a button as last rendered (-1 = never) - tests / Controller GUI. */
    int getButtonState (int cc) const noexcept { return juce::isPositiveAndBelow (cc, 128) ? ccState[(size_t) cc] : -1; }
    bool areMetersShown() const noexcept         { return metersShown; }
    StripMode getStripMode() const noexcept      { return stripMode; }
    bool isStripTouched() const noexcept         { return stripTouched; }
    int getPadColour (int row, int column) const noexcept { return padState[(size_t) (row * kGridSize + column)]; }
    juce::String getDisplayLine (int line) const { return displayState[(size_t) juce::jlimit (0, kDisplayLines - 1, line)]; }
    juce::String getNotice() const;
    /** v1.6 STEP mode: first step shown on the pads (multiple of 8). */
    int getStepPage() const noexcept { return stepPage; }

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
    void stepChoice (const juce::String& paramId, int delta, bool wrap);
    void handlePerformanceButton (int number, bool pressed);
    void nudge (const juce::String& paramId, int steps, float stepSize);
    void setParam (const juce::String& paramId, float normalised);
    float getParam (const juce::String& paramId) const;
    void toggleParam (const juce::String& paramId);
    void fxPadTouch (int row, int column);
    void xyApply (int row, int column, bool first);
    void xyPressure (float pressure);
    void xyRelease();
    void handleLoopPressure (int track, int value);
    void endChop();
    void handleEncoderTouch (int encoder, bool touched);
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
    // v1.6 STEP mode
    void handleStepPad (int row, int column, int velocity);
    bool handleStepButton (int number);
    int  stepPadColour (int row, int column, bool blinkOn) const;
    BeatPattern stepView() const;
    int  stepPlayingCell (const BeatPattern& p) const;
    std::array<juce::String, kDisplayLines> buildStepDisplay() const;

    LiveLooperProcessor& proc;
    std::function<juce::uint32()> clock;
    Mode mode = Mode::Loop;
    EncoderMode encoderMode = EncoderMode::Volume;
    int selectedTrack = 0;
    int fxTarget = 0;               // 0 .. 3 input FX A - D, 4 .. 7 track FX A - D
    XyKind xyKind = XyKind::Slot;   // v1.6
    int xyPunch = 0;                // v1.6: punch FX on the XY pad (0 .. 11)
    struct XySnapshot { float a = 0, b = 0, c = 0; std::array<float, 8> knobs {}; } xySnapshot;
    int chopTrack = -1;             // v1.6 pad pressure chop
    float chopPrevTarget = 0.0f, chopPrevRate = 0.0f;
    int lastLoopPadTrack = -1;
    std::array<juce::uint32, 8> encoderTouchAt {};
    bool shift = false, deleteHeld = false, muteHeld = false;
    bool selectHeld = false, soloHeld = false, duplicateHeld = false;
    bool repeatHeld = false, accentHeld = false;     // v1.5: REPEAT / ACCENT buttons held

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

    int stepPage = 0;               // v1.6 STEP mode: page of 8 steps
    BeatPattern stepCache;          // pattern shown on the pads (taken once per render)
    int stepCachePlaying = -1;
    std::array<int, 64> padState {};
    std::array<int, 128> ccState {};
    std::array<juce::String, kDisplayLines> displayState;
    bool sentOnce = false;
    juce::uint32 startTime = juce::Time::getMillisecondCounter();
};
} // namespace push
} // namespace livelooper
