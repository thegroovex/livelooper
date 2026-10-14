// livelooper standalone application (v1.5).
//
// Replaces JUCE's default standalone app (JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1) so that
// - the wrapper's "Options" button is called "Audio/MIDI Settings" (translated) and its menu
//   follows the livelooper language,
// - "Settings on start: default settings" starts without the last session's state,
// - on macOS the microphone permission is asked for once and then remembered (see
//   MacMicPermission.mm) - nothing else asks again,
// - the audio device's output latency is passed on for Ableton Link.
//
// The storage location of the JUCE wrapper settings (audio device, last state) is unchanged,
// so existing users keep their audio setup.

#include <juce_core/system/juce_TargetPlatform.h>

#if JucePlugin_Build_Standalone && JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP

#include <juce_audio_plugin_client/detail/juce_CheckSettingMacros.h>
#include <juce_audio_plugin_client/detail/juce_IncludeSystemHeaders.h>
#include <juce_audio_plugin_client/detail/juce_IncludeModuleHeaders.h>
#include <juce_audio_plugin_client/detail/juce_PluginUtilities.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

// juce_IncludeModuleHeaders.h defines "Component" as "juce::Component" (for the plug-in
// wrappers), which breaks qualified names like juce::Component::SafePointer below.
#ifdef Component
 #undef Component
#endif

#include "plugin/AppSettings.h"
#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"

namespace livelooper
{
#if JUCE_MAC
/** MacMicPermission.mm: 0 = not determined, 1 = granted, 2 = denied / restricted. */
int macMicrophonePermissionStatus();
void macRequestMicrophonePermission (std::function<void (bool)> done);
#endif

using i18n::tr;

class LooperWindow : public juce::StandaloneFilterWindow, private juce::Timer
{
public:
    LooperWindow (const juce::String& title, juce::Colour bg, std::unique_ptr<juce::StandalonePluginHolder> holder)
        : StandaloneFilterWindow (title, bg, std::move (holder))
    {
        // hide the wrapper's own button, show ours with a clear (translated) name
        for (auto* c : getChildren())
            if (auto* b = dynamic_cast<juce::TextButton*> (c); b != nullptr && b != &settingsButton && b->getButtonText() == "Options")
                b->setVisible (false);
        settingsButton.setButtonText (tr ("Audio/MIDI Settings"));
        settingsButton.setTooltip (tr ("Audio interface, inputs / outputs, sample rate, MIDI inputs"));
        settingsButton.setTriggeredOnMouseDown (true);
        settingsButton.onClick = [this] { showSettingsMenu(); };
        juce::Component::addAndMakeVisible (settingsButton);
        resized();
        startTimer (1000);
    }

    void resized() override
    {
        StandaloneFilterWindow::resized();
        const auto font = juce::FontOptions (14.0f);
        const int w = juce::GlyphArrangement::getStringWidthInt (font, settingsButton.getButtonText()) + 28;
        settingsButton.setBounds (8, 6, juce::jmax (120, w), juce::jmax (16, getTitleBarHeight() - 8));
    }

private:
    void showSettingsMenu()
    {
        settingsButton.setButtonText (tr ("Audio/MIDI Settings"));   // language may have changed
        juce::PopupMenu m;
        m.addItem (1, tr ("Audio/MIDI Settings ..."));
        m.addSeparator();
        m.addItem (2, tr ("Save current state ..."));
        m.addItem (3, tr ("Load a saved state ..."));
        m.addSeparator();
        m.addItem (4, tr ("Reset to default state"));
        juce::Component::SafePointer<LooperWindow> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settingsButton), [safe] (int r) {
            if (safe == nullptr)
                return;
            switch (r)
            {
                case 1: safe->pluginHolder->showAudioSettingsDialog(); break;
                case 2: safe->pluginHolder->askUserToSaveState(); break;
                case 3: safe->pluginHolder->askUserToLoadState(); break;
                case 4: safe->resetToDefaultState(); break;
                default: break;
            }
        });
    }

    void timerCallback() override
    {
        // output latency of the audio device for the Link phase
        if (auto* dev = getDeviceManager().getCurrentAudioDevice())
            if (auto* p = dynamic_cast<LiveLooperProcessor*> (getAudioProcessor()))
            {
                const double sr = dev->getCurrentSampleRate();
                if (sr > 0.0)
                    p->setOutputLatencySeconds ((dev->getOutputLatencyInSamples() + dev->getCurrentBufferSizeSamples()) / sr);
            }
        if (settingsButton.getButtonText() != tr ("Audio/MIDI Settings"))
        {
            settingsButton.setButtonText (tr ("Audio/MIDI Settings"));
            resized();
        }
    }

    juce::TextButton settingsButton;
};

class LooperApp : public juce::JUCEApplication
{
public:
    LooperApp()
    {
        // same storage as JUCE's default standalone app (audio device + last state stay)
        juce::PropertiesFile::Options options;
        options.applicationName = "livelooper";
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName = "~/.config";
       #else
        options.folderName = "";
       #endif
        appProperties.setStorageParameters (options);
    }

    const juce::String getApplicationName() override    { return "livelooper"; }
    const juce::String getApplicationVersion() override { return LIVELOOPER_VERSION; }
    bool moreThanOneInstanceAllowed() override          { return true; }
    void anotherInstanceStarted (const juce::String&) override {}

    void initialise (const juce::String&) override
    {
        // "Settings on start: default settings" - start without the state of the last session
        if (appsettings::getStartupMode() == appsettings::StartupMode::Defaults)
            if (auto* s = appProperties.getUserSettings())
                s->removeValue ("filterState");

       #if JUCE_MAC
        // ask for the microphone once (only while undecided); macOS remembers the answer for this app
        if (macMicrophonePermissionStatus() == 0)
        {
            macRequestMicrophonePermission ([this] (bool) { juce::MessageManager::callAsync ([this] { createWindow(); }); });
            return;
        }
       #endif
        createWindow();
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        appProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (mainWindow != nullptr)
            mainWindow->pluginHolder->savePluginState();
        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
            juce::Timer::callAfterDelay (100, [] {
                if (auto app = juce::JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
        else
            quit();
    }

private:
    void createWindow()
    {
        if (mainWindow != nullptr || juce::Desktop::getInstance().getDisplays().displays.isEmpty())
            return;
        auto holder = std::make_unique<juce::StandalonePluginHolder> (appProperties.getUserSettings(), false, juce::String(),
                                                                      nullptr, juce::Array<juce::StandalonePluginHolder::PluginInOuts>(),
                                                                      false);
        mainWindow = std::make_unique<LooperWindow> (getApplicationName(),
                                                     juce::LookAndFeel::getDefaultLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId),
                                                     std::move (holder));
        mainWindow->setVisible (true);

       #if JUCE_MAC
        if (macMicrophonePermissionStatus() == 2)
            juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon)
                                              .withTitle (tr ("No access to the microphone"))
                                              .withMessage (tr ("macOS does not allow livelooper to record. Allow it in System Settings > Privacy & Security > Microphone, then restart livelooper."))
                                              .withButton ("OK"),
                                          nullptr);
       #endif
    }

    juce::ApplicationProperties appProperties;
    std::unique_ptr<LooperWindow> mainWindow;
};
} // namespace livelooper

juce::JUCEApplicationBase* juce_CreateApplication() { return new livelooper::LooperApp(); }

#endif
