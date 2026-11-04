// v1.7 views (branch v1.7-wip, user bug report 2026-11-04): every control of the Full, Live, Live without FX
// and Performance views - in the editor and in the fullscreen windows - receives real mouse clicks.
// The clicks go through the window's ComponentPeer (the same path as a click of the OS: Desktop hit-test,
// MouseInputSource, modal checks), not by calling the controls directly. Also: the view switcher in the header
// of the full surface and the safe area (MacBook Pro notch) of the fullscreen windows.

#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "plugin/AppSettings.h"

using namespace livelooper;
using namespace livelooper::params;

namespace
{
struct MouseTempFolder
{
    juce::File dir { juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("ll_viewmouse", "") };
    MouseTempFolder()  { dir.createDirectory(); appsettings::setStorageFolderForTests (dir); }
    ~MouseTempFolder() { appsettings::setStorageFolderForTests ({}); dir.deleteRecursively(); theme::apply (false, false); }
};

/** Counts the mouse-downs a component (or any of its children) receives. watch() registers it, so a click that
    has to be dispatched directly (see clickAt) reaches it like JUCE's nested mouse listeners. */
struct DownCounter;
std::vector<std::pair<juce::Component*, DownCounter*>> watchedCounters;

struct DownCounter : juce::MouseListener
{
    int downs = 0, ups = 0;
    juce::Component::SafePointer<juce::Component> watched;
    void mouseDown (const juce::MouseEvent&) override { ++downs; }
    void mouseUp (const juce::MouseEvent&) override   { ++ups; }
    void watch (juce::Component& c) { watched = &c; c.addMouseListener (this, true); watchedCounters.push_back ({ &c, this }); }
    void unwatch()
    {
        if (watched != nullptr)
            watched->removeMouseListener (this);
        watchedCounters.erase (std::remove_if (watchedCounters.begin(), watchedCounters.end(), [this] (auto& w) { return w.second == this; }),
                               watchedCounters.end());
        watched = nullptr;
    }
    ~DownCounter() override { unwatch(); }
};

juce::int64 mouseTime = 0;

void pump (int ms = 5) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

/** Linux (Xvfb, our own X server) always delivers: every click there goes through the OS window hit-test. The
    macOS and Windows CI runners do not (macOS: notification banners, menu bar, Dock and the 1024 x 768 screen
    in front of / around the window - "found nothing" for whole regions; Windows: the test window behind other
    windows of the runner's desktop, every hit-test of the editor failed while the always-on-top fullscreen
    windows worked). There a point the OS reports as not ours falls back to the component-level hit-test of the
    window and a direct dispatch to the component found there: still proves nothing of our own UI (an overlay, an
    invisible or opaque view) is in front of the control and that it reacts, just without the OS window test.
    LIVELOOPER_STRICT_MOUSE_TESTS=1 turns the fallback off everywhere. */
bool forcedFallback()      // LIVELOOPER_FORCE_MOUSE_FALLBACK=1: exercise the fallback on Linux (as if the OS delivered nothing)
{
    static const bool forced = juce::SystemStats::getEnvironmentVariable ("LIVELOOPER_FORCE_MOUSE_FALLBACK", {}) == "1";
    return forced;
}

bool fallbackAllowed()
{
    if (forcedFallback())
        return true;
   #if JUCE_LINUX
    return false;
   #else
    return juce::SystemStats::getEnvironmentVariable ("LIVELOOPER_STRICT_MOUSE_TESTS", {}) != "1";
   #endif
}

int fallbackPoints = 0;     // points of the current test the OS did not deliver (fallback used)

/** The integer screen point for a point of c (Desktop::findComponentAt works on whole pixels): the rounded
    point, nudged by a pixel if rounding left the control (small controls in the scaled fullscreen view). */
juce::Point<int> screenPointOf (juce::Component& c, juce::Point<int> local)
{
    const auto exact = c.localPointToGlobal (local.toFloat());
    const auto base = exact.roundToInt();
    const auto inside = [&c] (juce::Point<int> p) { return c.getLocalBounds().toFloat().reduced (0.25f).contains (c.getLocalPoint (nullptr, p.toFloat())); };
    if (inside (base))
        return base;
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
            if (inside (base + juce::Point<int> (dx, dy)))
                return base + juce::Point<int> (dx, dy);
    return base;
}

/** The OS would deliver a press at this screen point to c's window: the same test JUCE's mouse dispatch makes
    (Component::contains of the window, i.e. ComponentPeer::contains - on macOS windowNumberAtPoint, on Windows
    WindowFromPoint). */
bool osDelivers (juce::Component& c, juce::Point<int> screen)
{
    if (forcedFallback())
        return false;
    auto* top = c.getTopLevelComponent();
    return top != nullptr && top->isOnDesktop() && top->getPeer() != nullptr && top->contains (top->getLocalPoint (nullptr, screen.toFloat()));
}

/** The component a press at this screen point reaches: the OS hit-test (Desktop::findComponentAt, like a real
    click); where the OS environment does not deliver (see fallbackAllowed) the window's own hit-test. */
juce::Component* hitAt (juce::Component& c, juce::Point<int> screen)
{
    if (osDelivers (c, screen) || ! fallbackAllowed())
        return juce::Desktop::getInstance().findComponentAt (screen);
    ++fallbackPoints;
    auto* top = c.getTopLevelComponent();
    return top->getComponentAt (top->getLocalPoint (nullptr, screen.toFloat()));
}

/** The position in the window's peer for a screen point, including the desktop scale (raw peer coordinates,
    what the OS passes to handleMouseEvent). */
juce::Point<float> peerPos (juce::ComponentPeer& peer, juce::Point<int> screen)
{
    return peer.globalToLocal (screen.toFloat() * juce::Desktop::getInstance().getGlobalScaleFactor());
}

juce::MouseEvent makeEvent (juce::Component& target, juce::Point<int> screen, juce::ModifierKeys mods, juce::int64 t, juce::Point<int> downScreen, juce::int64 downT)
{
    return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), target.getLocalPoint (nullptr, screen.toFloat()), mods,
                             juce::MouseInputSource::defaultPressure, juce::MouseInputSource::defaultOrientation, juce::MouseInputSource::defaultRotation,
                             0.0f, 0.0f, &target, &target, juce::Time (t), target.getLocalPoint (nullptr, downScreen.toFloat()), juce::Time (downT), 1, false);
}

