#pragma once

#include "Updates.h"
#include "LooperLookAndFeel.h"

#include <juce_gui_extra/juce_gui_extra.h>

namespace livelooper
{
class LiveLooperProcessor;

/** Version, git hash, CI run number and build date (injected by CMake). */
namespace buildinfo
{
juce::String version();       // "1.5.0"
juce::String gitHash();       // "c79b28c" or "unknown"
juce::String buildNumber();   // CI run number or "local"
juce::String buildDate();     // "2026-10-14"
juce::String fullVersion();   // "1.5.0 (build 123, c79b28c, 2026-10-14)"
}

/** Text resources compiled into the plug-in (help pages, licence texts) by original file name. */
juce::String getEmbeddedText (const juce::String& fileName);
juce::Image getEmbeddedImage (const juce::String& fileName);

/**
    In-app help: the user guide, the Push guide and the MIDI guide in the current language,
    split into sections, with a search field. Generated from the same Markdown files as the
    documentation, so it is always consistent with the user guide.
*/
class HelpComponent : public juce::Component, private juce::ListBoxModel
{
public:
    HelpComponent();
    void resized() override;
    void paint (juce::Graphics&) override;

    struct Section { juce::String document, title, text; int level = 2; };
    /** Parses Markdown into sections at '##' / '###' headings (public for tests). */
    static juce::Array<Section> parse (const juce::String& markdown, const juce::String& documentName);
    /** Plain text of a Markdown section (links, emphasis and table bars simplified). */
    static juce::String toPlainText (const juce::String& markdown);
    /** Sections that contain every word of the query (case-insensitive). */
    static juce::Array<int> search (const juce::Array<Section>& sections, const juce::String& query);

    void setQuery (const juce::String& q);
    int getNumVisibleSections() const { return visible.size(); }
    const juce::Array<Section>& getSections() const { return sections; }

private:
    int getNumRows() override { return visible.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void showSection (int index);

    juce::Array<Section> sections;
    juce::Array<int> visible;
    juce::TextEditor searchBox, text;
    juce::ListBox list;
    juce::Label hint;
};

class HelpWindow
{
public:
    static void show (juce::Component& parent);
};

/** Holds the update state for the whole process (all editors / plug-in instances). */
class UpdateManager : public juce::ChangeBroadcaster, private juce::DeletedAtShutdown
{
public:
    enum class State { Idle, Checking, UpToDate, Available, Downloading, Ready, Scheduled, Error };

    JUCE_DECLARE_SINGLETON_INLINE (UpdateManager, false)
    ~UpdateManager() override;

    void check();
    /** Automatic check on start: once per session; with "install automatically" the download
        follows. message() gets short status texts for the display. */
    void checkOnStart (bool standalone, std::function<void (const juce::String&)> message);
    void download();
    /** Standalone: replace the app when it quits (restart = start the new version). */
    bool installApp (bool restart, juce::String& error);
    juce::StringArray installPlugins (juce::String& error);

    State getState() const noexcept { return state; }
    const updates::ReleaseInfo& getRelease() const noexcept { return release; }
    const updates::Downloader::Result& getDownload() const noexcept { return downloaded; }
    float getProgress() const noexcept { return progress; }
    juce::String getError() const { return error; }
    bool isNewer() const;

private:
    void onChecked (const updates::ReleaseInfo& info);
    updates::Checker checker;
    updates::Downloader downloader;
    State state = State::Idle;
    updates::ReleaseInfo release;
    updates::Downloader::Result downloaded;
    juce::String error;
    float progress = 0.0f;
    bool startCheckDone = false, autoInstall = false, autoStandalone = false;
    std::function<void (const juce::String&)> autoMessage;
};

/** About livelooper: logo, version / build, licence and third-party notices, update check. */
class AboutComponent : public juce::Component, private juce::ChangeListener
{
public:
    AboutComponent (bool standalone, bool checkNow);
    ~AboutComponent() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refresh();
    void showText (const juce::String& title, const juce::String& body);

    bool standalone;
    juce::Image logo, icon;
    juce::TextButton licenceBtn, thirdPartyBtn, checkBtn, actionBtn, pluginsBtn, pageBtn;
    juce::ToggleButton autoCheck, autoInstall;
    juce::Label status, honesty;
    juce::TextEditor notes;
};

class AboutWindow
{
public:
    static void show (juce::Component& parent, LiveLooperProcessor& proc, bool checkNow);
};
} // namespace livelooper
