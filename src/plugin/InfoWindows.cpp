#include "InfoWindows.h"
#include "AppSettings.h"
#include "Localisation.h"
#include "PluginProcessor.h"

#include "BuildInfo.h"
#include "BinaryData.h"

namespace livelooper
{
using i18n::tr;

//============================================================================== build info
namespace buildinfo
{
juce::String version()     { return LIVELOOPER_VERSION; }
juce::String gitHash()     { return LIVELOOPER_GIT_HASH; }
juce::String buildNumber() { return LIVELOOPER_BUILD_NUMBER; }
juce::String buildDate()   { return LIVELOOPER_BUILD_DATE; }
juce::String fullVersion()
{
    return version() + " (" + tr ("build %1", buildNumber()) + ", " + gitHash() + ", " + buildDate() + ")";
}
}

juce::String getEmbeddedText (const juce::String& fileName)
{
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        if (fileName == BinaryData::getNamedResourceOriginalFilename (BinaryData::namedResourceList[i]))
        {
            int size = 0;
            if (const auto* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size))
                return juce::String::fromUTF8 (data, size);
        }
    return {};
}

juce::Image getEmbeddedImage (const juce::String& fileName)
{
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        if (fileName == BinaryData::getNamedResourceOriginalFilename (BinaryData::namedResourceList[i]))
        {
            int size = 0;
            if (const auto* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size))
                return juce::ImageCache::getFromMemory (data, size);
        }
    return {};
}

namespace
{
    void styleText (juce::TextEditor& t)
    {
        t.setMultiLine (true, true);
        t.setReadOnly (true);
        t.setScrollbarsShown (true);
        t.setCaretVisible (false);
        t.setColour (juce::TextEditor::backgroundColourId, colours::lcdBack);
        t.setColour (juce::TextEditor::textColourId, colours::text);
        t.setColour (juce::TextEditor::outlineColourId, colours::sectionEdge);
        t.setFont (LooperLookAndFeel::font (14.0f));
    }

    void launch (juce::Component& parent, std::unique_ptr<juce::Component> content, const juce::String& title, bool resizable)
    {
        juce::DialogWindow::LaunchOptions o;
        o.dialogTitle = title;
        o.dialogBackgroundColour = colours::panel;
        o.content.setOwned (content.release());
        o.componentToCentreAround = &parent;
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = true;
        o.resizable = resizable;
        if (auto* w = o.launchAsync())
            if (resizable)
                w->setResizeLimits (480, 360, 2400, 1800);
    }
}

//============================================================================== HelpComponent
juce::Array<HelpComponent::Section> HelpComponent::parse (const juce::String& markdown, const juce::String& documentName)
{
    juce::Array<Section> out;
    Section current { documentName, documentName, {}, 1 };
    bool inCode = false;
    for (auto line : juce::StringArray::fromLines (markdown))
    {
        if (line.trimStart().startsWith ("```"))
            inCode = ! inCode;
        const bool h2 = ! inCode && line.startsWith ("## "), h3 = ! inCode && line.startsWith ("### ");
        if (h2 || h3)
        {
            if (current.text.trim().isNotEmpty())
                out.add (current);
            current = { documentName, line.fromFirstOccurrenceOf (" ", false, false).trim(), {}, h2 ? 2 : 3 };
            continue;
        }
        if (! inCode && line.startsWith ("# "))
            continue;     // document title
        current.text << line << "\n";
    }
    if (current.text.trim().isNotEmpty())
        out.add (current);
    return out;
}

juce::String HelpComponent::toPlainText (const juce::String& markdown)
{
    juce::String out;
    for (auto line : juce::StringArray::fromLines (markdown))
    {
        auto l = line;
        if (l.trim().startsWith ("![") || l.trim().startsWith ("**English**") || l.trim().startsWith ("[English]"))
            continue;
        if (l.trim().startsWith ("|"))
        {
            if (l.containsOnly ("|-: "))
                continue;                                    // table separator row
            auto cells = juce::StringArray::fromTokens (l.trim().trimCharactersAtStart ("|").trimCharactersAtEnd ("|"), "|", "`");
            cells.trim();
            l = cells.joinIntoString ("  -  ");
        }
        // [text](link) -> text
        for (int guard = 0; guard < 32; ++guard)
        {
            const int open = l.indexOf ("](");
            if (open < 0)
                break;
            const int start = l.substring (0, open).lastIndexOfChar ('[');
            const int close = l.indexOf (open, ")");
            if (start < 0 || close < 0)
                break;
            l = l.substring (0, start) + l.substring (start + 1, open) + l.substring (close + 1);
        }
        l = l.replace ("**", "").replace ("`", "").replace ("<br>", " ");
        if (l.startsWith ("- ") || l.startsWith ("* "))
            l = juce::String::fromUTF8 ("\xe2\x80\xa2 ") + l.substring (2);
        out << l << "\n";
    }
    return out.trim();
}

