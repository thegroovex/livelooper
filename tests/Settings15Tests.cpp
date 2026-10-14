// v1.5: Ableton Link on / off (no other peer or network needed), app settings stored in a
// temporary folder, semantic version compare for the update check.

#include "plugin/PluginProcessor.h"
#include "plugin/AppSettings.h"
#include "plugin/LinkSync.h"
#include "plugin/Updates.h"

using namespace livelooper;
using namespace livelooper::params;

class Settings15Tests : public juce::UnitTest
{
public:
    Settings15Tests() : juce::UnitTest ("Link, app settings, versions (1.5)", "livelooper") {}

    void runTest() override
    {
        beginTest ("LinkSync: enable / disable without peers, blocks are only active while enabled");
        {
            LinkSync link;
            expect (! link.isEnabled(), "off after creation");
            auto blk = link.beginBlock (48000.0, 512, 0.005, 4.0, 0.0, false, false, 0.0);
            expect (! blk.active, "disabled: block inactive");
            expectEquals (link.getNumPeers(), 0);

            link.setEnabled (true);
            if (LinkSync::isAvailable())
            {
                expect (link.isEnabled(), "enabled");
                blk = link.beginBlock (48000.0, 512, 0.005, 4.0, 0.0, false, false, 0.0);
                expect (blk.active, "enabled: block active");
                expect (blk.bpm >= 20.0 && blk.bpm <= 999.0, "valid session tempo");
                // asking for a tempo (the user turned the tempo knob) sets the session tempo
                blk = link.beginBlock (48000.0, 512, 0.005, 4.0, 133.0, false, false, 0.0);
                blk = link.beginBlock (48000.0, 512, 0.005, 4.0, 0.0, false, false, 0.0);
                expectWithinAbsoluteError (blk.bpm, 133.0, 0.01, "tempo request reaches the session");
                const double beat1 = blk.beat;
                juce::Thread::sleep (30);
                blk = link.beginBlock (48000.0, 512, 0.005, 4.0, 0.0, false, false, 0.0);
                expect (blk.beat > beat1, "the beat runs on");
                expectEquals (link.getNumPeers(), 0, "no peers in the test");
            }
            link.setEnabled (false);
            expect (! link.isEnabled(), "disabled again");
            blk = link.beginBlock (48000.0, 512, 0.005, 4.0, 0.0, false, false, 0.0);
            expect (! blk.active, "disabled: block inactive again");
        }

        beginTest ("Processor: the Link parameter switches Link on / off, Link drives the timeline when HOST SYNC is off");
        {
            LiveLooperProcessor proc { false, false };
            proc.prepareToPlay (48000.0, 480);
            auto set = [&proc] (const juce::String& id, float v) { proc.getState().getParameter (id)->setValueNotifyingHost (v); };
            set (gSync, 0.0f);
            juce::AudioBuffer<float> buffer (2, 480);
            juce::MidiBuffer midi;
            auto run = [&] (int blocks) {
                for (int b = 0; b < blocks; ++b)
                {
                    buffer.clear();
                    proc.processBlock (buffer, midi);
                }
            };
            run (4);
            expect (! proc.isLinkDriving(), "Link off by default");
            expectEquals (proc.getLinkPeers(), 0);

            set (gLinkOn, 1.0f);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (150);   // the processor's timer turns Link on
            run (4);
            expect (proc.isLinkDriving() == proc.isLinkAvailable(), "Link drives the timeline");
            expect (proc.getLinkStatus().isNotEmpty());

            set (gSync, 1.0f);
            run (2);
            if (proc.wrapperType != juce::AudioProcessor::wrapperType_Standalone)
                expect (! proc.isLinkDriving(), "in a DAW, HOST SYNC has priority");
            set (gSync, 0.0f);

            set (gLinkOn, 0.0f);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
            run (4);
            expect (! proc.isLinkDriving(), "Link off again");
        }

        beginTest ("App settings: stored in the settings file, survive re-opening, recent sets");
        {
            const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getNonexistentChildFile ("livelooper-settings-test", "");
            appsettings::setStorageFolderForTests (dir);
            expect (appsettings::getSettingsFile().isAChildOf (dir), "settings file in the test folder");

            expect (appsettings::getStartupMode() == appsettings::StartupMode::Ask, "asks on the first start");
            appsettings::setStartupMode (appsettings::StartupMode::LastSettings);
            expect (appsettings::getStartupMode() == appsettings::StartupMode::LastSettings);
            appsettings::setStartupMode (appsettings::StartupMode::Defaults);
            expect (appsettings::getStartupMode() == appsettings::StartupMode::Defaults);

            expect (! appsettings::getCheckUpdatesOnStart() && ! appsettings::getAutoInstallUpdates(), "updates: off by default");
            appsettings::setCheckUpdatesOnStart (true);
            appsettings::setAutoInstallUpdates (true);
            expectEquals ((juce::int64) appsettings::getLastUpdateCheck(), (juce::int64) 0, "never checked");
            appsettings::setLastUpdateCheck (1760000000000LL);
            appsettings::setValue ("stagedUpdate", "1.5.1");

            expect (appsettings::loadLastSettings() == nullptr, "no last settings yet");
            juce::XmlElement snap ("LIVELOOPER");
            snap.setAttribute ("tempo", 128.5);
            snap.createNewChildElement ("MIDIMAP")->setAttribute ("entries", 3);
            appsettings::saveLastSettings (snap);

            // the file on disk holds everything (a new PropertiesFile reads it back)
            expect (appsettings::getSettingsFile().existsAsFile(), "written to disk");
            expect (appsettings::getCheckUpdatesOnStart() && appsettings::getAutoInstallUpdates());
            expectEquals ((juce::int64) appsettings::getLastUpdateCheck(), (juce::int64) 1760000000000LL);
            expectEquals (appsettings::getValue ("stagedUpdate"), juce::String ("1.5.1"));
            expectEquals (appsettings::getValue ("missing", "fallback"), juce::String ("fallback"));
            if (auto loaded = appsettings::loadLastSettings())
            {
                expectWithinAbsoluteError (loaded->getDoubleAttribute ("tempo"), 128.5, 1.0e-9);
                expect (loaded->getChildByName ("MIDIMAP") != nullptr);
            }
            else
                expect (false, "last settings loaded");

            // recent sets: most recent first, no duplicates, at most 10, missing files left out
            juce::Array<juce::File> sets;
            for (int i = 0; i < 12; ++i)
            {
                auto f = dir.getChildFile ("set" + juce::String (i) + ".llset");
                f.replaceWithText ("x");
                sets.add (f);
                appsettings::addRecentSet (f);
            }
            auto recent = appsettings::getRecentSets();
            expectEquals (recent.size(), appsettings::kMaxRecentSets);
            expectEquals (recent[0], sets[11].getFullPathName(), "most recent first");
            appsettings::addRecentSet (sets[5]);
            recent = appsettings::getRecentSets();
            expectEquals (recent[0], sets[5].getFullPathName(), "re-used set moves to the top");
            expectEquals (recent.size(), appsettings::kMaxRecentSets, "no duplicate");
            sets[11].deleteFile();
            expect (! appsettings::getRecentSets().contains (sets[11].getFullPathName()), "deleted file left out");
            appsettings::clearRecentSets();
            expect (appsettings::getRecentSets().isEmpty(), "cleared");

            appsettings::setStorageFolderForTests ({});
            expect (! appsettings::getSettingsFile().isAChildOf (dir), "back to the user folder");
            dir.deleteRecursively();
        }

        beginTest ("compareVersions: semantic versions with v prefix and pre-releases");
        {
            using updates::compareVersions;
            expectEquals (compareVersions ("1.5.0", "1.5.0"), 0);
            expectEquals (compareVersions ("v1.5.0", "1.5.0"), 0, "v prefix ignored");
            expectEquals (compareVersions ("1.5.0", "1.4.2"), 1);
            expectEquals (compareVersions ("1.4.2", "1.5.0"), -1);
            expectEquals (compareVersions ("2.0.0", "1.99.99"), 1, "major wins");
            expectEquals (compareVersions ("1.10.0", "1.9.0"), 1, "numeric, not alphabetic");
            expectEquals (compareVersions ("1.5.1", "1.5.0"), 1, "patch");
            expectEquals (compareVersions ("1.5", "1.5.0"), 0, "missing patch = 0");
            expectEquals (compareVersions ("1.5.0-beta.1", "1.5.0"), -1, "pre-release < release");
            expectEquals (compareVersions ("1.5.0", "1.5.0-rc.1"), 1);
            expectEquals (compareVersions ("1.5.0-beta.2", "1.5.0-beta.10"), -1, "numeric pre-release parts");
            expectEquals (compareVersions ("1.5.0-alpha", "1.5.0-beta"), -1, "alphabetic pre-release parts");
            expectEquals (compareVersions ("1.5.0-beta", "1.5.0-beta.1"), -1, "shorter pre-release first");
            expectEquals (compareVersions (" v1.6.0 ", "1.5.9"), 1, "whitespace trimmed");
        }
    }
};

static Settings15Tests settings15Tests;