/** The fallback dispatch: mouse-down / mouse-up straight to the component the window's hit-test found (and to
    the watched counters around it, like nested mouse listeners). A juce::Button clicks through triggerClick (its
    own mouseUp only clicks when the OS mouse is over it). */
struct DirectPress
{
    juce::Component::SafePointer<juce::Component> target;
    juce::Point<int> downScreen;
    juce::int64 downTime = 0;

    void down (juce::Component& t, juce::Point<int> screen, juce::int64 time)
    {
        target = &t;
        downScreen = screen;
        downTime = time;
        const auto e = makeEvent (t, screen, juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier), time, screen, time);
        t.mouseDown (e);
        for (auto w : std::vector<std::pair<juce::Component*, DownCounter*>> (watchedCounters))
            if (w.first == &t || w.first->isParentOf (&t))
                w.second->mouseDown (e);
    }
    void up (juce::Point<int> screen, juce::int64 time)
    {
        if (target == nullptr)
            return;
        auto& t = *target;
        const auto e = makeEvent (t, screen, juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier), time, downScreen, downTime);
        for (auto w : std::vector<std::pair<juce::Component*, DownCounter*>> (watchedCounters))
            if (w.first == &t || w.first->isParentOf (&t))
                w.second->mouseUp (e);
        if (auto* b = dynamic_cast<juce::Button*> (&t))
        {
            if (t.getLocalBounds().contains (e.getPosition()))
                b->triggerClick();
        }
        else
        {
            t.mouseUp (e);
        }
        target = nullptr;
    }
};

/** Press (and later release) at a point of c: through the window's ComponentPeer like the OS does (Desktop
    hit-test, MouseInputSource, modal checks); the direct fallback only where the OS environment does not deliver. */
struct Press
{
    juce::Component* window = nullptr;
    juce::ComponentPeer* peer = nullptr;
    DirectPress direct;
    bool viaOs = true;

