// v1.6 editor parts for the K.O. II / punch FX / Controller GUI: PUNCH FX tab, header chip, one-time
// question, SETUP > Controllers, Controller GUI (fallback on unplug), help page, German texts.

#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ControllerView.h"
#include "plugin/Localisation.h"
#include "plugin/InfoWindows.h"
#include "plugin/SettingsPanel.h"
#include "plugin/AppSettings.h"
#include "plugin/controllers/Ko2.h"

using namespace livelooper;
using namespace livelooper::params;

namespace
{
struct KoLang
{
    explicit KoLang (i18n::Language l) : previous (i18n::getSetting()) { i18n::setSetting (l, false); }
    ~KoLang() { i18n::setSetting (previous, false); }
    i18n::Language previous;
};

struct KoSettings
{
    juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("livelooper-tests-ko2-" + juce::String (juce::Random::getSystemRandom().nextInt64()));
    KoSettings()  { dir.createDirectory(); appsettings::setStorageFolderForTests (dir); }
    ~KoSettings() { appsettings::setStorageFolderForTests ({}); dir.deleteRecursively(); }
};

void koPump() { juce::MessageManager::getInstance()->runDispatchLoopUntil (30); }
}

class Ko2UiTests : public juce::UnitTest
{
public:
    Ko2UiTests() : juce::UnitTest ("K.O. II / punch FX UI", "livelooper") {}

