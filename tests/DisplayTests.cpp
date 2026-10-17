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
            LiveLooperProcessor proc { false, false };
            const auto& ps = proc.getParameters();
            expectEquals (ps.size(), 501);
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
            LiveLooperProcessor proc { false, false };
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
                LiveLooperProcessor proc { false, false };
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
            LiveLooperProcessor proc2 { false, false };
            std::unique_ptr<juce::AudioProcessorEditor> ed2 (proc2.createEditor());
            expectEquals (dynamic_cast<LiveLooperEditor*> (ed2.get())->getGuiScale(), 125, "a new editor opens at the remembered size");
            expectEquals (ed2->getHeight(), juce::roundToInt (LiveLooperEditor::kBaseHeight * 1.25f));
        }

        beginTest ("Display settings are remembered: a new instance starts with theme, stage mode and waveforms");
        {
            DisplayTempFolder tmp;
            {
                LiveLooperProcessor proc { false, false };
                std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
                auto* e = dynamic_cast<LiveLooperEditor*> (ed.get());
                setP (proc, gUiTheme, 1.0f);
                setP (proc, gStageMode, 1.0f);
                setP (proc, gWaveforms, 1.0f);
                e->applyDisplaySettings();
                expect (e->handleDisplayMenu (1922), "Live view strip off (SETUP > Display > Live view)");
                expect (! e->getLiveView()->getShowFxStrip());
            }
            LiveLooperProcessor proc { false, false };
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
            LiveLooperProcessor proc { false, false };
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

        beginTest ("Push 1: SHIFT + MASTER switches the Live view, MASTER alone still the meters");
        {
            LiveLooperProcessor proc { false, false };
            push::PushSurface surface { proc };
            auto button = [&surface] (int cc, bool down) { surface.handleMidi (juce::MidiMessage::controllerEvent (1, cc, down ? 127 : 0)); };
            button (push::cc::shift, true);
            button (push::cc::master, true);
            button (push::cc::master, false);
            button (push::cc::shift, false);
            expectEquals (getP (proc, gLiveView), 1.0f);
            expect (! surface.areMetersShown());
            button (push::cc::master, true);
            button (push::cc::master, false);
            expect (surface.areMetersShown());
            expectEquals (getP (proc, gLiveView), 1.0f);
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
            LiveLooperProcessor proc { false, false };
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
