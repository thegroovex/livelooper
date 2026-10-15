#include "ControllerView.h"
#include "PluginProcessor.h"
#include "Localisation.h"
#include "controllers/Ko2.h"
#include "push/PushDevice.h"

namespace livelooper
{
using i18n::tr;
using namespace params;

namespace
{
    const juce::Colour kKoBody { 0xffd9d6cf }, kKoEdge { 0xffb7b2a8 }, kKoOrange { 0xffff6a13 }, kKoInk { 0xff2b2b2b };
    const juce::Colour kPushBody { 0xff1b1c1f }, kPushEdge { 0xff34363b };
    constexpr int kPushDisplayChars = 68;
}

ControllerView::ControllerView (LiveLooperProcessor& p) : proc (p)
{
    setOpaque (true);
    startTimerHz (20);
}

ControllerView::~ControllerView()
{
    stopTimer();
}

void ControllerView::setDevice (Device d)
{
    device = d;
    resized();
    repaint();
}

int ControllerView::getShownKo2Page() const
{
    if (viewPage >= 0)
        return viewPage;
    if (auto* k = proc.getKo2Device())
        return juce::jmax (0, k->getController().getPage());
    return 0;
}

bool ControllerView::beatOn (double duty) const
{
    auto& e = proc.getEngine();
    if (! e.isTimelineRunning())
        return false;
    const double ppq = e.getTimelinePpq();
    return ppq - std::floor (ppq) < duty;
}

void ControllerView::timerCallback()
{
    repaint();
}

juce::Colour ControllerView::pushPadColour (int v)
{
    // Push 1 palette: bright entries at 1 + 4n (red 5, orange 9 ...), the dim variant two above
    if (v <= 0)
        return juce::Colour (0xff1e2024);
    if (v == 1) return juce::Colour (0xff55585e);
    if (v == 3) return juce::Colours::white;
    struct E { int bright; juce::uint32 argb; };
    static const E table[] = { { 5, 0xffff3b30 }, { 9, 0xffff9500 }, { 13, 0xffffd60a }, { 17, 0xffa8e23a }, { 21, 0xff30d158 },
                               { 25, 0xff30d158 }, { 29, 0xff3ee0b0 }, { 33, 0xff3ee0d0 }, { 37, 0xff40c8ff }, { 41, 0xff4a9dff },
                               { 45, 0xff5e6bff }, { 49, 0xff9d6bff }, { 53, 0xffff4fd8 }, { 57, 0xffff6f91 } };
    for (const auto& e : table)
    {
        if (v == e.bright)
            return juce::Colour (e.argb);
        if (v == e.bright + 2)
            return juce::Colour (e.argb).withMultipliedBrightness (0.38f);
    }
    return juce::Colour (0xff6a6d73);
}

//============================================================================== layout
void ControllerView::resized()
{
    auto r = getLocalBounds().toFloat().reduced (24.0f);
    auto top = r.removeFromTop (40.0f);
    backButton = top.removeFromLeft (150.0f).reduced (0.0f, 4.0f);
    switchButton = bothConnected ? top.removeFromRight (220.0f).reduced (0.0f, 4.0f) : juce::Rectangle<float>();
    r.removeFromTop (10.0f);
    r.removeFromBottom (60.0f);           // status strip
    deviceArea = r;

    if (device == Device::Ko2)
    {
        // the K.O. II is roughly 2 : 3 (portrait); scale to the height
        const float h = r.getHeight(), w = juce::jmin (r.getWidth() * 0.62f, h * 0.78f);
        ko2Body = r.withSizeKeepingCentre (w, h).translated (-90.0f, 0.0f);
        auto b = ko2Body.reduced (w * 0.06f);
        ko2Display = b.removeFromTop (h * 0.16f);
        b.removeFromTop (h * 0.17f);       // knobs + buttons row
        ko2Grid = b.removeFromLeft (b.getWidth() * 0.72f).withTrimmedTop (h * 0.02f);
        ko2Tabs = juce::Rectangle<float> (ko2Body.getRight() + 24.0f, ko2Grid.getY(), 170.0f, ko2Grid.getHeight());
    }
    else
    {
        const float w = juce::jmin (r.getWidth(), r.getHeight() * 1.45f);
        pushBody = r.withSizeKeepingCentre (w, r.getHeight());
        auto b = pushBody.reduced (w * 0.03f, 14.0f);
        b.removeFromTop (44.0f);           // encoders
        pushDisplay = b.removeFromTop (b.getHeight() * 0.13f).reduced (w * 0.1f, 4.0f);
        b.removeFromTop (70.0f);           // two button rows
        const float side = juce::jmin (b.getHeight(), b.getWidth() * 0.62f);
        pushGrid = juce::Rectangle<float> (pushBody.getCentreX() - side / 2.0f, b.getY(), side, side);
    }
}

juce::Rectangle<float> ControllerView::ko2PadRect (int offset) const
{
    const float cw = ko2Grid.getWidth() / 3.0f, ch = ko2Grid.getHeight() / 4.0f;
    return { ko2Grid.getX() + ko2::padColumn (offset) * cw, ko2Grid.getY() + ko2::padRow (offset) * ch, cw, ch };
}

juce::Rectangle<float> ControllerView::ko2TabRect (int group) const
{
    const float h = juce::jmin (64.0f, ko2Tabs.getHeight() / 4.0f);
    return { ko2Tabs.getX(), ko2Tabs.getY() + group * (h + 10.0f), ko2Tabs.getWidth(), h };
}

juce::Rectangle<float> ControllerView::pushPadRect (int row, int col) const
{
    const float c = pushGrid.getWidth() / 8.0f;
    return { pushGrid.getX() + col * c, pushGrid.getBottom() - (row + 1) * c, c, c };
}

juce::Rectangle<float> ControllerView::pushButtonRect (int cc) const
{
    const float c = pushGrid.getWidth() / 8.0f;
    if (cc >= push::kTopRowFirst && cc < push::kTopRowFirst + 8)
        return { pushGrid.getX() + (cc - push::kTopRowFirst) * c, pushGrid.getY() - 66.0f, c, 28.0f };
    if (cc >= push::kBottomRowFirst && cc < push::kBottomRowFirst + 8)
        return { pushGrid.getX() + (cc - push::kBottomRowFirst) * c, pushGrid.getY() - 34.0f, c, 28.0f };
    if (const int scene = push::sceneForCC (cc); scene >= 0)
        return { pushGrid.getRight() + 10.0f, pushGrid.getY() + scene * c, c * 0.9f, c };
    // a few function buttons on the left / right
    static const int left[] = { push::cc::tapTempo, push::cc::metronome, push::cc::undo, push::cc::deleteBtn, push::cc::doubleBtn,
                                push::cc::quantize, push::cc::fixedLength, push::cc::automation, push::cc::duplicate, push::cc::newBtn,
                                push::cc::record, push::cc::play };
    static const int right[] = { push::cc::master, push::cc::stop, push::cc::volume, push::cc::panSend, push::cc::track, push::cc::clip,
                                 push::cc::device, push::cc::browse, push::cc::session, push::cc::note, push::cc::scales, push::cc::shift };
    const float bh = pushGrid.getHeight() / 12.0f;
    for (int i = 0; i < 12; ++i)
    {
        if (left[i] == cc)
            return { pushGrid.getX() - 10.0f - c * 1.3f, pushGrid.getY() + i * bh, c * 1.3f, bh - 4.0f };
        if (right[i] == cc)
            return { pushGrid.getRight() + 20.0f + c * 0.9f, pushGrid.getY() + i * bh, c * 1.3f, bh - 4.0f };
    }
    return {};
}

//============================================================================== K.O. II
juce::Colour ControllerView::ko2PadColour (int group, int offset, bool& active) const
{
    using K = ko2::Action::Kind;
    active = false;
    auto* dev = proc.getKo2Device();
    if (dev == nullptr || ! dev->getController().isControlGroup (group))
        return juce::Colour (0xffbdb8ae);                 // a K.O. II instrument pad (not a livelooper control)
    int index = -1;
    const auto kind = dev->getController().functionOf (group, offset, index);
    auto& e = proc.getEngine();
    auto trackColour = [&] (int t) -> juce::Colour {
        if (t < 0 || t >= proc.getVisibleTracks())
            return juce::Colour (0xff4a4a4a);
        const auto& st = e.getTrackStatus (t);
        const auto state = (LoopTrack::State) st.state.load();
        const auto pending = (LooperEngine::Pending) st.pending.load();
        if (pending != LooperEngine::Pending::None)
            return beatOn (0.5) ? colours::yellow : colours::yellow.withMultipliedBrightness (0.4f);
        switch (state)
        {
            case LoopTrack::State::Recording:   active = true; return beatOn (0.5) ? colours::red : colours::red.withMultipliedBrightness (0.45f);
            case LoopTrack::State::Overdubbing: active = true; return colours::orange;
            case LoopTrack::State::Playing:     active = true; return colours::green;
            case LoopTrack::State::Stopped:     return colours::green.withMultipliedBrightness (0.4f);
            case LoopTrack::State::Empty:       break;
        }
        return juce::Colour (0xff3a3a3a);
    };
    switch (kind)
    {
        case K::RecPlay: return trackColour (index);
        case K::Stop:
            if (index >= 0)
                return e.getTrack (index).isEmpty() ? juce::Colour (0xff3a3a3a) : colours::yellow.withMultipliedBrightness (0.55f);
            return colours::yellow.withMultipliedBrightness (0.55f);
        case K::Punch:
            active = e.getPunchFx().isOn (index);
            return active ? colours::cyan : juce::Colour (0xff3d3350);
        case K::Scene:
            active = proc.getLastScene() == index;
            return active ? colours::green : (proc.sceneIsStored (index) ? colours::green.withMultipliedBrightness (0.4f) : juce::Colour (0xff3a3a3a));
        case K::Rhythm:   active = proc.getState().getRawParameterValue (gRhythmOn)->load() > 0.5f; return active ? colours::orange : juce::Colour (0xff4a3a2a);
        case K::Recorder: active = proc.isPerformanceRecording(); return active ? (beatOn() ? colours::red : colours::red.withMultipliedBrightness (0.4f)) : juce::Colour (0xff4a2a2a);
        case K::Metronome: active = proc.getState().getRawParameterValue (gMetroOn)->load() > 0.5f; return active ? colours::yellow : juce::Colour (0xff4a4530);
        case K::AllStartStop: case K::AllStop: case K::Capture: case K::Tap: case K::Undo: case K::Redo: case K::Fill:
        case K::Resample: case K::Clear: case K::Mute: case K::AllStart: case K::SceneStore: case K::RoleSwitch:
            return juce::Colour (0xff3b4250);
        case K::None:
            break;
    }
    return juce::Colour (0xff3a3a3a);
}

juce::String ControllerView::ko2PadText (int group, int offset) const
{
    using K = ko2::Action::Kind;
    auto* dev = proc.getKo2Device();
    if (dev == nullptr || ! dev->getController().isControlGroup (group))
        return "K.O. II";
    int index = -1;
    const auto kind = dev->getController().functionOf (group, offset, index);
    const auto n = juce::String (index + 1);
    switch (kind)
    {
        case K::RecPlay:      return tr ("T%1 %2", n, "REC");
        case K::Stop:         return index >= 0 ? tr ("STOP") + " " + n : tr ("STOP") + " / " + tr ("CLEAR");
        case K::Undo:         return tr ("UNDO") + " / " + tr ("REDO");
        case K::AllStartStop: return tr ("ALL START / STOP");
        case K::AllStop:      return tr ("ALL STOP");
        case K::Capture:      return tr ("CAPTURE");
        case K::Tap:          return tr ("TAP");
        case K::Metronome:    return tr ("METRONOME");
        case K::Punch:        return tr (getPunchFxName (index));
        case K::Scene:        return tr ("Scene %1", n);
        case K::Rhythm:       return tr ("RHYTHM");
        case K::Fill:         return tr ("FILL");
        case K::Recorder:     return tr ("REC SET");
        case K::Resample:     return tr ("RESAMPLE");
        case K::None:         return group == 1 ? "SHIFT" : tr ("ROLE (+ENTER)");
        case K::Redo: case K::Clear: case K::Mute: case K::AllStart: case K::SceneStore: case K::RoleSwitch:
            break;
    }
    return ko2::describe (kind, index);
}

void ControllerView::paintKo2 (juce::Graphics& g, juce::Rectangle<float>)
{
    auto* dev = proc.getKo2Device();
    const int page = getShownKo2Page();
    const auto devicePage = dev != nullptr ? dev->getController().getPage() : -1;

    // body
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (ko2Body.translated (0.0f, 8.0f), 22.0f);
    g.setGradientFill (juce::ColourGradient (kKoBody.brighter (0.08f), ko2Body.getX(), ko2Body.getY(), kKoBody.darker (0.06f), ko2Body.getX(), ko2Body.getBottom(), false));
    g.fillRoundedRectangle (ko2Body, 22.0f);
    g.setColour (kKoEdge);
    g.drawRoundedRectangle (ko2Body, 22.0f, 2.0f);

    // display: mirror of the looper
    g.setColour (juce::Colour (0xff101214));
    g.fillRoundedRectangle (ko2Display, 8.0f);
    auto& e = proc.getEngine();
    const double ppq = e.getTimelinePpq();
    const int bar = (int) std::floor (ppq / juce::jmax (1.0, e.getMeasurePpq())) + 1;
    const int beat = (int) std::floor (std::fmod (ppq, juce::jmax (1.0, e.getMeasurePpq()))) + 1;
    const bool record = proc.getControllerAudioMode() == 1;
    g.setColour (juce::Colour (0xffff8a3a));
    g.setFont (LooperLookAndFeel::font (ko2Display.getHeight() * 0.26f, true));
    auto dl = ko2Display.reduced (14.0f, 8.0f);
    g.drawText (juce::String (bar) + "." + juce::String (beat) + "   " + juce::String (e.getCurrentBpm(), 1) + " BPM   " + (record ? "REC" : "CTRL"),
                dl.removeFromTop (dl.getHeight() * 0.5f), juce::Justification::centredLeft);
    g.setFont (LooperLookAndFeel::font (ko2Display.getHeight() * 0.2f));
    const auto last = dev != nullptr ? dev->getLastActionText() : juce::String();
    g.drawText ((last.isNotEmpty() ? last : juce::String ("livelooper")) + "    "
                    + (dev != nullptr && dev->isClockRunning() ? juce::String (juce::CharPointer_UTF8 ("CLK \xe2\x86\x92 KO")) : tr ("no clock")),
                dl, juce::Justification::centredLeft);

    // knobs X / Y and the fader: they send no MIDI
    const float rowY = ko2Display.getBottom() + ko2Body.getHeight() * 0.03f;
    const float knob = ko2Body.getHeight() * 0.075f;
    for (int i = 0; i < 2; ++i)
    {
        juce::Rectangle<float> k (ko2Body.getX() + ko2Body.getWidth() * (0.12f + 0.2f * i), rowY, knob, knob);
        g.setColour (juce::Colour (0xffa9a49a));
        g.fillEllipse (k);
        g.setColour (kKoInk.withAlpha (0.5f));
        g.setFont (LooperLookAndFeel::font (11.0f, true));
        g.drawText (i == 0 ? "X" : "Y", k, juce::Justification::centred);
    }
    g.setColour (kKoInk.withAlpha (0.55f));
    g.setFont (LooperLookAndFeel::font (11.0f));
    g.drawText (tr ("sends no MIDI - not used"), juce::Rectangle<float> (ko2Body.getX() + ko2Body.getWidth() * 0.5f, rowY, ko2Body.getWidth() * 0.45f, knob),
                juce::Justification::centredLeft);
    // fader (grey) right of the pads
    auto fader = juce::Rectangle<float> (ko2Grid.getRight() + ko2Body.getWidth() * 0.05f, ko2Grid.getY(), ko2Body.getWidth() * 0.07f, ko2Grid.getHeight() * 0.8f);
    g.setColour (juce::Colour (0xffb9b4aa));
    g.fillRoundedRectangle (fader, 6.0f);
    g.setColour (juce::Colour (0xff8f8a80));
    g.fillRoundedRectangle (fader.withSizeKeepingCentre (fader.getWidth() * 1.3f, 14.0f), 3.0f);

    // pads
    for (int o = 0; o < ko2::kPadsPerGroup; ++o)
    {
        const auto r = ko2PadRect (o).reduced (6.0f);
        bool active = false;
        const auto c = ko2PadColour (page, o, active);
        const int noteNum = ko2::noteFor (page, o);
        g.setColour (juce::Colour (0xff8d887e));
        g.fillRoundedRectangle (r.translated (0.0f, 3.0f), 8.0f);
        g.setColour (c);
        g.fillRoundedRectangle (r, 8.0f);
        if (active)
        {
            g.setColour (c.withAlpha (0.35f));
            g.drawRoundedRectangle (r.expanded (3.0f), 10.0f, 3.0f);
        }
        if (dev != nullptr && (dev->getController().isHeld (noteNum) || pressedNote == noteNum))
        {
            g.setColour (juce::Colours::white);
            g.drawRoundedRectangle (r, 8.0f, 2.5f);
        }
        // the printed pad label (".", "0", "ENTER", "1" .. "9") and the livelooper function
        const bool lightPad = c.getPerceivedBrightness() > 0.55f;
        g.setColour ((lightPad ? kKoInk : juce::Colours::white).withAlpha (0.6f));
        g.setFont (LooperLookAndFeel::font (12.0f, true));
        g.drawText (ko2::padLabel (o), r.reduced (8.0f, 6.0f), juce::Justification::topLeft);
        g.setColour (lightPad ? kKoInk : juce::Colours::white);
        g.setFont (LooperLookAndFeel::font (juce::jlimit (10.0f, 14.0f, r.getWidth() * 0.12f), true));
        g.drawFittedText (ko2PadText (page, o), r.reduced (6.0f).withTrimmedTop (r.getHeight() * 0.3f).toNearestInt(),
                          juce::Justification::centred, 2, 0.75f);
    }

    // page tabs A - D (the groups)
    static const char* const tabNames[] = { "LOOPS", "EDIT", "PUNCH FX", "SCENES" };
    for (int grp = 0; grp < ko2::kNumGroups; ++grp)
    {
        const auto r = ko2TabRect (grp);
        const bool shown = grp == page, onDevice = grp == devicePage;
        const bool control = dev != nullptr && dev->getController().isControlGroup (grp);
        g.setColour (shown ? kKoOrange : juce::Colour (0xff2a2c31));
        g.fillRoundedRectangle (r, 10.0f);
        if (onDevice)
        {
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.drawRoundedRectangle (r.reduced (1.0f), 10.0f, 2.0f);
        }
        g.setColour (shown ? juce::Colours::white : colours::text);
        g.setFont (LooperLookAndFeel::font (22.0f, true));
        g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + grp)), r.withWidth (44.0f), juce::Justification::centred);
        g.setFont (LooperLookAndFeel::font (13.0f, true));
        juce::String name = grp == 3 && proc.getControllerAudioMode() == 1 ? tr ("COMPACT") : tr (tabNames[grp]);
        if (! control)
            name = "K.O. II";
        g.drawText (name, r.withTrimmedLeft (44.0f), juce::Justification::centredLeft);
    }
    g.setColour (colours::textDim);
    g.setFont (LooperLookAndFeel::font (12.0f));
    g.drawFittedText (tr ("Group buttons switch the page on the K.O. II. Click a tab to look at another page."),
                      ko2Tabs.withTop (ko2TabRect (3).getBottom() + 16.0f).withHeight (60.0f).toNearestInt(), juce::Justification::topLeft, 3);
}

