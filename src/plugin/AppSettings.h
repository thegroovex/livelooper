#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

/**
    v1.5 global app settings: everything that belongs to the user and this computer, not to
    a project. Stored in the user settings file livelooper.settings next to the language
    setting (macOS ~/Library/Application Support/livelooper/, Windows %APPDATA%\livelooper\,
    Linux ~/.config/livelooper/).

    - startup mode: what a new livelooper (standalone start, new plug-in instance without a
      saved project state) starts with - the last used settings or the defaults. Asked once
      on the first start, changeable in SETUP.
    - last settings: a snapshot of the looper's configuration (parameters, MIDI / keyboard
      mapping, tracks, routing, Push - no loops), written whenever it changes.
    - recent sets: the last 10 looper sets that were saved or loaded.
    - update check: check on start, install automatically, last check.

    The file is opened, changed and written in one go for every call, so several plug-in
    instances never overwrite each other's values with stale copies.
*/
namespace livelooper::appsettings
{
enum class StartupMode { Ask = 0, LastSettings, Defaults };

StartupMode getStartupMode();
void setStartupMode (StartupMode m);

/** The last used settings (null if there are none yet). */
std::unique_ptr<juce::XmlElement> loadLastSettings();
void saveLastSettings (const juce::XmlElement& snapshot);

inline constexpr int kMaxRecentSets = 10;
/** Most recent first; files that no longer exist are left out. */
juce::StringArray getRecentSets();
void addRecentSet (const juce::File& file);
void clearRecentSets();

bool getCheckUpdatesOnStart();
void setCheckUpdatesOnStart (bool b);
bool getAutoInstallUpdates();
void setAutoInstallUpdates (bool b);
juce::int64 getLastUpdateCheck();          // milliseconds since 1970, 0 = never
void setLastUpdateCheck (juce::int64 ms);

/** Any other value (generic access, e.g. a staged update). */
juce::String getValue (const juce::String& key, const juce::String& fallback = {});
void setValue (const juce::String& key, const juce::String& value);

/** The settings file. */
juce::File getSettingsFile();
/** Tests: store everything in another folder (an empty File = back to the user folder). */
void setStorageFolderForTests (const juce::File& folder);
} // namespace livelooper::appsettings
