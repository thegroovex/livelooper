// v1.7 (branch v1.7-wip): SETUP > Display - colour theme, interface size, stage mode, waveforms, Live view.
// Menu ids 1900 - 1949.
#include "PluginEditor.h"
#include "AppSettings.h"
#include "MouseLogNative.h"
#include <typeinfo>

namespace livelooper
{
using namespace params;
using i18n::tr;

namespace
{
    constexpr const char* kScaleKey = "display.scale";
    constexpr const char* kLiveWavesKey = "display.liveWaveforms";
    constexpr const char* kLiveFxKey = "display.liveFxStrip";
    constexpr const char* kViewsFullscreenKey = "display.viewsFullscreen";

    bool paramOn (LiveLooperProcessor& proc, const juce::String& id)
    {
        return proc.getState().getRawParameterValue (id)->load() >= 0.5f;
    }

    void setParam (LiveLooperProcessor& proc, const juce::String& id, float normalised)
    {
        if (auto* p = proc.getState().getParameter (id))
            if (std::abs (p->getValue() - normalised) > 1.0e-4f)
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (normalised);
                p->endChangeGesture();
            }
    }

    /** v1.7: the fullscreen Live / Performance view window: a borderless window over the whole screen, the
        view scaled to its height. It is a MidiMenuHost so right-click MIDI learn on its controls (pads, STOP,
        level, loop length pills, FX controls) reaches the editor like on the surface. */
    std::optional<juce::BorderSize<int>> safeInsetsOverride;     // tests

    /** v1.7: the fullscreen Live / Performance view window: a borderless window over the whole screen, the
        view scaled to its height. It is a MidiMenuHost so right-click MIDI learn on its controls (pads, STOP,
        level, loop length pills, FX controls) reaches the editor like on the surface.
        macOS: the view is laid out inside the display's safe area (below the camera housing / notch of
        MacBook Pro displays); the strip above it is filled with the view's background. */
    template <class View>
    class StageWindow : public juce::Component, public MidiMenuHost
    {
    public:
        StageWindow (LiveLooperProcessor& p, std::function<TrackStrip*(int)> strips, juce::LookAndFeel& lf, MidiMenuHost& h,
                     const juce::Displays::Display* display)
            : view (p, std::move (strips)), host (h)
        {
            setLookAndFeel (&lf);
            setOpaque (true);
            addAndMakeVisible (view);
            view.setFullscreenMode (true);      // v1.7.1: smaller displays, REVERSE / ONE SHOT / PITCH per card
            // a clicked button must not take the keys from the view (Space would press it instead of ALL START / STOP)
            std::function<void (juce::Component&)> walk = [&walk] (juce::Component& parent) {
                for (auto* c : parent.getChildren())
                {
                    c->setWantsKeyboardFocus (false);
                    walk (*c);
                }
            };
            walk (view);
            insets = LiveLooperEditor::getFullscreenSafeInsets (display);
            const auto area = display != nullptr ? display->totalArea : juce::Rectangle<int> (0, 0, 1920, 1080);
            setBounds (area);
        }
        ~StageWindow() override { setLookAndFeel (nullptr); }
        void resized() override
        {
            // designed at 856 px height: scale to the safe area, the width follows its aspect ratio
            const auto safe = insets.subtractedFrom (getLocalBounds());
            const float sc = juce::jmax (0.5f, (float) safe.getHeight() / (float) LiveLooperEditor::kBaseHeight);
            view.setBounds (0, 0, juce::roundToInt ((float) safe.getWidth() / sc), LiveLooperEditor::kBaseHeight);
            view.setTransform (juce::AffineTransform::scale (sc).translated ((float) safe.getX(), (float) safe.getY()));
        }
        void paint (juce::Graphics& g) override
        {
            // the strip beside the safe area (notch) in the view's background colour
            const bool light = theme::isLight(), stage = theme::isStage();
            g.fillAll (stage && ! light ? juce::Colour (0xff050506) : theme::pick (0xff1b1b21, 0xfff1f2f6));
        }
        // v1.7.1: the computer keyboard works in the fullscreen views like on the surface (the view only takes Esc;
        // every other key bubbles up to here and goes to the editor - incl. the held clear / emptying undo keys)
        bool keyPressed (const juce::KeyPress& key) override { return onKey != nullptr && onKey (key); }
        bool keyStateChanged (bool isKeyDown) override { return onKeyState != nullptr && onKeyState (isKeyDown); }
        std::function<bool (const juce::KeyPress&)> onKey;
        std::function<bool (bool)> onKeyState;
        void showMidiMenuFor (juce::Component& c) override { host.showMidiMenuFor (c); }
        void exportTrack (int t) override { host.exportTrack (t); }
        void importTrack (int t) override { host.importTrack (t); }
        void showTrackSettings (int t, juce::Component& anchor) override { host.showTrackSettings (t, anchor); }
        View view;
        MidiMenuHost& host;
        juce::BorderSize<int> insets;
    };

