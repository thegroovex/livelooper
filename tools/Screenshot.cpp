// Renders the livelooper editor into PNG files for the documentation.
// usage: livelooper_screenshot <output directory>
// Writes every picture in English (livelooper*.png) and German (livelooper*-de.png).

#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "plugin/Localisation.h"
#include "plugin/KeyMapEditor.h"
#include "plugin/InfoWindows.h"
#include "plugin/Parameters.h"
#include "plugin/PerformanceRecorder.h"
#include "plugin/SettingsPanel.h"
#include "plugin/WelcomeScreen.h"
#include "plugin/ControllerView.h"
#include "plugin/controllers/Ko2.h"
#include "plugin/AppSettings.h"

using namespace livelooper;
using namespace livelooper::params;

namespace
{
void runBlocks (LiveLooperProcessor& proc, int blocks, float input)
{
    // a pulsing tone, so the waveform overview shows something like a played phrase
    static long long pos = 0;
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < 512; ++i, ++pos)
        {
            const double t = (double) pos / 48000.0;
            const double beat = std::fmod (t * 2.0, 1.0);                     // 120 BPM
            const double env = std::exp (-beat * 5.0) * (0.55 + 0.45 * std::sin (t * 1.7));
            const float v = input * (float) (env * std::sin (t * 2.0 * 3.14159265 * 220.0));
            buffer.setSample (0, i, v);
            buffer.setSample (1, i, v);
        }
        proc.processBlock (buffer, midi);
    }
}

void press (LiveLooperProcessor& proc, const juce::String& id)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);   // let earlier triggers reset
    runBlocks (proc, 1, 0.0f);
    proc.pressTrigger (id);
}

