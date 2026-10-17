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

        beginTest ("Main window: classic arrangement - FX rows on top, all FX groups visible, no overlaps (5 + 8 tracks, EN + DE)");
        {
            LiveLooperProcessor proc (false, false);
            proc.prepareToPlay (48000.0, 512);
            std::unique_ptr<LiveLooperEditor> ed (dynamic_cast<LiveLooperEditor*> (proc.createEditor()));
            expect (ed != nullptr);
            pump();
            for (auto lang : { i18n::Language::English, i18n::Language::German })
            {
                LangScope l (lang);
                for (int tracks : { kDefaultVisibleTracks, kNumTracks })
                {
                    proc.setVisibleTracks (tracks);
                    pump();
                    const juce::String ctx = juce::String (tracks) + " tracks";
                    // FX groups: above the tracks, inside the window, not overlapping each other
                    juce::Array<juce::Rectangle<int>> groups;
                    for (int gIdx = 0; gIdx < LiveLooperEditor::numFxTabs; ++gIdx)
                        groups.add (ed->getFxGroupArea (gIdx));
                    juce::Component* surface = nullptr;
                    juce::Component* strip0 = nullptr;
                    std::function<void (juce::Component&)> findStrip = [&] (juce::Component& c) {
                        for (auto* ch : c.getChildren())
                        {
                            if (strip0 == nullptr && dynamic_cast<TrackStrip*> (ch) != nullptr)
                                strip0 = ch;
                            findStrip (*ch);
                        }
                    };
                    findStrip (*ed);
                    if (strip0 != nullptr)
                        surface = strip0->getParentComponent();
                    expect (strip0 != nullptr);
                    if (strip0 == nullptr)
                        continue;
                    const int tracksTop = strip0->getY();
                    for (int i = 0; i < groups.size(); ++i)
                    {
                        expect (! groups[i].isEmpty() && groups[i].getBottom() <= tracksTop, ctx + ": FX group " + juce::String (i) + " above the tracks");
                        expect (surface->getLocalBounds().contains (groups[i]), ctx + ": FX group inside the window");
                        for (int j = i + 1; j < groups.size(); ++j)
                            if (! (i == LiveLooperEditor::tabPunch || j == LiveLooperEditor::tabPunch))
                                expect (! groups[i].intersects (groups[j]), ctx + ": FX groups " + juce::String (i) + " / " + juce::String (j));
                    }
                    // right column narrower than a third of the tracks, tracks get the width
                    const auto right = ed->getRightColumnArea();
                    expect (right.getWidth() <= 250, ctx + ": right column " + juce::String (right.getWidth()));
                    expect (right.getY() >= tracksTop - 2, ctx + ": right column beside the tracks");
                    // every visible control on the surface: inside the window; controls of the FX rows do not overlap
                    juce::Array<juce::Component*> fxControls;
                    for (auto* c : surface->getChildren())
                    {
                        if (! c->isVisible() || c->getWidth() >= surface->getWidth() - 10)
                            continue;
                        expect (surface->getLocalBounds().contains (c->getBounds()), ctx + ": inside: " + c->getBounds().toString());
                        if (c->getBottom() <= tracksTop && c->getY() > 88)
                            fxControls.add (c);
                    }
                    expect (fxControls.size() > 40, ctx + ": FX controls visible at once: " + juce::String (fxControls.size()));
                    for (int i = 0; i < fxControls.size(); ++i)
                        for (int j = i + 1; j < fxControls.size(); ++j)
                            expect (! fxControls[i]->getBounds().intersects (fxControls[j]->getBounds()),
                                    ctx + ": overlap " + fxControls[i]->getBounds().toString() + " / " + fxControls[j]->getBounds().toString());
                    // each strip: display, buttons, mixer, big pad, STOP from top to bottom, no overlaps
                    for (auto* c : surface->getChildren())
                        if (auto* st = dynamic_cast<TrackStrip*> (c); st != nullptr && st->isVisible())
                        {
                            juce::Array<juce::Component*> kids;
                            for (auto* k : st->getChildren())
                                if (k->isVisible())
                                {
                                    kids.add (k);
                                    expect (st->getLocalBounds().contains (k->getBounds()), ctx + ": strip child inside");
                                }
                            for (int i = 0; i < kids.size(); ++i)
                                for (int j = i + 1; j < kids.size(); ++j)
                                    expect (! kids[i]->getBounds().intersects (kids[j]->getBounds()), ctx + ": strip overlap");
                            int padY = -1, stopY = -1, revY = -1;
                            for (auto* k : kids)
                            {
                                if (dynamic_cast<RecPlayButton*> (k) != nullptr) padY = k->getY();
                                if (dynamic_cast<StopButton*> (k) != nullptr) stopY = k->getY();
                                if (dynamic_cast<juce::Button*> (k) != nullptr && (revY < 0 || k->getY() < revY)) revY = k->getY();   // REV / 1SHOT / FX row
                            }
                            expect (revY > 0 && padY > revY && stopY > padY, ctx + ": classic strip order");
                            for (auto* k : kids)
                                if (dynamic_cast<RecPlayButton*> (k) != nullptr)
                                    expect (juce::jmin (k->getWidth(), k->getHeight()) >= 130, ctx + ": big pad " + k->getBounds().toString());
                        }
                }
            }
            proc.setVisibleTracks (kDefaultVisibleTracks);
            pump();
            // the focused FX group follows the slot selection; every control stays MIDI-mappable
            ed->setFxTab (LiveLooperEditor::tabBeat);
            expectEquals (ed->getFxTab(), (int) LiveLooperEditor::tabBeat);
            ed->selectFxSlot (false, 1);
            expectEquals (ed->getFxTab(), (int) LiveLooperEditor::tabTrack, "selecting a track FX slot focuses the track FX");
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

        beginTest ("SETUP window: pages, rows, track meters on by default");
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
            expect (TrackStrip::showMeters, "track meters on by default");
            ed->handleSetupResult (450);
            expect (! TrackStrip::showMeters);
            expectEquals (appsettings::getValue ("trackMeters"), juce::String ("0"));
            ed->handleSetupResult (450);
            expect (TrackStrip::showMeters);
            expectEquals (appsettings::getValue ("trackMeters"), juce::String ("1"));
            panel.reset();
        }

        beginTest ("Help: illustrated guide pages with embedded pictures, EN + DE");
        {
            for (auto lang : { i18n::Language::English, i18n::Language::German })
            {
                LangScope l (lang);
                const auto pages = HelpComponent::guidePages();
                expectEquals (pages.size(), 14);           // v1.6: + K.O. II, v1.7: + loop length, Live view, display, latency, controllers
                for (const auto& p : pages)
                {
                    expect (getEmbeddedImage (p.image).isValid(), "picture " + p.image);
                    expect (p.text.isNotEmpty() && p.level == 0);
                }
                expectEquals (pages[10].legend.size(), 22, "Push 1: 22 numbered controls (v1.6: beat flash, set recording)");
                expectEquals (pages[12].legend.size(), 8, "K.O. II: 8 numbered controls");
                expectEquals (pages[11].legend.size(), 10, "Push 1 STEP: 10 numbered controls");
                // v1.7 pages, in order after Link, each with its own picture (German variant when German)
                const char* v17[][2] = { { "Loop length", "help-looplen.png" }, { "Live view", "help-live.png" },
                                         { "Display", "help-display.png" }, { "Latency", "help-latency.png" },
                                         { "Controllers", "help-controllers.png" } };
                for (int i = 0; i < 5; ++i)
                {
                    expectEquals (pages[6 + i].title, i18n::tr (v17[i][0]));
                    expectEquals (pages[6 + i].image, juce::String (v17[i][1]));
                    expect (getEmbeddedImage (juce::String (v17[i][1]).replace (".png", "-de.png")).isValid(), "German picture");
                }
                expect (pages[6].text.contains ("FIXED LENGTH"), "loop length page: Push FIXED LENGTH gesture");
                expect (pages[10].text.contains ("SHIFT + MASTER"), "Push 1 page: SHIFT + MASTER = Live view");
                if (lang == i18n::Language::German)
                {
                    expectEquals (pages[0].title, juce::String ("Willkommen"));
                    expectEquals (pages[6].title, juce::String::fromUTF8 ("Loop-L\xc3\xa4nge"));
                    expectEquals (pages[7].title, juce::String ("Live-Ansicht"));
                    for (const auto& p : pages)          // every line of every page is translated
                        expect (! p.text.contains ("Every track has") && ! p.text.contains ("SETUP > Display")
                                    && ! p.text.contains ("Velocity curves") && ! p.text.contains ("Hold FIXED LENGTH"),
                                "German help page: " + p.title);
                }
                HelpComponent help;
                help.showSectionForTests (6);
                expectEquals (help.getShownSection(), 6);
            }
        }
    }
};

static UiPolish16Tests uiPolish16Tests;