    /** A mouse button is held on any mouse source (JUCE's view of it - the same state the click is dispatched with). */
    bool anyMouseButtonDown()
    {
        auto& desktop = juce::Desktop::getInstance();
        for (int i = 0; i < desktop.getNumMouseSources(); ++i)
            if (auto* m = desktop.getMouseSource (i); m != nullptr && m->isDragging())
                return true;
        return false;
    }

    bool performanceSelected (LiveLooperProcessor& proc)
    {
        return paramOn (proc, gLiveView) && juce::roundToInt (proc.getState().getRawParameterValue (gView)->load()) == kPerformanceView;
    }
}

//============================================================================== setup
void LiveLooperEditor::initDisplay()
{
    liveBtn = std::make_unique<LiveButton>();
    liveBtn->setTooltip (tr ("Live view: large track pads for the stage (Esc returns)"));
    liveBtn->onClick = [this] { setLiveView (! isLiveViewShown()); };
    liveBtn->setWantsKeyboardFocus (false);
    setParamId (*liveBtn, gLiveView);
    surface.addAndMakeVisible (*liveBtn);
    // v1.7 views: the view switcher in the header of the full surface too (under the logo, like in the Live view)
    viewChips = LiveView::makeViewSwitcher (proc);
    viewChips->onSelect = [this] (int v) { selectViewFromSwitcher (v); };
    viewChips->setWantsKeyboardFocus (false);
    surface.addAndMakeVisible (*viewChips);

    liveView = std::make_unique<LiveView> (proc, [this] (int t) { return juce::isPositiveAndBelow (t, kNumTracks) ? strips[(size_t) t].get() : nullptr; });
    wireStageView (*liveView);
    liveView->setShowWaveforms (appsettings::getValue (kLiveWavesKey) != "0");
    liveView->setShowFxStrip (paramOn (proc, gLiveFx));        // v1.7: host parameter "Live View FX Bar"
    surface.addChildComponent (*liveView);
    perfView = std::make_unique<PerformView> (proc, [this] (int t) { return juce::isPositiveAndBelow (t, kNumTracks) ? strips[(size_t) t].get() : nullptr; });
    wireStageView (*perfView);
    surface.addChildComponent (*perfView);

    // v1.7 mouse diagnostics: only with the environment variable LIVELOOPER_MOUSE_LOG=1 (developer note, docs DEVELOPMENT.md).
    // The SETUP > Display switch of the 1.7 test builds is gone; a saved "on" is reset, the log is no longer written.
    if (appsettings::getValue ("debug.mouseLog") == "1")
        appsettings::setValue ("debug.mouseLog", "0");
    if (juce::SystemStats::getEnvironmentVariable ("LIVELOOPER_MOUSE_LOG", {}) == "1")
        setMouseLogEnabled (true);
    viewsFullscreen = appsettings::getValue (kViewsFullscreenKey) == "1";

    guiScale = appsettings::getValue (kScaleKey, "100").getIntValue();
    if (std::find (std::begin (kGuiScales), std::end (kGuiScales), guiScale) == std::end (kGuiScales))
        guiScale = 100;
    applyDisplaySettings (true);
}

void LiveLooperEditor::layoutDisplay (int dx)
{
    juce::ignoreUnused (dx);
    // the LIVE pill sits right of the coloured logo dots (same geometry as the logo in renderSurfaceCache)
    const float tw = juce::GlyphArrangement::getStringWidth (LooperLookAndFeel::font (34.0f, true), "livelooper");
    const int x = juce::roundToInt (22.0f + tw + 12.0f + 4.0f * 16.0f + 9.0f) + 12;
    const int w = 46;
    liveBtn->setBounds (juce::jmin (x, lcd.getX() - 6 - w), 25, w, 22);
    // the view switcher under the tagline (header 0 - 88, the FX rows start at 94)
    viewChips->setBounds (22, 66, juce::jmin (296, lcd.getX() - 12 - 22), 22);
    liveView->setBounds (surface.getLocalBounds());
    perfView->setBounds (surface.getLocalBounds());
}

void LiveLooperEditor::setGuiScale (int percent)
{
    percent = juce::jlimit (kGuiScales[0], kGuiScales[4], percent);
    guiScale = percent;
    appsettings::setValue (kScaleKey, juce::String (percent));
    setSize (juce::roundToInt ((float) surfaceWidth() * (float) percent / 100.0f),
             juce::roundToInt ((float) kBaseHeight * (float) percent / 100.0f));
    showStatus (tr ("INTERFACE SIZE %1 %", juce::String (percent)));
}

void LiveLooperEditor::stepGuiScale (int direction)
{
    int i = 0;
    while (i < 4 && kGuiScales[i] < guiScale)
        ++i;
    if (kGuiScales[i] != guiScale && direction < 0)
        ++i;            // between two sizes: "smaller" starts from the next size up
    setGuiScale (kGuiScales[juce::jlimit (0, 4, i + (direction > 0 ? 1 : -1))]);
}

//============================================================================== Live view
bool LiveLooperEditor::isLiveViewShown() const noexcept
{
    return liveView != nullptr && liveView->isVisible();
}