juce::Array<int> HelpComponent::search (const juce::Array<Section>& secs, const juce::String& query)
{
    juce::Array<int> result;
    auto words = juce::StringArray::fromTokens (query.toLowerCase(), " ", "\"");
    words.removeEmptyStrings();
    for (int i = 0; i < secs.size(); ++i)
    {
        const auto hay = (secs[i].title + "\n" + secs[i].text).toLowerCase();
        bool all = true;
        for (const auto& w : words)
            all = all && hay.contains (w);
        if (all)
            result.add (i);
    }
    return result;
}

HelpComponent::HelpComponent()
{
    const bool de = i18n::isGerman();
    struct Doc { const char* file; const char* name; };
    const Doc docs[] = {
        { de ? "BEDIENUNGSANLEITUNG.md" : "USER-GUIDE.md", de ? "Bedienungsanleitung" : "User guide" },
        { de ? "PUSH-de.md" : "PUSH-en.md", "Ableton Push" },
        { de ? "MIDI-de.md" : "MIDI-en.md", "MIDI" },
        { de ? "INSTALLATION-de.md" : "INSTALLATION-en.md", de ? "Installation & Probleme" : "Installation & troubleshooting" },
    };
    for (const auto& d : docs)
        sections.addArray (parse (getEmbeddedText (d.file), d.name));

    searchBox.setTextToShowWhenEmpty (tr ("Search the help (e.g. stutter, Link, capture)"), colours::textDim);
    searchBox.setColour (juce::TextEditor::backgroundColourId, colours::lcdBack);
    searchBox.setColour (juce::TextEditor::textColourId, colours::text);
    searchBox.setFont (LooperLookAndFeel::font (15.0f));
    searchBox.onTextChange = [this] { setQuery (searchBox.getText()); };
    addAndMakeVisible (searchBox);

    list.setModel (this);
    list.setRowHeight (24);
    list.setColour (juce::ListBox::backgroundColourId, colours::section);
    addAndMakeVisible (list);

    styleText (text);
    addAndMakeVisible (text);

    hint.setText (tr ("Tip: hover over any control for a short explanation, right-click it for MIDI learn."), juce::dontSendNotification);
    hint.setColour (juce::Label::textColourId, colours::textDim);
    hint.setFont (LooperLookAndFeel::font (12.0f));
    addAndMakeVisible (hint);

    setQuery ({});
    setSize (980, 680);
}

void HelpComponent::setQuery (const juce::String& q)
{
    visible = search (sections, q);
    list.updateContent();
    list.repaint();
    if (! visible.isEmpty())
    {
        list.selectRow (0);
        showSection (visible[0]);
    }
    else
        text.setText (tr ("Nothing found."), false);
}

void HelpComponent::showSection (int index)
{
    if (! juce::isPositiveAndBelow (index, sections.size()))
        return;
    const auto& s = sections.getReference (index);
    text.setText (s.title.toUpperCase() + "\n\n" + toPlainText (s.text), false);
    text.moveCaretToTop (false);
}

void HelpComponent::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (! juce::isPositiveAndBelow (row, visible.size()))
        return;
    const auto& s = sections.getReference (visible[row]);
    if (selected)
        g.fillAll (colours::cyan.withAlpha (0.25f));
    g.setColour (s.level <= 1 ? colours::yellow : (s.level == 2 ? colours::text : colours::textDim));
    g.setFont (LooperLookAndFeel::font (13.0f, s.level <= 2));
    const int indent = s.level == 3 ? 22 : 8;
    g.drawText (s.title, indent, 0, w - indent - 4, h, juce::Justification::centredLeft, true);
}

void HelpComponent::selectedRowsChanged (int row)
{
    if (juce::isPositiveAndBelow (row, visible.size()))
        showSection (visible[row]);
}

void HelpComponent::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
}

