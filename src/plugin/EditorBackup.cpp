// v1.7 auto backup + crash recovery in the editor: SETUP > Sets > Auto backup, the restore menu and the
// small notice after a crash (never a modal dialog - the music may already be running).

#include "PluginEditor.h"
#include "AppSettings.h"

namespace livelooper
{
using i18n::tr;

namespace
{
    juce::String timeText (juce::Time t)
    {
        const auto today = juce::Time::getCurrentTime();
        const bool sameDay = t.getYear() == today.getYear() && t.getDayOfYear() == today.getDayOfYear();
        return sameDay ? t.formatted ("%H:%M:%S") : t.formatted ("%Y-%m-%d %H:%M");
    }

    /** Bottom-right notice in the style of the K.O. II notice: one line, two buttons, closes by itself. */
    class BackupToast : public juce::Component, private juce::Timer
    {
    public:
        BackupToast (const juce::String& text, const juce::String& first, const juce::String& second, std::function<void (int)> pick)
            : message (text), choose (std::move (pick))
        {
            buttons[0] = std::make_unique<LLButton> (first, colours::learn);
            buttons[1] = std::make_unique<LLButton> (second, colours::cyan);
            for (int i = 0; i < 2; ++i)
            {
                buttons[(size_t) i]->onClick = [this, i] { press (i); };
                buttons[(size_t) i]->setWantsKeyboardFocus (false);
                addAndMakeVisible (*buttons[(size_t) i]);
            }
            setWantsKeyboardFocus (false);
            startTimer (20000);          // closes by itself: nothing changes (the backups stay in SETUP)
        }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (2.0f);
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (r.translated (0.0f, 4.0f), 14.0f);
            g.setColour (juce::Colour (0xf0202228));
            g.fillRoundedRectangle (r, 14.0f);
            g.setColour (colours::learn.withAlpha (0.55f));
            g.drawRoundedRectangle (r, 14.0f, 1.0f);
            g.setColour (colours::text);
            g.setFont (LooperLookAndFeel::font (14.0f, true));
            g.drawFittedText (message, getLocalBounds().reduced (18, 12).removeFromTop (40), juce::Justification::centredLeft, 2, 0.85f);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (16, 12);
            r.removeFromTop (44);
            auto row = r.removeFromTop (34);
            const int w = (row.getWidth() - 8) / 2;
            buttons[0]->setBounds (row.removeFromLeft (w));
            row.removeFromLeft (8);
            buttons[1]->setBounds (row);
        }
        void press (int i)
        {
            stopTimer();
            if (choose) choose (i);
        }
        juce::String message;
    private:
        void timerCallback() override { press (-1); }
        std::array<std::unique_ptr<LLButton>, 2> buttons;
        std::function<void (int)> choose;
    };
}

//============================================================================== notice
void LiveLooperEditor::updateBackupNotice()
{
    if (! proc.hasBackupNotice())
        return;
    const auto n = proc.takeBackupNotice();
    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    const auto file = n.file;
    std::unique_ptr<BackupToast> t;
    if (n.kind == LiveLooperProcessor::BackupNotice::Kind::Restored)
    {
        // standalone after a crash: the newest backup is already loaded
        t = std::make_unique<BackupToast> (tr ("livelooper was not closed properly. The backup from %1 was restored.", timeText (n.time)),
                                           "Keep", "Start empty", [safe] (int i) {
            if (safe == nullptr)
                return;
            if (i == 1)
            {
                safe->proc.startEmptyAfterRestore();
                safe->showStatus (tr ("EMPTY SET"));
            }
            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->backupToast.reset(); });
        });
    }
    else
    {
        // plug-in: the host restored the project; a newer backup exists - offer it, never override silently
        t = std::make_unique<BackupToast> (tr ("livelooper was not closed properly. A backup from %1 is newer than the project.", timeText (n.time)),
                                           "Restore backup", "Keep project", [safe, file] (int i) {
            if (safe == nullptr)
                return;
            if (i == 0)
            {
                juce::String error;
                if (safe->proc.restoreBackup (file, error))
                    safe->showStatus (tr ("BACKUP RESTORED"));
                else
                    safe->showStatus (error);
            }
            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->backupToast.reset(); });
        });
    }
    backupToast = std::move (t);
    surface.addAndMakeVisible (*backupToast);
    backupToast->setBounds (surface.getWidth() - 560 - 24, surface.getHeight() - 112 - 24 - (ko2Toast != nullptr ? 124 : 0), 560, 112);
    backupToast->toFront (false);
}