//============================================================================== Push 1
void ControllerView::paintPush (juce::Graphics& g, juce::Rectangle<float>)
{
    auto* pd = proc.getPushDevice();
    if (pd == nullptr)
        return;
    auto& s = pd->getSurface();
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (pushBody.translated (0.0f, 8.0f), 16.0f);
    g.setColour (kPushBody);
    g.fillRoundedRectangle (pushBody, 16.0f);
    g.setColour (kPushEdge);
    g.drawRoundedRectangle (pushBody, 16.0f, 2.0f);

    const auto lines = s.buildDisplay();
    // encoders over the display
    const float encW = pushDisplay.getWidth() / 8.0f;
    for (int i = 0; i < 8; ++i)
    {
        auto er = juce::Rectangle<float> (pushDisplay.getX() + i * encW, pushDisplay.getY() - 42.0f, encW, 34.0f);
        g.setColour (juce::Colour (0xff45484e));
        g.fillEllipse (er.withSizeKeepingCentre (28.0f, 28.0f));
        g.setColour (juce::Colour (0xff8a8e96));
        g.drawEllipse (er.withSizeKeepingCentre (28.0f, 28.0f), 1.5f);
    }
    // display: 4 lines x 68 characters, as Push shows them
    g.setColour (juce::Colour (0xff0b1a0f));
    g.fillRoundedRectangle (pushDisplay, 6.0f);
    g.setColour (juce::Colour (0xff9dff9d));
    const float lh = pushDisplay.getHeight() / 4.0f;
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), lh * 0.78f, juce::Font::plain)));
    for (int l = 0; l < 4; ++l)
    {
        juce::String text;
        for (auto ch : lines[(size_t) l])
            text += (ch < 32 ? juce::String::charToString ((juce::juce_wchar) (ch == push::glyph::block || ch == push::glyph::fullBlock ? 0x2588 : 0x00b7))
                             : juce::String::charToString (ch));
        g.drawFittedText (text.substring (0, kPushDisplayChars), pushDisplay.reduced (8.0f, 0.0f).withY (pushDisplay.getY() + l * lh).withHeight (lh).toNearestInt(),
                          juce::Justification::centredLeft, 1, 0.5f);
    }

    // pads
    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 8; ++col)
        {
            const auto r = pushPadRect (row, col).reduced (4.0f);
            g.setColour (pushPadColour (s.getPadColour (row, col)));
            g.fillRoundedRectangle (r, 5.0f);
            if (pressedNote == push::padNote (row, col))
            {
                g.setColour (juce::Colours::white);
                g.drawRoundedRectangle (r, 5.0f, 2.0f);
            }
        }
    // labels of the two lowest rows in LOOP mode
    if (s.getMode() == push::PushSurface::Mode::Loop)
    {
        g.setFont (LooperLookAndFeel::font (10.0f, true));
        for (int col = 0; col < proc.getVisibleTracks(); ++col)
        {
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.drawText (tr ("T%1 %2", juce::String (col + 1), "REC"), pushPadRect (0, col).reduced (6.0f), juce::Justification::centredBottom);
            g.drawText (tr ("STOP"), pushPadRect (1, col).reduced (6.0f), juce::Justification::centredBottom);
        }
        if (proc.getVisibleTracks() <= push::PushSurface::kFreePadColumn)
        {
            g.drawText (tr ("BEAT"), pushPadRect (push::PushSurface::kBeatPadRow, 7).reduced (6.0f), juce::Justification::centredBottom);
            g.drawText (tr ("REC SET"), pushPadRect (push::PushSurface::kRecPadRow, 7).reduced (6.0f), juce::Justification::centredBottom);
        }
    }

    // buttons: top / lower row, scene buttons, function buttons with their LED state
    auto drawButton = [&] (int number, const juce::String& label, juce::Colour lit) {
        const auto r = pushButtonRect (number);
        if (r.isEmpty())
            return;
        const int v = s.getButtonState (number);
        const bool on = v >= push::button::on;
        const bool dim = v > 0 && ! on;
        g.setColour (on ? lit : (dim ? lit.withMultipliedBrightness (0.3f) : juce::Colour (0xff2a2c31)));
        g.fillRoundedRectangle (r.reduced (2.0f), 4.0f);
        g.setColour (on ? juce::Colours::black : colours::text.withAlpha (0.8f));
        g.setFont (LooperLookAndFeel::font (9.5f, true));
        g.drawFittedText (label, r.reduced (3.0f).toNearestInt(), juce::Justification::centred, 2, 0.6f);
    };
    for (int i = 0; i < 8; ++i)
    {
        drawButton (push::kTopRowFirst + i, i < proc.getVisibleTracks() ? "FX " + juce::String (i + 1) : juce::String(), colours::orange);
        drawButton (push::kBottomRowFirst + i, i < proc.getVisibleTracks() ? tr ("REV") : juce::String(), colours::cyan);
        drawButton (push::ccForScene (i), tr ("Scene %1", juce::String (i + 1)), colours::green);
    }
    using namespace push::cc;
    const std::pair<int, const char*> fn[] = { { tapTempo, "TAP" }, { metronome, "RHYTHM" }, { undo, "UNDO" }, { deleteBtn, "DELETE" },
                                               { doubleBtn, "DOUBLE" }, { quantize, "QUANTIZE" }, { fixedLength, "LENGTH" }, { automation, "AUTO REC" },
                                               { duplicate, "DUPLICATE" }, { newBtn, "STORE" }, { record, "CAPTURE" }, { play, "PLAY" },
                                               { master, "METERS" }, { stop, "STOP" }, { volume, "VOLUME" }, { panSend, "PAN" }, { track, "PITCH" },
                                               { clip, "FEEDBACK" }, { device, "FX" }, { browse, "BEAT FX" }, { session, "LOOP" }, { note, "XY FX" },
                                               { scales, "STEP" }, { shift, "SHIFT" } };
    for (const auto& [number, label] : fn)
        drawButton (number, tr (label), number == record || number == play ? (number == record ? colours::red : colours::green) : colours::text);
}

