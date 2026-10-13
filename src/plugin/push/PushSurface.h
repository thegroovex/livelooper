#pragma once

#include "PushProtocol.h"

#include <array>
#include <vector>

namespace livelooper
{
class LiveLooperProcessor;

namespace push
{
/**
    What livelooper does with an Ableton Push (1) in User mode, without any MIDI device:
    handleMidi() turns pads, buttons and encoders into looper actions, render() produces
    the LED and display messages that changed since the last call.

    LOOP mode (SESSION button)          FX PAD mode (NOTE button)
      column = track 1 .. 8               the 8 x 8 pads are an XY pad for one effect:
      row 1: REC / PLAY / DUB             X = parameter 1, Y = parameter 2.
      row 2: STOP                         Releasing the pads snaps the effect back
      rows 3 - 8: loop progress           (hold SHIFT while releasing to keep it).
        (press: select the track)         Top buttons 1 - 8 pick the effect:
      top buttons: track FX on / off        input FX A - D, track FX A - D
      lower buttons: reverse
    Both: scene buttons = scenes (SHIFT = store), encoders = level / pan / pitch / feedback /
    FX (VOLUME, PAN & SEND, TRACK, CLIP, DEVICE), master encoder = master level, tempo encoder
    = tempo, swing encoder = rhythm level. PLAY = all start / stop, RECORD = capture,
    METRONOME = rhythm, TAP = tap tempo, UNDO = undo (SHIFT = redo) of the selected track,
    DELETE + pad = clear the track, MUTE + pad = stop, AUTOMATION = auto record,
    STOP = stop all, QUANTIZE = next quantize value.
*/
class PushSurface
{
public:
    enum class Mode { Loop, FxPad };
    enum class EncoderMode { Volume, Pan, Pitch, Feedback, Fx };

    explicit PushSurface (LiveLooperProcessor& p);

    /** Message thread. */
    void handleMidi (const juce::MidiMessage& m);
    /** Appends the LED / display messages that changed. full = resend everything. */
    void render (std::vector<juce::MidiMessage>& out, bool full);
    /** Forget what Push shows (after reconnecting), so the next render() sends everything. */
    void invalidate();

    Mode getMode() const noexcept                { return mode; }
    EncoderMode getEncoderMode() const noexcept  { return encoderMode; }
    int getSelectedTrack() const noexcept        { return selectedTrack; }
    int getFxTarget() const noexcept             { return fxTarget; }
    int getPadColour (int row, int column) const noexcept { return padState[(size_t) (row * kGridSize + column)]; }
    juce::String getDisplayLine (int line) const { return displayState[(size_t) juce::jlimit (0, kDisplayLines - 1, line)]; }

    /** Text of the four display lines as they should be now. */
    std::array<juce::String, kDisplayLines> buildDisplay() const;

private:
    void handlePad (int row, int column, int velocity);
    void handleButton (int number, bool pressed);
    void handleEncoder (int number, int delta);
    void nudge (const juce::String& paramId, int steps, float stepSize);
    void setParam (const juce::String& paramId, float normalised);
    void toggleParam (const juce::String& paramId);
    void fxPadTouch (int row, int column);
    void fxPadReleaseAll();
    juce::String fxParamId (const char* name) const;

    int trackPadColour (int track, int row, bool blinkOn) const;
    /** first encoder of the master section (master level, tempo), -1 when the encoders are all in use */
    int masterColumn() const;
    int fxPadColour (int row, int column) const;

    LiveLooperProcessor& proc;
    Mode mode = Mode::Loop;
    EncoderMode encoderMode = EncoderMode::Volume;
    int selectedTrack = 0;
    int fxTarget = 0;               // 0 .. 3 input FX A - D, 4 .. 7 track FX A - D
    bool shift = false, deleteHeld = false, muteHeld = false;

    struct FxSnapshot { bool active = false; float on = 0, p1 = 0, p2 = 0; };
    FxSnapshot fxSnapshot;
    int fxTouches = 0;
    int fxTouchRow = -1, fxTouchColumn = -1;
    std::array<float, 8> encoderRemainder {};

    std::array<int, 64> padState {};
    std::array<int, 128> ccState {};
    std::array<juce::String, kDisplayLines> displayState;
    bool sentOnce = false;
    juce::uint32 startTime = juce::Time::getMillisecondCounter();
};
} // namespace push
} // namespace livelooper