    void runTest() override
    {
        KoSettings settings;

        beginTest ("FX rows: PUNCH FX panel (12 pads, XY field, amount, target, latch) and SEND FX panel always visible");
        {
            LiveLooperProcessor proc;
            std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
            auto& e = *dynamic_cast<LiveLooperEditor*> (ed.get());
            ed->setSize (LiveLooperEditor::kBaseWidth, LiveLooperEditor::kBaseHeight);
            e.setFxTab (LiveLooperEditor::tabPunch);
            koPump();
            expectEquals (e.getFxTab(), (int) LiveLooperEditor::tabPunch);
            int visibleHolds = 0;
            std::function<void (juce::Component&)> walk = [&] (juce::Component& c) {
                for (auto* ch : c.getChildren())
                {
                    if (auto* b = dynamic_cast<juce::Button*> (ch); b != nullptr && b->isVisible() && b->getButtonText().isNotEmpty())
                        for (auto* n : { "SCATTER", "TAPE STOP", "DECIMATE" })
                            visibleHolds += b->getButtonText() == n ? 1 : 0;
                    walk (*ch);
                }
            };
            walk (*ed);
            expectEquals (visibleHolds, 3, "punch pads visible");
            e.setFxTab (LiveLooperEditor::tabInput);
            visibleHolds = 0;
            walk (*ed);
            expectEquals (visibleHolds, 3, "and still visible with another group focused (no tabs)");
            expect (! e.getFxGroupArea (LiveLooperEditor::tabPunch).isEmpty());
        }

        beginTest ("K.O. II chip only while connected; question; Controller GUI only while a controller is connected");
        {
            LiveLooperProcessor proc;
            std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
            auto& e = *dynamic_cast<LiveLooperEditor*> (ed.get());
            ed->setSize (LiveLooperEditor::kBaseWidth, LiveLooperEditor::kBaseHeight);
            e.updateKo2Ui();
            expect (! e.getKo2Chip().isVisible());
            appsettings::setValue ("gui.layout", "controller");
            e.updateKo2Ui();
            expect (! e.isControllerGuiShown(), "no controller: standard GUI");

            proc.simulateControllers (true, false);
            e.updateKo2Ui();
            expect (e.getKo2Chip().isVisible());
            expect (e.isControllerGuiShown());
            expect (e.getControllerView()->getDevice() == ControllerView::Device::Ko2);
            e.showKo2Toast (true);
            expect (e.isKo2ToastVisible());

            proc.simulateControllers (false, true);
            e.updateKo2Ui();
            expect (! e.getKo2Chip().isVisible());
            expect (e.isControllerGuiShown() && e.getControllerView()->getDevice() == ControllerView::Device::Push);

            proc.simulateControllers (false, false);
            e.updateKo2Ui();
            expect (! e.isControllerGuiShown(), "unplugged: falls back to the standard GUI");
            expectEquals (appsettings::getValue ("gui.layout"), juce::String ("controller"), "the setting stays");
            e.setControllerGui (false);
            expectEquals (appsettings::getValue ("gui.layout"), juce::String ("standard"));
        }

        beginTest ("SETUP > Controllers: K.O. II role, groups, clock; GUI offered only with a controller");
        {
            LiveLooperProcessor proc;
            std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
            auto& e = *dynamic_cast<LiveLooperEditor*> (ed.get());
            juce::Array<int> toggles;
            auto menu = e.buildSetupMenu (toggles);
            auto hasItem = [] (const juce::PopupMenu& m, int id) {
                std::function<bool (const juce::PopupMenu&)> find = [&] (const juce::PopupMenu& mm) {
                    for (juce::PopupMenu::MenuItemIterator it (mm); it.next();)
                    {
                        if (it.getItem().itemID == id) return true;
                        if (it.getItem().subMenu != nullptr && find (*it.getItem().subMenu)) return true;
                    }
                    return false;
                };
                return find (m);
            };
            expect (hasItem (menu, 701) && hasItem (menu, 740) && hasItem (menu, 713));
            expect (! hasItem (menu, 781), "GUI choice hidden without a controller");
            proc.simulateControllers (true, false);
            toggles.clear();
            menu = e.buildSetupMenu (toggles);
            expect (hasItem (menu, 781));
            e.handleSetupResult (701);
            expectEquals (proc.getControllerAudioMode(), 1);
            expectEquals (appsettings::getValue ("ko2.role"), juce::String ("record"));
            e.handleSetupResult (711);       // toggle group B on top of the record default (D)
            expectEquals (proc.getKo2Groups(), ko2::kGroupD | 2);
            e.handleSetupResult (714);
            expectEquals (proc.getKo2Groups(), -1);
            const bool clock = proc.getKo2ClockOut();
            e.handleSetupResult (740);
            expect (proc.getKo2ClockOut() != clock);
            e.handleSetupResult (700);
            expectEquals (proc.getControllerAudioMode(), 0);
            // the settings window has the rows
            auto panel = e.createSettingsPanel ("Controllers");
            auto* sp = dynamic_cast<SettingsPanel*> (panel.get());
            expect (sp != nullptr && sp->getNumRows() > 0);
            proc.simulateControllers (false, false);
        }

        beginTest ("Controller view: Push palette colours, K.O. II page follows the device, click presses a pad");
        {
            expect (ControllerView::pushPadColour (0) != ControllerView::pushPadColour (5));
            expect (ControllerView::pushPadColour (7).getBrightness() < ControllerView::pushPadColour (5).getBrightness());
            LiveLooperProcessor proc;
            proc.prepareToPlay (48000.0, 480);
            proc.simulateControllers (true, true);
            ControllerView v (proc);
            v.setBounds (0, 0, 1180, 856);
            auto* d = proc.getKo2Device();
            d->injectMessage (juce::MidiMessage::noteOn (1, ko2::noteFor (2, 3), (juce::uint8) 90));
            expectEquals (v.getShownKo2Page(), 2);
            expectEquals (proc.getState().getParameter (punchHoldId (3))->getValue(), 1.0f);
            d->injectMessage (juce::MidiMessage::noteOff (1, ko2::noteFor (2, 3)));
            v.setKo2Page (0);
            expectEquals (v.getShownKo2Page(), 0);
            juce::Image img (juce::Image::ARGB, 1180, 856, true);
            juce::Graphics g (img);
            v.paint (g);                         // draws without a crash
            v.setDevice (ControllerView::Device::Push);
            v.paint (g);
            proc.simulateControllers (false, false);
        }

        beginTest ("Help has a K.O. II page with its picture; new texts are German in German");
        {
            KoLang de (i18n::Language::German);
            bool found = false;
            for (const auto& s : HelpComponent::guidePages())
                found = found || (s.title == "K.O. II" && s.image == "help-ko2.png" && ! s.legend.isEmpty());
            expect (found);
            expect (getEmbeddedImage ("help-ko2.png").isValid());
            for (auto* t : { "PUNCH FX", "Tape Stop", "Record K.O. II", "K.O. II connected - how should livelooper use it?",
                             "Send FX Amount", "Punch FX %1 (Hold)", "Encoders: Track colour", "Set recording on",
                             "Group D: '.' + ENTER held 1 s = Controller only / Record K.O. II", "Controller view",
                             "No free track", "Resample K.O. pattern" })
                expect (i18n::germanFor (t).isNotEmpty(), juce::String ("missing German text for: ") + t);
            for (int i = 0; i < kNumPunchFx; ++i)
                expect (i18n::germanFor (getPunchFxName (i)).isNotEmpty(), getPunchFxName (i));
            for (int i = 0; i < (int) SendFxType::NumTypes; ++i)
                expect (i18n::germanFor (getSendFxName ((SendFxType) i)).isNotEmpty(), getSendFxName ((SendFxType) i));
        }
    }
};

static Ko2UiTests ko2UiTests;
