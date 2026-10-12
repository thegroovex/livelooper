#include "Parameters.h"

#include "engine/RhythmGenerator.h"
#include "Localisation.h"

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

juce::StringArray translated (const juce::StringArray& englishChoices)
{
    juce::StringArray s;
    for (const auto& c : englishChoices)
        s.add (i18n::tr (c));
    return s;
}

namespace
{
    // parameter suffix -> English name part
    const std::pair<const char*, const char*> kTrackNames[] = {
        { kRecPlay, "Rec/Play" }, { kStop, "Stop" }, { kClear, "Clear" }, { kUndo, "Undo" }, { kRedo, "Redo" },
        { kLevel, "Level" }, { kPan, "Pan" }, { kReverse, "Reverse" }, { kOneShot, "One Shot" },
        { kLoopSync, "Loop Sync" }, { kTrackFx, "Track FX" }, { kInput, "Input" }, { kStartMode, "Start Mode" },
        { kStopMode, "Stop Mode" }, { kDubMode, "Dub Mode" }, { kMeasure, "Measures" }
    };
    const std::pair<const char*, const char*> kFxNames[] = {
        { kFxOn, "On" }, { kFxType, "Type" }, { kFxP1, "Param 1" }, { kFxP2, "Param 2" }, { kFxMix, "Mix" }
    };
    const std::pair<const char*, const char*> kGlobalNames[] = {
        { "allstartstop", "All Start/Stop" }, { "allstart", "All Start" }, { "allstop", "All Stop" },
        { "clearall", "Clear All" }, { "tap", "Tap Tempo" }, { "master", "Master Level" },
        { "monitor", "Input Monitor" }, { "quantize", "Quantize" }, { "recaction", "Rec Action" },
        { "sync", "Sync to Host" }, { "follow", "Follow Transport" }, { "tempo", "Internal Tempo" },
        { "fade", "Fade Time" }, { "rhythm_on", "Rhythm On" }, { "rhythm_level", "Rhythm Level" },
        { "rhythm_pattern", "Rhythm Pattern" }
    };

    template <size_t N>
    const char* lookup (const std::pair<const char*, const char*> (&table)[N], const juce::String& key)
    {
        for (const auto& [k, v] : table)
            if (key == k)
                return v;
        return nullptr;
    }

    juce::String makeName (const juce::String& paramId, bool localise)
    {
        auto t = [localise] (const juce::String& s) { return localise ? i18n::tr (s) : s; };
        auto t2 = [localise] (const juce::String& s, const juce::String& a, const juce::String& b) {
            return localise ? i18n::tr (s, a, b) : s.replace ("%1", a).replace ("%2", b);
        };

        if (auto* g = lookup (kGlobalNames, paramId))
            return t (g);

        // track: "t1_level"
        if (paramId.length() > 3 && paramId[0] == 't' && paramId[2] == '_')
            if (auto* n = lookup (kTrackNames, paramId.substring (3)))
                return t2 ("T%1 %2", juce::String::charToString (paramId[1]), t (n));

        // fx: "inA_type" / "trC_mix"
        const bool in = paramId.startsWith ("in"), tr = paramId.startsWith ("tr");
        if ((in || tr) && paramId.length() > 4 && paramId[3] == '_')
            if (auto* n = lookup (kFxNames, paramId.substring (4)))
                return t2 (in ? "In FX %1 %2" : "Trk FX %1 %2", juce::String::charToString (paramId[2]), t (n));

        return paramId;
    }
}