void LiveLooperEditor::pressBackupToastButton (int index)
{
    if (auto* t = dynamic_cast<BackupToast*> (backupToast.get()))
        t->press (index);
}

//============================================================================== SETUP > Sets > Auto backup
void LiveLooperEditor::addBackupMenu (juce::PopupMenu& menu, juce::Array<int>& toggles)
{
    const bool app = proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone || proc.isStandaloneSession();
    const int minutes = backup::getIntervalMinutes();
    juce::PopupMenu m;
    m.addItem (2100, tr ("Auto backup"), true, backup::isEnabled());
    m.addSectionHeader (tr ("Backup interval"));
    for (int i = 0; i < (int) std::size (backup::kIntervalChoices); ++i)
        m.addItem (2110 + i, tr (backup::kIntervalChoices[i] == 1 ? "Every minute" : "Every %1 minutes", juce::String (backup::kIntervalChoices[i])),
                   true, backup::kIntervalChoices[i] == minutes);
    if (app)
    {
        m.addSeparator();
        m.addItem (2101, tr ("Auto load last session on start"), true, backup::getAutoLoad());
    }
    m.addSeparator();
    m.addItem (2102, tr ("Restore backup ..."));
    m.addItem (2103, tr ("Open backup folder"));
    m.addSeparator();
    const auto last = proc.getAutoBackup().getLastBackupMs();
    m.addItem (2108, last > 0 ? tr ("Last backup: %1", timeText (juce::Time (last))) : tr ("No backup in this session yet"), false);
    m.addItem (2109, tr ("Also after every recording; only when something changed; the last %1 are kept", juce::String (backup::kKeep)), false);
    menu.addSubMenu (tr ("Auto backup"), m);
    toggles.addArray ({ 2100, -2110, -2111, -2112, -2113 });
    if (app)
        toggles.add (2101);
}

bool LiveLooperEditor::handleBackupMenu (int r)
{
    if (r < 2100 || r > 2149)
        return false;
    switch (r)
    {
        case 2100:
            backup::setEnabled (! backup::isEnabled());
            showStatus (tr (backup::isEnabled() ? "AUTO BACKUP ON" : "AUTO BACKUP OFF"));
            break;
        case 2101: backup::setAutoLoad (! backup::getAutoLoad()); break;
        case 2102: showRestoreBackupMenu(); break;
        case 2103: backup::revealFolder (proc.getBackupFolder()); break;
        default:
            if (r >= 2110 && r < 2110 + (int) std::size (backup::kIntervalChoices))
                backup::setIntervalMinutes (backup::kIntervalChoices[r - 2110]);
            break;
    }
    proc.applyBackupSettings();
    return true;
}

void LiveLooperEditor::showRestoreBackupMenu()
{
    const auto files = backup::list (proc.getBackupFolder());
    juce::PopupMenu m;
    m.addSectionHeader (tr ("Restore backup"));
    for (int i = 0; i < files.size(); ++i)
        m.addItem (1 + i, timeText (backup::timeOf (files[i])) + "   (" + juce::File::descriptionOfSizeInBytes (files[i].getSize()) + ")");
    if (files.isEmpty())
        m.addItem (-1, tr ("No backup in this session yet"), false);
    m.addSeparator();
    m.addItem (100, tr ("Choose a backup file ..."));
    m.addItem (-2, tr ("The current set is backed up first, so restoring can be undone"), false);

    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    auto restore = [safe] (const juce::File& f) {
        if (safe == nullptr || ! f.existsAsFile())
            return;
        juce::String error;
        if (safe->proc.restoreBackup (f, error))
            safe->showStatus (tr ("BACKUP RESTORED"));
        else
            safe->showStatus (error);
    };
    m.showMenuAsync (juce::PopupMenu::Options(), [safe, files, restore] (int r) {
        if (safe == nullptr)
            return;
        if (r >= 1 && r <= files.size())
            restore (files[r - 1]);
        else if (r == 100)
        {
            backup::getRootFolder().createDirectory();
            safe->backupChooser = std::make_unique<juce::FileChooser> (tr ("Restore backup"), backup::getRootFolder(),
                                                                       juce::String ("*") + backup::kExtension);
            safe->backupChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                              [restore] (const juce::FileChooser& fc) { restore (fc.getResult()); });
        }
    });
}
} // namespace livelooper