    bool down (juce::Component& c, juce::Point<int> screen, juce::int64 time)
    {
        peer = c.getPeer();
        if (peer == nullptr)
            return false;
        viaOs = osDelivers (c, screen) || ! fallbackAllowed();
        using Src = juce::MouseInputSource::InputSourceType;
        if (viaOs)
        {
            peer->handleMouseEvent (Src::mouse, peerPos (*peer, screen), {}, juce::MouseInputSource::defaultPressure, 0.0f, time);
            peer->handleMouseEvent (Src::mouse, peerPos (*peer, screen), juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier),
                                    juce::MouseInputSource::defaultPressure, 0.0f, time + 1);
            return true;
        }
        auto* top = c.getTopLevelComponent();
        auto* t = top->getComponentAt (top->getLocalPoint (nullptr, screen.toFloat()));
        if (t == nullptr)
            return false;
        direct.down (*t, screen, time + 1);
        return true;
    }
    void up (juce::Point<int> screen, juce::int64 time)
    {
        using Src = juce::MouseInputSource::InputSourceType;
        if (viaOs)
        {
            // a click can close the window it happened in: never touch a peer that is gone
            if (juce::ComponentPeer::isValidPeer (peer))
                peer->handleMouseEvent (Src::mouse, peerPos (*peer, screen), {}, juce::MouseInputSource::defaultPressure, 0.0f, time);
        }
        else
        {
            direct.up (screen, time);
        }
    }
    void moveAway (juce::int64 time)
    {
        using Src = juce::MouseInputSource::InputSourceType;
        if (viaOs && juce::ComponentPeer::isValidPeer (peer))
            peer->handleMouseEvent (Src::mouse, { -50.0f, -50.0f }, {}, juce::MouseInputSource::defaultPressure, 0.0f, time);
    }
};

/** A left click at a point of c. */
bool clickAt (juce::Component& c, juce::Point<int> local)
{
    const auto screen = screenPointOf (c, local);
    if (mouseTime == 0)
        mouseTime = juce::Time::currentTimeMillis();
    mouseTime += 400;      // no double clicks
    Press p;
    if (! p.down (c, screen, mouseTime))
        return false;
    if (! p.viaOs)
        ++fallbackPoints;
    p.up (screen, mouseTime + 60);
    pump();
    p.moveAway (mouseTime + 80);      // move the mouse away (exit / hover state)
    pump();
    return true;
}

/** The component the press at a point of c reaches (Desktop::findComponentAt like a real click, see hitAt). */
juce::Component* desktopHit (juce::Component& c, juce::Point<int> local)
{
    return hitAt (c, screenPointOf (c, local));
}

/** A point of c that is not covered by one of its own children (the free part of a knob, a chip). */
juce::Point<int> clickPoint (juce::Component& c)
{
    return c.getLocalBounds().getCentre();
}
/** The editor at the top left of the screen's user area, at an interface size that fits on it (the macOS /
    Windows CI runners have a 1024 x 768 screen: an editor larger than the screen got clicks on its off-screen
    part lost - the views themselves were fine). */
bool fitsOnScreen (int percent, juce::Rectangle<int> screen)
{
    return screen.isEmpty()
        || (LiveLooperEditor::kBaseWidth * percent / 100 <= screen.getWidth() && LiveLooperEditor::kBaseHeight * percent / 100 <= screen.getHeight());
}

int placeOnScreen (LiveLooperEditor& e, juce::Rectangle<int> screen)
{
    const int percent = fitsOnScreen (100, screen) ? 100 : 75;
    if (percent != 100)
        e.setGuiScale (percent);
    e.setTopLeftPosition (screen.getPosition());
    return percent;
}

/** The editor as a window in front (always on top like the fullscreen windows, which open later and in front of
    it): on the Windows runner the plain window stayed behind other windows and got no click. */
void showEditor (juce::AudioProcessorEditor& ed, LiveLooperEditor& e, juce::Rectangle<int> screen)
{
    ed.setSize (LiveLooperEditor::kBaseWidth, LiveLooperEditor::kBaseHeight);
    placeOnScreen (e, screen);
    ed.setAlwaysOnTop (true);
    ed.addToDesktop (juce::ComponentPeer::windowAppearsOnTaskbar);
    ed.setVisible (true);
    ed.toFront (true);
    pump (20);
}
} // namespace

class ViewMouseTests : public juce::UnitTest
{
public:
    ViewMouseTests() : juce::UnitTest ("v1.7 Views: mouse clicks in every view", "livelooper") {}

