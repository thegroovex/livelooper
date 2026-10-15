// v1.6 features of the editor (branch v1.6-wip), kept in their own file to ease merges:
// guided tour, first start (demo set + tour), performance recorder button, shared sets,
// demo / empty set, 9:16 clip export, controller templates menu.

#include "PluginEditor.h"
#include "AppSettings.h"
#include "ClipExport.h"
#include "GuidedTour.h"
#include "WelcomeScreen.h"
#include "InfoWindows.h"
#include "controllers/GridControllers.h"

namespace livelooper
{
using namespace params;
using i18n::tr;

//============================================================================== GuidedTour
GuidedTour::GuidedTour (std::vector<Step> s, std::function<void()> closeFn)
    : steps (std::move (s)), onClose (std::move (closeFn))
{
    setInterceptsMouseClicks (true, true);
    for (auto* b : { &nextBtn, &backBtn, &skipBtn })
        addAndMakeVisible (b);
    nextBtn.onClick = [this] { next(); };
    backBtn.onClick = [this] { back(); };
    skipBtn.onClick = [this] { close(); };
    update();
}

void GuidedTour::next()
{
    if (step + 1 >= (int) steps.size())
    {
        close();
        return;
    }
    ++step;
    update();
}

void GuidedTour::back()
{
    if (step > 0)
        --step;
    update();
}

void GuidedTour::close()
{
    setVisible (false);
    if (onClose)
    {
        auto fn = onClose;          // may delete this
        fn();
    }
}

void GuidedTour::update()
{
    if (juce::isPositiveAndBelow (step, (int) steps.size()) && steps[(size_t) step].enter)
        steps[(size_t) step].enter();
    nextBtn.setButtonText (step + 1 >= (int) steps.size() ? "DONE" : "NEXT");
    backBtn.setEnabled (step > 0);
    skipBtn.setVisible (step + 1 < (int) steps.size());
    resized();
    repaint();
}

namespace
{
    /** Width a tour button needs for its (translated) text in one line, at least minW. */
    int tourButtonWidth (juce::TextButton& b, int minW)
    {
        const auto f = b.getLookAndFeel().getTextButtonFont (b, 34);
        return juce::jmax (minW, (int) std::ceil (juce::GlyphArrangement::getStringWidth (f, tr (b.getButtonText()))) + 32);
    }
}

juce::Rectangle<int> GuidedTour::getCardBounds() const
{
    const auto area = steps.empty() || ! steps[(size_t) step].area ? juce::Rectangle<int>() : steps[(size_t) step].area();
    auto& self = const_cast<GuidedTour&> (*this);
    // wide enough for all three buttons in one line in every language
    const int need = tourButtonWidth (self.skipBtn, 110) + tourButtonWidth (self.backBtn, 84) + tourButtonWidth (self.nextBtn, 96) + 16 + 32 + 24;
    const int w = juce::jmin (juce::jmax (460, need), juce::jmax (460, getWidth() - 16)), h = 210;
    // below the highlighted area when there is room, otherwise above it, centred on it
    int y = area.getBottom() + 14;
    if (y + h > getHeight() - 8)
        y = std::max (8, area.getY() - h - 14);
    int x = juce::jlimit (8, std::max (8, getWidth() - w - 8), area.getCentreX() - w / 2);
    return { x, y, w, h };
}

void GuidedTour::resized()
{
    auto card = getCardBounds().reduced (16);
    auto row = card.removeFromBottom (34);
    nextBtn.setBounds (row.removeFromRight (tourButtonWidth (nextBtn, 96)));
    row.removeFromRight (8);
    backBtn.setBounds (row.removeFromRight (tourButtonWidth (backBtn, 84)));
    skipBtn.setBounds (row.removeFromLeft (tourButtonWidth (skipBtn, 110)));
}

void GuidedTour::paint (juce::Graphics& g)
{
    if (steps.empty())
        return;
    const auto& s = steps[(size_t) step];
    const auto area = s.area ? s.area().expanded (6) : juce::Rectangle<int>();

    // dim everything except the highlighted area
    juce::Path dim;
    dim.addRectangle (getLocalBounds().toFloat());
    dim.addRoundedRectangle (area.toFloat(), 10.0f);
    dim.setUsingNonZeroWinding (false);
    g.setColour (juce::Colours::black.withAlpha (0.62f));
    g.fillPath (dim);
    juce::ColourGradient brand (juce::Colour (0xffff7a18), (float) area.getX(), 0.0f, juce::Colour (0xffb14bff), (float) area.getRight(), 0.0f, false);
    brand.addColour (0.5, juce::Colour (0xffff2d6f));
    g.setGradientFill (brand);
    g.drawRoundedRectangle (area.toFloat(), 10.0f, 3.0f);

    // card
    const auto card = getCardBounds().toFloat();
    g.setColour (juce::Colour (0xf0181a1e));
    g.fillRoundedRectangle (card, 12.0f);
    g.setGradientFill (brand);
    g.fillRoundedRectangle (card.getX() + 16.0f, card.getY() + 14.0f, 56.0f, 5.0f, 2.5f);
    g.setColour (colours::textDim);
    g.setFont (LooperLookAndFeel::font (12.0f, true));
    g.drawText (tr ("STEP %1 / %2", juce::String (step + 1), juce::String ((int) steps.size())),
                card.withTrimmedLeft (16).withTrimmedRight (16).withTrimmedTop (8).withHeight (20), juce::Justification::centredRight);
    g.setColour (colours::text);
    g.setFont (LooperLookAndFeel::font (20.0f, true));
    g.drawText (tr (s.title), card.reduced (16).withTrimmedTop (12).withHeight (28), juce::Justification::centredLeft);
    g.setColour (colours::text.withAlpha (0.9f));
    g.setFont (LooperLookAndFeel::font (14.0f));
    g.drawFittedText (tr (s.text), card.reduced (16).withTrimmedTop (46).withTrimmedBottom (44).toNearestInt(),
                      juce::Justification::topLeft, 6, 1.0f);
}

//============================================================================== editor: setup
void LiveLooperEditor::initFeatures16()
{
    auto& st = proc.getState();
    perfRecAtt = std::make_unique<APVTS::ButtonAttachment> (st, gPerfRec, perfRecBtn);
    setParamId (perfRecBtn, gPerfRec);
    perfRecBtn.tooltipFn = [this] {
        return proc.isPerformanceRecording()
                   ? tr ("Recording the livelooper output - click to stop. File: %1", proc.getRecorder().getCurrentFile().getFileName())
                   : tr ("Performance recorder: records everything livelooper plays (master mix) as a WAV file into %1",
                         PerformanceRecorder::getFolder().getFullPathName())
                     + "\n" + tr ("Blinks with the beat while recording. Right-click: MIDI learn.");
    };
    surface.addAndMakeVisible (perfRecBtn);

    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    proc.onRecordingFinished = [safe] (const juce::File& f) {
        if (safe != nullptr)
            safe->showStatus (tr ("RECORDING SAVED: %1", f.getFileName()));
    };
}

void LiveLooperEditor::layoutFeatures16 (int dx)
{
    // round record button: right-aligned under MIDI LEARN / MIDI MAP / SETUP / ? (they end at x 1168 + dx, y 64)
    perfRecBtn.setBounds (1168 + dx - 112, 50, 112, 28);
    if (tour != nullptr)
        tour->setBounds (surface.getLocalBounds());
    if (welcome != nullptr)
        welcome->setBounds (surface.getLocalBounds());
}

void LiveLooperEditor::timerFeatures16()
{
    {
        // the record button blinks with the beat: timeline position while it runs, otherwise the tempo
        const auto& eng = proc.getEngine();
        const double nowMs = juce::Time::getMillisecondCounterHiRes();
        double phase = 0.0;
        if (eng.isTimelineRunning())
        {
            phase = eng.getTimelinePpq();
            recBlinkStartMs = nowMs;
            recBlinkBeat0 = phase;
        }
        else
            phase = recBlinkBeat0 + (nowMs - recBlinkStartMs) / 60000.0 * juce::jmax (20.0, eng.getCurrentBpm());
        perfRecBtn.setToggleState (proc.isPerformanceRecording(), juce::dontSendNotification);
        perfRecBtn.setBeatPhase (phase);
        perfRecBtn.setElapsedSeconds (proc.getPerformanceRecordingSeconds());
    }

    if (proc.takeClipExportRequest())
        exportClip (false);
    if (clipProgress >= 0.0f && clipThread == nullptr)
        clipProgress = -1.0f;
}

//============================================================================== first start + tour
void LiveLooperEditor::handleFirstRun (bool force)
{
    if (! force && (proc.wrapperType == juce::AudioProcessor::wrapperType_Undefined      // tests / screenshot tool
                    || appsettings::getValue ("firstRunDone") == "1"))
        return;
    appsettings::setValue ("firstRunDone", "1");
    showWelcome (true);           // the tour follows the choice
}

//============================================================================== welcome screen
bool LiveLooperEditor::isWelcomeVisible() const noexcept
{
    return welcome != nullptr && welcome->isVisible();
}

bool LiveLooperEditor::currentSetHasLoops() const
{
    for (int t = 0; t < kNumTracks; ++t)
        if (! proc.getEngine().getTrack (t).isEmpty())
            return true;
    return false;
}

void LiveLooperEditor::showWelcome (bool firstStart)
{
    if (tour != nullptr)
        tour->setVisible (false);
    welcome = std::make_unique<WelcomeScreen> (firstStart);
    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    welcome->onDemo  = [safe] { if (safe != nullptr) safe->chooseDemoSet(); };
    welcome->onEmpty = [safe] { if (safe != nullptr) safe->chooseEmptySet(); };
    welcome->onClose = [safe] {
        if (safe != nullptr)
            safe->closeWelcome (safe->welcome != nullptr && safe->welcome->isFirstStart());
    };
    surface.addAndMakeVisible (*welcome);
    welcome->setBounds (surface.getLocalBounds());
    welcome->toFront (false);
}

void LiveLooperEditor::closeWelcome (bool startTourAfter)
{
    if (welcome != nullptr)
        welcome->setVisible (false);
    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    juce::MessageManager::callAsync ([safe] { if (safe != nullptr && safe->welcome != nullptr && ! safe->welcome->isVisible()) safe->welcome.reset(); });
    if (startTourAfter && appsettings::getValue ("tourShown") != "1")
        startTour();
}

void LiveLooperEditor::chooseDemoSet (bool confirm)
{
    const bool first = welcome != nullptr && welcome->isFirstStart();
    auto load = [this, first] {
        proc.loadDemoSet();
        showStatus (tr ("DEMO SET LOADED - INPUT MUTED - PRESS ALL START / STOP"));
        closeWelcome (first);
    };
    if (! confirm || ! currentSetHasLoops())
    {
        load();
        return;
    }
    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon, tr ("Load demo set"),
                                        tr ("The demo set replaces the current set - your loops and settings are lost "
                                            "unless you saved them. Load the demo set?"),
                                        tr ("Load demo set"), tr ("Cancel"), this,
                                        juce::ModalCallbackFunction::create ([safe, load] (int ok) {
                                            if (safe != nullptr && ok != 0)
                                                load();
                                        }));
}

