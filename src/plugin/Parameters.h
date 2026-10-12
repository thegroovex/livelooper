#pragma once

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
inline const juce::String gAllStartStop { "allstartstop" };
inline const juce::String gAllStart     { "allstart" };
inline const juce::String gAllStop      { "allstop" };
inline const juce::String gClearAll     { "clearall" };
inline const juce::String gTap          { "tap" };

/** True for momentary "button" parameters (they reset to off right after triggering). */
bool isTrigger (const juce::String& paramId);

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

juce::StringArray quantizeChoices();
juce::StringArray recActionChoices();
juce::StringArray inputChoices();
juce::StringArray startModeChoices();
juce::StringArray stopModeChoices();
juce::StringArray dubModeChoices();
juce::StringArray measureChoices();
juce::StringArray fxTypeChoices();
juce::StringArray rhythmChoices();

/** The choice lists above are English; this translates a list into the current UI language. */
juce::StringArray translated (const juce::StringArray& englishChoices);

/** English name of a parameter ("T1 Level", "In FX A Mix", "Master Level"). */
juce::String englishName (const juce::String& paramId);
/** Name of a parameter in the current UI language ("S1 Pegel" in German). */
juce::String localisedName (const juce::String& paramId);
} // namespace livelooper::params