    /** Clicks every MIDI-learnable control inside root (except the ones in skip) through the peer and expects
        each to receive the mouse-down - nothing invisible or modal in front of it. */
    void clickEverything (juce::Component& root, const juce::String& where, const juce::Array<juce::Component*>& skip)
    {
        juce::Array<juce::Component*> controls;
        std::function<void (juce::Component&)> walk = [&] (juce::Component& p) {
            for (auto* c : p.getChildren())
            {
                if (! c->isVisible())
                    continue;
                if (getParamId (*c).isNotEmpty() && c->getWidth() > 2 && c->getHeight() > 2)
                    controls.add (c);
                else
                    walk (*c);
            }
        };
        walk (root);
        expect (controls.size() >= 5, where + ": controls found (" + juce::String (controls.size()) + ")");
        int clicked = 0;
        for (auto* c : controls)
        {
            if (skip.contains (c))
                continue;
            juce::Component::SafePointer<juce::Component> safe (c);
            const auto pt = clickPoint (*c);
            auto* hit = desktopHit (*c, pt);
            expect (hit == c || c->isParentOf (hit),
                    where + ": the desktop hit-test finds " + getParamId (*c) + " (found "
                        + (hit != nullptr ? juce::String (typeid (*hit).name()) + " " + getParamId (*hit) : juce::String ("nothing")) + ")");
            DownCounter counter;
            counter.watch (*c);
            expect (clickAt (*c, pt), where + ": on the desktop");
            counter.unwatch();
            expect (counter.downs == 1, where + ": " + getParamId (*c) + " receives the click");
            juce::PopupMenu::dismissAllActiveMenus();      // a pill / menu button may have opened its menu
            pump();
            ++clicked;
        }
        expect (clicked >= 5, where + ": clicked " + juce::String (clicked));
        logMessage (where + ": " + juce::String (clicked) + " controls clicked"
                    + (fallbackPoints > 0 ? ", " + juce::String (fallbackPoints) + " hit-tests / clicks the OS did not deliver to the window (component-level fallback)"
                                          : juce::String()));
        fallbackPoints = 0;
    }

