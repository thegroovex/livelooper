// v1.7 display (branch v1.7-wip): colour theme, stage mode, interface size, waveforms with the
// overdub layer, Live view (host parameter, Esc, Push SHIFT + MASTER), settings persistence and
// no allocation on the audio thread.

#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "plugin/AppSettings.h"
#include "plugin/push/PushSurface.h"

#include <atomic>
#include <cstdlib>
#include <new>

using namespace livelooper;
using namespace livelooper::params;

//============================================================================== allocation counter
// Counts operator new calls made while 'countHere' is set on the calling thread (the test sets it
// around the audio processing only).
namespace
{
std::atomic<int> allocations { 0 };
thread_local bool countHere = false;
}

void* operator new (std::size_t n)
{
    if (countHere)
        allocations.fetch_add (1, std::memory_order_relaxed);
    if (void* p = std::malloc (n > 0 ? n : 1))
        return p;
    throw std::bad_alloc();
}
void* operator new[] (std::size_t n)
{
    if (countHere)
        allocations.fetch_add (1, std::memory_order_relaxed);
    if (void* p = std::malloc (n > 0 ? n : 1))
        return p;
    throw std::bad_alloc();
}
void operator delete (void* p) noexcept                 { std::free (p); }
void operator delete[] (void* p) noexcept               { std::free (p); }
void operator delete (void* p, std::size_t) noexcept    { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept  { std::free (p); }

namespace
{
struct DisplayTempFolder
{
    juce::File dir { juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("ll_display", "") };
    DisplayTempFolder()  { dir.createDirectory(); appsettings::setStorageFolderForTests (dir); }
    ~DisplayTempFolder() { appsettings::setStorageFolderForTests ({}); dir.deleteRecursively(); theme::apply (false, false); }
};

void setP (LiveLooperProcessor& p, const juce::String& id, float v) { p.getState().getParameter (id)->setValueNotifyingHost (v); }
float getP (LiveLooperProcessor& p, const juce::String& id)         { return p.getState().getParameter (id)->getValue(); }

void fillTone (juce::AudioBuffer<float>& b, float amp, long long& pos)
{
    for (int i = 0; i < b.getNumSamples(); ++i, ++pos)
    {
        const float v = amp * (float) std::sin ((double) pos * 0.03);
        b.setSample (0, i, v);
        b.setSample (1, i, v);
    }
}
} // namespace

class DisplayTests : public juce::UnitTest
{
public:
    DisplayTests() : juce::UnitTest ("v1.7 Display / Live view", "livelooper") {}

    void runTest() override
    {
        beginTest ("Display host parameters 498 - 501 are appended (ids never change)");
        {
            auto procOwner = std::make_unique<LiveLooperProcessor> (false, false);     // heap: 15 processors on the stack overflowed Windows' 1 MB main stack
            auto& proc = *procOwner;
            const auto& ps = proc.getParameters();
            expectEquals (ps.size(), kNumParameters);
            const juce::StringArray ids { "live_view", "stage_mode", "ui_theme", "waveforms" };
            for (int i = 0; i < ids.size(); ++i)
                if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (ps[497 + i]))
                    expectEquals (p->getParameterID(), ids[i], "index " + juce::String (497 + i));
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (ps[496]))
                expectEquals (p->getParameterID(), juce::String ("push_progress"), "the last v1.7 parameter before stays in place");
            expectEquals (englishName (gLiveView), juce::String ("Live View"));
            expectEquals (englishName (gUiTheme), juce::String ("Colour Theme"));
            for (const auto& id : ids)
                expect (! englishName (id).containsIgnoreCase ("push") && ! englishName (id).containsIgnoreCase ("k.o."), "no device names");
            expect (! isTrigger (gLiveView), "Live View is a switch (MIDI: toggle)");
            expectEquals (getP (proc, gUiTheme), 0.0f, "dark is the default");
            expectEquals (getP (proc, gWaveforms), 0.0f, "waveforms are off by default");
        }

        beginTest ("Theme switch: light / dark / stage mode change the surface colours, the displays stay dark");
        {
            DisplayTempFolder tmp;
            auto procOwner = std::make_unique<LiveLooperProcessor> (false, false);
            auto& proc = *procOwner;
            std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
            auto* e = dynamic_cast<LiveLooperEditor*> (ed.get());
            expect (! theme::isLight());
            const auto darkText = colours::text;
            const auto lcd = colours::lcdBack;
            setP (proc, gUiTheme, 1.0f);
            e->applyDisplaySettings();
            expect (theme::isLight());
            expect (colours::text.getPerceivedBrightness() < 0.4f, "dark text on the light theme");
            expect (colours::panel.getPerceivedBrightness() > 0.8f, "light panels");
            expect (colours::lcdBack == lcd, "the LCDs stay dark");
            expect (e->getLookAndFeel().findColour (juce::PopupMenu::backgroundColourId).getPerceivedBrightness() > 0.8f, "menus follow the theme");
            expectEquals (appsettings::getValue ("display.theme"), juce::String ("1"), "remembered on this computer");
            setP (proc, gUiTheme, 0.0f);
            e->applyDisplaySettings();
            expect (! theme::isLight());
            expect (colours::text == darkText);
            // stage mode: larger type (small captions most), brighter secondary text
            const float normal = LooperLookAndFeel::font (10.0f).getHeight();
            const auto dim = colours::textDim;
            setP (proc, gStageMode, 1.0f);
            e->applyDisplaySettings();
            expect (theme::isStage());
            expect (LooperLookAndFeel::font (10.0f).getHeight() > normal * 1.15f, "small captions grow");
            expect (LooperLookAndFeel::font (30.0f).getHeight() < 30.0f * 1.1f, "titles grow a little (layout stays)");
            expect (colours::textDim.getPerceivedBrightness() > dim.getPerceivedBrightness(), "higher contrast");
            setP (proc, gStageMode, 0.0f);
            e->applyDisplaySettings();
            expect (! theme::isStage());
            // waveforms setting
            setP (proc, gWaveforms, 1.0f);
            e->applyDisplaySettings();
            expect (TrackStrip::showWaveforms);
            setP (proc, gWaveforms, 0.0f);
            e->applyDisplaySettings();
            expect (! TrackStrip::showWaveforms);
        }

        beginTest ("Interface size 75 - 200 %, Ctrl / Cmd + and -, remembered");
        {
            DisplayTempFolder tmp;
            {
                auto procOwner = std::make_unique<LiveLooperProcessor> (false, false);
                auto& proc = *procOwner;
                std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
                auto* e = dynamic_cast<LiveLooperEditor*> (ed.get());
                expectEquals (e->getGuiScale(), 100);
                expectEquals (ed->getWidth(), LiveLooperEditor::kBaseWidth);
                e->setGuiScale (150);
                expectEquals (ed->getHeight(), LiveLooperEditor::kBaseHeight * 3 / 2);
                expectEquals (appsettings::getValue ("display.scale"), juce::String ("150"));
                e->keyPressed (juce::KeyPress ('=', juce::ModifierKeys::commandModifier, 0));
                expectEquals (e->getGuiScale(), 200);
                e->keyPressed (juce::KeyPress ('=', juce::ModifierKeys::commandModifier, 0));
                expectEquals (e->getGuiScale(), 200, "200 % is the largest");
                for (int i = 0; i < 4; ++i)         // 150, 125, 100, 75
                    e->keyPressed (juce::KeyPress ('-', juce::ModifierKeys::commandModifier, 0));
                expectEquals (e->getGuiScale(), 75);
                e->keyPressed (juce::KeyPress ('-', juce::ModifierKeys::commandModifier, 0));
                expectEquals (e->getGuiScale(), 75, "75 % is the smallest");
                e->keyPressed (juce::KeyPress ('0', juce::ModifierKeys::commandModifier, 0));
                expectEquals (e->getGuiScale(), 100, "Ctrl / Cmd 0 = 100 %");
                e->setGuiScale (75);
                expectEquals (ed->getWidth(), juce::roundToInt (LiveLooperEditor::kBaseWidth * 0.75f));
                e->setGuiScale (125);
            }
            auto proc2Owner = std::make_unique<LiveLooperProcessor> (false, false);
            auto& proc2 = *proc2Owner;
            std::unique_ptr<juce::AudioProcessorEditor> ed2 (proc2.createEditor());
            expectEquals (dynamic_cast<LiveLooperEditor*> (ed2.get())->getGuiScale(), 125, "a new editor opens at the remembered size");
            expectEquals (ed2->getHeight(), juce::roundToInt (LiveLooperEditor::kBaseHeight * 1.25f));
        }

        beginTest ("Display settings are remembered: a new instance starts with theme, stage mode and waveforms");
        {
            DisplayTempFolder tmp;
            {
                auto procOwner = std::make_unique<LiveLooperProcessor> (false, false);
                auto& proc = *procOwner;
                std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
                auto* e = dynamic_cast<LiveLooperEditor*> (ed.get());
                setP (proc, gUiTheme, 1.0f);
                setP (proc, gStageMode, 1.0f);
                setP (proc, gWaveforms, 1.0f);
                e->applyDisplaySettings();
                expect (e->handleDisplayMenu (1922), "Live view strip off (SETUP > Display > Live view)");
                expect (! e->getLiveView()->getShowFxStrip());
            }
            auto procOwner = std::make_unique<LiveLooperProcessor> (false, false);
            auto& proc = *procOwner;
            expectEquals (getP (proc, gUiTheme), 1.0f);
            expectEquals (getP (proc, gStageMode), 1.0f);
            expectEquals (getP (proc, gWaveforms), 1.0f);
            std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
            auto* e = dynamic_cast<LiveLooperEditor*> (ed.get());
            expect (theme::isLight() && theme::isStage());
            expect (! e->getLiveView()->getShowFxStrip(), "Live view options remembered");
            expect (e->getLiveView()->getShowWaveforms(), "Live view waveforms on by default");
            // SETUP > Display page
            juce::Array<int> toggles;
            auto menu = e->buildSetupMenu (toggles);
            bool display = false;
            for (juce::PopupMenu::MenuItemIterator it (menu); it.next();)
                display = display || (it.getItem().isSectionHeader && it.getItem().text == i18n::tr ("Display"));
            expect (display, "SETUP has a Display page");
            expect (toggles.contains (1910) && toggles.contains (1911) && toggles.contains (1921) && toggles.contains (1922));
        }

        beginTest ("Live view: host parameter, LIVE button, EXIT LIVE and Esc");
        {
            DisplayTempFolder tmp;
            auto procOwner = std::make_unique<LiveLooperProcessor> (false, false);
            auto& proc = *procOwner;
            std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
            auto* e = dynamic_cast<LiveLooperEditor*> (ed.get());
            ed->setSize (LiveLooperEditor::kBaseWidth, LiveLooperEditor::kBaseHeight);
            expect (! e->isLiveViewShown());
            setP (proc, gLiveView, 1.0f);              // host / MIDI
            e->applyDisplaySettings();
            expect (e->isLiveViewShown());
            auto* live = e->getLiveView();
            expectEquals (live->getNumCards(), proc.getVisibleTracks());
            expect (live->getCardArea (0).getHeight() > 500, "large track cards");
            expect (! live->getFxStripArea().isEmpty(), "performance FX strip");
            expect (e->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)), "Esc");
            expect (! e->isLiveViewShown());
            expectEquals (getP (proc, gLiveView), 0.0f, "Esc switches the parameter off");
            e->getLiveButton().triggerClick();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
            expect (e->isLiveViewShown(), "LIVE button");
            live->getExitButton().triggerClick();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
            expect (! e->isLiveViewShown(), "EXIT LIVE");
            // the Live view follows 8 tracks
            proc.setVisibleTracks (8);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            e->setLiveView (true);
            live->refresh (false);
            expectEquals (live->getNumCards(), 8);
            live->setShowFxStrip (false);
            expect (live->getFxStripArea().isEmpty());
            e->setLiveView (false);
        }

        beginTest ("Push 1: SHIFT + MASTER cycles Full -> Live -> Live without FX -> Performance -> Full, MASTER alone still the meters");
        {
            auto procOwner = std::make_unique<LiveLooperProcessor> (false, false);
            auto& proc = *procOwner;
            push::PushSurface surface { proc };
            auto button = [&surface] (int cc, bool down) { surface.handleMidi (juce::MidiMessage::controllerEvent (1, cc, down ? 127 : 0)); };
            auto shiftMaster = [&button] { button (push::cc::shift, true); button (push::cc::master, true); button (push::cc::master, false); button (push::cc::shift, false); };
            expectEquals (proc.getView(), 0);
            shiftMaster();
            expectEquals (getP (proc, gLiveView), 1.0f);
            expectEquals (getP (proc, gLiveFx), 1.0f);
            expectEquals (proc.getView(), 1, "Live");
            expect (! surface.areMetersShown());
            shiftMaster();
            expectEquals (getP (proc, gLiveView), 1.0f);
            expectEquals (getP (proc, gLiveFx), 0.0f);
            expectEquals (proc.getView(), 2, "Live without FX");
            expectEquals (getP (proc, gView), 2.0f / 3.0f);
            shiftMaster();
            expectEquals (proc.getView(), 3, "Performance");
            expectEquals (getP (proc, gLiveView), 1.0f, "the Performance view is a stage view (live_view on)");
            expectEquals (getP (proc, gLiveFx), 0.0f, "the FX bar choice of the Live view stays");
            expectEquals (getP (proc, gView), 1.0f);
            proc.syncViews();
            expectEquals (proc.getView(), 3, "sync keeps the Performance view");
            shiftMaster();
            expectEquals (getP (proc, gLiveView), 0.0f);
            expectEquals (proc.getView(), 0, "back to Full");
            expectEquals (getP (proc, gView), 0.0f);
            shiftMaster();
            expectEquals (getP (proc, gLiveFx), 1.0f, "Live brings the FX bar back");
            button (push::cc::master, true);
            button (push::cc::master, false);
            expect (surface.areMetersShown());
            expectEquals (getP (proc, gLiveView), 1.0f, "MASTER alone does not change the view");
        }

        beginTest ("Views: host parameters 502 - 503 appended (Live View FX Bar, View), names EN / DE");
        {
            auto procOwner = std::make_unique<LiveLooperProcessor> (false, false);
            auto& proc = *procOwner;
            const auto& ps = proc.getParameters();
            expectEquals (ps.size(), 503);
            const juce::StringArray ids { "live_fx", "view" };
            for (int i = 0; i < ids.size(); ++i)
                if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (ps[501 + i]))
                    expectEquals (p->getParameterID(), ids[i], "id " + juce::String (502 + i));
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (ps[500]))
                expectEquals (p->getParameterID(), juce::String ("waveforms"), "501 stays in place");
            expectEquals (englishName (gLiveFx), juce::String ("Live View FX Bar"));
            expectEquals (englishName (gView), juce::String ("View"));
            expect (juce::String (i18n::germanFor ("Live View FX Bar")) == juce::CharPointer_UTF8 ("Live-Ansicht FX-Leiste"));
            expect (juce::String (i18n::germanFor ("View")) == "Ansicht");
            for (const auto& id : ids)
                expect (! englishName (id).containsIgnoreCase ("push") && ! englishName (id).containsIgnoreCase ("k.o."), "no device names");
            expect (! isTrigger (gLiveFx) && ! isTrigger (gView));
            expectEquals (viewChoices().size(), kNumViews);
            expect (viewChoices() == juce::StringArray { "Full", "Live", "Live without FX", "Performance" }, "choice order never changes (Performance appended)");
            if (auto* v = dynamic_cast<juce::AudioParameterChoice*> (proc.getState().getParameter (gView)))
                expectEquals (v->choices.size(), 4);
            expect (juce::String (i18n::germanFor ("Performance")).isNotEmpty() && juce::String (i18n::germanFor ("Performance view")).isNotEmpty());
            expectEquals (getP (proc, gLiveFx), 1.0f, "FX bar on by default");
            expectEquals (getP (proc, gView), 0.0f, "Full by default");
            expectEquals (proc.getMidiMap().getNumParams(), kNumParameters, "both MIDI-learnable");
        }

        beginTest ("Views: selector drives live_view + live_fx, and follows them (LIVE button, Esc, SETUP, MIDI)");
        {
            auto procOwner = std::make_unique<LiveLooperProcessor> (false, false);
            auto& proc = *procOwner;
            auto setView = [&proc] (int v) { setP (proc, gView, (float) v / 3.0f); proc.syncViews(); };
            setView (2);
            expectEquals (getP (proc, gLiveView), 1.0f);
            expectEquals (getP (proc, gLiveFx), 0.0f);
            setView (1);
            expectEquals (getP (proc, gLiveFx), 1.0f);
            setView (0);
            expectEquals (getP (proc, gLiveView), 0.0f);
            expectEquals (getP (proc, gLiveFx), 1.0f, "Full keeps the FX bar choice");
            setP (proc, gLiveFx, 0.0f); proc.syncViews();
            expectEquals (getP (proc, gView), 0.0f, "FX bar off while Full: still Full");
            setP (proc, gLiveView, 1.0f); proc.syncViews();
            expectEquals (proc.getView(), 2);
            expectEquals (getP (proc, gView), 2.0f / 3.0f, "LIVE with the FX bar off = Live without FX");
            setP (proc, gLiveFx, 1.0f); proc.syncViews();
            expectEquals (getP (proc, gView), 1.0f / 3.0f, "FX bar on = Live");
            setP (proc, gLiveView, 0.0f); proc.syncViews();
            expectEquals (getP (proc, gView), 0.0f, "Esc = Full");
            // Performance: live_view on, the FX bar choice untouched; Esc / LIVE off = Full, the FX bar switch = that Live view
            setView (3);
            expectEquals (proc.getView(), 3);
            expectEquals (getP (proc, gLiveView), 1.0f);
            expectEquals (getP (proc, gLiveFx), 1.0f, "Performance keeps the FX bar choice");
            setP (proc, gLiveView, 0.0f); proc.syncViews();
            expectEquals (proc.getView(), 0, "Esc in the Performance view = Full");
            expectEquals (getP (proc, gView), 0.0f);
            setView (3);
            setP (proc, gLiveFx, 0.0f); proc.syncViews();
            expectEquals (proc.getView(), 2, "the FX bar switch in the Performance view = Live without FX");
            setView (3);
            {
                // a saved state in the Performance view comes back in the Performance view
                juce::MemoryBlock mb;
                proc.getStateInformation (mb);
                auto proc2Owner = std::make_unique<LiveLooperProcessor> (false, false);
                auto& proc2 = *proc2Owner;
                proc2.setStateInformation (mb.getData(), (int) mb.getSize());
                proc2.syncViews();
                expectEquals (proc2.getView(), 3, "restored Performance view");
                expectEquals (getP (proc2, gLiveView), 1.0f);
            }
            setView (0);
            // MIDI: a CC on View selects by range, a note steps through the views
            proc.prepareToPlay (48000.0, 256);
            auto send = [&proc] (const juce::MidiMessage& m) {
                juce::AudioBuffer<float> buf (2, 256); buf.clear();
                juce::MidiBuffer midi; midi.addEvent (m, 0);
                proc.processBlock (buf, midi);
                proc.syncViews();
            };
            proc.getMidiMap().assign ({ 0, true, 20 }, proc.getParamIndex (gView));      // one assignment per function
            proc.getMidiMap().assign ({ 0, true, 21 }, proc.getParamIndex (gLiveFx));
            // a CC splits into 4 ranges: 0 - 21 Full, 22 - 63 Live, 64 - 105 Live without FX, 106 - 127 Performance
            const std::pair<int, int> ccs[] = { { 127, 3 }, { 106, 3 }, { 105, 2 }, { 64, 2 }, { 63, 1 }, { 22, 1 }, { 21, 0 }, { 0, 0 }, { 40, 1 } };
            for (const auto& [cc, view] : ccs)
            {
                send (juce::MidiMessage::controllerEvent (1, 20, cc));
                expectEquals (proc.getView(), view, "CC " + juce::String (cc));
            }
            proc.getMidiMap().assign ({ 0, false, 61 }, proc.getParamIndex (gView));
            auto note = [&] { send (juce::MidiMessage::noteOn (1, 61, (juce::uint8) 100)); send (juce::MidiMessage::noteOff (1, 61)); };
            note();
            expectEquals (proc.getView(), 2, "note steps");
            note();
            expectEquals (proc.getView(), 3, "note steps to Performance");
            note();
            expectEquals (proc.getView(), 0, "and wraps to Full");
            note();
            expectEquals (proc.getView(), 1);
            send (juce::MidiMessage::controllerEvent (1, 21, 127));
            expectEquals (proc.getView(), 2, "MIDI on the FX bar toggles it");
            proc.releaseResources();
        }

        beginTest ("Views: the editor follows Live View FX Bar (main + SETUP menu), the old FX strip setting migrates");
        {
            DisplayTempFolder tmp;
            appsettings::setValue ("display.liveFxStrip", "0");
            auto procOwner = std::make_unique<LiveLooperProcessor> (false, false);
            auto& proc = *procOwner;
            expectEquals (getP (proc, gLiveFx), 0.0f, "the setting of 1.7 builds before becomes the parameter");
            std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
            auto* e = dynamic_cast<LiveLooperEditor*> (ed.get());
            ed->setSize (LiveLooperEditor::kBaseWidth, LiveLooperEditor::kBaseHeight);
            setP (proc, gLiveView, 1.0f);
            e->applyDisplaySettings();
            expect (e->getLiveView()->getFxStripArea().isEmpty(), "no FX bar");
            expect (e->handleDisplayMenu (1922));
            expectEquals (getP (proc, gLiveFx), 1.0f, "SETUP toggle = parameter");
            expect (! e->getLiveView()->getFxStripArea().isEmpty(), "FX bar back");
            expect (appsettings::getValue ("display.liveFxStrip") == "1", "remembered on this computer");
            setP (proc, gLiveFx, 0.0f);       // host / MIDI
            e->applyDisplaySettings();
            expect (e->getLiveView()->getFxStripArea().isEmpty());
            e->setLiveView (false);
        }

        beginTest ("Performance view: View = Performance shows it, Esc / EXIT return, every control MIDI-learnable and framed");
        {
            DisplayTempFolder tmp;
            auto procOwner = std::make_unique<LiveLooperProcessor> (false, false);
            auto& proc = *procOwner;
            std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
            auto* e = dynamic_cast<LiveLooperEditor*> (ed.get());
            ed->setSize (LiveLooperEditor::kBaseWidth, LiveLooperEditor::kBaseHeight);
            setP (proc, gView, 1.0f);                   // host / MIDI: Performance
            proc.syncViews();
            e->applyDisplaySettings();
            expect (e->isPerformViewShown() && ! e->isLiveViewShown(), "Performance view instead of the Live view");
            expect (e->isStageViewShown());
            auto* pv = e->getPerformView();
            pv->refresh (false);
            expectEquals (pv->getNumCards(), proc.getVisibleTracks());
            expect (pv->getFxArea().getHeight() > 400, "the FX area is the large part");
            expect (pv->getCardArea (0).getHeight() < 200, "compact track row");
            expect (pv->getCardArea (0).getBottom() < pv->getFxArea().getY());
            expect (pv->getXYPad().getWidth() > 250 && pv->getXYPad().getHeight() > 250, "large XY field");
            for (int i = 0; i < kNumPunchFx; ++i)
                expect (! pv->getPunchPad (i).getBounds().isEmpty() && pv->getFxArea().contains (pv->getPunchPad (i).getBounds()), "punch pad " + juce::String (i + 1));
            // everything inside the view, nothing overlapping the header / track row / FX area borders
            for (auto* c : pv->getChildren())
                if (c->isVisible())
                    expect (pv->getLocalBounds().contains (c->getBounds()), "inside the view: " + getParamId (*c));
            // view switcher: 4 chips, the 4th selected, a click selects Live
            auto& vs = pv->getViewSwitcher();
            expectEquals (vs.getNumChips(), 4);
            expectEquals (vs.getSelectedChip(), 3);
            expectEquals (getParamId (vs), gView);
            vs.selectChip (1);
            e->applyDisplaySettings();
            expect (e->isLiveViewShown() && ! e->isPerformViewShown(), "switcher: Live");
            expectEquals (e->getLiveView()->getViewSwitcher().getSelectedChip(), 1, "the Live view has the switcher too");
            e->getLiveView()->getViewSwitcher().selectChip (3);
            e->applyDisplaySettings();
            expect (e->isPerformViewShown(), "switcher in the Live view: Performance");
            expect (e->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)), "Esc");
            expect (! e->isStageViewShown());
            expectEquals (proc.getView(), 0, "Esc = Full");
            e->setPerformView (true);
            expect (e->isPerformViewShown());
            pv->getExitButton().onPress();             // EXIT (the click calls onPress)
            e->applyDisplaySettings();
            expect (! e->isStageViewShown(), "EXIT");

            // controls map to existing parameters (no new functions)
            e->setPerformView (true);
            expectEquals (getParamId (pv->getTargetChips()), gPfxTarget);
            expectEquals (getParamId (pv->getRepeatRateChips()), gRepeatRate);
            expectEquals (getParamId (pv->getBeatTypeChips()), gBfxType);
            expectEquals (getParamId (pv->getBeatChips()), gBfxBeat);
            expectEquals (getParamId (pv->getRepeatPad()), gRepeatHold);
            expectEquals (getParamId (pv->getBeatFxPad()), gBfxOn);
            expectEquals (getParamId (pv->getExitButton()), gLiveView);
            expectEquals (getParamId (pv->getPunchPad (4)), punchHoldId (4));
            pv->getTargetChips().selectChip (2);
            expectEquals (juce::roundToInt (proc.getState().getRawParameterValue (gPfxTarget)->load()), 2, "TARGET chip = Punch FX Target");
            pv->getBeatChips().selectChip (0);
            expectEquals (juce::roundToInt (proc.getState().getRawParameterValue (gBfxBeat)->load()), 1, "1/16");
            pv->getRepeatRateChips().selectChip (3);
            expectEquals (juce::roundToInt (proc.getState().getRawParameterValue (gRepeatRate)->load()), 3, "1/16");
            setP (proc, gPerfFilter, 0.9f);
            pv->getCenterButton().triggerClick();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
            expectWithinAbsoluteError (proc.getState().getRawParameterValue (gPerfFilter)->load(), 0.0f, 0.001f, "CENTER = filter off");

            // MIDI learn: the overlay frames the Performance view's controls only, every one has a parameter
            e->setLearnMode (true);
            e->applyDisplaySettings (true);
            expect (e->isLearnOverlayInFrontForTests(), "learn frames over the Performance view");
            auto learnables = e->getLearnablesForTests();
            juce::StringArray ids;
            for (auto* c : learnables)
            {
                expect (c == pv || pv->isParentOf (c), "only Performance view controls: " + getParamId (*c));
                ids.addIfNotAlreadyThere (getParamId (*c));
            }
            for (const auto& id : { gView, gTap, gLiveView, gPfxTarget, gPfxX, gPfxY, gPfxAmount, gPfxLatch, gPerfFilter, gBfxOn, gBfxType,
                                    gBfxBeat, gBfxDepth, gBfxHold, gRepeatHold, gStutterHold, gRhythmFill, gRepeatRate, gMaster,
                                    gAllStartStop, gCapture, trackId (0, kRecPlay), trackId (0, kStop), trackId (0, kLevel), trackId (0, kMeasure) })
                expect (ids.contains (id), "framed for MIDI learn: " + id);
            for (int i = 0; i < kNumPunchFx; ++i)
                expect (ids.contains (punchHoldId (i)), "punch pad framed " + juce::String (i + 1));
            for (auto& s : e->getChildren())
                juce::ignoreUnused (s);
            e->setLearnMode (false);
            e->setLiveView (false);
        }

        beginTest ("Performance view: mini waveforms only with the global waveforms setting, 8 tracks, sizes 75 - 200 %, light theme, stage mode");
        {
            DisplayTempFolder tmp;
            auto procOwner = std::make_unique<LiveLooperProcessor> (false, false);
            auto& proc = *procOwner;
            std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
            auto* e = dynamic_cast<LiveLooperEditor*> (ed.get());
            ed->setSize (LiveLooperEditor::kBaseWidth, LiveLooperEditor::kBaseHeight);
            e->setPerformView (true);
            auto* pv = e->getPerformView();
            expectEquals (getP (proc, gWaveforms), 0.0f, "waveforms off by default");
            e->applyDisplaySettings (true);
            expect (! pv->areWaveformsShown(), "no mini waveforms by default");
            setP (proc, gWaveforms, 1.0f);
            e->applyDisplaySettings();
            expect (pv->areWaveformsShown(), "the global setting turns them on");
            setP (proc, gWaveforms, 0.0f);
            e->applyDisplaySettings();
            proc.setVisibleTracks (8);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            pv->refresh (false);
            expectEquals (pv->getNumCards(), 8);
            for (int t = 0; t < 8; ++t)
                expect (pv->getCardArea (t).getWidth() > 100, "card " + juce::String (t + 1));
            for (int scale : { 75, 100, 125, 150, 200 })
            {
                e->setGuiScale (scale);
                pv->refresh (false);
                expect (pv->isVisible() && pv->getFxArea().getHeight() > 400, "size " + juce::String (scale));
                juce::Image img (juce::Image::ARGB, ed->getWidth(), ed->getHeight(), true);
                juce::Graphics g (img);
                ed->paintEntireComponent (g, false);
            }
            e->setGuiScale (100);
            for (int look = 0; look < 3; ++look)
            {
                setP (proc, gUiTheme, look == 1 ? 1.0f : 0.0f);
                setP (proc, gStageMode, look == 2 ? 1.0f : 0.0f);
                e->applyDisplaySettings();
                // software image, Graphics gone before reading: on Windows a native (Direct2D) image only
                // has the pixels once its Graphics is flushed (read too early: transparent, the check failed)
                juce::Image img (juce::Image::ARGB, pv->getWidth(), pv->getHeight(), true, juce::SoftwareImageType());
                {
                    juce::Graphics g (img);
                    pv->paintEntireComponent (g, false);
                }
                expect (img.getPixelAt (pv->getWidth() / 2, 5).getAlpha() == 255, "painted (opaque) look " + juce::String (look));
            }
            setP (proc, gUiTheme, 0.0f);
            setP (proc, gStageMode, 0.0f);
            // SETUP > Display > Live view: open the Performance view fullscreen
            juce::Array<int> toggles;
            juce::PopupMenu menu;
            e->addDisplayMenu (menu, toggles);
            bool found = false;
            for (juce::PopupMenu::MenuItemIterator it (menu, true); it.next();)
                found = found || it.getItem().itemID == 1923;
            expect (found, "SETUP > Display: Open Performance view (fullscreen)");
            e->setLiveView (false);
            proc.setVisibleTracks (kDefaultVisibleTracks);
        }

        beginTest ("Waveform peaks: the last overdub layer is read on the message thread, gone after undo");
        {
            LoopTrack t;
            t.allocate (48000 * 4);
            t.prepare (48000.0);
            std::vector<float> inL (480), inR (480), outL (480), outR (480), fxL (480), fxR (480);
            auto block = [&] (float amp) {
                for (int i = 0; i < 480; ++i)
                    inL[(size_t) i] = inR[(size_t) i] = amp * (float) std::sin ((double) i * 0.05);
                std::fill (outL.begin(), outL.end(), 0.0f);
                std::fill (outR.begin(), outR.end(), 0.0f);
                t.process (inL.data(), inR.data(), outL.data(), outR.data(), fxL.data(), fxR.data(), 480);
            };
            std::array<float, 64> bins {}, layer {};
            expect (! t.getOverdubLayer (layer.data(), 64), "empty track");
            t.startRecording (0.0, 24000.0, 0, RecAction::RecPlay);
            for (int b = 0; b < 100; ++b)
                block (0.5f);
            t.finishRecording (RecAction::RecPlay);
            for (int b = 0; b < 10; ++b)
                block (0.0f);
            expect (t.getWaveform (bins.data(), 64));
            expect (! t.getOverdubLayer (layer.data(), 64), "no overdub yet");
            t.startOverdub();
            for (int b = 0; b < 30; ++b)
                block (0.3f);
            t.endOverdub();
            expect (t.getOverdubLayer (layer.data(), 64), "overdub layer");
            int withLayer = 0;
            for (float v : layer)
                withLayer += v > 0.05f ? 1 : 0;
            expect (withLayer > 5 && withLayer < 64, "the layer covers the overdubbed part only");
            expect (t.undo());
            expect (! t.getOverdubLayer (layer.data(), 64), "undo removes the layer");
            expect (t.redo());
            expect (t.getOverdubLayer (layer.data(), 64), "redo brings it back");
        }

        beginTest ("No allocation on the audio thread while the waveforms / Live view are shown");
        {
            DisplayTempFolder tmp;
            auto procOwner = std::make_unique<LiveLooperProcessor> (false, false);
            auto& proc = *procOwner;
            proc.prepareToPlay (48000.0, 512);
            setP (proc, "quantize", 0.0f);
            setP (proc, "recaction", 1.0f);
            setP (proc, gWaveforms, 1.0f);
            std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
            auto* e = dynamic_cast<LiveLooperEditor*> (ed.get());
            juce::AudioBuffer<float> buf (2, 512);
            juce::MidiBuffer midi;
            long long pos = 0;
            auto run = [&] (int blocks, float amp, bool count) {
                for (int b = 0; b < blocks; ++b)
                {
                    fillTone (buf, amp, pos);
                    countHere = count;
                    proc.processBlock (buf, midi);
                    countHere = false;
                    // the message thread: triggers released, the editor timer draws peaks + layers
                    for (auto* p : proc.getParameters())
                        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
                            if (isTrigger (r->getParameterID()) && r->getValue() >= 0.5f)
                                r->setValueNotifyingHost (0.0f);
                    if (b % 4 == 0)
                        juce::MessageManager::getInstance()->runDispatchLoopUntil (1);
                }
            };
            proc.pressTrigger ("t1_recplay");
            run (100, 0.4f, false);
            proc.pressTrigger ("t1_recplay");     // play
            run (20, 0.0f, false);
            proc.pressTrigger ("t1_recplay");     // overdub
            run (20, 0.3f, false);                // warm up
            e->setLiveView (true);
            allocations = 0;
            run (60, 0.3f, true);                 // overdubbing, waveforms + Live view on screen
            proc.pressTrigger ("t1_recplay");
            run (40, 0.0f, true);                 // playing
            expectEquals (allocations.load(), 0, "processBlock allocates nothing");
            expect (e->isLiveViewShown());
            e->setLiveView (false);
        }
    }
};

static DisplayTests displayTests;
