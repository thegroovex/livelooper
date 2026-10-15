// v1.6 UI polish: tour buttons, FX tabs, SETUP window, illustrated help, BEATS header.

#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "plugin/Localisation.h"
#include "plugin/GuidedTour.h"
#include "plugin/InfoWindows.h"
#include "plugin/SettingsPanel.h"
#include "plugin/AppSettings.h"
#include "plugin/Parameters.h"

using namespace livelooper;
using namespace livelooper::params;

namespace
{
struct LangScope
{
    explicit LangScope (i18n::Language l) : previous (i18n::getSetting()) { i18n::setSetting (l, false); }
    ~LangScope() { i18n::setSetting (previous, false); }
    i18n::Language previous;
};

struct TempSettings
{
    juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("livelooper-tests-ui-" + juce::String (juce::Random::getSystemRandom().nextInt64()));
    TempSettings()  { dir.createDirectory(); appsettings::setStorageFolderForTests (dir); }
    ~TempSettings() { appsettings::setStorageFolderForTests ({}); dir.deleteRecursively(); }
};

void pump() { juce::MessageManager::getInstance()->runDispatchLoopUntil (30); }
}

class UiPolish16Tests : public juce::UnitTest
{
public:
    UiPolish16Tests() : juce::UnitTest ("1.6 UI polish", "livelooper") {}

    void runTest() override
    {
        TempSettings settings;

        beginTest ("Tour: SKIP TOUR fits on one line in every language");
        {
            for (auto lang : { i18n::Language::English, i18n::Language::German })
            {
                LangScope l (lang);
                std::vector<GuidedTour::Step> steps { { "A", "a", [] { return juce::Rectangle<int> (20, 20, 100, 100); } },
                                                      { "B", "b", [] { return juce::Rectangle<int> (20, 20, 100, 100); } } };
                GuidedTour tour (steps, [] {});
                LooperLookAndFeel lf;
                tour.setLookAndFeel (&lf);
                tour.setBounds (0, 0, 1180, 856);
                auto& skip = tour.getSkipButton();
                const auto f = lf.getTextButtonFont (dynamic_cast<juce::TextButton&> (skip), skip.getHeight());
                const float textW = juce::GlyphArrangement::getStringWidth (f, i18n::tr ("SKIP TOUR"));
                expect ((float) skip.getWidth() - 8.0f >= textW, "skip button " + juce::String (skip.getWidth()) + " px for " + juce::String (textW) + " px text");
                expect (tour.getCardBounds().contains (skip.getBounds()) && tour.getCardBounds().contains (tour.getNextButton().getBounds()));
                expect (! skip.getBounds().intersects (tour.getNextButton().getBounds()));
                tour.setLookAndFeel (nullptr);
            }
        }

        beginTest ("FX panel: one group at a time, LED tabs, slot selection follows the tab");
        {
            LiveLooperProcessor proc (false, false);
            proc.prepareToPlay (48000.0, 512);
            std::unique_ptr<LiveLooperEditor> ed (dynamic_cast<LiveLooperEditor*> (proc.createEditor()));
            expect (ed != nullptr);
            pump();
            expectEquals (ed->getFxTab(), (int) LiveLooperEditor::tabInput);
            ed->setFxTab (LiveLooperEditor::tabBeat);
            expectEquals (ed->getFxTab(), (int) LiveLooperEditor::tabBeat);
            ed->selectFxSlot (false, 2);
            expectEquals (ed->getFxTab(), (int) LiveLooperEditor::tabBeat, "a slot selection does not leave the beat FX tab");
            ed->setFxTab (LiveLooperEditor::tabInput);
            ed->selectFxSlot (false, 1);
            expectEquals (ed->getFxTab(), (int) LiveLooperEditor::tabTrack, "selecting a track FX slot shows the track FX");
            // every control of every tab stays MIDI-mappable: their parameter ids are set
            int mappable = 0;
            std::function<void (juce::Component&)> walk = [&] (juce::Component& c) {
                for (auto* ch : c.getChildren())
                {
                    if (getParamId (*ch).isNotEmpty())
                        ++mappable;
                    walk (*ch);
                }
            };
            walk (*ed);
            expect (mappable > 100, "mappable controls: " + juce::String (mappable));
        }

        beginTest ("SETUP window: pages, rows, track meters off by default");
        {
            LiveLooperProcessor proc (false, false);
            proc.prepareToPlay (48000.0, 512);
            std::unique_ptr<LiveLooperEditor> ed (dynamic_cast<LiveLooperEditor*> (proc.createEditor()));
            auto panel = ed->createSettingsPanel();
            auto* sp = dynamic_cast<SettingsPanel*> (panel.get());
            expect (sp != nullptr);
            panel->setSize (820, 600);
            const auto pages = sp->getPageNames();
            for (auto name : { "Sets", "Looper", "Rhythm", "Recording", "Controllers", "Sync", "App" })
                expect (pages.contains (i18n::tr (name)), juce::String ("page ") + name);
            for (int i = 0; i < pages.size(); ++i)
            {
                sp->showPage (i);
                expect (sp->getNumRows() > 0, "rows on " + pages[i]);
            }
            expect (! TrackStrip::showMeters, "track meters off by default");
            ed->handleSetupResult (450);
            expect (TrackStrip::showMeters);
            expectEquals (appsettings::getValue ("trackMeters"), juce::String ("1"));
            ed->handleSetupResult (450);
            expect (! TrackStrip::showMeters);
            panel.reset();
        }

        beginTest ("Help: illustrated guide pages with embedded pictures, EN + DE");
        {
            for (auto lang : { i18n::Language::English, i18n::Language::German })
            {
                LangScope l (lang);
                const auto pages = HelpComponent::guidePages();
                expectEquals (pages.size(), 9);            // v1.6: + K.O. II
                for (const auto& p : pages)
                {
                    expect (getEmbeddedImage (p.image).isValid(), "picture " + p.image);
                    expect (p.text.isNotEmpty() && p.level == 0);
                }
                expectEquals (pages[6].legend.size(), 22, "Push 1: 22 numbered controls (v1.6: beat flash, set recording)");
                expectEquals (pages[8].legend.size(), 8, "K.O. II: 8 numbered controls");
                expectEquals (pages[7].legend.size(), 10, "Push 1 STEP: 10 numbered controls");
                if (lang == i18n::Language::German)
                    expectEquals (pages[0].title, juce::String ("Willkommen"));
                HelpComponent help;
                help.showSectionForTests (6);
                expectEquals (help.getShownSection(), 6);
            }
        }
    }
};

static UiPolish16Tests uiPolish16Tests;
