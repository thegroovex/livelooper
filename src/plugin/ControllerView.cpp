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
// Reference geometry of both devices, the same numbers as tools/illustrations/make_illustrations.py
// (Push 1: 1000 x 790 landscape; K.O. II: 880 x 1180 portrait), checked against photos of the real devices.
namespace
{
    struct RefRect { float x, y, w, h; };
    constexpr float kPushW = 1000.0f, kPushH = 790.0f;
    constexpr float kPushGridX = 151.0f, kPushColDx = 76.25f, kPushPadW = 68.0f;
    constexpr float kPushGridY = 270.0f, kPushRowDy = 60.0f, kPushPadH = 52.0f;
    constexpr float kPushRow1Y = 196.0f, kPushRow2Y = 230.0f, kPushRowH = 24.0f;
    constexpr float kPushSceneX = 784.0f, kPushSceneW = 54.0f;
    constexpr float kPushLeftX = 14.0f, kPushLeftW = 62.0f, kPushRightX0 = 852.0f, kPushRightX1 = 912.0f, kPushRightW = 54.0f;
    struct PushKey { int cc; RefRect r; const char* printed; };
    const PushKey kPushKeys[] = {
        { push::cc::tapTempo,    { kPushLeftX,  92, kPushLeftW, 44 }, "Tap Tempo" },
        { push::cc::metronome,   { kPushLeftX, 144, kPushLeftW, 40 }, "Metronome" },
        { push::cc::undo,        { kPushLeftX, 268, kPushLeftW, 32 }, "Undo" },
        { push::cc::deleteBtn,   { kPushLeftX, 306, kPushLeftW, 32 }, "Delete" },
        { push::cc::doubleBtn,   { kPushLeftX, 344, kPushLeftW, 32 }, "Double" },
        { push::cc::quantize,    { kPushLeftX, 382, kPushLeftW, 32 }, "Quantize" },
        { push::cc::fixedLength, { kPushLeftX, 444, kPushLeftW, 32 }, "Fixed Length" },
        { push::cc::automation,  { kPushLeftX, 482, kPushLeftW, 32 }, "Automation" },
        { push::cc::duplicate,   { kPushLeftX, 520, kPushLeftW, 32 }, "Duplicate" },
        { push::cc::newBtn,      { kPushLeftX, 558, kPushLeftW, 32 }, "New" },
        { push::cc::record,      { kPushLeftX, 618, kPushLeftW, 54 }, "Record" },
        { push::cc::play,        { kPushLeftX, 688, kPushLeftW, 54 }, "Play" },
        { push::cc::master,      { kPushSceneX, 194, kPushSceneW, 28 }, "Master" },
        { push::cc::stop,        { kPushSceneX, 228, kPushSceneW, 28 }, "Stop" },
        { push::cc::volume,      { kPushRightX0,  70, kPushRightW, 30 }, "Volume" },
        { push::cc::panSend,     { kPushRightX1,  70, kPushRightW, 30 }, "Pan & Send" },
        { push::cc::track,       { kPushRightX0, 106, kPushRightW, 30 }, "Track" },
        { push::cc::clip,        { kPushRightX1, 106, kPushRightW, 30 }, "Clip" },
        { push::cc::device,      { kPushRightX0, 142, kPushRightW, 30 }, "Device" },
        { push::cc::browse,      { kPushRightX1, 142, kPushRightW, 30 }, "Browse" },
        { push::cc::inBtn,       { kPushRightX0, 196, kPushRightW, 28 }, "In" },
        { push::cc::outBtn,      { kPushRightX1, 196, kPushRightW, 28 }, "Out" },
        { push::cc::mute,        { kPushRightX0, 230, kPushRightW, 28 }, "Mute" },
        { push::cc::solo,        { kPushRightX1, 230, kPushRightW, 28 }, "Solo" },
        { push::cc::scales,      { kPushRightX0, 264, kPushRightW, 30 }, "Scales" },
        { push::cc::user,        { kPushRightX1, 264, kPushRightW, 30 }, "User" },
        { push::cc::repeat,      { kPushRightX0, 300, kPushRightW, 30 }, "Repeat" },
        { push::cc::accent,      { kPushRightX1, 300, kPushRightW, 30 }, "Accent" },
        { push::cc::octaveDown,  { kPushRightX0, 336, kPushRightW, 36 }, "Octave Down" },
        { push::cc::octaveUp,    { kPushRightX1, 336, kPushRightW, 36 }, "Octave Up" },
        { push::cc::addEffect,   { kPushRightX0, 448, kPushRightW, 30 }, "Add Effect" },
        { push::cc::addTrack,    { kPushRightX1, 448, kPushRightW, 30 }, "Add Track" },
        { push::cc::note,        { kPushRightX0, 484, kPushRightW, 30 }, "Note" },
        { push::cc::session,     { kPushRightX1, 484, kPushRightW, 30 }, "Session" },
        { push::cc::select,      { kPushRightX0, 520, kPushRightW, 30 }, "Select" },
        { push::cc::shift,       { kPushRightX1, 520, kPushRightW, 30 }, "Shift" },
        { push::cc::up,          { 902, 604, 36, 36 }, "^" },
        { push::cc::down,        { 902, 688, 36, 36 }, "v" },
        { push::cc::left,        { 860, 646, 36, 36 }, "<" },
        { push::cc::right,       { 944, 646, 36, 36 }, ">" },
    };
    const char* const kPushScenePrint[] = { "1/4", "1/4t", "1/8", "1/8t", "1/16", "1/16t", "1/32", "1/32t" };