juce::BorderSize<int> LiveLooperEditor::getFullscreenSafeInsets (const juce::Displays::Display* display)
{
    if (safeInsetsOverride.has_value())
        return *safeInsetsOverride;
   #if JUCE_MAC
    // JUCE reads NSScreen.safeAreaInsets (macOS 12+, guarded with @available) into Display::safeAreaInsets.
    // v1.7: plus the menu bar and a visible Dock (the parts of the display outside its user area): they stay in
    // front of every app window, so the controls of the bottom row (STOP, CAPTURE, FILL ...) were under the Dock
    // and the top of the view under the menu bar on a Mac without notch - unclickable. The strips are background.
    if (display != nullptr)
    {
        auto insets = display->safeAreaInsets;
        const auto total = display->logicalBounds.toNearestInt();
        const auto user = display->userBounds.toNearestInt();
        if (! user.isEmpty() && total.contains (user))
        {
            insets.setTop    (juce::jmax (insets.getTop(),    user.getY() - total.getY()));
            insets.setLeft   (juce::jmax (insets.getLeft(),   user.getX() - total.getX()));
            insets.setBottom (juce::jmax (insets.getBottom(), total.getBottom() - user.getBottom()));
            insets.setRight  (juce::jmax (insets.getRight(),  total.getRight() - user.getRight()));
        }
        return insets;
    }
   #else
    juce::ignoreUnused (display);
   #endif
    return {};
}

void LiveLooperEditor::setSafeInsetsForTests (std::optional<juce::BorderSize<int>> insets)
{
    safeInsetsOverride = insets;
}

void LiveLooperEditor::selectViewFromSwitcher (int view)
{
    // the view switchers (header, Live view, Performance view): set the host parameter View, then apply it after
    // the click has finished - applying can close the fullscreen window the clicked switcher lives in
    // (v1.7 bug report, macOS: after a view switch no click reached the UI any more, only Esc): the switch -
    // showing an opaque view over the surface, keyboard focus, opening / closing a fullscreen window - now
    // happens after the mouse button is up, never while macOS is still tracking the press
    proc.setView (view);
    logMouseNote ("view switcher: View = " + juce::String (view));
    runWhenMouseReleased ([this] { applyDisplaySettings(); });
}

void LiveLooperEditor::runWhenMouseReleased (std::function<void()> fn, int tries)
{
    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    juce::MessageManager::callAsync ([safe, fn = std::move (fn), tries]() mutable {
        if (safe == nullptr)
            return;
        if (anyMouseButtonDown() && tries < 100)        // at most ~1.5 s, then anyway
        {
            juce::Timer::callAfterDelay (15, [safe, fn = std::move (fn), tries]() mutable {
                if (safe != nullptr)
                    safe->runWhenMouseReleased (std::move (fn), tries + 1);
            });
            return;
        }
        fn();
    });
}

void LiveLooperEditor::wireStageView (LiveView& v)
{
    v.onExit = [this] { if (learnMode) setLearnMode (false); else setLiveView (false); };     // Esc ends MIDI learn first
    v.getViewSwitcher().onSelect = [this] (int view) { selectViewFromSwitcher (view); };
    v.selectedPunch = [this] { return selectedPunch; };
    v.livePunchAmount = [this] { return shownLivePunch; };
}

void LiveLooperEditor::wireStageView (PerformView& v)
{
    v.onExit = [this] { if (learnMode) setLearnMode (false); else setLiveView (false); };
    v.getViewSwitcher().onSelect = [this] (int view) { selectViewFromSwitcher (view); };
    v.selectedPunch = [this] { return selectedPunch; };
    v.onSelectPunch = [this] (int i) {
        selectedPunch = juce::jlimit (0, kNumPunchFx - 1, i);
        if (punchXY != nullptr)
            punchXY->repaint();
    };
    v.livePunchAmount = [this] { return shownLivePunch; };
    v.masterMeter = [this] (float& l, float& r) {
        l = MeterBallistics::toPosition (perfMeterL);
        r = MeterBallistics::toPosition (perfMeterR);
    };
}

