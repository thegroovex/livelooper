// v1.7 (branch v1.7-wip): SETUP > Display - colour theme, interface size, stage mode, waveforms, Live view.
// Menu ids 1900 - 1949.
#include "PluginEditor.h"
#include "AppSettings.h"

namespace livelooper
{
using namespace params;
using i18n::tr;

namespace
{
    constexpr const char* kScaleKey = "display.scale";
    constexpr const char* kLiveWavesKey = "display.liveWaveforms";
    constexpr const char* kLiveFxKey = "display.liveFxStrip";

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
        void showMidiMenuFor (juce::Component& c) override { host.showMidiMenuFor (c); }
        void exportTrack (int t) override { host.exportTrack (t); }
        void importTrack (int t) override { host.importTrack (t); }
        void showTrackSettings (int t, juce::Component& anchor) override { host.showTrackSettings (t, anchor); }
        View view;
        MidiMenuHost& host;
        juce::BorderSize<int> insets;
    };

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
    // JUCE reads NSScreen.safeAreaInsets (macOS 12+, guarded with @available) into Display::safeAreaInsets
    if (display != nullptr)
        return display->safeAreaInsets;
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
    proc.setView (view);
    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->applyDisplaySettings(); });
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
        focus = fullscreenPerf;
        w = std::move (pw);
    }
    else
    {
        auto lw = std::make_unique<StageWindow<LiveView>> (proc, strips, lookAndFeel, *this, display);
        fullscreenLive = &lw->view;
        wireStageView (*fullscreenLive);
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
    if (on && fullscreen && (liveWindow == nullptr || fullscreenLive == nullptr))
        openStageWindow (false);
    applyDisplaySettings();
}

bool LiveLooperEditor::isPerformViewShown() const noexcept
{
    return perfView != nullptr && perfView->isVisible();
}

void LiveLooperEditor::setPerformView (bool on, bool fullscreen)
{
    proc.setView (on ? kPerformanceView : 0);
    if (on && fullscreen && (liveWindow == nullptr || fullscreenPerf == nullptr))
        openStageWindow (true);
    applyDisplaySettings();
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
            v.toFront (false);
            if (learnMode)
                overlay.toFront (false);    // v1.7: the learn frames stay visible over the Live / Performance view
            if (isShowing())
                v.grabKeyboardFocus();
        }
    };
    showView (*liveView, live);
    showView (*perfView, perf);
    if (! stageOn && shownLive && learnMode)
    {
        overlay.toFront (false);
        learnBtn.toFront (false);
    }
    if (! stageOn && liveWindow != nullptr)
        closeStageWindow();
    else if (liveWindow != nullptr && ((perf && fullscreenPerf == nullptr) || (live && fullscreenLive == nullptr)))
        openStageWindow (perf);             // the view changed while fullscreen (Push SHIFT + MASTER, View): follow it
    shownLive = stageOn;
}

void LiveLooperEditor::timerDisplay()
{
    applyDisplaySettings();
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
    toggles.addArray ({ 1910, 1911, 1921, 1922, -1900, -1901 });
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
        case 1920: setLiveView (true, true); break;
        case 1923: setPerformView (true, true); break;
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
} // namespace livelooper
