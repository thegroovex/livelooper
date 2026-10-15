#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

/**
    Ableton Push 1 in "User" mode: note / CC numbers, LED colours and display sysex.

    In User mode Push sends and receives on its "User Port" (Windows: the second port,
    often called "MIDIIN2 (Ableton Push)" / "MIDIOUT2 (Ableton Push)"). Everything here is
    plain data, so it can be tested without hardware.
*/
namespace livelooper::push
{
//============================================================================== controls
inline constexpr int kPadNoteFirst   = 36;      // bottom-left pad; row r, column c -> 36 + r * 8 + c
inline constexpr int kPadNoteLast    = 99;
inline constexpr int kGridSize       = 8;

inline constexpr int kTopRowFirst    = 20;      // buttons above the pads (CC 20 .. 27)
inline constexpr int kBottomRowFirst = 102;     // second row above the pads (CC 102 .. 109)
inline constexpr int kSceneFirst     = 36;      // scene buttons right of the pads: CC 43 (top) .. 36 (bottom)
inline constexpr int kEncoderFirst   = 71;      // the eight encoders above the display (CC 71 .. 78)
inline constexpr int kMasterEncoder  = 79;
inline constexpr int kTempoEncoder   = 14;
inline constexpr int kSwingEncoder   = 15;
inline constexpr int kEncoderTouchFirst = 0;    // note 0 .. 7 = touching encoder 1 .. 8, 8 = master
inline constexpr int kTouchStripNote = 12;      // note on / off: finger on / off the touch strip (position = pitch bend)

namespace cc
{
    inline constexpr int tapTempo = 3, metronome = 9, stop = 29;
    inline constexpr int left = 44, right = 45, up = 46, down = 47, select = 48, shift = 49;
    inline constexpr int note = 50, session = 51, addEffect = 52, addTrack = 53;
    inline constexpr int octaveDown = 54, octaveUp = 55, repeat = 56, accent = 57, scales = 58, user = 59;
    inline constexpr int mute = 60, solo = 61;
    inline constexpr int inBtn = 62, outBtn = 63;          // v1.6: IN = FILL (hold), OUT = drums through the FX
    inline constexpr int play = 85, record = 86, newBtn = 87, duplicate = 88, automation = 89, fixedLength = 90;
    inline constexpr int device = 110, browse = 111, track = 112, clip = 113, volume = 114, panSend = 115;
    inline constexpr int quantize = 116, doubleBtn = 117, deleteBtn = 118, undo = 119;
    inline constexpr int master = 28;
    /** Footswitch jacks on the back of Push 1 (127 = pressed, 0 = released). */
    inline constexpr int footswitch1 = 64, footswitch2 = 69;
}

inline constexpr int padNote (int row, int column) noexcept { return kPadNoteFirst + row * kGridSize + column; }
inline constexpr bool isPadNote (int note) noexcept         { return note >= kPadNoteFirst && note <= kPadNoteLast; }
inline constexpr int padRow (int note) noexcept             { return (note - kPadNoteFirst) / kGridSize; }
inline constexpr int padColumn (int note) noexcept          { return (note - kPadNoteFirst) % kGridSize; }
/** Scene button CC -> scene index 0 (top) .. 7 (bottom), or -1. */
inline constexpr int sceneForCC (int number) noexcept       { return number >= kSceneFirst && number < kSceneFirst + 8 ? 7 - (number - kSceneFirst) : -1; }
inline constexpr int ccForScene (int scene) noexcept        { return kSceneFirst + 7 - scene; }

/** Relative encoder value: 1 .. 63 = clockwise steps, 65 .. 127 = counter-clockwise. */
inline constexpr int encoderDelta (int value) noexcept      { return value == 0 ? 0 : (value < 64 ? value : value - 128); }

//============================================================================== LED colours
/** Pad colours (velocity of a note-on to the pad), Push 1 palette. Dim = darker variant. */
namespace pad
{
    inline constexpr int off = 0, white = 3, grey = 1;
    inline constexpr int red = 5, redDim = 7;
    inline constexpr int orange = 9, orangeDim = 11;
    inline constexpr int yellow = 13, yellowDim = 15;
    inline constexpr int green = 21, greenDim = 23;
    inline constexpr int cyan = 37, cyanDim = 39;
    inline constexpr int blue = 45, blueDim = 47;
    inline constexpr int magenta = 53, magentaDim = 55;
}

/** Single-colour buttons (Play, Record, Shift ...): CC value. */
namespace button
{
    inline constexpr int off = 0, dim = 1, dimBlink = 2, on = 4, onBlink = 5;
}

/** Two-colour buttons (top row, scene buttons): CC value = colour base + brightness. */
namespace twoColour
{
    inline constexpr int red = 1, orange = 7, yellow = 13, green = 19;
    inline constexpr int dim = 0, dimBlink = 1, dimFast = 2, full = 3, fullBlink = 4, fullFast = 5;
    inline constexpr int value (int colour, int brightness) noexcept { return colour + brightness; }
}

//============================================================================== display
inline constexpr int kDisplayLines = 4;
inline constexpr int kDisplayChars = 68;        // 4 segments of 17 characters

/** Special characters of the Push 1 display (as used by Live's own Push script for its value graphs). */
namespace glyph
{
    inline constexpr juce::juce_wchar barLeft = 0x03;      // half-filled cell
    inline constexpr juce::juce_wchar block = 0x05;        // filled cell of a bar
    inline constexpr juce::juce_wchar dash = 0x06;         // empty cell of a bar
    inline constexpr juce::juce_wchar fullBlock = 0x1d;    // solid block (used for clipping)
}
/** A level bar of 'cells' display characters for a value 0 .. 1 (two steps per cell). */
juce::String levelBar (float value, int cells);

/** Sysex that writes one display line (0 .. 3). Text is padded / cut to 68 characters: ASCII,
    the special characters 1 .. 31 (bars) and German umlauts (mapped to the display's own codes). */
juce::MidiMessage displayLine (int line, const juce::String& text);
/** Sysex that clears one display line. */
juce::MidiMessage clearDisplayLine (int line);
/** Sysex that switches Push 1 between Live mode (false) and User mode (true). */
juce::MidiMessage userModeSysex (bool userMode);
/** v1.6: Sysex that sets the pad aftertouch mode of Push 1: polyphonic key pressure (true, one value per
    pad) or channel pressure (false). F0 47 7F 15 5C 00 01 <mode> F7 (as Live's own Push script sends it). */
juce::MidiMessage aftertouchModeSysex (bool polyphonic);
/** Lays out up to eight texts in the eight 8.5-character columns under the encoders. */
juce::String columns (const juce::StringArray& texts);

/** True when a MIDI device name looks like Push 1's User port. */
bool isPushUserPort (const juce::String& deviceName, int indexAmongPushPorts);
bool isPushDevice (const juce::String& deviceName);
} // namespace livelooper::push