//============================================================================== frame
void ControllerView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0e0f12));
    // header: back to the standard GUI, device switch
    g.setColour (juce::Colour (0xff24262b));
    g.fillRoundedRectangle (backButton, 10.0f);
    g.setColour (colours::text);
    g.setFont (LooperLookAndFeel::font (14.0f, true));
    g.drawText (tr (juce::String::fromUTF8 ("\xe2\x97\x80 Standard")), backButton, juce::Justification::centred);
    g.setFont (LooperLookAndFeel::font (18.0f, true));
    const auto title = tr ("Controller view") + "  -  " + (device == Device::Ko2 ? juce::String ("EP-133 K.O. II") : juce::String ("Ableton Push 1"));
    g.drawText (title, backButton.withX (backButton.getRight() + 20.0f).withWidth (600.0f), juce::Justification::centredLeft);
    if (! switchButton.isEmpty())
    {
        g.setColour (juce::Colour (0xff24262b));
        g.fillRoundedRectangle (switchButton, 10.0f);
        g.setColour (colours::text);
        g.setFont (LooperLookAndFeel::font (14.0f, true));
        g.drawText (device == Device::Ko2 ? tr ("Show Push") : tr ("Show K.O. II"), switchButton, juce::Justification::centred);
    }

    if (device == Device::Ko2)
        paintKo2 (g, deviceArea);
    else
        paintPush (g, deviceArea);

    // status strip: tracks
    auto strip = getLocalBounds().toFloat().reduced (24.0f).removeFromBottom (50.0f);
    const int n = proc.getVisibleTracks();
    const float w = strip.getWidth() / (float) n;
    for (int t = 0; t < n; ++t)
    {
        auto r = juce::Rectangle<float> (strip.getX() + t * w, strip.getY(), w, strip.getHeight()).reduced (4.0f);
        const auto& st = proc.getEngine().getTrackStatus (t);
        const auto state = (LoopTrack::State) st.state.load();
        g.setColour (juce::Colour (0xff1c1e22));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (colours::track (t).withAlpha (0.9f));
        g.fillRoundedRectangle (r.removeFromLeft (6.0f), 3.0f);
        const char* names[] = { "empty", "REC", "PLAY", "DUB", "stop" };
        const int si = state == LoopTrack::State::Empty ? 0 : state == LoopTrack::State::Recording ? 1 : state == LoopTrack::State::Playing ? 2
                     : state == LoopTrack::State::Overdubbing ? 3 : 4;
        g.setColour (colours::text);
        g.setFont (LooperLookAndFeel::font (13.0f, true));
        g.drawText (tr ("T%1 %2", juce::String (t + 1), names[si]), r.reduced (8.0f, 0.0f), juce::Justification::centredLeft);
        if (state != LoopTrack::State::Empty)
        {
            g.setColour (colours::track (t).withAlpha (0.5f));
            g.fillRect (r.removeFromBottom (3.0f).withWidth (r.getWidth() * juce::jlimit (0.0f, 1.0f, st.progress.load())));
        }
    }
}