void LiveLooperEditor::openStageWindow (bool performance)
{
    closeStageWindow();
    dismissModalDialogs();      // a modal SETUP window left behind the window blocked every click in it (only Esc worked)
    // the display the editor is on (a second monitor), else the main display
    const auto& displays = juce::Desktop::getInstance().getDisplays();
    const auto* display = isShowing() ? displays.getDisplayForRect (getScreenBounds()) : nullptr;
    if (display == nullptr)
        display = displays.getPrimaryDisplay();
    auto strips = [this] (int t) { return juce::isPositiveAndBelow (t, kNumTracks) ? this->strips[(size_t) t].get() : nullptr; };
    std::unique_ptr<juce::Component> w;
    juce::Component* focus = nullptr;
    if (performance)
    {
        auto pw = std::make_unique<StageWindow<PerformView>> (proc, strips, lookAndFeel, *this, display);
        fullscreenPerf = &pw->view;
        wireStageView (*fullscreenPerf);
        pw->onKey = [this] (const juce::KeyPress& k) { return keyPressed (k); };
        pw->onKeyState = [this] (bool down) { return keyStateChanged (down); };
        focus = fullscreenPerf;
        w = std::move (pw);
    }
    else
    {
        auto lw = std::make_unique<StageWindow<LiveView>> (proc, strips, lookAndFeel, *this, display);
        fullscreenLive = &lw->view;
        wireStageView (*fullscreenLive);
        lw->onKey = [this] (const juce::KeyPress& k) { return keyPressed (k); };
        lw->onKeyState = [this] (bool down) { return keyStateChanged (down); };
        fullscreenLive->setShowWaveforms (liveView->getShowWaveforms());
        fullscreenLive->setShowFxStrip (liveView->getShowFxStrip());
        focus = fullscreenLive;
        w = std::move (lw);
    }
    w->setAlwaysOnTop (true);
    w->addToDesktop (juce::ComponentPeer::windowAppearsOnTaskbar);
    w->setVisible (true);
    w->toFront (true);
    focus->grabKeyboardFocus();
    liveWindow = std::move (w);
}

void LiveLooperEditor::closeStageWindow()
{
    fullscreenLive = nullptr;
    fullscreenPerf = nullptr;
    if (liveWindow == nullptr)
        return;
    // The window is usually closed from inside one of its own events (EXIT / the FULL chip on mouse-down, Esc,
    // the view switcher). Deleting it there destroyed its peer while the peer was still dispatching that event
    // (crash "pure virtual method called", the mouse source left on a dead window). Hide it now, delete it after
    // the event has finished.
    liveWindow->setVisible (false);
    retiredStageWindow = std::move (liveWindow);
    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->retiredStageWindow.reset(); });
}

void LiveLooperEditor::setLiveView (bool on, bool fullscreen)
{
    if (on && isPerformViewShown())
        proc.setView (1);                   // LIVE from the Performance view: the Live view
    setParam (proc, gLiveView, on ? 1.0f : 0.0f);
    if (on && fullscreen)
        fullscreenRequested = true;         // SETUP > Display > Open Live view (fullscreen): fullscreen until the view is left
    applyDisplaySettings();
}

bool LiveLooperEditor::isPerformViewShown() const noexcept
{
    return perfView != nullptr && perfView->isVisible();
}

void LiveLooperEditor::setPerformView (bool on, bool fullscreen)
{
    proc.setView (on ? kPerformanceView : 0);
    if (on && fullscreen)
        fullscreenRequested = true;
    applyDisplaySettings();
}

void LiveLooperEditor::setViewsFullscreen (bool on)
{
    viewsFullscreen = on;
    appsettings::setValue (kViewsFullscreenKey, on ? "1" : "0");
    applyDisplaySettings();
}

void LiveLooperEditor::updateStageWindow (bool stageOn, bool perf)
{
    // v1.7: the one place that decides between the view over the surface and the fullscreen window, for every way
    // a view is selected (header / Live / Performance view switcher, LIVE, Esc / EXIT, host parameters View /
    // Live View / Live View FX Bar from the host or MIDI, Push 1 SHIFT + MASTER, SETUP > Display): fullscreen when
    // "Open the views fullscreen" is on, or while a view opened with SETUP > Display > Open ... (fullscreen) is shown
    // (switching views there follows in the window)
    if (! stageOn)
        fullscreenRequested = false;
    const bool wantWindow = stageOn && (viewsFullscreen || fullscreenRequested);
    if (! wantWindow)
    {
        if (liveWindow != nullptr)
            closeStageWindow();
        return;
    }
    if (liveWindow == nullptr || (perf && fullscreenPerf == nullptr) || (! perf && fullscreenLive == nullptr))
        openStageWindow (perf);
}

void LiveLooperEditor::dismissModalDialogs()
{
    // Every modal component here is livelooper's own (JUCE is linked into the plug-in / app, a host's windows are not
    // in this list): the SETUP window, help / info dialogs, the key map editor, call-out boxes, menus. They end with
    // result 0 (cancel) and are hidden now; the ones launched asynchronously delete themselves after this event.
    juce::PopupMenu::dismissAllActiveMenus();
    auto& mcm = *juce::ModalComponentManager::getInstance();
    for (int guard = 0; guard < 16 && mcm.getNumModalComponents() > 0; ++guard)
    {
        auto* m = mcm.getModalComponent (0);
        if (m == nullptr)
            break;
        logMouseNote ("modal component closed for the view: " + juce::String (typeid (*m).name()) + " \"" + m->getName() + "\"");
        m->exitModalState (0);
        if (m->isOnDesktop())
            m->setVisible (false);
    }
    settingsWindow = nullptr;   // gone (deleted by the modal manager after this event)
}