juce::String englishName (const juce::String& paramId)   { return makeName (paramId, false); }
juce::String localisedName (const juce::String& paramId) { return makeName (paramId, true); }

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
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("track" + juce::String (t + 1),
                                                                            i18n::tr ("Track %1", juce::String (t + 1)), "|");
        group->addChild (boolParam (trackId (t, kRecPlay), localisedName (trackId (t, kRecPlay)), false));
        group->addChild (boolParam (trackId (t, kStop), localisedName (trackId (t, kStop)), false));
        group->addChild (boolParam (trackId (t, kClear), localisedName (trackId (t, kClear)), false));
        group->addChild (boolParam (trackId (t, kUndo), localisedName (trackId (t, kUndo)), false));
        group->addChild (boolParam (trackId (t, kRedo), localisedName (trackId (t, kRedo)), false));
        group->addChild (percentParam (trackId (t, kLevel), localisedName (trackId (t, kLevel)), 2.0f, 1.0f));
        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            PID { trackId (t, kPan), 1 }, localisedName (trackId (t, kPan)), juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) {
                const int p = juce::roundToInt (v * 50.0f);
                return p == 0 ? juce::String ("C") : (p < 0 ? "L" + juce::String (-p) : "R" + juce::String (p));
            })));
        group->addChild (boolParam (trackId (t, kReverse), localisedName (trackId (t, kReverse)), false));
        group->addChild (boolParam (trackId (t, kOneShot), localisedName (trackId (t, kOneShot)), false));
        group->addChild (boolParam (trackId (t, kLoopSync), localisedName (trackId (t, kLoopSync)), true));
        group->addChild (boolParam (trackId (t, kTrackFx), localisedName (trackId (t, kTrackFx)), false));
        group->addChild (choiceParam (trackId (t, kInput), localisedName (trackId (t, kInput)), translated (inputChoices()), 0));
        group->addChild (choiceParam (trackId (t, kStartMode), localisedName (trackId (t, kStartMode)), translated (startModeChoices()), 0));
        group->addChild (choiceParam (trackId (t, kStopMode), localisedName (trackId (t, kStopMode)), translated (stopModeChoices()), 0));
        group->addChild (choiceParam (trackId (t, kDubMode), localisedName (trackId (t, kDubMode)), translated (dubModeChoices()), 0));
        group->addChild (choiceParam (trackId (t, kMeasure), localisedName (trackId (t, kMeasure)), translated (measureChoices()), 0));
        layout.add (std::move (group));
    }

    // ---------------------------------------------------------------- global
    {
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("global", i18n::tr ("Global"), "|");
        group->addChild (boolParam (gAllStartStop, localisedName (gAllStartStop), false));
        group->addChild (boolParam (gAllStart, localisedName (gAllStart), false));
        group->addChild (boolParam (gAllStop, localisedName (gAllStop), false));
        group->addChild (boolParam (gClearAll, localisedName (gClearAll), false));
        group->addChild (boolParam (gTap, localisedName (gTap), false));
        group->addChild (percentParam (gMaster, localisedName (gMaster), 2.0f, 1.0f));
        group->addChild (percentParam (gMonitor, localisedName (gMonitor), 2.0f, 1.0f));
        group->addChild (choiceParam (gQuantize, localisedName (gQuantize), translated (quantizeChoices()), 2));
        group->addChild (choiceParam (gRecAction, localisedName (gRecAction), translated (recActionChoices()), 0));
        group->addChild (boolParam (gSync, localisedName (gSync), true));
        group->addChild (boolParam (gFollow, localisedName (gFollow), false));
        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            PID { gTempo, 1 }, localisedName (gTempo), juce::NormalisableRange<float> (40.0f, 300.0f, 0.1f), 120.0f,
            juce::AudioParameterFloatAttributes().withLabel ("BPM")));
        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            PID { gFade, 1 }, localisedName (gFade), juce::NormalisableRange<float> (10.0f, 8000.0f, 1.0f, 0.4f), 1000.0f,
            juce::AudioParameterFloatAttributes().withLabel ("ms")));
        group->addChild (boolParam (gRhythmOn, localisedName (gRhythmOn), false));
        group->addChild (percentParam (gRhythmLevel, localisedName (gRhythmLevel), 1.0f, 0.6f));
        group->addChild (choiceParam (gRhythmPattern, localisedName (gRhythmPattern), translated (rhythmChoices()), 1));
        layout.add (std::move (group));
    }

    // ---------------------------------------------------------------- effects
    for (int pass = 0; pass < 2; ++pass)
    {
        const bool input = pass == 0;
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> (input ? "inputfx" : "trackfx",
                                                                            i18n::tr (input ? "Input FX" : "Track FX"), "|");
        const std::array<FxType, 4> defaults = input
            ? std::array<FxType, 4> { FxType::LowPass, FxType::Phaser, FxType::Delay, FxType::Reverb }
            : std::array<FxType, 4> { FxType::Slicer, FxType::Flanger, FxType::LoFi, FxType::Tremolo };

        for (int s = 0; s < kNumFxSlots; ++s)
        {
            group->addChild (boolParam (fxId (input, s, kFxOn), localisedName (fxId (input, s, kFxOn)), false));
            group->addChild (choiceParam (fxId (input, s, kFxType), localisedName (fxId (input, s, kFxType)), translated (fxTypeChoices()), (int) defaults[(size_t) s]));
            group->addChild (percentParam (fxId (input, s, kFxP1), localisedName (fxId (input, s, kFxP1)), 1.0f, 0.5f));
            group->addChild (percentParam (fxId (input, s, kFxP2), localisedName (fxId (input, s, kFxP2)), 1.0f, 0.5f));
            group->addChild (percentParam (fxId (input, s, kFxMix), localisedName (fxId (input, s, kFxMix)), 1.0f, 0.5f));
        }
        layout.add (std::move (group));
    }

    return layout;
}
} // namespace livelooper::params