void ControllerView::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position;
    if (backButton.contains (p))
    {
        if (onStandard) onStandard();
        return;
    }
    if (switchButton.contains (p))
    {
        if (onSwitchDevice) onSwitchDevice (device == Device::Ko2 ? Device::Push : Device::Ko2);
        return;
    }
    if (device == Device::Ko2)
    {
        for (int grp = 0; grp < ko2::kNumGroups; ++grp)
            if (ko2TabRect (grp).contains (p))
            {
                setKo2Page (grp);
                return;
            }
        if (auto* dev = proc.getKo2Device())
            for (int o = 0; o < ko2::kPadsPerGroup; ++o)
                if (ko2PadRect (o).contains (p))
                {
                    // clicking a drawn pad = pressing it on the device (velocity from the height of the click)
                    pressedNote = ko2::noteFor (getShownKo2Page(), o);
                    const auto r = ko2PadRect (o);
                    const int vel = juce::jlimit (30, 127, juce::roundToInt (127.0f * (1.0f - (p.y - r.getY()) / r.getHeight() * 0.6f)));
                    dev->injectMessage (juce::MidiMessage::noteOn (1, pressedNote, (juce::uint8) vel));
                    repaint();
                    return;
                }
        return;
    }
    if (auto* pd = proc.getPushDevice())
    {
        for (int row = 0; row < 8; ++row)
            for (int col = 0; col < 8; ++col)
                if (pushPadRect (row, col).contains (p))
                {
                    pressedNote = push::padNote (row, col);
                    pd->getSurface().handleMidi (juce::MidiMessage::noteOn (1, pressedNote, (juce::uint8) 100));
                    return;
                }
        for (int number = 0; number < 128; ++number)
            if (pushButtonRect (number).contains (p))
            {
                pressedCC = number;
                pd->getSurface().handleMidi (juce::MidiMessage::controllerEvent (1, number, 127));
                return;
            }
    }
}

void ControllerView::mouseUp (const juce::MouseEvent&)
{
    if (device == Device::Ko2)
    {
        if (auto* dev = proc.getKo2Device(); dev != nullptr && pressedNote >= 0)
            dev->injectMessage (juce::MidiMessage::noteOff (1, pressedNote));
    }
    else if (auto* pd = proc.getPushDevice())
    {
        if (pressedNote >= 0)
            pd->getSurface().handleMidi (juce::MidiMessage::noteOff (1, pressedNote));
        if (pressedCC >= 0)
            pd->getSurface().handleMidi (juce::MidiMessage::controllerEvent (1, pressedCC, 0));
    }
    pressedNote = pressedCC = -1;
    repaint();
}
} // namespace livelooper