void LiveLooperEditor::raiseModalDialogsOverStageWindow()
{
    // a livelooper dialog opened while a fullscreen view is up (keyboard shortcut, controller): in front of the
    // always-on-top window, reachable and closable - never hidden behind it, blocking it
    if (liveWindow == nullptr || ! liveWindow->isVisible())
        return;
    for (int i = 0; i < juce::Component::getNumCurrentlyModalComponents(); ++i)
        if (auto* m = juce::Component::getCurrentlyModalComponent (i))
            if (auto* top = m->getTopLevelComponent(); top != nullptr && top->isOnDesktop() && top != liveWindow.get() && ! top->isAlwaysOnTop())
            {
                top->setAlwaysOnTop (true);
                top->toFront (true);
            }
}

//============================================================================== apply
void LiveLooperEditor::applyDisplaySettings (bool force)
{
    const bool light = juce::roundToInt (proc.getState().getRawParameterValue (gUiTheme)->load()) == 1;
    const bool stage = paramOn (proc, gStageMode);
    const bool waves = paramOn (proc, gWaveforms);
    const int key = (light ? 1 : 0) | (stage ? 2 : 0) | (waves ? 4 : 0);
    if (force || key != shownDisplayState || theme::isLight() != light || theme::isStage() != stage)
    {
        const bool changed = shownDisplayState >= 0 && key != shownDisplayState;
        shownDisplayState = key;
        if (theme::isLight() != light || theme::isStage() != stage || force)
            theme::apply (light, stage);
        TrackStrip::showWaveforms = waves;
        lookAndFeel.applyTheme();
        sendLookAndFeelChange();
        if (! force)
            layoutSurface();        // stage mode type changes the logo width (LIVE button position)
        surfaceCacheKey = {};
        repaint();
        surface.repaint();
        if (changed)
        {
            // remembered on this computer (new instances start with it)
            appsettings::setValue ("display.theme", light ? "1" : "0");
            appsettings::setValue ("display.stage", stage ? "1" : "0");
            appsettings::setValue ("display.waveforms", waves ? "1" : "0");
        }
    }

    // v1.7: the FX bar is the host parameter live_fx (MIDI-mappable, view selector); remembered on this computer
    const bool fx = paramOn (proc, gLiveFx);
    if (fx != liveView->getShowFxStrip())
    {
        liveView->setShowFxStrip (fx);
        appsettings::setValue (kLiveFxKey, fx ? "1" : "0");
    }
    if (fullscreenLive != nullptr && fullscreenLive->getShowFxStrip() != fx)
        fullscreenLive->setShowFxStrip (fx);

    const bool stageOn = paramOn (proc, gLiveView);
    const bool perf = performanceSelected (proc);        // v1.7: the 4th view
    const bool live = stageOn && ! perf;
    TrackStrip::layersWanted = live && liveView->getShowWaveforms();
    liveBtn->setToggleState (stageOn, juce::dontSendNotification);
    viewChips->refresh();
    auto showView = [this] (juce::Component& v, bool on) {
        if (on == v.isVisible())
            return;
        v.setVisible (on);
        if (on)
        {
            dismissModalDialogs();      // the view must not sit behind / under a modal SETUP window (clicks blocked)
            v.toFront (false);
            if (learnMode)
                overlay.toFront (false);    // v1.7: the learn frames stay visible over the Live / Performance view
            if (isShowing())
            {
                if (anyMouseButtonDown())
                    pendingViewFocus = true;    // taken by the timer once the button is up (no focus change mid-press)
                else
                    v.grabKeyboardFocus();
            }
        }
    };
    const bool liveWas = liveView->isVisible(), perfWas = perfView->isVisible();
    showView (*liveView, live);
    showView (*perfView, perf);
    if (mouseLog != nullptr && (liveWas != live || perfWas != perf))
        logMouseNote ("view shown: " + describeViewState());
    if (! stageOn && shownLive && learnMode)
    {
        overlay.toFront (false);
        learnBtn.toFront (false);
    }
    updateStageWindow (stageOn, perf);
    shownLive = stageOn;
}

void LiveLooperEditor::timerDisplay()
{
    applyDisplaySettings();
    raiseModalDialogsOverStageWindow();
    if (pendingViewFocus && ! anyMouseButtonDown())
    {
        pendingViewFocus = false;
        juce::Component* v = isPerformViewShown() ? (juce::Component*) perfView.get()
                           : isLiveViewShown() ? (juce::Component*) liveView.get() : nullptr;
        if (v != nullptr && isShowing() && liveWindow == nullptr)
            v->grabKeyboardFocus();
    }
    if (isLiveViewShown())
        liveView->refresh (blinkCounter % 16 < 8);
    if (fullscreenLive != nullptr)
        fullscreenLive->refresh (blinkCounter % 16 < 8);
    if (isPerformViewShown())
        perfView->refresh (blinkCounter % 16 < 8);
    if (fullscreenPerf != nullptr)
        fullscreenPerf->refresh (blinkCounter % 16 < 8);
}

