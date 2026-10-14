#pragma once

#include "engine/BeatFx.h"
#include "engine/Effects.h"
#include "engine/MidiMap.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace livelooper::params
{
// ---- ids
juce::String trackId (int track, const char* name);          // "t1_level"
juce::String fxId (bool inputFx, int slot, const char* name); // "inA_type" / "trC_mix"

// per-track triggers
inline constexpr const char* kRecPlay = "recplay";
inline constexpr const char* kStop    = "stop";
inline constexpr const char* kClear   = "clear";
inline constexpr const char* kUndo    = "undo";
inline constexpr const char* kRedo    = "redo";
// per-track settings
inline constexpr const char* kLevel     = "level";
inline constexpr const char* kPan       = "pan";
inline constexpr const char* kReverse   = "reverse";
inline constexpr const char* kOneShot   = "oneshot";
inline constexpr const char* kLoopSync  = "loopsync";
inline constexpr const char* kTrackFx   = "fx";
inline constexpr const char* kInput     = "input";
inline constexpr const char* kStartMode = "startmode";
inline constexpr const char* kStopMode  = "stopmode";
inline constexpr const char* kDubMode   = "dubmode";
inline constexpr const char* kMeasure   = "measure";
// v1.3
inline constexpr const char* kCapture   = "capture";    // trigger: retrospective capture into this track
inline constexpr const char* kFeedback  = "feedback";
inline constexpr const char* kDuck      = "duck";
inline constexpr const char* kOutput    = "output";
inline constexpr const char* kPitch     = "pitch";
inline constexpr const char* kStretch   = "stretch";
// v1.4 performance
inline constexpr int kNumSlices = 6;
inline constexpr const char* kSlices[kNumSlices] = { "slice1", "slice2", "slice3", "slice4", "slice5", "slice6" };   // momentary: hold = held pad
inline constexpr const char* kCutSync   = "cutsync";    // trigger: back to the running position
inline constexpr const char* kMute      = "mute";
inline constexpr const char* kSolo      = "solo";
inline constexpr const char* kDouble    = "double";     // trigger: loop x 2 (multiply)
inline constexpr const char* kHalve     = "halve";      // trigger: loop / 2
inline constexpr const char* kOctUp     = "octup";      // trigger: pitch +12
inline constexpr const char* kOctDown   = "octdown";    // trigger: pitch -12
inline constexpr const char* kCopy      = "copy";       // trigger: copy the loop to the first empty track
inline constexpr const char* kRecLen    = "reclen";     // choice, fires once: length of the next / running recording
inline constexpr int kRecLenBars[] = { 0, 1, 2, 4, 8, 16, 32 };
// fx slot
inline constexpr const char* kFxOn   = "on";
inline constexpr const char* kFxType = "type";
inline constexpr const char* kFxP1   = "p1";
inline constexpr const char* kFxP2   = "p2";
inline constexpr const char* kFxMix  = "mix";

// global
inline const juce::String gMaster       { "master" };
inline const juce::String gMonitor      { "monitor" };
inline const juce::String gQuantize     { "quantize" };
inline const juce::String gRecAction    { "recaction" };
inline const juce::String gSync         { "sync" };
inline const juce::String gFollow       { "follow" };
inline const juce::String gTempo        { "tempo" };
inline const juce::String gFade         { "fade" };
inline const juce::String gRhythmOn     { "rhythm_on" };
inline const juce::String gRhythmLevel  { "rhythm_level" };
inline const juce::String gRhythmPattern{ "rhythm_pattern" };
inline const juce::String gClickSound   { "click_sound" };
inline const juce::String gRhythmKit    { "rhythm_kit" };       // since 1.3
inline const juce::String gRhythmSwing  { "rhythm_swing" };
inline const juce::String gRhythmHuman  { "rhythm_humanize" };
inline const juce::String gAllStartStop { "allstartstop" };
inline const juce::String gAllStart     { "allstart" };
inline const juce::String gAllStop      { "allstop" };
inline const juce::String gClearAll     { "clearall" };
inline const juce::String gTap          { "tap" };
// v1.3
inline const juce::String gCapture      { "capture" };
inline const juce::String gCaptureBars  { "capture_bars" };
inline const juce::String gAutoRec      { "autorec" };
inline const juce::String gAutoRecThresh{ "autorec_threshold" };
inline const juce::String gDuckSource   { "duck_source" };
inline const juce::String gSceneStore   { "scene_store" };
// v1.4
inline const juce::String gTapeStop     { "tape_stop" };     // 0 = running, 1 = tape stands still
inline const juce::String gPerfFilter   { "perf_filter" };   // -1 low-pass .. 0 off .. 1 high-pass
// v1.5 performance FX (beat FX, colour FX, beat repeat / stutter), metronome block, Link
inline const juce::String gBfxOn        { "bfx_on" };
inline const juce::String gBfxHold      { "bfx_hold" };     // momentary: on while held
inline const juce::String gBfxType      { "bfx_type" };
inline const juce::String gBfxBeat      { "bfx_beat" };
inline const juce::String gBfxDepth     { "bfx_depth" };
inline const juce::String gBfxTarget    { "bfx_target" };
inline const juce::String gColorType    { "color_type" };
inline constexpr const char* kColor     = "color";        // per track: colour FX knob -1 .. 1
inline const juce::String gRepeatHold   { "brep_hold" };    // momentary
inline const juce::String gRepeatLatch  { "brep_latch" };
inline const juce::String gRepeatRate   { "brep_rate" };
inline const juce::String gStutterHold  { "stut_hold" };    // momentary
inline const juce::String gStutterLatch { "stut_latch" };
inline const juce::String gStutterRate  { "stut_rate" };
inline const juce::String gMetroOn      { "metro_on" };
inline const juce::String gMetroLevel   { "metro_level" };
inline const juce::String gMetroAccent  { "metro_accent" };
inline const juce::String gLinkOn       { "link_on" };
inline const juce::String gLinkStartStop{ "link_startstop" };
inline const juce::String gRhythmGroove { "rhythm_groove" };
inline const juce::String gRhythmFills  { "rhythm_fills" };
// ---- v1.6 features (branch v1.6-wip: loop tricks, performance recorder, clip export) -- appended
inline constexpr const char* kHalfSpeed = "halfspeed";  // trigger: loop at half speed (x2 length), quantized
inline constexpr const char* kDblSpeed  = "dblspeed";   // trigger: loop at double speed (played twice), quantized
inline constexpr const char* kRevLoop   = "revloop";    // trigger: reverse the recorded audio, quantized
inline const juce::String gPerfRec      { "perf_rec" };     // toggle: performance recorder (master mix to WAV)
inline const juce::String gClipExport   { "clip_export" };  // trigger: 9:16 clip of the last 20 s / the recording
// ---- end v1.6 features
// ---- v1.6 rhythm (branch v1.6-rhythm): kits, BEATS step sequencer, fill on demand, drum FX routing
inline const juce::String gRhythmVariation { "rhythm_variation" };   // 0 .. 1: sample / accent variation
inline const juce::String gRhythmFill   { "rhythm_fill" };          // momentary: fill while held
inline const juce::String gBeatSwitch   { "beat_switch" };          // pattern change: immediately / beat / bar
inline const juce::String gDrumsBfx     { "drums_bfx" };            // drums through the beat FX
inline const juce::String gDrumsRepeat  { "drums_repeat" };         // drums through beat repeat / stutter
inline constexpr int kNumBeatRows = 8;
/** "beat_mute1" .. "beat_mute8", "beat_solo1".., "beat_vol1".., "beat_pitch1".. (row = kit voice) */
juce::String beatRowId (const char* what, int row);
inline constexpr const char* kBeatMute  = "beat_mute";
inline constexpr const char* kBeatSolo  = "beat_solo";
inline constexpr const char* kBeatVol   = "beat_vol";
inline constexpr const char* kBeatPitch = "beat_pitch";
juce::StringArray beatSwitchChoices();
// ---- end v1.6 rhythm

inline constexpr int kNumScenes = 8;
inline constexpr int kCaptureBars[] = { 1, 2, 4, 8, 16, 32 };   // 16 / 32 since 1.5 (appended)
inline constexpr int kNumCaptureChoices = 6;
/** "scene1" .. "scene8" */
juce::String sceneId (int scene);

/** True for momentary "button" parameters (they reset to off right after triggering). */
bool isTrigger (const juce::String& paramId);
/** True for parameters that follow a held button (on while held): the slice pads. MIDI: note on / off. */
bool isMomentary (const juce::String& paramId);

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

juce::StringArray quantizeChoices();
juce::StringArray recActionChoices();
juce::StringArray clickSoundChoices();
juce::StringArray inputChoices();
juce::StringArray startModeChoices();
juce::StringArray stopModeChoices();
juce::StringArray dubModeChoices();
juce::StringArray measureChoices();
juce::StringArray fxTypeChoices();
juce::StringArray rhythmChoices();
juce::StringArray kitChoices();
juce::StringArray outputChoices();
juce::StringArray duckSourceChoices();
juce::StringArray captureBarsChoices();
juce::StringArray recLengthChoices();
juce::StringArray beatFxChoices();
juce::StringArray beatFractionChoices();
juce::StringArray fxTargetChoices();
juce::StringArray colorFxChoices();
juce::StringArray repeatRateChoices();
juce::StringArray grooveChoices();
juce::StringArray fillChoices();

/** The choice lists above are English; this translates a list into the current UI language. */
juce::StringArray translated (const juce::StringArray& englishChoices);

/** English name of a parameter ("T1 Level", "In FX A Mix", "Master Level"). */
juce::String englishName (const juce::String& paramId);
/** Name of a parameter in the current UI language ("S1 Pegel" in German). */
juce::String localisedName (const juce::String& paramId);
} // namespace livelooper::params
