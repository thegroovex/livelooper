#include "AppSettings.h"

namespace livelooper::appsettings
{
namespace
{
    juce::File& testFolder()
    {
        static juce::File f;
        return f;
    }

    juce::CriticalSection& fileLock()
    {
        static juce::CriticalSection c;
        return c;
    }

    juce::PropertiesFile::Options options()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "livelooper";
        o.filenameSuffix = ".settings";
       #if JUCE_LINUX || JUCE_BSD
        o.folderName = ".config/livelooper";
       #else
        o.folderName = "livelooper";
       #endif
        o.osxLibrarySubFolder = "Application Support";
        o.storageFormat = juce::PropertiesFile::storeAsXML;
        o.millisecondsBeforeSaving = -1;    // written explicitly
        return o;
    }

    std::unique_ptr<juce::PropertiesFile> open()
    {
        if (testFolder() != juce::File())
            return std::make_unique<juce::PropertiesFile> (testFolder().getChildFile ("livelooper.settings"), options());
        return std::make_unique<juce::PropertiesFile> (options());
    }

    template <typename Fn>
    void modify (Fn&& fn)
    {
        const juce::ScopedLock sl (fileLock());
        auto p = open();
        fn (*p);
        p->saveIfNeeded();
    }
}

juce::File getSettingsFile()
{
    const juce::ScopedLock sl (fileLock());
    return open()->getFile();
}

void setStorageFolderForTests (const juce::File& folder)
{
    const juce::ScopedLock sl (fileLock());
    testFolder() = folder;
    if (folder != juce::File())
        folder.createDirectory();
}

StartupMode getStartupMode()
{
    const juce::ScopedLock sl (fileLock());
    const auto v = open()->getValue ("startupSettings", "ask");
    return v == "last" ? StartupMode::LastSettings : (v == "default" ? StartupMode::Defaults : StartupMode::Ask);
}

void setStartupMode (StartupMode m)
{
    modify ([m] (juce::PropertiesFile& p) {
        p.setValue ("startupSettings", m == StartupMode::LastSettings ? "last" : (m == StartupMode::Defaults ? "default" : "ask"));
    });
}

std::unique_ptr<juce::XmlElement> loadLastSettings()
{
    const juce::ScopedLock sl (fileLock());
    return open()->getXmlValue ("lastSettings");
}

void saveLastSettings (const juce::XmlElement& snapshot)
{
    modify ([&snapshot] (juce::PropertiesFile& p) { p.setValue ("lastSettings", &snapshot); });
}

juce::StringArray getRecentSets()
{
    juce::StringArray list;
    {
        const juce::ScopedLock sl (fileLock());
        list = juce::StringArray::fromLines (open()->getValue ("recentSets"));
    }
    list.removeEmptyStrings();
    juce::StringArray existing;
    for (const auto& path : list)
        if (juce::File::isAbsolutePath (path) && juce::File (path).existsAsFile())
            existing.addIfNotAlreadyThere (path);
    return existing;
}

void addRecentSet (const juce::File& file)
{
    modify ([&file] (juce::PropertiesFile& p) {
        auto list = juce::StringArray::fromLines (p.getValue ("recentSets"));
        list.removeEmptyStrings();
        list.removeString (file.getFullPathName());
        list.insert (0, file.getFullPathName());
        while (list.size() > kMaxRecentSets)
            list.remove (list.size() - 1);
        p.setValue ("recentSets", list.joinIntoString ("\n"));
    });
}

void clearRecentSets()
{
    modify ([] (juce::PropertiesFile& p) { p.removeValue ("recentSets"); });
}

bool getCheckUpdatesOnStart()
{
    const juce::ScopedLock sl (fileLock());
    return open()->getBoolValue ("checkUpdates", false);
}

void setCheckUpdatesOnStart (bool b)
{
    modify ([b] (juce::PropertiesFile& p) { p.setValue ("checkUpdates", b); });
}

bool getAutoInstallUpdates()
{
    const juce::ScopedLock sl (fileLock());
    return open()->getBoolValue ("autoInstallUpdates", false);
}

void setAutoInstallUpdates (bool b)
{
    modify ([b] (juce::PropertiesFile& p) { p.setValue ("autoInstallUpdates", b); });
}

juce::int64 getLastUpdateCheck()
{
    const juce::ScopedLock sl (fileLock());
    return open()->getValue ("lastUpdateCheck", "0").getLargeIntValue();
}

void setLastUpdateCheck (juce::int64 ms)
{
    modify ([ms] (juce::PropertiesFile& p) { p.setValue ("lastUpdateCheck", juce::String (ms)); });
}

juce::String getValue (const juce::String& key, const juce::String& fallback)
{
    const juce::ScopedLock sl (fileLock());
    return open()->getValue (key, fallback);
}

void setValue (const juce::String& key, const juce::String& value)
{
    modify ([&] (juce::PropertiesFile& p) { p.setValue (key, value); });
}
} // namespace livelooper::appsettings