//============================================================================== SETUP > Display
void LiveLooperEditor::addDisplayMenu (juce::PopupMenu& menu, juce::Array<int>& toggles)
{
    const bool light = juce::roundToInt (proc.getState().getRawParameterValue (gUiTheme)->load()) == 1;
    juce::PopupMenu themeMenu, size, live;
    themeMenu.addItem (1900, tr ("Dark"), true, ! light);
    themeMenu.addItem (1901, tr ("Light"), true, light);
    for (int i = 0; i < 5; ++i)
        size.addItem (1905 + i, juce::String (kGuiScales[i]) + " %", true, kGuiScales[i] == guiScale);
    live.addItem (1920, tr ("Open Live view (fullscreen)"));
    live.addItem (1923, tr ("Open Performance view (fullscreen)"));
    live.addItem (1924, tr ("Open the views fullscreen (LIVE, view switcher, MIDI, Push)"), true, viewsFullscreen);
    live.addItem (1921, tr ("Waveforms in the track displays"), true, liveView->getShowWaveforms());
    live.addItem (1922, tr ("Performance FX strip"), true, paramOn (proc, gLiveFx));
    live.addSeparator();
    live.addItem (1946, tr ("Also with the LIVE button in the header, the host parameters \"View\", \"Live View\" and \"Live View FX Bar\" (MIDI learn) and SHIFT + MASTER on Push 1 (see the docs)"), false);
    live.addItem (1948, tr ("Performance view: the 4th choice of \"View\" (also the view switcher in the Live view header); Esc returns"), false);

    menu.addSubMenu (tr ("Colour theme"), themeMenu);
    menu.addSubMenu (tr ("Interface size"), size);
    menu.addItem (1945, tr ("75 %, 100 %, 125 %, 150 % or 200 % - also Ctrl / Cmd + and -"), false);
    menu.addItem (1910, tr ("Stage mode (high contrast, larger text)"), true, paramOn (proc, gStageMode));
    menu.addItem (1911, tr ("Show waveforms in the loop tracks"), true, paramOn (proc, gWaveforms));
    menu.addSubMenu (tr ("Live view"), live);
    menu.addSeparator();
    menu.addItem (1947, tr ("Interface size and theme are remembered on this computer"), false);
    toggles.addArray ({ 1910, 1911, 1921, 1922, 1924, -1900, -1901 });
}

bool LiveLooperEditor::handleDisplayMenu (int r)
{
    if (r < 1900 || r > 1949)
        return false;
    switch (r)
    {
        case 1900: case 1901: setParam (proc, gUiTheme, r == 1901 ? 1.0f : 0.0f); break;
        case 1905: case 1906: case 1907: case 1908: case 1909: setGuiScale (kGuiScales[r - 1905]); break;
        case 1910: setParam (proc, gStageMode, paramOn (proc, gStageMode) ? 0.0f : 1.0f); break;
        case 1911: setParam (proc, gWaveforms, paramOn (proc, gWaveforms) ? 0.0f : 1.0f); break;
        case 1920:
        case 1923:
        {
            // from the SETUP window: close it first (it stayed open and modal behind the always-on-top window - no
            // click reached the view, only Esc; v1.7 bug report), open the view after this click has finished
            dismissModalDialogs();
            juce::Component::SafePointer<LiveLooperEditor> safe (this);
            runWhenMouseReleased ([safe, r] {
                if (safe == nullptr)
                    return;
                if (r == 1923)
                    safe->setPerformView (true, true);
                else
                    safe->setLiveView (true, true);
            });
            return true;
        }
        case 1924: setViewsFullscreen (! viewsFullscreen); return true;
        case 1921:
            liveView->setShowWaveforms (! liveView->getShowWaveforms());
            if (fullscreenLive != nullptr)
                fullscreenLive->setShowWaveforms (liveView->getShowWaveforms());
            appsettings::setValue (kLiveWavesKey, liveView->getShowWaveforms() ? "1" : "0");
            break;
        case 1922: setParam (proc, gLiveFx, paramOn (proc, gLiveFx) ? 0.0f : 1.0f); break;    // applied below
        default: break;
    }
    applyDisplaySettings();
    return true;
}
//============================================================================== v1.7 mouse diagnostics
namespace
{
    juce::File mouseLogFileOverride;      // tests

    juce::String describeComponent (const juce::Component* c)
    {
        if (c == nullptr)
            return "none";
        juce::String s (typeid (*c).name());
        if (c->getName().isNotEmpty())
            s << " \"" << c->getName() << "\"";
        if (const auto id = getParamId (*c); id.isNotEmpty())
            s << " param " << id;
        bool self = true, children = true;
        c->getInterceptsMouseClicks (self, children);
        s << " screen " << c->getScreenBounds().toString()
          << " visible " << (c->isVisible() ? 1 : 0) << " showing " << (c->isShowing() ? 1 : 0)
          << " clicks " << (self ? 1 : 0) << (children ? 1 : 0)
          << (c->isTransformed() ? " transformed" : "")
          << (c->isOnDesktop() ? " [desktop]" : "");
        return s;
    }

