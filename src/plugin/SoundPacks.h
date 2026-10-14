#pragma once

#include "engine/RhythmGenerator.h"

#include <juce_core/juce_core.h>
#include <memory>
#include <mutex>

namespace livelooper
{
/**
    v1.6 sound packs and the BEATS pattern library.

    A sound pack is a folder (or a .zip of that folder) with a "livelooper-pack.json" file,
    the samples (WAV / AIFF / FLAC) and optional patterns - see docs/en/SOUND-PACKS.md for the
    format. livelooper scans the packs folder (Music/livelooper/Sound Packs); every kit of a
    pack gets one of the eight "Pack Kit" slots of the kit selector. The slot of a kit is
    remembered on this computer, so a set keeps finding its pack kit.

    The pattern library (Music/livelooper/Patterns, *.llpattern) holds the BEATS patterns the
    user saved; together with the patterns of the packs they show up in the BEATS page.

    One instance per process (shared by all plug-in instances), message thread only. The banks
    handed to the audio thread are never freed while the process runs, so a raw pointer to a
    bank stays valid (they are small: the embedded kits are shared, not copied).
*/
class SoundPacks
{
public:
    static constexpr const char* kPackFileName = "livelooper-pack.json";
    static constexpr const char* kPatternExtension = ".llpattern";
    static constexpr int kMaxSamplesPerVoice = 32;

    /** v1.6: what a saved beat keeps besides the steps - swing and the BEATS row mixer.
        Optional in the file (pack patterns and beats saved before have none). */
    struct BeatSettings
    {
        struct Row { bool mute = false, solo = false; float level = 1.0f; float pitch = 0.0f; };
        bool  hasSwing = false;
        float swing = 50.0f;                     // percent, 50 = straight .. 75
        bool  hasMix = false;
        std::array<Row, BeatPattern::kRows> rows {};
        bool operator== (const BeatSettings& o) const noexcept;
    };

    struct NamedPattern
    {
        juce::String name;      // pattern name
        juce::String source;    // pack name, or empty for the user library
        juce::File file;        // user library file (empty for pack patterns)
        BeatPattern pattern;
        BeatSettings settings;  // v1.6: swing + row mixer, when the file has them
    };
    struct PackInfo
    {
        juce::String name, author, license, description;
        juce::File folder;
        juce::StringArray kitNames;
        int numPatterns = 0;
    };
    /** One pack read from disk (also used by the tests). */
    struct LoadedPack
    {
        PackInfo info;
        std::vector<std::shared_ptr<DrumBank::Kit>> kits;
        std::vector<NamedPattern> patterns;
    };

    static SoundPacks& get();

    /** Music/livelooper/Sound Packs (created on demand). */
    static juce::File getDefaultPackFolder();
    /** Music/livelooper/Patterns (created on demand). */
    static juce::File getDefaultPatternFolder();
    /** Tests: other folders (empty File = back to the defaults). */
    void setFoldersForTests (const juce::File& packs, const juce::File& patterns);
    juce::File getPackFolder() const;
    juce::File getPatternFolder() const;

    /** Reads all packs again and rebuilds the drum bank (message thread). */
    void rescan();
    /** Increases whenever the bank / the pack list changed. */
    int getVersion() const noexcept { return version.load(); }

    /** Embedded kits + the pack kit slots. Never null. */
    std::shared_ptr<const DrumBank> getBank();
    /** Name of the kit in pack slot 0 .. 7 ("Pack / Kit"), empty when the slot is free. */
    juce::String getSlotName (int slot) const;
    std::vector<PackInfo> getPacks() const;
    std::vector<NamedPattern> getPackPatterns() const;

    /** Copies a pack folder or extracts a .zip into the packs folder and rescans. */
    bool importPack (const juce::File& folderOrZip, juce::String& error);

    // ---- pattern library
    std::vector<NamedPattern> getUserPatterns() const;
    /** Writes the pattern as <name>.llpattern into the library; returns the file (empty on error). */
    juce::File saveUserPattern (const BeatPattern& p, const juce::String& name);
    /** v1.6: saves a beat with its swing and row mixer - into the library (target empty) or to any file
        ("Save as..."). Returns the file written, empty on error. */
    juce::File saveBeat (const BeatPattern& p, const juce::String& name, const BeatSettings& settings,
                         const juce::File& target = {});
    /** The library file a beat with this name is saved to (it may exist already). */
    juce::File getLibraryFileFor (const juce::String& name) const;
    /** Reads one .llpattern file (library or anywhere). */
    static bool loadBeatFile (const juce::File& file, NamedPattern& out);

    // ---- format helpers (public for tests)
    static bool loadPack (const juce::File& folder, LoadedPack& out, juce::String& error);
    /** Pattern object of a pack / .llpattern: name, steps_per_bar, bars, rows { kick: "X..." or [127, 0, ...] }. */
    static bool patternFromVar (const juce::var& v, BeatPattern& out, juce::String& name);
    static juce::var patternToVar (const BeatPattern& p, const juce::String& name);
    /** "swing": 56.0, "mixer": { "kick": { "level": 1.0, "pitch": 0, "mute": false, "solo": false }, ... } */
    static void settingsToVar (const BeatSettings& s, juce::var& patternObject);
    static BeatSettings settingsFromVar (const juce::var& patternObject);
    /** "kick", "snare", "clap", "hat_closed", "hat_open", "rim", "shaker", "tom" */
    static const char* voiceKey (int voice) noexcept;
    static int voiceFromKey (const juce::String& key) noexcept;
    /** Row text: '.' / '-' = off, 'g' = ghost (40), 'x' = normal (96), 'X' = accent (127), '1' .. '9' = 14 .. 126. */
    static juce::String rowToText (const BeatPattern& p, int row);

private:
    SoundPacks();
    void ensureScanned();
    void rebuildBank();

    mutable std::recursive_mutex lock;
    bool scanned = false;
    juce::File packFolderOverride, patternFolderOverride;
    std::vector<LoadedPack> packs;
    std::array<std::shared_ptr<const DrumBank::Kit>, RhythmGenerator::kNumPackKitSlots> slotKits {};
    std::array<juce::String, RhythmGenerator::kNumPackKitSlots> slotNames {};
    std::shared_ptr<const DrumBank> bank;
    std::vector<std::shared_ptr<const DrumBank>> retired;   // kept alive (audio thread may still use them)
    std::atomic<int> version { 0 };
};
} // namespace livelooper