void LiveLooperEditor::chooseEmptySet (bool confirm)
{
    const bool first = welcome != nullptr && welcome->isFirstStart();
    auto clear = [this, first] {
        if (currentSetHasLoops() || ! first)
            proc.loadEmptySet();
        showStatus (tr ("EMPTY SET"));
        closeWelcome (first);
    };
    if (! confirm || ! currentSetHasLoops())
    {
        clear();
        return;
    }
    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon, tr ("Empty set (clear everything)"),
                                        tr ("All loops are cleared and every setting goes back to its default. Continue?"),
                                        tr ("Clear everything"), tr ("Cancel"), this,
                                        juce::ModalCallbackFunction::create ([safe, clear] (int ok) {
                                            if (safe != nullptr && ok != 0)
                                                clear();
                                        }));
}

bool LiveLooperEditor::isTourVisible() const
{
    return tour != nullptr && tour->isVisible();
}

void LiveLooperEditor::startTour()
{
    appsettings::setValue ("tourShown", "1");
    auto stripArea = [this] { return strips[0]->getBounds(); };
    auto sceneArea = [this] {
        juce::Rectangle<int> r = sceneStore.getBounds();
        for (auto& b : sceneBtns)
            r = r.getUnion (b->getBounds());
        return r;
    };
    std::vector<GuidedTour::Step> steps {
        { "RECORD",
          "Press REC/PLAY on a track and play something. Press it again: the loop closes and plays right away. "
          "The demo set already holds three loops - press ALL START / STOP to hear them.",
          stripArea },
        { "OVERDUB",
          "Press REC/PLAY while the loop plays to record on top of it. Press it again to stop overdubbing. "
          "UNDO removes the last layer, REDO brings it back.",
          stripArea },
        { "GROOVE",
          "RHYTHM starts a drum groove in time with your loops. Choose pattern and kit; SWING and FEEL make it feel played - BEATS lets you build your own patterns.",
          [this] { return rhythmArea; } },
        { "BEAT FX",
          "Pick a beat FX and press ON - or hold HOLD for a short effect. The FX channel decides where it works: master, loops, input or one track.",
          [this] { return fxPanelArea; }, [this] { setFxTab (tabBeat); } },
        { "SCENE",
          "Scenes remember which tracks play. Press STORE, then a scene button. Later that button starts exactly those tracks, in time. "
          "Have fun - the tour is always in the help menu (?).",
          sceneArea },
    };
    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    tour = std::make_unique<GuidedTour> (std::move (steps), [safe] {
        if (safe != nullptr)
            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->tour.reset(); });
    });
    surface.addAndMakeVisible (*tour);
    tour->setBounds (surface.getLocalBounds());
    tour->toFront (false);
}