void save (juce::Component& c, const juce::File& file, float scale = 1.0f)
{
    auto img = c.createComponentSnapshot (c.getLocalBounds(), true, scale);
    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat png;
    png.writeImageToStream (img, out);
    std::cout << "wrote " << file.getFullPathName() << std::endl;
}
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    i18n::setSetting (i18n::Language::English, false);
    const juce::File outDir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
                                      : juce::File::getCurrentWorkingDirectory());
    outDir.createDirectory();

    LiveLooperProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    auto set = [&proc] (const juce::String& id, float plain) {
        auto* p = proc.getState().getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
    };
    set ("quantize", 0.0f);
    set ("recaction", 1.0f);

    // Track 1: playing loop, track 2: overdubbing, track 3: stopped, track 4: recording
    press (proc, "t1_recplay"); runBlocks (proc, 300, 0.4f);
    press (proc, "t1_recplay"); runBlocks (proc, 60, 0.0f);
    press (proc, "t2_recplay"); runBlocks (proc, 200, 0.3f);
    press (proc, "t2_recplay"); runBlocks (proc, 20, 0.0f);
    press (proc, "t2_recplay"); runBlocks (proc, 70, 0.2f);
    press (proc, "t3_recplay"); runBlocks (proc, 150, 0.3f);
    press (proc, "t3_recplay"); runBlocks (proc, 2, 0.0f);
    press (proc, "t3_stop");    runBlocks (proc, 2, 0.0f);
    press (proc, "t4_recplay"); runBlocks (proc, 90, 0.5f);

    set ("t1_fx", 1.0f);
    set ("t2_reverse", 1.0f);
    set ("inA_on", 1.0f);
    set ("trA_on", 1.0f);
    set ("rhythm_on", 1.0f);
    set ("t3_level", 0.8f);
    set ("t2_pan", -0.3f);
    press (proc, "scene1");     // store a scene: shows its LED in the footer
    runBlocks (proc, 1, 0.0f);
    proc.loadDefaultMidiMap();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    runBlocks (proc, 1, 0.0f);

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    // feed the meters: a few frames with audio running
    for (int f = 0; f < 4; ++f)
    {
        runBlocks (proc, 3, 0.5f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
    }
    editor->setSize (LiveLooperEditor::kBaseWidth, LiveLooperEditor::kBaseHeight);
    auto* ed = dynamic_cast<LiveLooperEditor*> (editor.get());

    for (const auto& [language, suffix] : { std::pair<i18n::Language, const char*> { i18n::Language::English, "" },
                                            std::pair<i18n::Language, const char*> { i18n::Language::German, "-de" } })
    {
        i18n::setSetting (language, false);
        const juce::String sfx (suffix);
        ed->showStatus ({});
        juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
        save (*editor, outDir.getChildFile ("livelooper" + sfx + ".png"));

        // v1.6: the welcome screen (demo set / empty set)
        {
            ed->showWelcome (false);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
            save (*editor, outDir.getChildFile ("livelooper-welcome" + sfx + ".png"));
            if (auto* w = ed->getWelcome())
                w->setVisible (false);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        }
        // v1.6: the SETUP window (System Settings style), two pages
        {
            for (const auto& [page, name] : { std::pair<const char*, const char*> { "Looper", "setup" }, std::pair<const char*, const char*> { "App", "setup-app" },
                                                     std::pair<const char*, const char*> { "Audio", "setup-audio" } /* v1.7: latency */ })
            {
                auto panel = ed->createSettingsPanel();
                if (auto* sp = dynamic_cast<SettingsPanel*> (panel.get()))
                {
                    sp->showPage (i18n::tr (page));
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
                    save (*sp, outDir.getChildFile (juce::String ("livelooper-") + name + sfx + ".png"));
                }
            }
        }

        // v1.6 rhythm: the BEATS page with the Boom Bap kit and a user pattern
        {
            set (gRhythmKit, 6.0f);
            set (gRhythmPattern, (float) (RhythmGenerator::getNumPatterns() - 14));    // Boom Bap
            ed->showBeatsPage (true);
            if (auto* page = ed->getBeatsPage())
            {
                page->loadPreset (RhythmGenerator::getNumPatterns() - 14);
                proc.setUserPatternName (page->getEditSlot(), "Boom Bap Flip");
                page->setResolution (16, 2);
                auto& bank = proc.getUserPatterns();
                bank.setCell (page->getEditSlot(), RhythmGenerator::Snare, 7, 40);
                bank.setCell (page->getEditSlot(), RhythmGenerator::Rim, 14, 96);
                bank.setCell (page->getEditSlot(), RhythmGenerator::HatOpen, 15, 110);
            }
            set (beatRowId (kBeatMute, 6), 1.0f);
            set (gDrumsBfx, 1.0f);
            {
                // an own sample on the snare row (shows its name under the voice)
                juce::AudioBuffer<float> hit (1, 8000);
                for (int i = 0; i < hit.getNumSamples(); ++i)
                    hit.setSample (0, i, 0.5f * std::sin ((float) i * 0.05f) * std::exp (-(float) i / 1500.0f));
                proc.setRowSampleAudio (1, hit, 44100.0, "Vinyl Snare 03", {});
            }
            for (int f = 0; f < 6; ++f)
            {
                runBlocks (proc, 3, 0.0f);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
            }
            save (*editor, outDir.getChildFile ("livelooper-beats" + sfx + ".png"));
            proc.clearRowSample (1);
            ed->showBeatsPage (false);
            set (beatRowId (kBeatMute, 6), 0.0f);
            set (gDrumsBfx, 0.0f);
            set (gRhythmKit, 0.0f);
            set (gRhythmPattern, 1.0f);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        }

        // v1.5 live performance: beat FX, colour FX, beat repeat latch, metronome and Link on
        set (gBfxOn, 1.0f);
        ed->setFxTab (LiveLooperEditor::tabBeat);       // v1.6: the FX panel shows the beat FX
        set (gColorType, 1.0f);
        set (gRepeatLatch, 1.0f);
        set (gMetroOn, 1.0f);
        set (gLinkOn, 1.0f);
        set ("sync", 0.0f);    // Link drives the tempo like in the standalone app
        for (int f = 0; f < 4; ++f)
        {
            runBlocks (proc, 3, 0.6f);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
        }
        save (*editor, outDir.getChildFile ("livelooper-performance" + sfx + ".png"));

        // v1.6 performance recorder: the round record button while it records (light on the beat)
        {
            const auto recDir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("livelooper-screenshot-rec");
            PerformanceRecorder::setFolder (recDir);
            proc.startPerformanceRecording();
            for (int f = 0; f < 8; ++f)
            {
                runBlocks (proc, 40, 0.3f);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
            }
            auto& rec = ed->getPerfRecButton();
            rec.setBeatPhase (0.1);          // light on for the picture
            juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
            save (*editor, outDir.getChildFile ("livelooper-recording" + sfx + ".png"));
            proc.stopPerformanceRecording();
            PerformanceRecorder::setFolder ({});
            recDir.deleteRecursively();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
        }
        set (gBfxOn, 0.0f);
        ed->setFxTab (LiveLooperEditor::tabInput);
        set (gColorType, 0.0f);
        set (gRepeatLatch, 0.0f);
        set (gMetroOn, 0.0f);
        set (gLinkOn, 0.0f);
        set ("sync", 1.0f);
        runBlocks (proc, 2, 0.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);

        // v1.6 punch FX tab (Tape Stop held, send FX reverb)
        {
            ed->setFxTab (LiveLooperEditor::tabPunch);
            set (gSfxType, 2.0f);
            set (gSfxLevel, 0.35f);
            set (punchHoldId (4), 1.0f);
            for (int f = 0; f < 4; ++f)
            {
                runBlocks (proc, 3, 0.4f);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            }
            save (*editor, outDir.getChildFile ("livelooper-punch-fx" + sfx + ".png"));
            set (punchHoldId (4), 0.0f);
            set (gSfxType, 0.0f);
            ed->setFxTab (LiveLooperEditor::tabInput);
            runBlocks (proc, 4, 0.0f);
        }
        // v1.6 K.O. II (simulated): header chip + the one-time question, SETUP > Controllers, Controller GUI
        {
            proc.simulateControllers (true, true);
            ed->updateKo2Ui();
            ed->showKo2Toast (true);
            for (int f = 0; f < 4; ++f)
            {
                runBlocks (proc, 3, 0.3f);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            }
            save (*editor, outDir.getChildFile ("livelooper-ko2" + sfx + ".png"));
            if (auto* t = ed->getKo2Toast())
                t->setVisible (false);
            {
                auto panel = ed->createSettingsPanel();
                if (auto* sp = dynamic_cast<SettingsPanel*> (panel.get()))
                {
                    sp->showPage (i18n::tr ("Controllers"));
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
                    save (*sp, outDir.getChildFile ("livelooper-setup-controllers" + sfx + ".png"));
                    // the whole page (K.O. II audio output, hold to intensify, notes)
                    const auto size = sp->getBounds();
                    sp->setSize (size.getWidth(), 1900);   // 1.7: velocity curves, Push 1 display, MIDI feedback below K.O. II
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
                    save (*sp, outDir.getChildFile ("livelooper-setup-controllers-full" + sfx + ".png"));
                    sp->setBounds (size);
                }
            }
            appsettings::setValue ("gui.layout", "controller");
            appsettings::setValue ("gui.controller", "ko2");
            ed->updateKo2Ui();
            if (auto* cv = ed->getControllerView())
                cv->setKo2Page (0);
            for (int f = 0; f < 4; ++f)
            {
                runBlocks (proc, 3, 0.3f);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            }
            save (*editor, outDir.getChildFile ("livelooper-controller-ko2" + sfx + ".png"));
            // hold to intensify: a punch pad (group C) held for about a beat - live amount on the display
            // and in the PUNCH FX title row of the standard surface
            if (auto* dev = proc.getKo2Device())
            {
                const int note = ko2::noteFor (2, ko2::padNumberOffset (2));
                dev->injectMessage (juce::MidiMessage::noteOn (1, note, (juce::uint8) 50));
                if (auto* cv = ed->getControllerView())
                    cv->setKo2Page (2);
                for (int f = 0; f < 10; ++f)
                {
                    runBlocks (proc, 2, 0.3f);
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
                }
                ed->updateKo2Ui();
                juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
                save (*editor, outDir.getChildFile ("livelooper-ko2-hold" + sfx + ".png"));
                appsettings::setValue ("gui.layout", "standard");
                ed->updateKo2Ui();
                for (int f = 0; f < 3; ++f)
                {
                    runBlocks (proc, 2, 0.3f);
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
                }
                ed->updateKo2Ui();
                juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
                save (*editor, outDir.getChildFile ("livelooper-punch-hold" + sfx + ".png"));
                dev->injectMessage (juce::MidiMessage::noteOff (1, note));
                appsettings::setValue ("gui.layout", "controller");
                if (auto* cv = ed->getControllerView())
                    cv->setKo2Page (0);
                ed->updateKo2Ui();
                juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
            }
            appsettings::setValue ("gui.controller", "push");
            ed->updateKo2Ui();
            // v1.7: two loops playing (progress bars on display line 3), a held punch FX (amount bar on line 4)
            const bool p1 = proc.getEngine().getTrack (0).isRunning(), p2 = proc.getEngine().getTrack (1).isRunning();
            if (! p1) press (proc, "t1_recplay");
            if (! p2) press (proc, "t2_recplay");
            runBlocks (proc, 40, 0.0f);
            set ("pfx_amount", 0.6f);
            proc.getState().getParameter (punchHoldId (4))->setValueNotifyingHost (1.0f);
            for (int f = 0; f < 4; ++f)
            {
                runBlocks (proc, 3, 0.3f);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
            }
            save (*editor, outDir.getChildFile ("livelooper-controller-push" + sfx + ".png"));
            proc.getState().getParameter (punchHoldId (4))->setValueNotifyingHost (0.0f);
            if (! p1) press (proc, "t1_stop");
            if (! p2) press (proc, "t2_stop");
            runBlocks (proc, 2, 0.0f);
            appsettings::setValue ("gui.layout", "standard");
            appsettings::setValue ("gui.controller", "ko2");
            if (auto* cv = ed->getControllerView())
                cv->setKo2Page (-1);
            proc.simulateControllers (false, false);
            ed->updateKo2Ui();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
        }

        // in-app help (searchable user guide) and the about window
        {
            HelpComponent help;
            help.setLookAndFeel (&editor->getLookAndFeel());
            // v1.6: the illustrated guide - Push 1 layout and the Loops page
            const auto& secs = help.getSections();
            auto find = [&secs] (const juce::String& title) { for (int i = 0; i < secs.size(); ++i) if (secs[i].title == title) return i; return 0; };
            help.showSectionForTests (find (i18n::tr ("Push 1")));
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            save (help, outDir.getChildFile ("livelooper-help" + sfx + ".png"));
            help.showSectionForTests (find (i18n::tr ("Loops")));
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            save (help, outDir.getChildFile ("livelooper-help-loops" + sfx + ".png"));
            help.showSectionForTests (find (i18n::tr ("K.O. II")));          // v1.6
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            save (help, outDir.getChildFile ("livelooper-help-ko2" + sfx + ".png"));
            help.setQuery ("Link");
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            save (help, outDir.getChildFile ("livelooper-help-search" + sfx + ".png"));
            help.setLookAndFeel (nullptr);
        }
        {
            AboutComponent about (false, false);
            about.setLookAndFeel (&editor->getLookAndFeel());
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            save (about, outDir.getChildFile ("livelooper-about" + sfx + ".png"));
            about.setLookAndFeel (nullptr);
        }

        ed->setLearnMode (true);
        proc.startMidiLearn ("t5_recplay");
        juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
        save (*editor, outDir.getChildFile ("livelooper-midi-learn" + sfx + ".png"));
        proc.cancelMidiLearn();
        ed->setLearnMode (false);

        // typed-in tempo on the display
        ed->showTempoEntry();
        ed->getTempoEntry().setText ("128");
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        save (*editor, outDir.getChildFile ("livelooper-tempo-entry" + sfx + ".png"));
        ed->getTempoEntry().onEscapeKey();

        // keyboard mapping window (standalone app)
        {
            KeyMapEditor keys (proc);
            keys.startCapture (keys.getActions().indexOf ("t2_reverse"));
            save (keys, outDir.getChildFile ("livelooper-keyboard" + sfx + ".png"), 2.0f);
        }

        // track setup panel on its own
        TrackSettingsPanel panel (proc, *ed, 0);
        panel.setLookAndFeel (&editor->getLookAndFeel());
        save (panel, outDir.getChildFile ("livelooper-track-setup" + sfx + ".png"), 2.0f);
        panel.setLookAndFeel (nullptr);
    }

    // v1.7 display: light theme, stage mode, waveforms, Live view, SETUP > Display (EN + DE)
    {
        auto setParam = [&proc] (const juce::String& id, float normalised) {
            proc.getState().getParameter (id)->setValueNotifyingHost (normalised);
        };
        auto frames = [&proc] (int n) {
            for (int f = 0; f < n; ++f)
            {
                runBlocks (proc, 3, 0.4f);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
            }
        };
        // an overdub layer on track 1 for the waveform pictures (track 2 keeps overdubbing)
        press (proc, "t1_recplay"); runBlocks (proc, 120, 0.25f);
        press (proc, "t1_recplay"); runBlocks (proc, 30, 0.0f);
        for (const auto& [language, suffix] : { std::pair<i18n::Language, const char*> { i18n::Language::English, "" },
                                                std::pair<i18n::Language, const char*> { i18n::Language::German, "-de" } })
        {
            i18n::setSetting (language, false);
            const juce::String sfx (suffix);
            ed->showStatus ({});
            setParam (gUiTheme, 1.0f);
            frames (6);
            save (*editor, outDir.getChildFile ("livelooper-light" + sfx + ".png"));
            {
                auto panel = ed->createSettingsPanel();
                if (auto* sp = dynamic_cast<SettingsPanel*> (panel.get()))
                {
                    sp->showPage (i18n::tr ("Display"));
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
                    save (*sp, outDir.getChildFile ("livelooper-setup-display-light" + sfx + ".png"));
                }
            }
            setParam (gUiTheme, 0.0f);
            setParam (gStageMode, 1.0f);
            frames (6);
            save (*editor, outDir.getChildFile ("livelooper-stage" + sfx + ".png"));
            setParam (gStageMode, 0.0f);
            setParam (gWaveforms, 1.0f);
            frames (6);
            save (*editor, outDir.getChildFile ("livelooper-waveforms" + sfx + ".png"));
            save (*editor, outDir.getChildFile ("livelooper-waveforms-2x" + sfx + ".png"), 2.0f);
            setParam (gWaveforms, 0.0f);
            setParam (gPfxAmount, 0.62f);
            setParam (gLiveView, 1.0f);
            frames (6);
            save (*editor, outDir.getChildFile ("livelooper-live" + sfx + ".png"));
            setParam (gStageMode, 1.0f);
            frames (6);
            save (*editor, outDir.getChildFile ("livelooper-live-stage" + sfx + ".png"));
            setParam (gStageMode, 0.0f);
            setParam (gUiTheme, 1.0f);
            frames (6);
            save (*editor, outDir.getChildFile ("livelooper-live-light" + sfx + ".png"));
            setParam (gUiTheme, 0.0f);
            setParam (gLiveView, 0.0f);
            frames (4);
            {
                auto panel = ed->createSettingsPanel();
                if (auto* sp = dynamic_cast<SettingsPanel*> (panel.get()))
                {
                    sp->showPage (i18n::tr ("Display"));
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
                    save (*sp, outDir.getChildFile ("livelooper-setup-display" + sfx + ".png"));
                }
            }
        }
        // interface size 75 % (rendered at its real size)
        ed->setGuiScale (75);
        frames (3);
        ed->showStatus ({});
        frames (2);
        save (*editor, outDir.getChildFile ("livelooper-size-75.png"));
        ed->setGuiScale (100);
        ed->showStatus ({});
        frames (2);
        i18n::setSetting (i18n::Language::English, false);
    }

    // optional 8 tracks
    editor.reset();
    proc.setVisibleTracks (8);
    press (proc, "t6_recplay"); runBlocks (proc, 120, 0.35f);
    press (proc, "t6_recplay"); runBlocks (proc, 2, 0.0f);
    press (proc, "t8_recplay"); runBlocks (proc, 60, 0.45f);
    editor.reset (proc.createEditor());
    for (int f = 0; f < 4; ++f)
    {
        runBlocks (proc, 3, 0.5f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
    }
    for (const auto& [language, suffix] : { std::pair<i18n::Language, const char*> { i18n::Language::English, "" },
                                            std::pair<i18n::Language, const char*> { i18n::Language::German, "-de" } })
    {
        i18n::setSetting (language, false);
        dynamic_cast<LiveLooperEditor*> (editor.get())->showStatus ({});
        juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
        save (*editor, outDir.getChildFile ("livelooper-8-tracks" + juce::String (suffix) + ".png"));
    }
    i18n::setSetting (i18n::Language::English, false);
    return 0;
}
