#include "Parameters.h"

#include "engine/RhythmGenerator.h"

namespace livelooper::params
{
using APVTS = juce::AudioProcessorValueTreeState;

juce::String trackId (int track, const char* name)
{
    return "t" + juce::String (track + 1) + "_" + name;
}

juce::String fxId (bool inputFx, int slot, const char* name)
{
    return juce::String (inputFx ? "in" : "tr") + juce::String::charToString ((juce::juce_wchar) ('A' + slot)) + "_" + name;
}

bool isTrigger (const juce::String& id)
{
    static const juce::StringArray globalTriggers { gAllStartStop, gAllStart, gAllStop, gClearAll, gTap };
    if (globalTriggers.contains (id))
        return true;
    if (id.startsWithChar ('t') && id.length() > 3 && id[2] == '_')
    {
        const auto name = id.substring (3);
        return name == kRecPlay || name == kStop || name == kClear || name == kUndo || name == kRedo;
    }
    return false;
}

juce::StringArray quantizeChoices()  { return { "Off", "Beat", "Measure" }; }
juce::StringArray recActionChoices() { return { "Rec > Dub", "Rec > Play" }; }
juce::StringArray inputChoices()     { return { "Stereo", "Left", "Right" }; }
juce::StringArray startModeChoices() { return { "Immediate", "Fade In" }; }
juce::StringArray stopModeChoices()  { return { "Immediate", "Fade Out", "Loop End" }; }
juce::StringArray dubModeChoices()   { return { "Overdub", "Replace" }; }
juce::StringArray measureChoices()   { return { "Auto", "1", "2", "4", "8", "16" }; }

juce::StringArray fxTypeChoices()
{
    juce::StringArray s;
    for (int i = 0; i < (int) FxType::NumTypes; ++i)
        s.add (getFxTypeName ((FxType) i));
    return s;
}

juce::StringArray rhythmChoices()
{
    juce::StringArray s;
    for (int i = 0; i < RhythmGenerator::getNumPatterns(); ++i)
        s.add (RhythmGenerator::getPatternName (i));
    return s;
}

namespace
{
    using PID = juce::ParameterID;

    std::unique_ptr<juce::AudioParameterBool> boolParam (const juce::String& id, const juce::String& name, bool def)
    {
        return std::make_unique<juce::AudioParameterBool> (PID { id, 1 }, name, def);
    }

    std::unique_ptr<juce::AudioParameterChoice> choiceParam (const juce::String& id, const juce::String& name,
                                                             const juce::StringArray& choices, int def)
    {
        return std::make_unique<juce::AudioParameterChoice> (PID { id, 1 }, name, choices, def);
    }

    std::unique_ptr<juce::AudioParameterFloat> percentParam (const juce::String& id, const juce::String& name,
                                                             float maxValue, float def)
    {
        return std::make_unique<juce::AudioParameterFloat> (
            PID { id, 1 }, name, juce::NormalisableRange<float> (0.0f, maxValue, 0.0f), def,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)); })
                .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue() / 100.0f; })
                .withLabel ("%"));
    }
}