//============================================================================== help menu (help + tour)
void LiveLooperEditor::showHelpMenu()
{
    juce::PopupMenu m;
    m.addItem (1, tr ("Help (every function, searchable) ..."));
    m.addItem (2, tr ("Guided tour (60 seconds)"));
    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&helpBtn), [safe] (int r) {
        if (safe == nullptr)
            return;
        if (r == 1) safe->showHelp();
        if (r == 2) safe->startTour();
    });
}

//============================================================================== SETUP menu block
void LiveLooperEditor::addFeatures16Menu (juce::PopupMenu& menu)
{
    menu.addItem (600, tr ("Save set to share (.livelooper, one file with all audio) ..."));
    menu.addItem (604, tr ("Welcome / demo set ..."));
    menu.addItem (601, tr ("Load demo set"));
    menu.addItem (602, tr ("Empty set (clear everything)"));
    menu.addSeparator();

    juce::PopupMenu rec;
    rec.addItem (610, proc.isPerformanceRecording() ? tr ("Stop performance recording") : tr ("Start performance recording"));
    rec.addItem (611, tr ("Show recordings folder"));
    rec.addItem (612, tr ("Choose recordings folder ..."));
    rec.addSeparator();
    rec.addItem (613, PerformanceRecorder::getFolder().getFullPathName(), false);
    menu.addSubMenu (tr ("Performance recorder"), rec);

    juce::PopupMenu clipMenu;
    clipMenu.addItem (620, tr ("Export the last 20 seconds as 9:16 video ..."), clipThread == nullptr);
    clipMenu.addItem (621, tr ("Export the last performance recording as 9:16 video ..."),
                      clipThread == nullptr && proc.getRecorder().getLastFile().existsAsFile());
    menu.addSubMenu (tr ("Clip export 9:16 (Reels, TikTok, Shorts)"), clipMenu);

    juce::PopupMenu ctl;
    auto* gc = proc.getGridControllers();
    const auto chosen = gc != nullptr ? gc->getSelection() : controllers::Selection::Automatic;
    ctl.addItem (638, tr (juce::String::fromUTF8 ("Experimental \xe2\x80\x93 untested on hardware, feedback welcome")), false);
    ctl.addSeparator();
    ctl.addItem (630, tr ("Automatic (by MIDI port name)"), gc != nullptr, chosen == controllers::Selection::Automatic);
    ctl.addItem (631, tr ("Off"), gc != nullptr, chosen == controllers::Selection::Off);
    for (int i = 0; i < controllers::kNumTemplates; ++i)
        ctl.addItem (640 + i, controllers::templateName ((controllers::TemplateId) i) + " (" + tr ("experimental") + ")", gc != nullptr,
                     chosen == (controllers::Selection) ((int) controllers::Selection::First + i));
    ctl.addSeparator();
    ctl.addItem (639, gc != nullptr ? gc->getStatusText() : tr ("Off"), false);
    menu.addSubMenu (tr ("Controller templates (grid controllers, experimental)"), ctl);
    menu.addItem (603, tr ("Guided tour (60 seconds)"));
    menu.addSeparator();
}