void HelpComponent::resized()
{
    auto b = getLocalBounds().reduced (12);
    hint.setBounds (b.removeFromBottom (22));
    searchBox.setBounds (b.removeFromTop (32));
    b.removeFromTop (8);
    list.setBounds (b.removeFromLeft (juce::jmin (330, b.getWidth() / 3)));
    b.removeFromLeft (8);
    text.setBounds (b);
}

void HelpWindow::show (juce::Component& parent)
{
    launch (parent, std::make_unique<HelpComponent>(), tr ("livelooper help"), true);
}

//============================================================================== UpdateManager
UpdateManager::~UpdateManager()
{
    clearSingletonInstance();
}

bool UpdateManager::isNewer() const
{
    return release.ok && updates::compareVersions (release.version, buildinfo::version()) > 0;
}

void UpdateManager::check()
{
    if (state == State::Checking || state == State::Downloading)
        return;
    state = State::Checking;
    error = {};
    sendChangeMessage();
    checker.check ([this] (const updates::ReleaseInfo& info) { onChecked (info); });
}

void UpdateManager::onChecked (const updates::ReleaseInfo& info)
{
    appsettings::setLastUpdateCheck (juce::Time::currentTimeMillis());
    if (! info.ok)
    {
        state = State::Error;
        error = info.error;
    }
    else
    {
        release = info;
        state = isNewer() ? State::Available : State::UpToDate;
    }
    sendChangeMessage();

    if (state == State::Available && autoMessage)
    {
        autoMessage (tr ("UPDATE AVAILABLE: LIVELOOPER %1", release.version));
        if (autoInstall)
            download();
    }
}

void UpdateManager::checkOnStart (bool standalone, std::function<void (const juce::String&)> message)
{
    if (startCheckDone)
        return;
    startCheckDone = true;
    autoMessage = std::move (message);
    autoInstall = appsettings::getAutoInstallUpdates();
    autoStandalone = standalone;
    check();
}

void UpdateManager::download()
{
    const auto* asset = release.platformAsset();
    if (asset == nullptr)
    {
        state = State::Error;
        error = "This release has no download for " + updates::platformName();
        sendChangeMessage();
        return;
    }
    state = State::Downloading;
    progress = 0.0f;
    sendChangeMessage();
    downloader.start (*asset, release.version,
        [this] (const updates::Downloader::Result& r) {
            downloaded = r;
            if (! r.ok)
            {
                state = State::Error;
                error = r.error;
            }
            else
                state = State::Ready;
            sendChangeMessage();
            if (r.ok && autoInstall && autoMessage)
            {
                juce::String e;
                if (autoStandalone && installApp (false, e))
                    autoMessage (tr ("UPDATE %1 IS INSTALLED WHEN YOU QUIT", release.version));
                else
                    autoMessage (tr ("UPDATE %1 DOWNLOADED - SEE SETUP > ABOUT", release.version));
            }
        },
        [this] (float p) { progress = p; sendChangeMessage(); });
}

bool UpdateManager::installApp (bool restart, juce::String& e)
{
    if (state != State::Ready && state != State::Scheduled)
    {
        e = "Nothing downloaded yet";
        return false;
    }
    if (! updates::scheduleAppReplacement (downloaded.app, restart, e))
        return false;
    state = State::Scheduled;
    sendChangeMessage();
    return true;
}

juce::StringArray UpdateManager::installPlugins (juce::String& e)
{
    return updates::installPlugins (downloaded, e);
}