    constexpr float kKoW = 880.0f, kKoH = 1180.0f;
    constexpr float kKoPadX[] = { 265.0f, 380.0f, 495.0f };
    constexpr float kKoPadY[] = { 665.0f, 780.0f, 895.0f, 1010.0f };
    constexpr float kKoPadW = 75.0f, kKoPadH = 76.0f;
    const char* const kKoPadSmall[4][3] = { { "LEVEL", "PITCH", "TIME" }, { "LPF", "HPF", "\xe2\x86\x92" "FX" },
                                            { "ATK", "REL", "PAN" }, { "TUNE", "VEL", "MOD" } };
}

juce::Rectangle<float> ControllerView::koRef (float x, float y, float w, float h) const
{
    return { ko2Body.getX() + x * koScale, ko2Body.getY() + y * koScale, w * koScale, h * koScale };
}

juce::Rectangle<float> ControllerView::pushRef (float x, float y, float w, float h) const
{
    return { pushBody.getX() + x * pushScale, pushBody.getY() + y * pushScale, w * pushScale, h * pushScale };
}

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
        // the K.O. II stands upright (portrait) on the left; beside it the pages A - D and a large map of the 12 pads
        koScale = juce::jmin (r.getHeight() / kKoH, r.getWidth() * 0.5f / kKoW);
        ko2Body = juce::Rectangle<float> (r.getX() + 10.0f, r.getY(), kKoW * koScale, kKoH * koScale);
        ko2Display = koRef (0.0f, 268.0f, kKoW, 210.0f);
        ko2Grid = koRef (kKoPadX[0], kKoPadY[0], kKoPadX[2] + kKoPadW - kKoPadX[0], kKoPadY[3] + kKoPadH - kKoPadY[0]);
        auto side = r.withTrimmedLeft (ko2Body.getWidth() + 40.0f);
        ko2Tabs = side.removeFromTop (56.0f);
        side.removeFromTop (16.0f);
        side.removeFromBottom (46.0f);    // note on the hatched controls
        const float mapW = juce::jmin (side.getWidth(), side.getHeight() * 1.05f);
        ko2Map = side.withWidth (mapW);
    }
    else
    {
        pushScale = juce::jmin (r.getWidth() / kPushW, r.getHeight() / kPushH);
        pushBody = r.withSizeKeepingCentre (kPushW * pushScale, kPushH * pushScale);
        pushDisplay = pushRef (147.0f, 92.0f, 618.0f, 86.0f);
        pushGrid = pushRef (kPushGridX, kPushGridY, 7.0f * kPushColDx + kPushPadW, 7.0f * kPushRowDy + kPushPadH);
    }
}

juce::Rectangle<float> ControllerView::ko2PadRect (int offset) const
{
    const int row = ko2::padRow (offset), col = ko2::padColumn (offset);
    return koRef (kKoPadX[col], kKoPadY[row], kKoPadW, kKoPadH);
}

juce::Rectangle<float> ControllerView::ko2MapPadRect (int offset) const
{
    const float cw = ko2Map.getWidth() / 3.0f, ch = ko2Map.getHeight() / 4.0f;
    return { ko2Map.getX() + ko2::padColumn (offset) * cw, ko2Map.getY() + ko2::padRow (offset) * ch, cw, ch };
}

juce::Rectangle<float> ControllerView::ko2TabRect (int group) const
{
    const float w = ko2Tabs.getWidth() / 4.0f;
    return { ko2Tabs.getX() + group * w, ko2Tabs.getY(), w - 8.0f, ko2Tabs.getHeight() };
}

juce::Rectangle<float> ControllerView::ko2GroupRect (int group) const
{
    return koRef (152.0f, kKoPadY[juce::jlimit (0, 3, group)] + 5.0f, 78.0f, 72.0f);
}

juce::Rectangle<float> ControllerView::pushPadRect (int row, int col) const
{
    return pushRef (kPushGridX + col * kPushColDx, kPushGridY + (7 - row) * kPushRowDy, kPushPadW, kPushPadH);
}

juce::Rectangle<float> ControllerView::pushButtonRect (int cc) const
{
    if (cc >= push::kTopRowFirst && cc < push::kTopRowFirst + 8)
        return pushRef (kPushGridX + (cc - push::kTopRowFirst) * kPushColDx, kPushRow1Y, kPushPadW, kPushRowH);
    if (cc >= push::kBottomRowFirst && cc < push::kBottomRowFirst + 8)
        return pushRef (kPushGridX + (cc - push::kBottomRowFirst) * kPushColDx, kPushRow2Y, kPushPadW, kPushRowH);
    if (const int scene = push::sceneForCC (cc); scene >= 0)
        return pushRef (kPushSceneX, kPushGridY + scene * kPushRowDy, kPushSceneW, kPushPadH);
    for (const auto& k : kPushKeys)
        if (k.cc == cc)
            return pushRef (k.r.x, k.r.y, k.r.w, k.r.h);
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

namespace
{
    /** Diagonal hatching over a control that sends no MIDI (drawn, but not used by livelooper). */
    void drawUnused (juce::Graphics& g, juce::Rectangle<float> r, float corner)
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (r, corner);
        g.reduceClipRegion (clip);
        g.setColour (juce::Colour (0xff505050).withAlpha (0.5f));
        const float step = juce::jmax (4.0f, r.getHeight() * 0.09f);
        for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += step)
            g.drawLine (x, r.getBottom(), x + r.getHeight(), r.getY(), 1.2f);
    }

    void drawSplitButton (juce::Graphics& g, juce::Rectangle<float> r, const char* top, const char* bottom,
                          juce::Colour topC, juce::Colour botC, float fontSize)
    {
        const float c = r.getHeight() * 0.08f;
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillRoundedRectangle (r.translated (0.0f, 2.0f), c);
        auto t = r.withHeight (r.getHeight() * 0.5f), b = r.withTrimmedTop (r.getHeight() * 0.5f);
        g.setColour (topC);
        g.fillRoundedRectangle (t.withHeight (t.getHeight() + c), c);
        g.setColour (botC);
        g.fillRoundedRectangle (b, c);
        g.fillRect (b.withHeight (c));
        auto ink = [] (juce::Colour col) { return col.getPerceivedBrightness() < 0.6f ? juce::Colours::white : juce::Colour (0xff262626); };
        g.setFont (LooperLookAndFeel::font (fontSize, true));
        g.setColour (ink (topC));
        g.drawFittedText (top, t.toNearestInt(), juce::Justification::centred, 1, 0.6f);
        g.setColour (ink (botC));
        g.drawFittedText (bottom, b.toNearestInt(), juce::Justification::centred, 1, 0.6f);
    }
}