bool LiveLooperEditor::handleFeatures16Menu (int r)
{
    switch (r)
    {
        case 600: saveSharedSet(); return true;
        case 604: showWelcome (false); return true;
        case 601: chooseDemoSet(); return true;
        case 602: chooseEmptySet(); return true;
        case 603: startTour(); return true;
        case 610:
            if (proc.isPerformanceRecording()) proc.stopPerformanceRecording();
            else if (! proc.startPerformanceRecording()) showStatus (tr ("RECORDING FOLDER IS NOT WRITABLE"));
            return true;
        case 611:
            PerformanceRecorder::getFolder().createDirectory();
            PerformanceRecorder::getFolder().startAsProcess();
            return true;
        case 612:
        {
            chooser = std::make_unique<juce::FileChooser> (tr ("Choose recordings folder"), PerformanceRecorder::getFolder());
            juce::Component::SafePointer<LiveLooperEditor> safe (this);
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                  [safe] (const juce::FileChooser& fc) {
                if (safe != nullptr && fc.getResult() != juce::File())
                    PerformanceRecorder::setFolder (fc.getResult());
            });
            return true;
        }
        case 620: exportClip (false); return true;
        case 621: exportClip (true); return true;
        case 630:
        case 631:
            if (auto* gc = proc.getGridControllers())
                gc->setSelection (r == 630 ? controllers::Selection::Automatic : controllers::Selection::Off);
            return true;
        default: break;
    }
    if (r >= 640 && r < 640 + controllers::kNumTemplates)
    {
        if (auto* gc = proc.getGridControllers())
            gc->setSelection ((controllers::Selection) ((int) controllers::Selection::First + r - 640));
        return true;
    }
    return false;
}