//============================================================================== AboutComponent
AboutComponent::AboutComponent (bool isStandalone, bool checkNow) : standalone (isStandalone)
{
    logo = getEmbeddedImage ("beattactics-logo-transparent.png");

    auto setup = [this] (juce::TextButton& b, const juce::String& text) {
        b.setButtonText (text);
        addAndMakeVisible (b);
    };
    setup (licenceBtn, tr ("License (AGPLv3)"));
    setup (thirdPartyBtn, tr ("Third-party notices"));
    setup (checkBtn, tr ("Check for updates"));
    setup (actionBtn, {});
    setup (pluginsBtn, tr ("Install plug-ins"));
    setup (pageBtn, tr ("Release page"));
    licenceBtn.onClick = [this] { showText (tr ("License"), getEmbeddedText ("LICENSE")); };
    thirdPartyBtn.onClick = [this] { showText (tr ("Third-party notices"), getEmbeddedText ("THIRD-PARTY.md")); };
    checkBtn.onClick = [] { UpdateManager::getInstance()->check(); };
    pageBtn.onClick = [] {
        const auto& r = UpdateManager::getInstance()->getRelease();
        juce::URL (r.pageUrl.isNotEmpty() ? r.pageUrl : juce::String (updates::kReleasesPage)).launchInDefaultBrowser();
    };
    actionBtn.onClick = [this] {
        auto* um = UpdateManager::getInstance();
        if (um->getState() == UpdateManager::State::Available)
            um->download();
        else if (um->getState() == UpdateManager::State::Ready)
        {
            if (standalone)
            {
                juce::String e;
                if (um->installApp (true, e))
                    juce::JUCEApplicationBase::quit();
                else
                    status.setText (tr ("Install failed: %1", tr (e)), juce::dontSendNotification);
            }
            else
                um->getDownload().folder.revealToUser();
        }
    };
    pluginsBtn.onClick = [this] {
        auto opts = juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon)
                        .withTitle (tr ("Install plug-ins"))
                        .withMessage (tr ("Close every DAW that uses livelooper first - a plug-in is never replaced while a DAW has it loaded.\n\n"
                                          "The new VST3 (and AU on macOS) will be copied to your user plug-in folder."))
                        .withButton (tr ("Install")).withButton (tr ("Cancel")).withAssociatedComponent (this);
        juce::Component::SafePointer<AboutComponent> safe (this);
        juce::AlertWindow::showAsync (opts, [safe] (int r) {
            if (safe == nullptr || r != 1)
                return;
            juce::String e;
            const auto done = UpdateManager::getInstance()->installPlugins (e);
            safe->status.setText (done.isEmpty() ? tr ("Install failed: %1", tr (e))
                                                 : tr ("Installed: %1", done.joinIntoString (", ")), juce::dontSendNotification);
        });
    };

    autoCheck.setButtonText (tr ("Check automatically on start"));
    autoCheck.setToggleState (appsettings::getCheckUpdatesOnStart(), juce::dontSendNotification);
    autoCheck.onClick = [this] { appsettings::setCheckUpdatesOnStart (autoCheck.getToggleState()); };
    autoInstall.setButtonText (tr ("Install updates automatically"));
    autoInstall.setToggleState (appsettings::getAutoInstallUpdates(), juce::dontSendNotification);
    autoInstall.onClick = [this] { appsettings::setAutoInstallUpdates (autoInstall.getToggleState()); };
    for (auto* t : { &autoCheck, &autoInstall })
    {
        t->setColour (juce::ToggleButton::textColourId, colours::text);
        t->setColour (juce::ToggleButton::tickColourId, colours::cyan);
        addAndMakeVisible (t);
    }

    honesty.setText (tr (standalone
                             ? "Automatic means: a new version is downloaded and checked in the background and replaces the app when you quit livelooper. Plug-ins are never replaced while a DAW uses them."
                             : "Automatic means: a new version is downloaded and checked in the background. A plug-in is never replaced while your DAW runs - install it with the DAW closed (button above, or with the standalone app)."),
                     juce::dontSendNotification);
    honesty.setJustificationType (juce::Justification::topLeft);
    for (auto* l : { &status, &honesty })
    {
        l->setColour (juce::Label::textColourId, l == &status ? colours::text : colours::textDim);
        l->setFont (LooperLookAndFeel::font (l == &status ? 14.0f : 12.0f, l == &status));
        addAndMakeVisible (l);
    }
    styleText (notes);
    addAndMakeVisible (notes);

    UpdateManager::getInstance()->addChangeListener (this);
    refresh();
    if (checkNow)
        UpdateManager::getInstance()->check();
    setSize (640, 640);
}

AboutComponent::~AboutComponent()
{
    if (auto* um = UpdateManager::getInstanceWithoutCreating())
        um->removeChangeListener (this);
}

void AboutComponent::changeListenerCallback (juce::ChangeBroadcaster*) { refresh(); }