void ControllerView::paintKo2 (juce::Graphics& g, juce::Rectangle<float>)
{
    auto* dev = proc.getKo2Device();
    const int page = getShownKo2Page();
    const auto devicePage = dev != nullptr ? dev->getController().getPage() : -1;
    const float s = koScale;
    const juce::Colour cream { 0xffefebe1 }, grille { 0xff9a9a98 }, deck { 0xffd2d2d0 }, dark { 0xff2b2b2c }, light { 0xffe9e8e4 };
    const juce::Colour orange { 0xfff2541b }, ink { 0xff262626 };
    const float fs = juce::jmax (7.0f, 12.0f * s);
    const float bfs = juce::jmax (7.0f, 13.0f * s);

    // top edge: jacks / tabs
    struct Tab { const char* name; float x, w; juce::uint32 fill, ink; };
    const Tab tabs[] = { { "OUTPUT", 30, 72, 0xffe4e1da, 0xff262626 }, { "INPUT", 175, 72, 0xfff2541b, 0xffffffff },
                         { "SYNC", 285, 85, 0xff1b1b1c, 0xffe8e8e8 }, { "MIDI", 370, 88, 0xff1b1b1c, 0xffe8e8e8 },
                         { "USB", 585, 75, 0xff8f8f8d, 0xfff2f2f2 }, { "POWER", 665, 155, 0xffdedcd6, 0xff262626 } };
    g.setFont (LooperLookAndFeel::font (fs, true));
    for (const auto& t : tabs)
    {
        const auto r = koRef (t.x, 4.0f, t.w, 44.0f);
        g.setColour (juce::Colour (t.fill));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (juce::Colour (t.ink));
        g.drawFittedText (t.name, r.withTrimmedTop (r.getHeight() * 0.35f).toNearestInt(), juce::Justification::centred, 1, 0.6f);
    }
    // shadow, cream label panel, speaker grille
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (koRef (0.0f, 48.0f, kKoW, kKoH - 40.0f), 12.0f * s);
    g.setColour (cream);
    g.fillRoundedRectangle (koRef (0.0f, 40.0f, 552.0f, 234.0f), 4.0f);
    g.setColour (ink.withAlpha (0.85f));
    g.setFont (LooperLookAndFeel::font (34.0f * s));
    g.drawText ("K.O. II", koRef (34.0f, 84.0f, 300.0f, 46.0f), juce::Justification::centredLeft);
    g.setColour (grille);
    g.fillRoundedRectangle (koRef (552.0f, 40.0f, 290.0f, 228.0f), 4.0f);
    g.setColour (juce::Colour (0xff2e2e2e));
    for (int row = 0; row < 11; ++row)
        for (int col = 0; col < 15; ++col)
        {
            const auto c = koRef (570.0f + col * 18.2f, 58.0f + row * 19.5f, 0.0f, 0.0f).getPosition();
            g.fillEllipse (c.x - 5.4f * s, c.y - 5.4f * s, 10.8f * s, 10.8f * s);
        }

    // display strip: the segment display shows the tempo; the looper state below it (the device itself shows nothing of livelooper)
    g.setColour (juce::Colour (0xff121214));
    g.fillRoundedRectangle (ko2Display, 3.0f);
    auto& e = proc.getEngine();
    const double ppq = e.getTimelinePpq();
    const int bar = (int) std::floor (ppq / juce::jmax (1.0, e.getMeasurePpq())) + 1;
    const int beat = (int) std::floor (std::fmod (ppq, juce::jmax (1.0, e.getMeasurePpq()))) + 1;
    const bool record = proc.getControllerAudioMode() == 1;
    {
        auto d = ko2Display.reduced (24.0f * s, 16.0f * s);
        g.setColour (juce::Colour (0xffff6a2a));
        g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 60.0f * s, juce::Font::bold)));
        g.drawText (juce::String (juce::roundToInt (e.getCurrentBpm())), d.removeFromTop (d.getHeight() * 0.5f), juce::Justification::centred);
        g.setFont (LooperLookAndFeel::font (juce::jmax (9.0f, 15.0f * s), true));
        g.drawText (juce::String (bar) + "." + juce::String (beat) + "   " + (record ? "REC" : "CTRL") + "   "
                        + (dev != nullptr && dev->isClockRunning() ? juce::String (juce::CharPointer_UTF8 ("CLK \xe2\x86\x92 KO")) : tr ("no clock")),
                    d.removeFromTop (d.getHeight() * 0.5f), juce::Justification::centred);
        const auto last = dev != nullptr ? dev->getLastActionText() : juce::String();
        const float live = proc.getKo2LivePunchAmount();     // held punch pads: hold ramp / pressure
        g.setColour (juce::Colour (0xffff8a3a).withAlpha (0.8f));
        if (live >= 0.0f)
        {
            auto line = d;
            const auto bar = line.removeFromBottom (juce::jmax (3.0f, 7.0f * s)).reduced (d.getWidth() * 0.12f, 0.0f);
            g.drawText (last + "  " + juce::String (juce::roundToInt (live * 100.0f)) + " %", line, juce::Justification::centred);
            g.setColour (juce::Colour (0xff3a2a20));
            g.fillRoundedRectangle (bar, 2.0f);
            g.setColour (juce::Colour (0xffff6a2a));
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, live)), 2.0f);
        }
        else
            g.drawText (last.isNotEmpty() ? last : juce::String ("livelooper"), d, juce::Justification::centred);
    }

    // control deck
    const auto deckR = koRef (0.0f, 478.0f, kKoW, kKoH - 478.0f);
    g.setColour (deck);
    g.fillRoundedRectangle (deckR, 10.0f * s);
    g.setColour (juce::Colour (0xffa9a9a6));
    g.drawRoundedRectangle (deckR, 10.0f * s, 1.5f);
    g.setColour (ink.withAlpha (0.8f));
    g.setFont (LooperLookAndFeel::font (fs, true));
    g.drawText ("VOLUME", koRef (28.0f, 538.0f, 100.0f, 20.0f), juce::Justification::centred);
    g.drawText ("BPM", koRef (625.0f, 518.0f, 60.0f, 20.0f), juce::Justification::centred);
    g.drawText ("METRONOME", koRef (700.0f, 500.0f, 130.0f, 20.0f), juce::Justification::centred);
    // knobs (no MIDI)
    auto knob = [&] (float cx, float cy, float r, juce::Colour base, juce::Colour cap) {
        const auto k = koRef (cx - r, cy - r, 2.0f * r, 2.0f * r);
        g.setColour (base);
        g.fillEllipse (k);
        g.setColour (cap);
        g.fillEllipse (k.reduced (k.getWidth() * 0.22f).translated (-k.getWidth() * 0.06f, -k.getWidth() * 0.08f));
        drawUnused (g, k, k.getWidth() * 0.5f);
    };
    knob (65.0f, 600.0f, 33.0f, juce::Colour (0xfff4f2ec), juce::Colour (0xff8e8e8c));
    knob (645.0f, 574.0f, 34.0f, orange, juce::Colour (0xffff7a45));
    knob (765.0f, 565.0f, 36.0f, juce::Colour (0xff1d1d1e), juce::Colour (0xff4a4a4a));
    // SOUND / EDIT, MAIN / COMMIT, TEMPO / LOOP (no MIDI)
    drawSplitButton (g, koRef (150.0f, 558.0f, 76.0f, 72.0f), "SOUND", "EDIT", dark, light, bfs);
    drawSplitButton (g, koRef (260.0f, 558.0f, 76.0f, 72.0f), "MAIN", "COMMIT", dark, orange, bfs);
    drawSplitButton (g, koRef (372.0f, 558.0f, 76.0f, 72.0f), "TEMPO", "LOOP", dark, juce::Colour (0xff9d9d9b), bfs);
    for (float x : { 150.0f, 260.0f, 372.0f })
        drawUnused (g, koRef (x, 558.0f, 76.0f, 72.0f), 6.0f * s);
    // left column: KEYS, FADER, the fader, SHIFT (no MIDI)
    for (auto [name, y] : { std::pair<const char*, float> { "KEYS", 675.0f }, { "FADER", 750.0f } })
    {
        const auto r = koRef (45.0f, y, 70.0f, 35.0f);
        g.setColour (dark);
        g.fillRoundedRectangle (r, 4.0f * s);
        g.setColour (juce::Colours::white);
        g.setFont (LooperLookAndFeel::font (bfs, true));
        g.drawFittedText (name, r.toNearestInt(), juce::Justification::centred, 1, 0.6f);
        drawUnused (g, r, 4.0f * s);
    }
    g.setColour (juce::Colour (0xff3a3a3a));
    g.fillRoundedRectangle (koRef (76.0f, 840.0f, 8.0f, 165.0f), 4.0f * s);
    g.setColour (juce::Colour (0xfff4f2ec));
    g.fillEllipse (koRef (45.0f, 972.0f, 46.0f, 46.0f));
    drawUnused (g, koRef (44.0f, 836.0f, 72.0f, 186.0f), 8.0f * s);
    {
        const auto r = koRef (50.0f, 1058.0f, 70.0f, 32.0f);
        g.setColour (juce::Colour (0xffdcdbd6));
        g.fillRoundedRectangle (r, 4.0f * s);
        g.setColour (juce::Colour (0xff8a8a88));
        g.drawFittedText ("SHIFT", r.toNearestInt(), juce::Justification::centred, 1, 0.6f);
        drawUnused (g, r, 4.0f * s);
    }

    // group buttons A - D: the pages (LED = the page the device plays, outline = the page shown here)
    for (int grp = 0; grp < ko2::kNumGroups; ++grp)
    {
        const auto r = ko2GroupRect (grp);
        g.setColour (juce::Colours::black.withAlpha (0.22f));
        g.fillRoundedRectangle (r.translated (0.0f, 3.0f), 6.0f * s);
        g.setColour (juce::Colour (0xffe2e1dd));
        g.fillRoundedRectangle (r, 6.0f * s);
        if (grp == page)
        {
            g.setColour (orange);
            g.drawRoundedRectangle (r.expanded (2.0f), 7.0f * s, 2.5f);
        }
        g.setColour (juce::Colour (0xff8a8a88));
        g.setFont (LooperLookAndFeel::font (24.0f * s));
        g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + grp)), r.reduced (10.0f * s, 4.0f * s), juce::Justification::topLeft);
        g.setColour (grp == devicePage ? orange : juce::Colour (0xff4a4a4a));
        g.fillEllipse (koRef (167.0f, kKoPadY[grp] - 23.0f, 10.0f, 10.0f));
    }

    // number pads: dark keys; the LED bar shows the livelooper state, small labels above each row
    for (int o = 0; o < ko2::kPadsPerGroup; ++o)
    {
        const auto r = ko2PadRect (o);
        const int row = ko2::padRow (o), col = ko2::padColumn (o);
        bool active = false;
        const auto c = ko2PadColour (page, o, active);
        const int noteNum = ko2::noteFor (page, o);
        g.setColour (ink.withAlpha (0.8f));
        g.setFont (LooperLookAndFeel::font (juce::jmax (6.5f, 11.0f * s), true));
        g.drawText (juce::CharPointer_UTF8 (kKoPadSmall[row][col]), r.translated (26.0f * s, -24.0f * s).withHeight (18.0f * s).toNearestInt(), juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff4a4a4a));
        g.fillEllipse (r.getX() + 13.0f * s, r.getY() - 19.0f * s, 9.0f * s, 9.0f * s);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (r.translated (0.0f, 3.0f), 5.0f * s);
        g.setColour (dark);
        g.fillRoundedRectangle (r, 5.0f * s);
        const auto ledBar = r.reduced (6.0f * s, 0.0f).removeFromBottom (12.0f * s).withTrimmedBottom (5.0f * s);
        if (active)
        {
            g.setColour (c.withAlpha (0.35f));
            g.fillRoundedRectangle (ledBar.expanded (3.0f), 4.0f);
        }
        g.setColour (c);
        g.fillRoundedRectangle (ledBar, 2.5f);
        if (dev != nullptr && (dev->getController().isHeld (noteNum) || pressedNote == noteNum))
        {
            g.setColour (juce::Colours::white);
            g.drawRoundedRectangle (r, 5.0f * s, 2.0f);
        }
        g.setColour (juce::Colour (0xffe6e6e6));
        g.setFont (LooperLookAndFeel::font (ko2::padLabel (o).length() > 1 ? 13.0f * s : 22.0f * s));
        g.drawText (ko2::padLabel (o), r.reduced (8.0f * s, 4.0f * s), juce::Justification::topLeft);
    }

    // right: SAMPLE / CHOP, TIMING / CORRECT, FX / OUTPUT, ERASE / SYSTEM, - / +, RECORD (no MIDI), PLAY (start / stop)
    const juce::Rectangle<float> fnKeys[] = { koRef (610.0f, 650.0f, 80.0f, 80.0f), koRef (730.0f, 650.0f, 85.0f, 80.0f),
                                              koRef (615.0f, 770.0f, 80.0f, 76.0f), koRef (735.0f, 770.0f, 85.0f, 76.0f) };
    drawSplitButton (g, fnKeys[0], "SAMPLE", "CHOP", orange, light, bfs);
    drawSplitButton (g, fnKeys[1], "TIMING", "CORRECT", dark, light, bfs);
    drawSplitButton (g, fnKeys[2], "FX", "OUTPUT", dark, light, bfs);
    drawSplitButton (g, fnKeys[3], "ERASE", "SYSTEM", light, juce::Colour (0xffdcdbd6), bfs);
    for (const auto& rr : fnKeys)
        drawUnused (g, rr, 6.0f * s);
    for (auto [label, x] : { std::pair<const char*, float> { "-", 620.0f }, { "+", 740.0f } })
    {
        const auto r = koRef (x, 890.0f, 82.0f, 80.0f);
        g.setColour (juce::Colour (0xfff3f1ea));
        g.fillRoundedRectangle (r, 6.0f * s);
        g.setColour (ink);
        g.setFont (LooperLookAndFeel::font (34.0f * s));
        g.drawText (label, r, juce::Justification::centred);
        drawUnused (g, r, 6.0f * s);
    }
    {
        const auto rec = koRef (625.0f, 1010.0f, 82.0f, 80.0f), play = koRef (745.0f, 1010.0f, 82.0f, 80.0f);
        g.setColour (orange);
        g.fillRoundedRectangle (rec, 6.0f * s);
        g.setColour (juce::Colour (0xff8a8a88));
        g.fillRoundedRectangle (play, 6.0f * s);
        g.setColour (juce::Colours::white);
        g.setFont (LooperLookAndFeel::font (bfs, true));
        g.drawText ("RECORD", rec.withHeight (rec.getHeight() * 0.45f), juce::Justification::centred);
        g.drawText ("PLAY", play.withHeight (play.getHeight() * 0.45f), juce::Justification::centred);
        g.setFont (LooperLookAndFeel::font (juce::jmax (7.0f, 10.0f * s), true));
        g.drawFittedText (tr ("ALL START"), play.withTrimmedTop (play.getHeight() * 0.5f).reduced (3.0f, 0.0f).toNearestInt(), juce::Justification::centred, 2, 0.6f);
        drawUnused (g, rec, 6.0f * s);
    }

    // beside the device: the pages A - D and a large map of the shown page
    static const char* const tabNames[] = { "LOOPS", "EDIT", "PUNCH FX", "SCENES" };
    for (int grp = 0; grp < ko2::kNumGroups; ++grp)
    {
        const auto r = ko2TabRect (grp);
        const bool shown = grp == page, onDevice = grp == devicePage;
        const bool control = dev != nullptr && dev->getController().isControlGroup (grp);
        g.setColour (shown ? orange : juce::Colour (0xff2a2c31));
        g.fillRoundedRectangle (r, 10.0f);
        if (onDevice)
        {
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.drawRoundedRectangle (r.reduced (1.0f), 10.0f, 2.0f);
        }
        g.setColour (shown ? juce::Colours::white : colours::text);
        g.setFont (LooperLookAndFeel::font (20.0f, true));
        g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + grp)), r.withWidth (34.0f), juce::Justification::centred);
        g.setFont (LooperLookAndFeel::font (12.0f, true));
        juce::String name = grp == 3 && record ? tr ("COMPACT") : tr (tabNames[grp]);
        if (! control)
            name = "K.O. II";
        g.drawFittedText (name, r.withTrimmedLeft (34.0f).reduced (2.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft, 2, 0.7f);
    }
    for (int o = 0; o < ko2::kPadsPerGroup; ++o)
    {
        const auto r = ko2MapPadRect (o).reduced (5.0f);
        bool active = false;
        const auto c = ko2PadColour (page, o, active);
        const int noteNum = ko2::noteFor (page, o);
        g.setColour (juce::Colour (0xff1e1f23));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (c);
        g.fillRoundedRectangle (r.withTrimmedTop (r.getHeight() - 8.0f).reduced (8.0f, 0.0f).translated (0.0f, -6.0f), 3.0f);
        if (active)
        {
            g.setColour (c.withAlpha (0.4f));
            g.drawRoundedRectangle (r.expanded (2.0f), 10.0f, 3.0f);
        }
        if (dev != nullptr && (dev->getController().isHeld (noteNum) || pressedNote == noteNum))
        {
            g.setColour (juce::Colours::white);
            g.drawRoundedRectangle (r, 8.0f, 2.0f);
        }
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.setFont (LooperLookAndFeel::font (12.0f, true));
        g.drawText (ko2::padLabel (o), r.reduced (8.0f, 6.0f), juce::Justification::topLeft);
        g.setColour (juce::Colours::white);
        g.setFont (LooperLookAndFeel::font (juce::jlimit (10.0f, 14.0f, r.getWidth() * 0.11f), true));
        g.drawFittedText (ko2PadText (page, o), r.reduced (6.0f).withTrimmedTop (r.getHeight() * 0.25f).withTrimmedBottom (10.0f).toNearestInt(),
                          juce::Justification::centred, 2, 0.7f);
    }
    g.setColour (colours::textDim);
    g.setFont (LooperLookAndFeel::font (12.0f));
    g.drawFittedText (tr ("Group buttons switch the page on the K.O. II. Click a tab to look at another page.") + "\n"
                          + tr ("Hatched: sends no MIDI - not used by livelooper"),
                      juce::Rectangle<float> (ko2Map.getX(), ko2Map.getBottom() + 8.0f, ko2Map.getWidth(), 40.0f).toNearestInt(),
                      juce::Justification::topLeft, 3);
}