//============================================================================== shared sets
void LiveLooperEditor::saveSharedSet()
{
    const auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    chooser = std::make_unique<juce::FileChooser> (tr ("Save set to share"), dir.getChildFile ("my-set.livelooper"), "*.livelooper");
    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe] (const juce::FileChooser& fc) {
        if (safe == nullptr || fc.getResult() == juce::File())
            return;
        const auto file = fc.getResult().withFileExtension ("livelooper");
        const bool ok = safe->proc.saveSetContainer (file);
        if (ok)
            appsettings::addRecentSet (file);
        safe->showStatus (ok ? tr ("SET SAVED: %1", i18n::upper (file.getFileNameWithoutExtension())) : tr ("SET COULD NOT BE SAVED"));
    });
}

//============================================================================== clip export
namespace
{
    class ClipJob : public juce::Thread
    {
    public:
        ClipJob (juce::AudioBuffer<float> a, juce::File t, clip::Options o, std::function<void (clip::Result)> d,
                 std::atomic<float>& p)
            : juce::Thread ("livelooper clip export"), audio (std::move (a)), target (std::move (t)), opt (std::move (o)),
              done (std::move (d)), progress (p) {}
        void run() override
        {
            auto res = clip::exportClip (audio, target, opt, [this] (float p) { progress.store (p); return ! threadShouldExit(); });
            auto fn = done;
            juce::MessageManager::callAsync ([fn, res] { fn (res); });
        }
    private:
        juce::AudioBuffer<float> audio;
        juce::File target;
        clip::Options opt;
        std::function<void (clip::Result)> done;
        std::atomic<float>& progress;
    };
}

void LiveLooperEditor::exportClip (bool fromRecording)
{
    if (clipThread != nullptr)
        return;
    juce::AudioBuffer<float> audio;
    if (fromRecording)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (proc.getRecorder().getLastFile()));
        if (reader == nullptr)
        {
            showStatus (tr ("NO PERFORMANCE RECORDING YET"));
            return;
        }
        const int len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 90.0));
        juce::AudioBuffer<float> raw (2, len);
        reader->read (&raw, 0, len, 0, true, true);
        audio = clip::prepareAudio (raw, reader->sampleRate, 90.0, true);     // up to 90 s from the start
    }
    else
        audio = clip::prepareAudio (proc.getRecorder().getHistory (20.0), proc.getRecorder().getSampleRate(), 20.0, false);

    if (audio.getNumSamples() < 24000)
    {
        showStatus (tr ("NOTHING TO EXPORT YET - PLAY SOMETHING FIRST"));
        return;
    }
    clip::Options opt;
    opt.subtitle = tr ("live loop performance") + " - " + juce::String (juce::roundToInt (proc.getEngine().getCurrentBpm())) + " BPM";
    const auto target = clip::defaultTarget (PerformanceRecorder::getFolder());
    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    clipProgress = 0.0f;
    auto job = std::make_unique<ClipJob> (std::move (audio), target, opt, [safe] (clip::Result res) {
        if (safe == nullptr)
            return;
        if (safe->clipThread != nullptr)
            safe->clipThread->stopThread (2000);
        safe->clipThread.reset();
        if (! res.ok())
            safe->showStatus (tr ("CLIP EXPORT FAILED: %1", tr (res.error)));
        else
        {
            safe->showStatus (res.kind == clip::Result::Kind::Video ? tr ("CLIP SAVED: %1", res.file.getFileName())
                                                                    : tr ("CLIP FRAMES + WAV SAVED (NO VIDEO ENCODER): %1", res.file.getFileName()));
            res.file.revealToUser();
        }
    }, clipProgress);
    clipThread = std::move (job);
    clipThread->startThread();
    showStatus (tr ("EXPORTING 9:16 CLIP ..."));
}
} // namespace livelooper