void AboutComponent::refresh()
{
    auto* um = UpdateManager::getInstance();
    const auto& r = um->getRelease();
    juce::String s;
    bool action = false, plugins = false;
    switch (um->getState())
    {
        case UpdateManager::State::Idle:       s = tr ("Last check: %1", appsettings::getLastUpdateCheck() > 0
                                                                          ? juce::Time (appsettings::getLastUpdateCheck()).toString (true, true, false)
                                                                          : tr ("never")); break;
        case UpdateManager::State::Checking:   s = tr ("Checking GitHub for a new version ..."); break;
        case UpdateManager::State::UpToDate:   s = tr ("livelooper is up to date (latest: %1)", r.version); break;
        case UpdateManager::State::Available:
            s = tr ("Version %1 is available.", r.version);
            actionBtn.setButtonText (tr (standalone ? "Download and install" : "Download"));
            action = true;
            break;
        case UpdateManager::State::Downloading:
            s = tr ("Downloading %1 ... %2 %", r.version, juce::String (juce::roundToInt (um->getProgress() * 100.0f)));
            break;
        case UpdateManager::State::Ready:
            s = tr (standalone ? "Version %1 is downloaded and verified." : "Version %1 is downloaded and verified: %2", r.version,
                    um->getDownload().folder.getFullPathName());
            actionBtn.setButtonText (tr (standalone ? "Quit and install now" : "Show folder"));
            action = true;
            plugins = um->getDownload().vst3.exists();
            break;
        case UpdateManager::State::Scheduled:
            s = tr ("Version %1 replaces this app when you quit livelooper.", r.version);
            plugins = um->getDownload().vst3.exists();
            break;
        case UpdateManager::State::Error:      s = tr ("Update check: %1", tr (um->getError())); break;
    }
    status.setText (s, juce::dontSendNotification);
    actionBtn.setVisible (action);
    pluginsBtn.setVisible (plugins);
    checkBtn.setEnabled (um->getState() != UpdateManager::State::Checking && um->getState() != UpdateManager::State::Downloading);
    notes.setText (r.ok ? (r.name.isNotEmpty() ? r.name : r.tag) + "\n\n" + r.notes : juce::String(), false);
    notes.setVisible (r.ok);
    resized();
}

void AboutComponent::showText (const juce::String& title, const juce::String& body)
{
    auto t = std::make_unique<juce::TextEditor>();
    styleText (*t);
    t->setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
    t->setText (body, false);
    t->setSize (720, 600);
    launch (*this, std::move (t), title, true);
}

void AboutComponent::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
    auto b = getLocalBounds().reduced (20);
    auto top = b.removeFromTop (150);
    if (logo.isValid())
        g.drawImageWithin (logo, top.getX(), top.getY(), 150, 150, juce::RectanglePlacement::centred);
    auto t = top.withTrimmedLeft (170);
    g.setColour (colours::text);
    g.setFont (LooperLookAndFeel::font (38.0f, true));
    g.drawText ("livelooper", t.removeFromTop (46), juce::Justification::centredLeft);
    g.setColour (colours::cyan);
    g.setFont (LooperLookAndFeel::font (15.0f, true));
    g.drawText (tr ("designed by groovex"), t.removeFromTop (22), juce::Justification::centredLeft);
    g.setColour (colours::orange);
    g.drawText ("BeatTactics", t.removeFromTop (22), juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (LooperLookAndFeel::font (13.0f));
    g.drawText (tr ("Version %1", buildinfo::fullVersion()), t.removeFromTop (20), juce::Justification::centredLeft);
    g.drawFittedText (tr ("Free software under the GNU Affero General Public License v3 (AGPLv3), without any warranty. Uses JUCE (AGPLv3) and Ableton Link (GPLv2+)."),
                      t.removeFromTop (36), juce::Justification::topLeft, 2);
}

void AboutComponent::resized()
{
    auto b = getLocalBounds().reduced (20);
    b.removeFromTop (160);
    auto row = b.removeFromTop (30);
    licenceBtn.setBounds (row.removeFromLeft (170));
    row.removeFromLeft (8);
    thirdPartyBtn.setBounds (row.removeFromLeft (170));
    b.removeFromTop (16);
    row = b.removeFromTop (30);
    checkBtn.setBounds (row.removeFromLeft (170));
    row.removeFromLeft (8);
    actionBtn.setBounds (row.removeFromLeft (170));
    row.removeFromLeft (8);
    pageBtn.setBounds (row.removeFromLeft (130));
    b.removeFromTop (6);
    status.setBounds (b.removeFromTop (40));
    autoCheck.setBounds (b.removeFromTop (24));
    autoInstall.setBounds (b.removeFromTop (24));
    honesty.setBounds (b.removeFromTop (48));
    pluginsBtn.setBounds (b.removeFromTop (30).removeFromLeft (170));
    b.removeFromTop (6);
    notes.setBounds (b);
}

void AboutWindow::show (juce::Component& parent, LiveLooperProcessor& proc, bool checkNow)
{
    const bool standalone = proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
    launch (parent, std::make_unique<AboutComponent> (standalone, checkNow), tr ("About livelooper"), false);
}
} // namespace livelooper