    juce::String parentChain (const juce::Component* c)
    {
        juce::StringArray names;
        for (auto* p = c != nullptr ? c->getParentComponent() : nullptr; p != nullptr && names.size() < 6; p = p->getParentComponent())
            names.add (juce::String (typeid (*p).name()) + (p->getName().isNotEmpty() ? " \"" + p->getName() + "\"" : juce::String()));
        return names.joinIntoString (" < ");
    }
}

/** Writes what the mouse did, and the state around it, to the mouse log (see setMouseLogEnabled). A global mouse
    listener sees every mouse-down JUCE dispatches to a component; a 15 ms poll of the OS / JUCE button state
    catches presses that never reached a component (blocked by a window in front, a modal component, a mouse
    source stuck in a drag). */
struct LiveLooperEditor::MouseLog : public juce::Timer, public juce::MouseListener
{
    MouseLog (LiveLooperEditor& e) : editor (e), file (LiveLooperEditor::getMouseLogFile())
    {
        file.getParentDirectory().createDirectory();
        if (file.getSize() > 4 * 1024 * 1024)
            file.deleteFile();
        auto& d = juce::Desktop::getInstance();
        juce::String s;
        s << "\n===== livelooper " << LIVELOOPER_VERSION << " mouse log started " << juce::Time::getCurrentTime().toString (true, true, true, true)
          << " | " << juce::SystemStats::getOperatingSystemName() << " | " << juce::SystemStats::getJUCEVersion()
          << " | host " << juce::PluginHostType().getHostDescription()
          << " | " << (juce::JUCEApplicationBase::isStandaloneApp() ? "standalone" : "plug-in") << "\n";
        for (auto& disp : d.getDisplays().displays)
            s << "  display " << disp.logicalBounds.toString() << " user " << disp.userBounds.toString() << " scale " << disp.scale
              << " safe insets " << disp.safeAreaInsets.getTop() << "," << disp.safeAreaInsets.getLeft() << ","
              << disp.safeAreaInsets.getBottom() << "," << disp.safeAreaInsets.getRight() << (disp.isMain ? " main" : "") << "\n";
        s << "  global scale " << d.getGlobalScaleFactor() << ", editor desktop scale " << editor.getDesktopScaleFactor() << "\n";
        write (s);
        d.addGlobalMouseListener (this);
        startTimer (15);
    }

    ~MouseLog() override
    {
        stopTimer();
        juce::Desktop::getInstance().removeGlobalMouseListener (this);
    }

    void write (const juce::String& text)
    {
        file.appendText (text.endsWithChar ('\n') ? text : text + "\n", false, false, "\n");
    }

    juce::String stamp() const { return juce::Time::getCurrentTime().formatted ("%H:%M:%S.") + juce::String (juce::Time::getCurrentTime().getMilliseconds()).paddedLeft ('0', 3); }

    juce::String snapshot (juce::Point<float> screenPos)
    {
        auto& d = juce::Desktop::getInstance();
        juce::String s;
        s << "  state: " << editor.describeViewState() << "\n";
        s << "  focused: " << describeComponent (juce::Component::getCurrentlyFocusedComponent())
          << " | modal components " << juce::Component::getNumCurrentlyModalComponents();
        if (auto* m = juce::Component::getCurrentlyModalComponent())
            s << " top modal: " << describeComponent (m);
        s << " | foreground process " << (juce::Process::isForegroundProcess() ? 1 : 0) << "\n";
        for (int i = 0; i < d.getNumMouseSources(); ++i)
            if (auto* m = d.getMouseSource (i))
                s << "  mouse source " << i << ": at " << m->getScreenPosition().toString() << " dragging " << (m->isDragging() ? 1 : 0)
                  << " under mouse: " << describeComponent (m->getComponentUnderMouse()) << "\n";
        const auto at = screenPos.roundToInt();
        auto* hit = d.findComponentAt (at);
        s << "  Desktop::findComponentAt " << at.toString() << ": " << describeComponent (hit) << " | parents: " << parentChain (hit) << "\n";
        for (int i = 0; i < d.getNumComponents(); ++i)
        {
            auto* c = d.getComponent (i);
            if (c == nullptr)
                continue;
            s << "  desktop[" << i << "] " << describeComponent (c) << " alwaysOnTop " << (c->isAlwaysOnTop() ? 1 : 0);
            if (auto* peer = c->getPeer())
            {
                const auto local = peer->globalToLocal (screenPos).roundToInt();
                s << " | peer style 0x" << juce::String::toHexString (peer->getStyleFlags())
                  << " focused " << (peer->isFocused() ? 1 : 0) << " minimised " << (peer->isMinimised() ? 1 : 0)
                  << " platform scale " << peer->getPlatformScaleFactor()
                  << " peer bounds " << peer->getBounds().toString()
                  << " contains(mouse) " << (peer->contains (local, false) ? 1 : 0) << "/" << (peer->contains (local, true) ? 1 : 0);
            }
            s << "\n";
        }
        s << juce::String (mouselog::native::describeWindows());
        return s;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        juceDowns++;
        const auto screen = e.getScreenPosition();
        juce::String s;
        s << stamp() << " mouseDown " << (e.mods.isPopupMenu() ? "(right) " : "") << "at " << screen.toString()
          << " -> " << describeComponent (e.eventComponent);
        // the global listener also gets presses JUCE's modal check keeps from the component: say so
        if (e.eventComponent != nullptr && e.eventComponent->isCurrentlyBlockedByAnotherModalComponent())
        {
            auto* m = juce::Component::getCurrentlyModalComponent();
            s << "\n  BLOCKED BY MODAL " << (m != nullptr ? juce::String (typeid (*m).name()) + " \"" + m->getName() + "\"" : juce::String ("component"))
              << " - the component does not get this press";
        }
        s << "\n  parents: " << parentChain (e.eventComponent)
          << "\n  peer: " << describeComponent (e.eventComponent != nullptr && e.eventComponent->getPeer() != nullptr
                                                  ? &e.eventComponent->getPeer()->getComponent() : nullptr) << "\n";
        s << snapshot (screen.toFloat());
        write (s);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        write (stamp() + " mouseUp at " + e.getScreenPosition().toString() + " -> " + describeComponent (e.eventComponent));
    }