//============================================================================== Push 1
void ControllerView::paintPush (juce::Graphics& g, juce::Rectangle<float>)
{
    auto* pd = proc.getPushDevice();
    if (pd == nullptr)
        return;
    auto& surf = pd->getSurface();
    const float s = pushScale;
    const juce::Colour print { 0xffc98a48 };
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (pushBody.translated (0.0f, 8.0f), 22.0f * s);
    g.setColour (juce::Colour (0xff151518));
    g.fillRoundedRectangle (pushBody, 22.0f * s);
    g.setColour (juce::Colour (0xff34363b));
    g.drawRoundedRectangle (pushBody, 22.0f * s, 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.28f));
    g.setFont (LooperLookAndFeel::font (15.0f * s, true));
    g.drawText ("Push 1", pushRef (22.0f, 34.0f, 120.0f, 22.0f), juce::Justification::centredLeft);

    // encoders: 8 track encoders + master encoder on top, tempo / swing encoders top left
    auto encoder = [&] (float cx, float cy, float r) {
        const auto e = pushRef (cx - r, cy - r, 2.0f * r, 2.0f * r);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6a6a74), e.getX() + e.getWidth() * 0.4f, e.getY() + e.getHeight() * 0.3f,
                                                 juce::Colour (0xff1c1c21), e.getRight(), e.getBottom(), true));
        g.fillEllipse (e);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (e, 1.0f);
        g.setColour (juce::Colour (0xfff4f4f8));
        g.drawLine (e.getCentreX(), e.getCentreY(), e.getCentreX(), e.getY() + 4.0f * s, 2.0f);
    };
    for (int i = 0; i < 9; ++i)
        encoder (181.0f + i * 77.25f, 40.0f, 21.0f);
    encoder (38.0f, 222.0f, 16.0f);
    encoder (106.0f, 222.0f, 16.0f);

    // display: 4 lines x 68 characters, orange as on the device
    const auto lines = surf.buildDisplay();
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (pushDisplay, 4.0f);
    g.setColour (juce::Colour (0xff1f1206));
    g.fillRoundedRectangle (pushDisplay.reduced (3.0f), 3.0f);
    g.setColour (juce::Colour (0xffff9a2e));
    const auto txt = pushDisplay.reduced (10.0f * s, 6.0f * s);
    const float lh = txt.getHeight() / 4.0f;
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), lh * 0.86f, juce::Font::bold)));
    for (int l = 0; l < 4; ++l)
    {
        juce::String text;
        for (auto ch : lines[(size_t) l])
            text += (ch < 32 ? juce::String::charToString ((juce::juce_wchar) (ch == push::glyph::block || ch == push::glyph::fullBlock ? 0x2588 : ch == push::glyph::barLeft ? 0x258c : 0x00b7))
                             : juce::String::charToString (ch));
        g.drawFittedText (text.substring (0, kPushDisplayChars), txt.withY (txt.getY() + l * lh).withHeight (lh).toNearestInt(),
                          juce::Justification::centredLeft, 1, 0.5f);
    }

    // touch strip between the left button column and the pads
    {
        const auto st = pushRef (88.0f, 270.0f, 46.0f, 472.0f);
        g.setColour (juce::Colour (0xff0d0d10));
        g.fillRoundedRectangle (st, 5.0f * s);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawRoundedRectangle (st, 5.0f * s, 1.0f);
        g.setColour (print.withAlpha (0.35f));
        g.setFont (LooperLookAndFeel::font (juce::jmax (7.0f, 9.0f * s), true));
        g.drawFittedText (tr ("TAPE STOP"), st.reduced (2.0f).toNearestInt(), juce::Justification::centred, 3, 0.5f);
    }

    // pads (rectangular, as on the device)
    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 8; ++col)
        {
            const auto r = pushPadRect (row, col);
            const auto c = pushPadColour (surf.getPadColour (row, col));
            if (c.getBrightness() > 0.5f)
            {
                g.setColour (c.withAlpha (0.3f));
                g.fillRoundedRectangle (r.expanded (3.0f), 7.0f * s);
            }
            g.setColour (c);
            g.fillRoundedRectangle (r, 5.0f * s);
            if (pressedNote == push::padNote (row, col))
            {
                g.setColour (juce::Colours::white);
                g.drawRoundedRectangle (r, 5.0f * s, 2.0f);
            }
        }
    // labels of the two lowest rows in LOOP mode
    if (surf.getMode() == push::PushSurface::Mode::Loop)
    {
        g.setFont (LooperLookAndFeel::font (juce::jmax (7.5f, 10.0f * s), true));
        for (int col = 0; col < proc.getVisibleTracks(); ++col)
        {
            g.setColour (juce::Colours::black.withAlpha (0.75f));
            g.drawText (tr ("T%1 %2", juce::String (col + 1), "REC"), pushPadRect (0, col).reduced (4.0f), juce::Justification::centredBottom);
            g.drawText (tr ("STOP"), pushPadRect (1, col).reduced (4.0f), juce::Justification::centredBottom);
        }
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        if (proc.getVisibleTracks() <= push::PushSurface::kFreePadColumn)
        {
            g.drawText (tr ("BEAT"), pushPadRect (push::PushSurface::kBeatPadRow, 7).reduced (4.0f), juce::Justification::centredBottom);
            g.drawText (tr ("REC SET"), pushPadRect (push::PushSurface::kRecPadRow, 7).reduced (4.0f), juce::Justification::centredBottom);
        }
    }

    // buttons: printed name small on top, the livelooper function below; unused buttons hatched
    auto drawButton = [&] (int number, const juce::String& printed, const juce::String& function, juce::Colour lit) {
        const auto r = pushButtonRect (number);
        if (r.isEmpty())
            return;
        const int v = surf.getButtonState (number);
        const bool on = v >= push::button::on;
        const bool dim = v > 0 && ! on;
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 4.0f * s);
        g.setColour (on ? lit : (dim ? lit.withMultipliedBrightness (0.35f) : juce::Colour (0xff232327)));
        g.fillRoundedRectangle (r, 4.0f * s);
        if (pressedCC == number)
        {
            g.setColour (juce::Colours::white);
            g.drawRoundedRectangle (r, 4.0f * s, 1.5f);
        }
        auto inner = r.reduced (3.0f * s, 2.0f * s);
        const bool tall = r.getHeight() > 26.0f && printed.isNotEmpty() && function.isNotEmpty();
        if (printed.isNotEmpty())
        {
            g.setColour ((on ? juce::Colours::black : print).withAlpha (function.isEmpty() ? 0.9f : 0.75f));
            g.setFont (LooperLookAndFeel::font (juce::jmax (6.5f, 8.0f * s), true));
            g.drawFittedText (printed, (tall ? inner.removeFromTop (inner.getHeight() * 0.42f) : inner).toNearestInt(),
                              tall ? juce::Justification::centredLeft : juce::Justification::centred, 1, 0.5f);
        }
        if (function.isNotEmpty() && (tall || printed.isEmpty()))
        {
            g.setColour (on ? juce::Colours::black : colours::text);
            g.setFont (LooperLookAndFeel::font (juce::jmax (7.0f, 9.0f * s), true));
            g.drawFittedText (function, inner.toNearestInt(), juce::Justification::centredLeft, 1, 0.5f);
        }
        if (function.isEmpty())
            drawUnused (g, r, 4.0f * s);
    };
    for (int i = 0; i < 8; ++i)
    {
        const bool used = i < proc.getVisibleTracks();
        drawButton (push::kTopRowFirst + i, {}, used ? "FX " + juce::String (i + 1) : juce::String(), colours::orange);
        drawButton (push::kBottomRowFirst + i, {}, used ? tr ("REV") : juce::String(), colours::cyan);
        drawButton (push::ccForScene (i), kPushScenePrint[i], tr ("Scene %1", juce::String (i + 1)), colours::green);
    }
    using namespace push::cc;
    const std::pair<int, const char*> fn[] = {
        { tapTempo, "TAP" }, { metronome, "RHYTHM" }, { undo, "UNDO" }, { deleteBtn, "DELETE" }, { doubleBtn, "DOUBLE" },
        { quantize, "QUANTIZE" }, { fixedLength, "LENGTH" }, { automation, "AUTO REC" }, { duplicate, "DUPLICATE" }, { newBtn, "STORE" },
        { record, "CAPTURE" }, { play, "PLAY" }, { master, "METERS" }, { stop, "ALL STOP" }, { volume, "VOLUME" }, { panSend, "PAN" },
        { track, "PITCH" }, { clip, "FEEDBACK" }, { push::cc::device, "FX" }, { browse, "BEAT FX" }, { inBtn, "FILL" }, { outBtn, "DRUMS" },
        { mute, "MUTE" }, { solo, "SOLO" }, { scales, "STEP" }, { user, "USER" }, { repeat, "REPEAT" }, { accent, "STUTTER" },
        { octaveDown, "PITCH -" }, { octaveUp, "PITCH +" }, { addEffect, "BFX ON" }, { addTrack, "BFX HOLD" }, { note, "XY FX" },
        { session, "LOOP" }, { select, "SELECT" }, { shift, "SHIFT" }, { left, "BEAT -" }, { right, "BEAT +" }, { up, "PATTERN +" },
        { down, "PATTERN -" } };
    for (const auto& k : kPushKeys)
    {
        juce::String function;
        for (const auto& [number, label] : fn)
            if (number == k.cc)
                function = tr (label);
        const bool arrow = k.r.w == 36.0f && k.r.h == 36.0f;
        const auto lit = k.cc == record ? colours::red : (k.cc == play ? colours::green : colours::text);
        if (arrow)
        {
            // D-pad: the arrow is the printed label, the function sits next to it in the tooltip-free drawing
            drawButton (k.cc, juce::String(), juce::String(), lit);
            const auto r = pushButtonRect (k.cc);
            juce::Path p;
            p.startNewSubPath (-6.0f, 4.0f); p.lineTo (0.0f, -5.0f); p.lineTo (6.0f, 4.0f);
            const float ang = k.cc == up ? 0.0f : k.cc == down ? juce::MathConstants<float>::pi
                            : k.cc == left ? -juce::MathConstants<float>::halfPi : juce::MathConstants<float>::halfPi;
            p.applyTransform (juce::AffineTransform::scale (s).rotated (ang).translated (r.getCentreX(), r.getCentreY()));
            {
                juce::Graphics::ScopedSaveState save (g);
                juce::Path clip;
                clip.addRoundedRectangle (r, 4.0f * s);
                g.reduceClipRegion (clip);
                g.setColour (juce::Colour (0xff232327));
                g.fillRect (r);
            }
            g.setColour (print);
            g.strokePath (p, juce::PathStrokeType (2.0f * s, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            continue;
        }
        drawButton (k.cc, k.printed, function, lit);
    }
    // D-pad functions as a caption
    g.setColour (print.withAlpha (0.7f));
    g.setFont (LooperLookAndFeel::font (juce::jmax (7.0f, 9.0f * s), true));
    g.drawFittedText (tr ("BEAT - / +") + "\n" + tr ("PATTERN - / +"), pushRef (852.0f, 734.0f, 128.0f, 40.0f).toNearestInt(), juce::Justification::centred, 2, 0.6f);
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
    const auto title = tr ("Controller view") + "  -  " + (device == Device::Ko2 ? juce::String ("EP-133 K.O. II") : juce::String ("Push 1"));
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
        const char* names[] = { "EMPTY", "REC", "PLAY", "DUB", "STOP" };
        const int si = state == LoopTrack::State::Empty ? 0 : state == LoopTrack::State::Recording ? 1 : state == LoopTrack::State::Playing ? 2
                     : state == LoopTrack::State::Overdubbing ? 3 : 4;
        g.setColour (colours::text);
        g.setFont (LooperLookAndFeel::font (13.0f, true));
        g.drawText (tr ("TRACK %1", juce::String (t + 1)) + "  " + tr (names[si]), r.reduced (8.0f, 0.0f), juce::Justification::centredLeft);
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
            if (ko2TabRect (grp).contains (p) || ko2GroupRect (grp).contains (p))
            {
                setKo2Page (grp);
                return;
            }
        if (auto* dev = proc.getKo2Device())
            for (int o = 0; o < ko2::kPadsPerGroup; ++o)
                if (ko2PadRect (o).contains (p) || ko2MapPadRect (o).contains (p))
                {
                    // clicking a drawn pad = pressing it on the device (velocity from the height of the click)
                    pressedNote = ko2::noteFor (getShownKo2Page(), o);
                    const auto r = ko2PadRect (o).contains (p) ? ko2PadRect (o) : ko2MapPadRect (o);
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