APVTS::ParameterLayout createLayout()
{
    APVTS::ParameterLayout layout;

    // ---------------------------------------------------------------- tracks
    for (int t = 0; t < kNumTracks; ++t)
    {
        const auto n = "T" + juce::String (t + 1) + " ";
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("track" + juce::String (t + 1),
                                                                            "Track " + juce::String (t + 1), "|");
        group->addChild (boolParam (trackId (t, kRecPlay), n + "Rec/Play", false));
        group->addChild (boolParam (trackId (t, kStop),    n + "Stop", false));
        group->addChild (boolParam (trackId (t, kClear),   n + "Clear", false));
        group->addChild (boolParam (trackId (t, kUndo),    n + "Undo", false));
        group->addChild (boolParam (trackId (t, kRedo),    n + "Redo", false));
        group->addChild (percentParam (trackId (t, kLevel), n + "Level", 2.0f, 1.0f));
        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            PID { trackId (t, kPan), 1 }, n + "Pan", juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) {
                const int p = juce::roundToInt (v * 50.0f);
                return p == 0 ? juce::String ("C") : (p < 0 ? "L" + juce::String (-p) : "R" + juce::String (p));
            })));
        group->addChild (boolParam (trackId (t, kReverse),  n + "Reverse", false));
        group->addChild (boolParam (trackId (t, kOneShot),  n + "One Shot", false));
        group->addChild (boolParam (trackId (t, kLoopSync), n + "Loop Sync", true));
        group->addChild (boolParam (trackId (t, kTrackFx),  n + "Track FX", false));
        group->addChild (choiceParam (trackId (t, kInput),     n + "Input", inputChoices(), 0));
        group->addChild (choiceParam (trackId (t, kStartMode), n + "Start Mode", startModeChoices(), 0));
        group->addChild (choiceParam (trackId (t, kStopMode),  n + "Stop Mode", stopModeChoices(), 0));
        group->addChild (choiceParam (trackId (t, kDubMode),   n + "Dub Mode", dubModeChoices(), 0));
        group->addChild (choiceParam (trackId (t, kMeasure),   n + "Measures", measureChoices(), 0));
        layout.add (std::move (group));
    }

    // ---------------------------------------------------------------- global
    {
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("global", "Global", "|");
        group->addChild (boolParam (gAllStartStop, "All Start/Stop", false));
        group->addChild (boolParam (gAllStart,     "All Start", false));
        group->addChild (boolParam (gAllStop,      "All Stop", false));
        group->addChild (boolParam (gClearAll,     "Clear All", false));
        group->addChild (boolParam (gTap,          "Tap Tempo", false));
        group->addChild (percentParam (gMaster,  "Master Level", 2.0f, 1.0f));
        group->addChild (percentParam (gMonitor, "Input Monitor", 2.0f, 1.0f));
        group->addChild (choiceParam (gQuantize,  "Quantize", quantizeChoices(), 2));
        group->addChild (choiceParam (gRecAction, "Rec Action", recActionChoices(), 0));
        group->addChild (boolParam (gSync,   "Sync to Host", true));
        group->addChild (boolParam (gFollow, "Follow Transport", false));
        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            PID { gTempo, 1 }, "Internal Tempo", juce::NormalisableRange<float> (40.0f, 300.0f, 0.1f), 120.0f,
            juce::AudioParameterFloatAttributes().withLabel ("BPM")));
        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            PID { gFade, 1 }, "Fade Time", juce::NormalisableRange<float> (10.0f, 8000.0f, 1.0f, 0.4f), 1000.0f,
            juce::AudioParameterFloatAttributes().withLabel ("ms")));
        group->addChild (boolParam (gRhythmOn, "Rhythm On", false));
        group->addChild (percentParam (gRhythmLevel, "Rhythm Level", 1.0f, 0.6f));
        group->addChild (choiceParam (gRhythmPattern, "Rhythm Pattern", rhythmChoices(), 1));
        layout.add (std::move (group));
    }

    // ---------------------------------------------------------------- effects
    for (int pass = 0; pass < 2; ++pass)
    {
        const bool input = pass == 0;
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> (input ? "inputfx" : "trackfx",
                                                                            input ? "Input FX" : "Track FX", "|");
        const std::array<FxType, 4> defaults = input
            ? std::array<FxType, 4> { FxType::LowPass, FxType::Phaser, FxType::Delay, FxType::Reverb }
            : std::array<FxType, 4> { FxType::Slicer, FxType::Flanger, FxType::LoFi, FxType::Tremolo };

        for (int s = 0; s < kNumFxSlots; ++s)
        {
            const auto n = juce::String (input ? "In FX " : "Trk FX ") + juce::String::charToString ((juce::juce_wchar) ('A' + s)) + " ";
            group->addChild (boolParam (fxId (input, s, kFxOn), n + "On", false));
            group->addChild (choiceParam (fxId (input, s, kFxType), n + "Type", fxTypeChoices(), (int) defaults[(size_t) s]));
            group->addChild (percentParam (fxId (input, s, kFxP1), n + "Param 1", 1.0f, 0.5f));
            group->addChild (percentParam (fxId (input, s, kFxP2), n + "Param 2", 1.0f, 0.5f));
            group->addChild (percentParam (fxId (input, s, kFxMix), n + "Mix", 1.0f, 0.5f));
        }
        layout.add (std::move (group));
    }

    return layout;
}
} // namespace livelooper::params