    void runTest() override
    {
        juce::Rectangle<int> screen;
        if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        {
            screen = d->userBounds.toNearestInt();
            logMessage ("primary display " + d->logicalBounds.toString() + ", user area " + screen.toString() + ", scale " + juce::String (d->scale)
                        + (fitsOnScreen (100, screen) ? juce::String() : juce::String (" - editor at 75 %")));
        }

        beginTest ("Full view: the view switcher in the header is clickable and selects every view (host parameter View)");
        {
            MouseTempFolder tmp;
            auto procOwner = std::make_unique<LiveLooperProcessor> (false, false);
            auto& proc = *procOwner;
            std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
            auto* e = dynamic_cast<LiveLooperEditor*> (ed.get());
            showEditor (*ed, *e, screen);
            auto& vs = e->getViewSwitcher();
            expect (vs.isShowing(), "switcher shown on the full surface");
            expectEquals (getParamId (vs), gView, "right-click MIDI learn of the host parameter View");
            expectEquals (vs.getNumChips(), 4);
            expectEquals (vs.getSelectedChip(), 0, "FULL selected");
            // inside the header (0 - 88), not over the LCD or the logo row
            const auto sb = vs.getBounds();
            expect (sb.getBottom() <= 88 && sb.getY() >= 60, "in the header under the logo");
            expect (sb.getRight() < e->getLiveButton().getRight() + 40 && ! sb.intersects (e->getLiveButton().getBounds()), "not over the LIVE pill");
            for (int chip : { 1, 2, 3, 0 })
            {
                auto pt = vs.getChipBounds (chip).getCentre();
                if (chip == 0)
                {
                    // back to FULL from the stage view: the switcher of the shown view
                    juce::Component* sw = e->isPerformViewShown() ? (juce::Component*) &e->getPerformView()->getViewSwitcher()
                                                                  : (juce::Component*) &e->getLiveView()->getViewSwitcher();
                    auto& chips = *dynamic_cast<ChoiceChips*> (sw);
                    expect (clickAt (chips, chips.getChipBounds (0).getCentre()));
                }
                else
                {
                    if (e->isStageViewShown())
                    {
                        auto& chips = e->isPerformViewShown() ? e->getPerformView()->getViewSwitcher() : e->getLiveView()->getViewSwitcher();
                        expect (clickAt (chips, chips.getChipBounds (chip).getCentre()));
                    }
                    else
                    {
                        expect (desktopHit (vs, pt) == &vs, "hit-test finds the switcher");
                        expect (clickAt (vs, pt));
                    }
                }
                pump (20);
                expectEquals (proc.getView(), chip, "chip " + juce::String (chip) + " selects the view");
                expect (e->isLiveViewShown() == (chip == 1 || chip == 2), "Live view shown for chip " + juce::String (chip));
                expect (e->isPerformViewShown() == (chip == 3), "Performance view shown for chip " + juce::String (chip));
            }
            expect (! e->isStageViewShown(), "FULL from the stage view switcher");
            // the LIVE pill still works with the mouse
            expect (clickAt (e->getLiveButton(), e->getLiveButton().getLocalBounds().getCentre()));
            pump (20);
            expect (e->isLiveViewShown(), "LIVE pill click");
            e->setLiveView (false);
            ed->removeFromDesktop();
        }

        beginTest ("Live, Live without FX and Performance view in the editor: every control receives mouse clicks");
        {
            MouseTempFolder tmp;
            auto procOwner = std::make_unique<LiveLooperProcessor> (false, false);
            auto& proc = *procOwner;
            std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
            auto* e = dynamic_cast<LiveLooperEditor*> (ed.get());
            showEditor (*ed, *e, screen);

            for (int view : { 1, 2, 3 })
            {
                const juce::String name = view == 1 ? "Live" : (view == 2 ? "Live without FX" : "Performance");
                // switch with the mouse (header switcher of the full surface), like the user
                auto& vs = e->getViewSwitcher();
                expect (clickAt (vs, vs.getChipBounds (view).getCentre()), name);
                pump (20);
                expectEquals (proc.getView(), view, name + " selected by the header switcher");
                juce::Component* root = view == 3 ? (juce::Component*) e->getPerformView() : (juce::Component*) e->getLiveView();
                expect (root->isShowing(), name + " shown");
                juce::Array<juce::Component*> skip;
                if (view == 3)
                    skip.addArray (juce::Array<juce::Component*> { &e->getPerformView()->getViewSwitcher(), &e->getPerformView()->getExitButton() });
                else
                    skip.addArray (juce::Array<juce::Component*> { &e->getLiveView()->getViewSwitcher(), &e->getLiveView()->getExitButton() });
                clickEverything (*root, name + " (editor)", skip);
                e->applyDisplaySettings();
                expectEquals (proc.getView(), view, name + ": still shown after clicking its controls");
                // EXIT with the mouse
                juce::Component& exitBtn = view == 3 ? (juce::Component&) e->getPerformView()->getExitButton() : (juce::Component&) e->getLiveView()->getExitButton();
                expect (desktopHit (exitBtn, exitBtn.getLocalBounds().getCentre()) == &exitBtn, name + ": EXIT hit-test");
                expect (clickAt (exitBtn, exitBtn.getLocalBounds().getCentre()));
                pump (20);
                e->applyDisplaySettings();
                expect (! e->isStageViewShown(), name + ": EXIT click returns to the full surface");
            }
            // interface size 150 % (125 % on a small screen): the clicks still land
            const int bigSize = fitsOnScreen (150, screen) ? 150 : fitsOnScreen (125, screen) ? 125 : 0;
            if (bigSize == 0)
                logMessage ("screen " + screen.toString() + " too small for 125 / 150 %: larger interface size not clicked");
            else
                e->setGuiScale (bigSize);
            pump (20);
            for (int view : { 1, 3 })
            {
                if (bigSize == 0)
                    break;
                const juce::String name = juce::String (view == 1 ? "Live" : "Performance") + " at " + juce::String (bigSize) + " %";
                auto& vs = e->getViewSwitcher();
                expect (clickAt (vs, vs.getChipBounds (view).getCentre()), name);
                pump (20);
                expectEquals (proc.getView(), view, name + " selected");
                juce::Component* root = view == 3 ? (juce::Component*) e->getPerformView() : (juce::Component*) e->getLiveView();
                juce::Array<juce::Component*> skip;
                if (view == 3)
                    skip.addArray (juce::Array<juce::Component*> { &e->getPerformView()->getViewSwitcher(), &e->getPerformView()->getExitButton() });
                else
                    skip.addArray (juce::Array<juce::Component*> { &e->getLiveView()->getViewSwitcher(), &e->getLiveView()->getExitButton() });
                clickEverything (*root, name, skip);
                auto& chips = view == 3 ? e->getPerformView()->getViewSwitcher() : e->getLiveView()->getViewSwitcher();
                expect (clickAt (chips, chips.getChipBounds (0).getCentre()));
                pump (20);
                expect (! e->isStageViewShown(), name + ": FULL chip");
            }
            e->setGuiScale (fitsOnScreen (100, screen) ? 100 : 75);
            pump (20);
            // MIDI learn mode inside a stage view: the learn overlay takes every click (it picks the learn target), the
            // MIDI LEARN button of the header is hidden behind the view - before, nothing reacted and only Esc (which
            // left the view, learn mode still on) got out. Now the overlay has MIDI LEARN - DONE and Esc ends learn first.
            for (int view : { 1, 3 })
            {
                const juce::String name = view == 1 ? "Live" : "Performance";
                e->setLearnMode (true);
                proc.setView (view);
                e->applyDisplaySettings();
                pump (20);
                expect (e->isLearnMode() && e->isLearnOverlayInFrontForTests(), name + ": learn overlay in front");
                juce::Component& pad = view == 1 ? (juce::Component&) e->getLiveView()->getExitButton() : (juce::Component&) e->getPerformView()->getExitButton();
                auto* hit = desktopHit (pad, pad.getLocalBounds().getCentre());
                expect (hit == &e->getLearnOverlayForTests(), name + ": in learn mode the overlay takes the click");
                expect (clickAt (pad, pad.getLocalBounds().getCentre()));
                pump (20);
                expect (e->isStageViewShown(), name + ": the click picked a learn target, it did not press EXIT");
                expectEquals (proc.getMidiLearnParamId(), gLiveView, name + ": EXIT is the learn target");
                // DONE on the overlay ends MIDI learn, the view stays and its controls react again
                auto& ov = e->getLearnOverlayForTests();
                const auto done = e->getLearnDoneAreaForTests().getCentre();
                expect (desktopHit (ov, done) == &ov, name + ": DONE hit-test");
                expect (clickAt (ov, done));
                pump (20);
                expect (! e->isLearnMode(), name + ": DONE ends MIDI learn");
                expectEquals (proc.getView(), view, name + ": still in the view");
                juce::Component* root = view == 3 ? (juce::Component*) e->getPerformView() : (juce::Component*) e->getLiveView();
                juce::Array<juce::Component*> skip;
                if (view == 3)
                    skip.addArray (juce::Array<juce::Component*> { &e->getPerformView()->getViewSwitcher(), &e->getPerformView()->getExitButton() });
                else
                    skip.addArray (juce::Array<juce::Component*> { &e->getLiveView()->getViewSwitcher(), &e->getLiveView()->getExitButton() });
                clickEverything (*root, name + " after MIDI learn", skip);
                // Esc in learn mode: ends learn mode first, a second Esc leaves the view
                e->setLearnMode (true);
                e->applyDisplaySettings (true);
                expect (e->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
                expect (! e->isLearnMode() && e->isStageViewShown(), name + ": Esc ends MIDI learn, the view stays");
                expect (e->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
                e->applyDisplaySettings();
                expect (! e->isStageViewShown(), name + ": second Esc leaves the view");
            }
            ed->removeFromDesktop();
        }

        beginTest ("Fullscreen Live / Performance windows: every control receives mouse clicks, the switcher follows the view");
        {
            MouseTempFolder tmp;
            auto procOwner = std::make_unique<LiveLooperProcessor> (false, false);
            auto& proc = *procOwner;
            std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
            auto* e = dynamic_cast<LiveLooperEditor*> (ed.get());
            showEditor (*ed, *e, screen);

            e->setLiveView (true, true);
            pump (20);
            expect (e->isLiveFullscreen() && e->getFullscreenLiveView() != nullptr, "fullscreen Live view");
            if (auto* lv = e->getFullscreenLiveView())
            {
                clickEverything (*lv, "Live (fullscreen)", { &lv->getViewSwitcher(), &lv->getExitButton() });
                // LIVE - FX with the switcher of the fullscreen window
                auto& vs = lv->getViewSwitcher();
                expect (desktopHit (vs, vs.getChipBounds (2).getCentre()) == &vs, "fullscreen switcher hit-test");
                expect (clickAt (vs, vs.getChipBounds (2).getCentre()));
                pump (30);
                expectEquals (proc.getView(), 2, "fullscreen switcher: Live without FX");
            }
            if (auto* lv = e->getFullscreenLiveView())
            {
                expect (lv->getFxStripArea().isEmpty(), "no FX bar");
                clickEverything (*lv, "Live without FX (fullscreen)", { &lv->getViewSwitcher(), &lv->getExitButton() });
                auto& vs = lv->getViewSwitcher();
                expect (clickAt (vs, vs.getChipBounds (3).getCentre()));
                pump (30);
                expectEquals (proc.getView(), 3, "fullscreen switcher: Performance (window follows)");
            }
            expect (e->isLiveFullscreen() && e->getFullscreenPerformView() != nullptr, "fullscreen Performance view");
            if (auto* pv = e->getFullscreenPerformView())
            {
                clickEverything (*pv, "Performance (fullscreen)", { &pv->getViewSwitcher(), &pv->getExitButton() });
                auto& ex = pv->getExitButton();
                expect (desktopHit (ex, ex.getLocalBounds().getCentre()) == &ex, "fullscreen EXIT hit-test");
                expect (clickAt (ex, ex.getLocalBounds().getCentre()));
                pump (30);
                e->applyDisplaySettings();
                expect (! e->isLiveFullscreen() && ! e->isStageViewShown(), "fullscreen EXIT click closes the window");
            }
            // FULL chip in the fullscreen window closes it (deferred, no crash while the clicked window goes away)
            e->setLiveView (true, true);
            pump (20);
            if (auto* lv = e->getFullscreenLiveView())
            {
                auto& vs = lv->getViewSwitcher();
                expect (clickAt (vs, vs.getChipBounds (0).getCentre()));
                pump (30);
                expect (! e->isLiveFullscreen() && proc.getView() == 0, "FULL chip closes the fullscreen window");
            }
            // after leaving, the full surface takes clicks again
            auto& hs = e->getViewSwitcher();
            expect (clickAt (hs, hs.getChipBounds (1).getCentre()));
            pump (20);
            expect (e->isLiveViewShown(), "header switcher after fullscreen");
            e->setLiveView (false);
            ed->removeFromDesktop();
        }

        beginTest ("Fullscreen windows: content inside the display's safe area (macOS notch), the strip is background");
        {
            MouseTempFolder tmp;
           #if ! JUCE_MAC
            expect (LiveLooperEditor::getFullscreenSafeInsets (juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()).isEmpty(),
                    "other systems: no inset (unchanged)");
           #else
            // macOS: the menu bar and a visible Dock stay in front of every window - the view keeps out of them
            if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            {
                const auto ins = LiveLooperEditor::getFullscreenSafeInsets (d);
                const auto total = d->logicalBounds.toNearestInt(), user = d->userBounds.toNearestInt();
                expect (ins.getTop() >= user.getY() - total.getY() && ins.getBottom() >= total.getBottom() - user.getBottom()
                            && ins.getLeft() >= user.getX() - total.getX() && ins.getRight() >= total.getRight() - user.getRight(),
                        "macOS: menu bar / Dock outside the view (insets " + juce::String (ins.getTop()) + " " + juce::String (ins.getBottom()) + ")");
            }
           #endif
            LiveLooperEditor::setSafeInsetsForTests (juce::BorderSize<int> (38, 0, 0, 0));      // a notched MacBook Pro display
            auto procOwner = std::make_unique<LiveLooperProcessor> (false, false);
            auto& proc = *procOwner;
            std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
            auto* e = dynamic_cast<LiveLooperEditor*> (ed.get());
            showEditor (*ed, *e, screen);
            for (bool perf : { false, true })
            {
                if (perf)
                    e->setPerformView (true, true);
                else
                    e->setLiveView (true, true);
                pump (20);
                auto* w = e->getStageWindow();
                juce::Component* v = perf ? (juce::Component*) e->getFullscreenPerformView() : (juce::Component*) e->getFullscreenLiveView();
                expect (w != nullptr && v != nullptr, perf ? "Performance fullscreen" : "Live fullscreen");
                if (w == nullptr || v == nullptr)
                    continue;
                const auto vb = w->getLocalArea (v, v->getLocalBounds());
                expectEquals (vb.getY(), 38, "view starts below the notch");
                expect (vb.getBottom() <= w->getHeight() + 1 && vb.getRight() <= w->getWidth() + 1, "view inside the window");
                expect (vb.getBottom() >= w->getHeight() - 2, "fills the safe height");
                // the strip is the window itself (painted with the background), no control of the view is up there
                auto* topHit = desktopHit (*w, juce::Point<int> (w->getWidth() / 2, 10));
                expect (topHit == w, "notch strip belongs to the window");
                // the header controls are still clickable below the strip
                auto& sw = perf ? e->getFullscreenPerformView()->getViewSwitcher() : e->getFullscreenLiveView()->getViewSwitcher();
                expect (desktopHit (sw, sw.getChipBounds (1).getCentre()) == &sw, "switcher below the notch is hit");
                e->setLiveView (false);
                pump (10);
            }
            LiveLooperEditor::setSafeInsetsForTests (std::nullopt);
            ed->removeFromDesktop();
        }

        beginTest ("View switch only after the mouse button is up (macOS: no click reached the UI after a switch); mouse diagnostics log");
        {
            MouseTempFolder tmp;
            const auto logFile = tmp.dir.getChildFile ("mouse.log");
            LiveLooperEditor::setMouseLogFileForTests (logFile);
            auto procOwner = std::make_unique<LiveLooperProcessor> (false, false);
            auto& proc = *procOwner;
            std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
            auto* e = dynamic_cast<LiveLooperEditor*> (ed.get());
            showEditor (*ed, *e, screen);
            expect (! e->isMouseLogEnabled(), "mouse log off by default");
            e->setMouseLogEnabled (true);
            expect (e->isMouseLogEnabled() && logFile.existsAsFile(), "log file started");

            // press on the LIVE chip and keep the button down: nothing switches yet
            auto& vs = e->getViewSwitcher();
            expect (vs.getPeer() != nullptr);
            fallbackPoints = 0;
            const auto pos = screenPointOf (vs, vs.getChipBounds (1).getCentre());
            mouseTime += 400;
            Press hold;
            expect (hold.down (vs, pos, mouseTime));
            bool allViaOs = hold.viaOs;
            pump (40);
            expectEquals (proc.getView(), 0, "no switch while the button is held");
            expect (! e->isStageViewShown(), "no view while the button is held");
            hold.up (pos, mouseTime + 60);
            pump (40);
            expectEquals (proc.getView(), 1, "switch on release");
            expect (e->isLiveViewShown(), "Live view after the release");
            hold.moveAway (mouseTime + 80);
            pump (40);
            // press on a chip, release outside it: no switch (like a button)
            auto& lvs = e->getLiveView()->getViewSwitcher();
            const auto p3 = screenPointOf (lvs, lvs.getChipBounds (3).getCentre());
            mouseTime += 400;
            Press outside;
            expect (outside.down (lvs, p3, mouseTime));
            allViaOs = allViaOs && outside.viaOs;
            outside.up (p3 + juce::Point<int> (0, 300), mouseTime + 60);
            pump (40);
            expectEquals (proc.getView(), 1, "released outside the chip: no switch");
            outside.moveAway (mouseTime + 80);
            pump (20);
            // every control of the Live view still takes clicks after the switch
            DownCounter dc;
            dc.watch (*e->getLiveView());
            expect (clickAt (lvs, lvs.getChipBounds (3).getCentre()));
            pump (40);
            dc.unwatch();
            expect (dc.downs >= 1, "the click after the switch reaches the Live view");
            expect (e->isPerformViewShown(), "PERFORM chip");
            auto& pvs = e->getPerformView()->getViewSwitcher();
            expect (clickAt (pvs, pvs.getChipBounds (0).getCentre()));
            pump (40);
            expect (! e->isStageViewShown(), "FULL");
            allViaOs = allViaOs && fallbackPoints == 0;

            const auto log = logFile.loadFileAsString();
            expect (log.contains ("mouse log started"), "log header");
            expect (log.contains ("view shown: View 1") && log.contains ("view shown: View 3") && log.contains ("view shown: View 0"), "view switches logged");
            if (allViaOs)
            {
                // the press lines come from the global mouse listener: only presses dispatched through the OS path
                expect (log.contains ("mouseDown"), "mouse-downs logged");
                expect (log.contains ("ChoiceChips") && log.contains ("param " + gView), "target component (view switcher, its parameter) logged");
                expect (log.contains ("desktop[0]") && log.contains ("contains(mouse)"), "windows on the desktop and the peer hit test logged");
                expect (log.contains ("focused:") && log.contains ("modal components"), "focus / modal state logged");
                expect (! log.contains ("PRESS NOT DELIVERED"), "every press reached a component");
            }
            else
            {
                logMessage ("the OS did not deliver the presses to the test window (component-level fallback): press lines of the mouse log not checked");
            }
            fallbackPoints = 0;
            e->setMouseLogEnabled (false);
            const auto size = logFile.getSize();
            expect (clickAt (e->getViewSwitcher(), e->getViewSwitcher().getChipBounds (0).getCentre()));
            pump (20);
            expectEquals (logFile.getSize(), size, "nothing logged once off");
            LiveLooperEditor::setMouseLogFileForTests ({});
            ed->removeFromDesktop();
        }
    }
};

static ViewMouseTests viewMouseTests;