    void timerCallback() override
    {
        // a press the OS saw (macOS) or JUCE's mouse source has, compared with the mouse-downs that arrived
        const int os = mouselog::native::pressedMouseButtons();
        const bool down = os >= 0 ? (os != 0) : anyMouseButtonDown();
        if (down && ! wasDown)
        {
            // the mouse-downs before this press: a mouse-down JUCE dispatched since the last tick belongs to it
            // (taking the count now reported a delivered press as not delivered when the tick fell inside it)
            pressDowns = downsAtLastTick;
            pressAt = juce::Desktop::getMousePosition().toFloat();
            pressTime = juce::Time::getMillisecondCounter();
            pressReported = false;
        }
        if (wasDown && ! pressReported && juceDowns == pressDowns
            && (! down || juce::Time::getMillisecondCounter() - pressTime > 300))
        {
            pressReported = true;
            const bool elsewhere = ! juce::Process::isForegroundProcess()
                                && juce::Desktop::getInstance().findComponentAt (pressAt.roundToInt()) == nullptr;
            write (stamp() + " PRESS NOT DELIVERED to any component (the OS reported a mouse button down at "
                   + pressAt.toString() + ", JUCE dispatched no mouse-down)"
                   + (elsewhere ? juce::String (" - livelooper is not the active app and none of its windows is under the mouse: a press in another application")
                                : juce::String())
                   + "\n" + snapshot (pressAt));
        }
        if (! down)
            pressReported = true;
        wasDown = down;
        downsAtLastTick = juceDowns;
    }

    LiveLooperEditor& editor;
    juce::File file;
    int juceDowns = 0, pressDowns = 0, downsAtLastTick = 0;
    bool wasDown = false, pressReported = true;
    juce::Point<float> pressAt;
    juce::uint32 pressTime = 0;
};

juce::File LiveLooperEditor::getMouseLogFile()
{
    if (mouseLogFileOverride != juce::File())
        return mouseLogFileOverride;
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Library/Logs/livelooper-mouse.log");
   #else
    return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("livelooper-mouse.log");
   #endif
}

void LiveLooperEditor::setMouseLogFileForTests (const juce::File& f)
{
    mouseLogFileOverride = f;
}

void LiveLooperEditor::setMouseLogEnabled (bool on)
{
    if (on == (mouseLog != nullptr))
        return;
    if (on)
        mouseLog = std::make_unique<MouseLog> (*this);
    else
    {
        logMouseNote ("mouse log stopped");
        mouseLog.reset();
    }
}

void LiveLooperEditor::logMouseNote (const juce::String& text)
{
    if (auto* m = dynamic_cast<MouseLog*> (mouseLog.get()))
        m->write (m->stamp() + " " + text);
}

juce::String LiveLooperEditor::describeViewState() const
{
    juce::String s;
    s << "View " << juce::roundToInt (proc.getState().getRawParameterValue (gView)->load())
      << " live_view " << (paramOn (proc, gLiveView) ? 1 : 0)
      << " | Live view shown " << (isLiveViewShown() ? 1 : 0) << " Performance view shown " << (isPerformViewShown() ? 1 : 0)
      << " | fullscreen window " << (liveWindow != nullptr ? (liveWindow->isVisible() ? "visible" : "hidden") : "none")
      << (retiredStageWindow != nullptr ? " (+ retired one)" : "")
      << " | MIDI learn " << (learnMode ? 1 : 0) << " | editor " << getScreenBounds().toString()
      << " scale " << (float) getHeight() / (float) kBaseHeight
      << " | pending view focus " << (pendingViewFocus ? 1 : 0);
    return s;
}

#if ! JUCE_MAC
namespace mouselog::native
{
int pressedMouseButtons() { return -1; }
std::string describeWindows() { return {}; }
}
#endif
} // namespace livelooper
