// v1.7 Performance view (branch v1.7-wip): compact track row + large performance FX area.
#include "PerformView.h"
#include "Parameters.h"

namespace livelooper
{
using namespace params;
using i18n::tr;

namespace
{
    constexpr int kHeadH = 64, kGap = 12;
    constexpr const char* kPunchShort[kNumPunchFx] = { "SCATTER", "SWAP", "GRAINS", "REPEAT", "TAPE STOP", "WOBBLE",
                                                       "LOW-PASS", "HIGH-PASS", "SEND", "TREMOLO", "OCT DROP", "DECIMATE" };

    /** Punch pad colours: orange - pink - purple across the 12 pads (the brand gradient). */
    juce::Colour punchColour (int i)
    {
        const float p = (float) i / (float) (kNumPunchFx - 1);
        return p < 0.5f ? colours::brand0.interpolatedWith (colours::brand1, p * 2.0f)
                        : colours::brand1.interpolatedWith (colours::brand2, (p - 0.5f) * 2.0f);
    }

    void setParamNorm (LiveLooperProcessor& proc, const juce::String& id, float v)
    {
        if (auto* p = proc.getState().getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (v);
            p->endChangeGesture();
        }
    }

    float rawParam (LiveLooperProcessor& proc, const juce::String& id)
    {
        return proc.getState().getRawParameterValue (id)->load();
    }

    juce::String paramText (LiveLooperProcessor& proc, const juce::String& id)
    {
        auto* p = proc.getState().getParameter (id);
        return p != nullptr ? p->getCurrentValueAsText() : juce::String();
    }
}

const char* PerformView::punchLabel (int i) noexcept { return juce::isPositiveAndBelow (i, kNumPunchFx) ? kPunchShort[i] : ""; }

//============================================================================== XY field
/** The XY field of the punch FX: hold = the selected punch FX on, position = X / Y (like the PUNCH FX
    panel of the full surface), with a short trail, crosshair and the puck in the brand gradient. */
class PerformView::XYPad : public juce::Component, public juce::SettableTooltipClient
{
public:
    XYPad (PerformView& v) : view (v)
    {
        setTooltip (tr ("XY: hold to play the selected punch FX, move for X / Y (MIDI learn: the X and Y captions)"));
    }
    void paint (juce::Graphics& g) override
    {
        auto& proc = view.proc;
        const bool light = theme::isLight();
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (light ? juce::Colour (0xff1a1c22) : juce::Colour (0xff0b0c10));
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        for (int i = 1; i < 8; ++i)
        {
            g.fillRect (r.getX() + r.getWidth() * (float) i / 8.0f, r.getY() + 6.0f, 1.0f, r.getHeight() - 12.0f);
            g.fillRect (r.getX() + 6.0f, r.getY() + r.getHeight() * (float) i / 8.0f, r.getWidth() - 12.0f, 1.0f);
        }
        const int punch = view.shownPunch();
        const float x = rawParam (proc, gPfxX), y = rawParam (proc, gPfxY);
        const auto field = r.reduced (8.0f);
        const auto c = juce::Point<float> (field.getX() + x * field.getWidth(), field.getBottom() - y * field.getHeight());
        const bool on = proc.getEngine().getPunchFx().isOn (punch);
        // trail of the last positions while held
        if (trail.size() > 1)
        {
            juce::Path p;
            p.startNewSubPath (trail.front());
            for (size_t i = 1; i < trail.size(); ++i)
                p.lineTo (trail[i]);
            g.setGradientFill (juce::ColourGradient (colours::brand0.withAlpha (0.0f), trail.front(), colours::brand1, trail.back(), false));
            g.strokePath (p, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (colours::brand1.withAlpha (on ? 0.8f : 0.45f));
        g.fillRect (c.x - 0.75f, r.getY() + 8.0f, 1.5f, r.getHeight() - 16.0f);
        g.fillRect (r.getX() + 8.0f, c.y - 0.75f, r.getWidth() - 16.0f, 1.5f);
        const float d = juce::jlimit (22.0f, 44.0f, r.getWidth() * 0.09f);
        auto puck = juce::Rectangle<float> (d, d).withCentre (c);
        g.setColour (colours::brand1.withAlpha (on ? 0.35f : 0.15f));
        g.fillEllipse (puck.expanded (d * 0.35f));
        juce::ColourGradient pg (juce::Colour (0xffffd0dc), puck.getX(), puck.getY(), colours::brand1, puck.getRight(), puck.getBottom(), false);
        g.setGradientFill (pg);
        g.fillEllipse (puck);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawEllipse (puck, 2.0f);
        // name + amount
        auto top = r.reduced (16.0f, 12.0f).removeFromTop (18.0f);
        g.setFont (LooperLookAndFeel::font (12.0f, true));
        g.setColour (colours::cyan);
        g.drawText ("XY", top, juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xffe6e8ee));
        g.drawFittedText (i18n::upper (tr (getPunchFxName (punch))), top.withTrimmedLeft (30.0f).withTrimmedRight (60.0f).toNearestInt(),
                          juce::Justification::centredLeft, 1, 0.7f);
        const float live = view.livePunchAmount ? view.livePunchAmount() : -1.0f;
        const float amount = live >= 0.0f ? live : rawParam (proc, gPfxAmount);
        g.setColour (colours::brand1.brighter (0.3f));
        g.drawText (juce::String (juce::roundToInt (amount * 100.0f)) + " %", top, juce::Justification::centredRight);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawRoundedRectangle (r, 12.0f, 1.0f);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
            return;
        trail.clear();
        held = view.selectedPunch ? view.selectedPunch() : 0;
        setParamNorm (view.proc, punchHoldId (held), 1.0f);
        move (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) move (e); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() || held < 0)
            return;
        setParamNorm (view.proc, punchHoldId (held), 0.0f);
        held = -1;
    }
    void fadeTrail()
    {
        if (held < 0 && ! trail.empty())
        {
            trail.erase (trail.begin(), trail.begin() + (std::ptrdiff_t) juce::jmin ((size_t) 3, trail.size()));
            repaint();
        }
    }

private:
    void move (const juce::MouseEvent& e)
    {
        const auto field = getLocalBounds().toFloat().reduced (9.0f);
        setParamNorm (view.proc, gPfxX, juce::jlimit (0.0f, 1.0f, (e.position.x - field.getX()) / field.getWidth()));
        setParamNorm (view.proc, gPfxY, juce::jlimit (0.0f, 1.0f, (field.getBottom() - e.position.y) / field.getHeight()));
        trail.push_back (e.position);
        if (trail.size() > 40)
            trail.erase (trail.begin());
        repaint();
    }
    PerformView& view;
    std::vector<juce::Point<float>> trail;
    int held = -1;
};

//============================================================================== X / Y captions (sliders)
/** "X  STOP TIME . 72 %" along the bottom of the XY field (Y: rotated along the left edge); drag to set,
    right-click = MIDI learn of Punch FX X / Y. */
class PerformView::AxisSlider : public LLSlider
{
public:
    AxisSlider (PerformView& v, bool isX)
        : LLSlider (isX ? juce::Slider::LinearHorizontal : juce::Slider::LinearVertical, colours::cyan), view (v), x (isX)
    {
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    }
    void paint (juce::Graphics& g) override
    {
        const int punch = view.shownPunch();
        const auto text = juce::String (x ? "X  " : "Y  ") + i18n::upper (tr (x ? getPunchXName (punch) : getPunchYName (punch)))
                          + juce::String::fromUTF8 ("  \xc2\xb7  ") + juce::String (juce::roundToInt (getValue() * 100.0)) + " %";
        g.setColour (colours::cyan);
        g.setFont (LooperLookAndFeel::font (11.0f, true));
        if (x)
            g.drawFittedText (text, getLocalBounds(), juce::Justification::centred, 1, 0.7f);
        else
        {
            juce::Graphics::ScopedSaveState s (g);
            g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, 0.0f, 0.0f)
                                .translated (0.0f, (float) getHeight()));
            g.drawFittedText (text, juce::Rectangle<int> (0, 0, getHeight(), getWidth()), juce::Justification::centred, 1, 0.7f);
        }
    }
private:
    PerformView& view;
    bool x;
};

/** PUNCH AMOUNT: a bar in the brand gradient (the live amount while a punch FX is held). */
class PerformView::AmountBar : public LLSlider
{
public:
    explicit AmountBar (PerformView& v) : LLSlider (juce::Slider::LinearHorizontal, colours::learn), view (v)
    {
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    }
    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().withSizeKeepingCentre ((float) getWidth(), 12.0f);
        g.setColour (juce::Colours::black.withAlpha (0.45f * theme::shade()));
        g.fillRoundedRectangle (r, 6.0f);
        const float live = view.livePunchAmount ? view.livePunchAmount() : -1.0f;
        const float a = juce::jlimit (0.0f, 1.0f, live >= 0.0f ? live : (float) getValue());
        g.setGradientFill (LooperLookAndFeel::brandGradient (r));
        g.fillRoundedRectangle (r.withWidth (juce::jmax (12.0f, r.getWidth() * a)), 6.0f);
    }
private:
    PerformView& view;
};

//============================================================================== construction
PerformView::PerformView (LiveLooperProcessor& p, std::function<TrackStrip*(int)> strips)
    : proc (p), stripFn (std::move (strips))
{
    setOpaque (true);
    setWantsKeyboardFocus (true);
    auto& st = proc.getState();
    for (int t = 0; t < kNumTracks; ++t)
    {
        auto& c = cardParts[(size_t) t];
        c.pad = std::make_unique<RecPlayButton> (proc, t);
        c.stop = std::make_unique<StopButton> (proc, t);
        c.level = std::make_unique<LiveView::LevelBar>();
        c.level->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        c.levelAtt = std::make_unique<APVTS::SliderAttachment> (st, trackId (t, kLevel), *c.level);
        setParamId (*c.level, trackId (t, kLevel));
        c.length = std::make_unique<LoopLengthPill> (proc, t);
        for (juce::Component* comp : { (juce::Component*) c.length.get(), (juce::Component*) c.pad.get(),
                                       (juce::Component*) c.stop.get(), (juce::Component*) c.level.get() })
            addChildComponent (comp);
    }

    // header
    viewChips = LiveView::makeViewSwitcher (proc);
    tapPad = std::make_unique<StagePad> (st, gTap, "TAP", colours::yellow, StagePad::Mode::trigger);
    tapPad->neutral = true;
    tapPad->onPress = [this] { proc.pressTrigger (gTap); };
    tapPad->setTooltip (tr ("Tap tempo (internal tempo, used when host sync is off)"));
    exitPad = std::make_unique<StagePad> (st, gLiveView, "EXIT", colours::cyan, StagePad::Mode::trigger);
    exitPad->neutral = true;
    exitPad->subtitle = [] { return juce::String ("Esc"); };
    exitPad->onPress = [this] { if (onExit) onExit(); };
    exitPad->setTooltip (tr ("Back to the full surface (Esc)"));

    // performance FX: target, XY, amount, latch
    targetChips = std::make_unique<ChoiceChips> (st, gPfxTarget, juce::StringArray { "Loops", "Input", "Master", "Drums" });
    targetChips->setBrandStyle (true);
    targetChips->setTooltip (tr ("Where the punch FX work: loops, input, master or drums"));
    xyPad = std::make_unique<XYPad> (*this);
    xAxis = std::make_unique<AxisSlider> (*this, true);
    yAxis = std::make_unique<AxisSlider> (*this, false);
    xAtt = std::make_unique<APVTS::SliderAttachment> (st, gPfxX, *xAxis);
    yAtt = std::make_unique<APVTS::SliderAttachment> (st, gPfxY, *yAxis);
    setParamId (*xAxis, gPfxX);
    setParamId (*yAxis, gPfxY);
    amountBar = std::make_unique<AmountBar> (*this);
    amountAtt = std::make_unique<APVTS::SliderAttachment> (st, gPfxAmount, *amountBar);
    setParamId (*amountBar, gPfxAmount);
    latchBtn.setClickingTogglesState (true);
    latchAtt = std::make_unique<APVTS::ButtonAttachment> (st, gPfxLatch, latchBtn);
    setParamId (latchBtn, gPfxLatch);
    latchBtn.setTooltip ("Latch: a press switches a punch FX on, the next press off");

    // filter
    filterKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    filterAtt = std::make_unique<APVTS::SliderAttachment> (st, gPerfFilter, filterKnob);
    setParamId (filterKnob, gPerfFilter);
    filterKnob.setDoubleClickReturnValue (true, 0.0);
    filterKnob.setTooltip (tr ("Performance filter: left = low-pass, right = high-pass, centre = off (double-click: centre)"));
    centerBtn.onClick = [this] { setParamNorm (proc, gPerfFilter, 0.5f); };
    setParamId (centerBtn, gPerfFilter);        // right-click learns the filter itself
    centerBtn.setTooltip ("Filter off (centre)");

    // beat FX
    bfxPad = std::make_unique<StagePad> (st, gBfxOn, "ECHO", colours::orange, StagePad::Mode::toggle);
    bfxPad->setTooltip (tr ("Beat FX on / off (type and beat fraction as on the full surface)"));
    bfxTypeChips = std::make_unique<ChoiceChips> (st, gBfxType,
                                                   juce::StringArray { "Delay", "Echo", "Ping Pong", "Roll" },
                                                   std::vector<int> { (int) BeatFxType::Delay, (int) BeatFxType::Echo, (int) BeatFxType::PingPong, (int) BeatFxType::Roll },
                                                   colours::orange);
    bfxTypeChips->setTooltip (tr ("Beat FX type (all types on the full surface)"));
    bfxBeatChips = std::make_unique<ChoiceChips> (st, gBfxBeat, juce::StringArray { "1/16", "1/8", "1/4", "1/2", "1" },
                                                   std::vector<int> { 1, 2, 3, 4, 6 }, colours::orange);
    bfxBeatChips->setTooltip (tr ("Beat FX beat fraction"));
    bfxLevelKnob = std::make_unique<Knob> (st, gBfxDepth, "Level", colours::orange);
    bfxHoldPad = std::make_unique<StagePad> (st, gBfxHold, "HOLD", colours::orange, StagePad::Mode::momentary);
    bfxHoldPad->subtitle = [] { return tr ("while held"); };
    bfxHoldPad->setTooltip (tr ("Beat FX only while held"));

    // repeat / stutter / fill
    repeatPad = std::make_unique<StagePad> (st, gRepeatHold, "REPEAT", colours::cyan, StagePad::Mode::momentary);
    repeatPad->subtitle = [this] {
        const bool latch = rawParam (proc, gRepeatLatch) >= 0.5f;
        return paramText (proc, gRepeatRate) + juce::String::fromUTF8 (" \xc2\xb7 ")
               + (repeatPad->isLit() ? tr (latch ? "LATCHED" : "HELD") : tr ("hold"));
    };
    repeatPad->setTooltip (tr ("Beat repeat while held"));
    stutterPad = std::make_unique<StagePad> (st, gStutterHold, "STUTTER", colours::learn, StagePad::Mode::momentary);
    stutterPad->subtitle = [] { return tr ("hold"); };
    stutterPad->mark = 2;
    stutterPad->setTooltip (tr ("Stutter while held"));
    fillPad = std::make_unique<StagePad> (st, gRhythmFill, "FILL", colours::brand1, StagePad::Mode::momentary);
    fillPad->subtitle = [] { return tr ("hold"); };
    fillPad->mark = 2;
    fillPad->setTooltip (tr ("Hold for a fill; on release the groove comes back on the next downbeat"));
    rateChips = std::make_unique<ChoiceChips> (st, gRepeatRate, juce::StringArray { "1/2", "1/4", "1/8", "1/16", "1/32" }, std::vector<int> {}, colours::cyan);
    rateChips->setTooltip (tr ("Beat repeat timing"));

    // punch pads
    for (int i = 0; i < kNumPunchFx; ++i)
    {
        auto& b = punchPads[(size_t) i];
        b = std::make_unique<StagePad> (st, punchHoldId (i), kPunchShort[i], punchColour (i), StagePad::Mode::momentary);
        b->mark = 1;
        b->brandLit = true;
        b->onPress = [this, i] { if (onSelectPunch) onSelectPunch (i); };
        b->amount = [this, i] {
            const float live = livePunchAmount ? livePunchAmount() : -1.0f;
            return shownPunch() == i ? (live >= 0.0f ? live : rawParam (proc, gPfxAmount)) : -1.0f;
        };
        b->setTooltip (tr ("Punch FX %1: %2 - hold to play, right-click: MIDI learn", juce::String (i + 1), tr (getPunchFxName (i))));
    }

    // master
    masterKnob = std::make_unique<Knob> (st, gMaster, "", colours::green);
    allPad = std::make_unique<StagePad> (st, gAllStartStop, "ALL START / STOP", colours::green, StagePad::Mode::trigger);
    allPad->neutral = true;
    allPad->onPress = [this] { proc.pressTrigger (gAllStartStop); };
    allPad->setTooltip (tr ("Start / stop all tracks"));
    capturePad = std::make_unique<StagePad> (st, gCapture, "CAPTURE", colours::red, StagePad::Mode::trigger);
    capturePad->neutral = true;
    capturePad->onPress = [this] { proc.pressTrigger (gCapture); };
    capturePad->subtitle = [this] { return tr ("last %1 bars", paramText (proc, gCaptureBars)); };
    capturePad->setTooltip (tr ("Capture: the last bars you played become a loop on the next free track"));

    for (juce::Component* c : { (juce::Component*) viewChips.get(), (juce::Component*) tapPad.get(), (juce::Component*) exitPad.get(),
                                (juce::Component*) targetChips.get(), (juce::Component*) xyPad.get(), (juce::Component*) xAxis.get(),
                                (juce::Component*) yAxis.get(), (juce::Component*) amountBar.get(), (juce::Component*) &latchBtn,
                                (juce::Component*) &filterKnob, (juce::Component*) &centerBtn, (juce::Component*) bfxPad.get(),
                                (juce::Component*) bfxTypeChips.get(), (juce::Component*) bfxBeatChips.get(), (juce::Component*) bfxLevelKnob.get(),
                                (juce::Component*) bfxHoldPad.get(), (juce::Component*) repeatPad.get(), (juce::Component*) stutterPad.get(),
                                (juce::Component*) fillPad.get(), (juce::Component*) rateChips.get(), (juce::Component*) masterKnob.get(),
                                (juce::Component*) allPad.get(), (juce::Component*) capturePad.get() })
        addAndMakeVisible (c);
    for (auto& b : punchPads)
        addAndMakeVisible (*b);
    bfxPad->setTitle (beatFxText(), false);
}

PerformView::~PerformView()
{
    // the attachments go first: filterAtt is declared before filterKnob, so the default order would
    // detach it from an already destroyed slider (crashed the Windows unit tests)
    filterAtt.reset();
    xAtt.reset();
    yAtt.reset();
    amountAtt.reset();
    latchAtt.reset();
}

juce::Component& PerformView::getXYPad() noexcept { return *xyPad; }

juce::String PerformView::beatFxText() const
{
    auto* type = proc.getState().getParameter (gBfxType);
    auto* beat = proc.getState().getParameter (gBfxBeat);
    juce::String t = type != nullptr ? i18n::upper (tr (type->getCurrentValueAsText())) : juce::String ("BEAT FX");
    if (beat != nullptr)
        t << " " << beat->getCurrentValueAsText();
    return t;
}

int PerformView::shownPunch() const
{
    const auto& punch = proc.getEngine().getPunchFx();
    for (int i = 0; i < kNumPunchFx; ++i)
        if (punch.isOn (i))
            return i;
    return juce::jlimit (0, kNumPunchFx - 1, selectedPunch ? selectedPunch() : 0);
}

bool PerformView::areWaveformsShown() const noexcept
{
    return TrackStrip::showWaveforms;       // the global setting (host parameter Waveforms, default off)
}

//============================================================================== layout
void PerformView::resized()
{
    auto b = getLocalBounds().reduced (18, 14);
    headArea = b.removeFromTop (kHeadH);
    {
        auto head = headArea;
        exitPad->setBounds (head.removeFromRight (112).withSizeKeepingCentre (112, 46));
        head.removeFromRight (10);
        tapPad->setBounds (head.removeFromRight (76).withSizeKeepingCentre (76, 40));
        head.removeFromRight (16);
        const float logoW = juce::GlyphArrangement::getStringWidth (LooperLookAndFeel::font (28.0f, true), "livelooper")
                          + 14.0f + juce::GlyphArrangement::getStringWidth (LooperLookAndFeel::font (11.0f, true), "PERFORM") + 26.0f;
        const int sx = headArea.getX() + 6 + juce::roundToInt (logoW) + 22;
        const int sw = juce::jlimit (200, 330, head.getRight() - sx - 220);
        viewChips->setBounds (sx, headArea.getY() + 8, sw, 34);
        const int tl = viewChips->getRight() + 16;
        const int tw = juce::jlimit (160, 380, head.getRight() - tl);
        transportArea = juce::Rectangle<int> (tw, 62).withCentre ({ tl + (head.getRight() - tl) / 2, headArea.getCentreY() });
        hintArea = { headArea.getX() + 6, headArea.getY() + 46, viewChips->getRight() - headArea.getX() - 6, 16 };
    }
    b.removeFromTop (10);

    // compact track row
    const int rowH = juce::jlimit (124, 172, getHeight() * 150 / 856);
    auto row = b.removeFromTop (rowH);
    b.removeFromTop (kGap);
    numCards = proc.getVisibleTracks();
    const int w = (row.getWidth() - (numCards - 1) * kGap) / juce::jmax (1, numCards);
    for (int t = 0; t < kNumTracks; ++t)
    {
        auto& c = cardParts[(size_t) t];
        const bool shown = t < numCards;
        c.pad->setVisible (shown);
        c.stop->setVisible (shown);
        c.level->setVisible (shown);
        if (! shown)
        {
            c.length->setVisible (false);
            cards[(size_t) t] = {};
            continue;
        }
        const auto card = juce::Rectangle<int> (row.getX() + t * (w + kGap), row.getY(), w, row.getHeight());
        cards[(size_t) t] = card;
        auto inner = card.reduced (12, 10);
        c.title = inner.removeFromTop (20);
        {
            auto title = c.title;
            const int ph = 18;
            const int pw = juce::jmin (c.length->getPreferredWidth (ph), title.getWidth() - 70);
            c.length->setBounds (title.removeFromRight (juce::jmax (30, pw)).withSizeKeepingCentre (juce::jmax (30, pw), ph));
        }
        inner.removeFromTop (5);
        auto bottom = inner.removeFromBottom (42);
        inner.removeFromBottom (6);
        c.display = inner;
        const int d = 40;
        c.pad->setBounds (bottom.removeFromLeft (d + 4).withSizeKeepingCentre (d + 4, d + 4));
        bottom.removeFromLeft (10);
        c.stop->setBounds (bottom.removeFromRight (34).withSizeKeepingCentre (34, 34));
        bottom.removeFromRight (10);
        auto lv = bottom.reduced (0, 3);
        c.levelCaption = lv.removeFromTop (14);
        c.level->setBounds (lv.withSizeKeepingCentre (lv.getWidth(), 18));
    }

    // performance FX area
    fxArea = b;
    auto f = fxArea.reduced (18, 12);
    auto titleRow = f.removeFromTop (30);
    {
        const int cw = juce::jmin (100, (titleRow.getWidth() - 260) / 4);
        auto chips = titleRow.removeFromRight (cw * 4 + 30);
        targetChips->setBounds (chips.withSizeKeepingCentre (chips.getWidth(), 28));
        targetCaption = { chips.getX() - 70, titleRow.getY(), 62, titleRow.getHeight() };
    }
    f.removeFromTop (10);
    const int colA = juce::roundToInt ((float) f.getWidth() * 0.335f);
    const int colB = juce::roundToInt ((float) f.getWidth() * 0.235f);
    auto a = f.removeFromLeft (colA);
    f.removeFromLeft (18);
    auto bcol = f.removeFromLeft (colB);
    f.removeFromLeft (18);
    auto ccol = f;

    // column A: XY + amount + hint
    hintBottom = a.removeFromBottom (16);
    a.removeFromBottom (6);
    auto amountRow = a.removeFromBottom (30);
    a.removeFromBottom (10);
    xyArea = a;
    xyPad->setBounds (xyArea);
    xAxis->setBounds (xyArea.getX() + 40, xyArea.getBottom() - 26, xyArea.getWidth() - 80, 20);
    yAxis->setBounds (xyArea.getX() + 6, xyArea.getY() + 50, 20, xyArea.getHeight() - 100);
    latchBtn.setBounds (amountRow.removeFromRight (84).withSizeKeepingCentre (84, 30));
    amountRow.removeFromRight (12);
    amountCaption = amountRow.removeFromLeft (120);
    amountBar->setBounds (amountRow.withSizeKeepingCentre (amountRow.getWidth(), 20));

    // column B: filter, beat FX
    filterArea = bcol.removeFromTop (juce::roundToInt ((float) bcol.getHeight() * 0.5f));
    bcol.removeFromTop (12);
    beatArea = bcol;
    {
        auto r = filterArea.reduced (16, 12);
        r.removeFromTop (24);
        filterCaption = r.removeFromBottom (36);
        const int knob = juce::jmax (60, juce::jmin (r.getHeight(), r.getWidth() - 96));
        filterKnob.setBounds (juce::Rectangle<int> (knob, knob).withCentre ({ r.getX() + (r.getWidth() - 80) / 2 + 4, r.getCentreY() }));
        centerBtn.setBounds (juce::Rectangle<int> (76, 28).withCentre ({ r.getRight() - 38, r.getCentreY() }));
    }
    {
        auto r = beatArea.reduced (16, 12);
        r.removeFromTop (24);
        bfxPad->setBounds (r.removeFromTop (juce::jlimit (32, 52, r.getHeight() / 4)));
        r.removeFromTop (10);
        bfxTypeChips->setBounds (r.removeFromTop (26));
        r.removeFromTop (8);
        bfxBeatChips->setBounds (r.removeFromTop (26));
        r.removeFromTop (8);
        const int half = r.getWidth() / 2;
        bfxLevelKnob->setBounds (r.removeFromLeft (half).withSizeKeepingCentre (juce::jmin (half, 90), juce::jmin (r.getHeight(), 76)));
        bfxHoldPad->setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth() - 8, 110), juce::jmin (r.getHeight() - 8, 54)));
    }

    // column C: repeat / stutter / fill, rate, punch pads, master
    {
        auto tiles = ccol.removeFromTop (juce::jlimit (56, 88, ccol.getHeight() / 7));
        const int tw = (tiles.getWidth() - 2 * 14) / 3;
        repeatPad->setBounds (tiles.removeFromLeft (tw));
        tiles.removeFromLeft (14);
        stutterPad->setBounds (tiles.removeFromLeft (tw));
        tiles.removeFromLeft (14);
        fillPad->setBounds (tiles);
        ccol.removeFromTop (10);
        auto rate = ccol.removeFromTop (26);
        rateCaption = rate.removeFromLeft (110);
        rateChips->setBounds (rate.removeFromLeft (juce::jmin (rate.getWidth(), 5 * 62)));
        ccol.removeFromTop (14);
        punchCaption = ccol.removeFromTop (20);
        ccol.removeFromTop (6);
        const int ph = juce::jlimit (36, 64, (ccol.getHeight() - 150) / 2);
        for (int rowI = 0; rowI < 2; ++rowI)
        {
            auto pr = ccol.removeFromTop (ph);
            const int pw = (pr.getWidth() - 5 * 10) / 6;
            for (int i = 0; i < 6; ++i)
                punchPads[(size_t) (rowI * 6 + i)]->setBounds (pr.getX() + i * (pw + 10), pr.getY(), pw, ph);
            ccol.removeFromTop (10);
        }
        ccol.removeFromTop (4);
        masterArea = ccol;
        auto m = masterArea.reduced (16, 12);
        m.removeFromTop (24);
        const int kw = juce::jmin (110, m.getWidth() / 4);
        masterKnob->setBounds (m.removeFromLeft (kw).withSizeKeepingCentre (kw, juce::jmin (m.getHeight(), kw + 14)));
        m.removeFromLeft (10);
        meterArea = m.removeFromLeft (28).withSizeKeepingCentre (28, juce::jmin (m.getHeight() - 8, 110));
        m.removeFromLeft (16);
        const int bh = juce::jmin (m.getHeight() - 8, 110);
        const int capW = juce::roundToInt ((float) m.getWidth() * 0.4f);
        capturePad->setBounds (m.removeFromRight (capW).withSizeKeepingCentre (capW, bh));
        m.removeFromRight (14);
        allPad->setBounds (m.withSizeKeepingCentre (m.getWidth(), bh));
    }
}

void PerformView::refresh (bool blinkPhase)
{
    for (int t = 0; t < numCards; ++t)
    {
        auto& c = cardParts[(size_t) t];
        c.pad->setBlinkPhase (blinkPhase);
        c.pad->repaint();
        if (auto* s = stripFn ? stripFn (t) : nullptr)
        {
            const float m = MeterBallistics::toPosition (juce::jmax (s->getMeter (0).level, s->getMeter (1).level));
            if (std::abs (m - c.level->meter) > 0.002f)
            {
                c.level->meter = m;
                c.level->repaint();
            }
        }
        repaint (c.display);
        repaint (c.levelCaption);
        repaint (c.title);
        const auto state = (LoopTrack::State) proc.getEngine().getTrackStatus (t).state.load();
        const bool want = state == LoopTrack::State::Empty || state == LoopTrack::State::Recording;
        if (c.length->isVisible() != want)
            c.length->setVisible (want);
        if (want)
            c.length->refresh();
    }
    if (proc.getVisibleTracks() != numCards)
        resized();
    const auto bfx = beatFxText();
    if (bfx != bfxPad->getTitle())
        bfxPad->setTitle (bfx, false);
    for (auto* chips : { viewChips.get(), targetChips.get(), rateChips.get(), bfxTypeChips.get(), bfxBeatChips.get() })
        chips->refresh();
    for (auto* pad : { tapPad.get(), exitPad.get(), repeatPad.get(), stutterPad.get(), fillPad.get(), bfxPad.get(),
                       bfxHoldPad.get(), allPad.get(), capturePad.get() })
        pad->refresh();
    for (auto& pad : punchPads)
        pad->refresh();
    xyPad->fadeTrail();
    xyPad->repaint();
    xAxis->repaint();
    yAxis->repaint();
    amountBar->repaint();
    if (masterMeter)
    {
        float l = 0.0f, r = 0.0f;
        masterMeter (l, r);
        meterL = juce::jmax (l, meterL * 0.85f);
        meterR = juce::jmax (r, meterR * 0.85f);
        repaint (meterArea.expanded (4));
    }
    auto masterText = paramText (proc, gMaster);
    if (masterText.isNotEmpty() && masterText.containsOnly ("0123456789.-"))
        masterText << " %";
    if (masterText != shownMaster)
    {
        shownMaster = masterText;
        masterKnob->setCaption (masterText);
    }
    const juce::String depth (getBeatFxDepthName ((BeatFxType) juce::roundToInt (rawParam (proc, gBfxType))));
    if (depth != shownDepth)
    {
        shownDepth = depth;
        bfxLevelKnob->setCaption (depth);
    }
    repaint (filterCaption);
    repaint (transportArea);
    if (shownTheme != theme::version() || shownWaves != TrackStrip::showWaveforms)
    {
        shownTheme = theme::version();
        shownWaves = TrackStrip::showWaveforms;
        repaint();
    }
}

bool PerformView::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onExit)
            onExit();
        return true;
    }
    return false;
}

//============================================================================== painting
void PerformView::paintSection (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, juce::Colour mark, const juce::String& right)
{
    LooperLookAndFeel::drawGlassPanel (g, r.toFloat(), 12.0f, 1);
    auto t = r.reduced (16, 10).removeFromTop (20).toFloat();
    if (mark.getAlpha() > 0)
    {
        g.setColour (mark);
        g.fillRoundedRectangle (t.getX(), t.getCentreY() - 6.0f, 3.0f, 12.0f, 1.5f);
        t.removeFromLeft (10.0f);
    }
    g.setColour (colours::text);
    g.setFont (LooperLookAndFeel::font (12.0f, true));
    g.drawText (i18n::upper (title), t, juce::Justification::centredLeft);
    if (right.isNotEmpty())
    {
        g.setColour (colours::textDim);
        g.setFont (LooperLookAndFeel::font (10.0f, true));
        g.drawText (right, t, juce::Justification::centredRight);
    }
}

void PerformView::paint (juce::Graphics& g)
{
    const float w = (float) getWidth(), h = (float) getHeight();
    const bool light = theme::isLight(), stage = theme::isStage();
    juce::ColourGradient bg (stage && ! light ? juce::Colour (0xff050506) : theme::pick (0xff1b1b21, 0xfff1f2f6), 0.0f, 0.0f,
                             stage && ! light ? juce::Colour (0xff000000) : theme::pick (0xff0c0c10, 0xffdfe2e9), 0.0f, h, false);
    g.setGradientFill (bg);
    g.fillAll();
    if (! stage)
    {
        auto glow = [&g] (float cx, float cy, float r, juce::Colour c, float a) {
            juce::ColourGradient rg (c.withAlpha (a), cx, cy, c.withAlpha (0.0f), cx + r, cy, true);
            g.setGradientFill (rg);
            g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
        };
        glow (120.0f, 40.0f, 520.0f, colours::brand0, 0.10f);
        glow (w * 0.45f, h + 80.0f, 620.0f, colours::brand1, 0.08f);
        glow (w - 80.0f, h - 120.0f, 560.0f, colours::brand2, 0.10f);
    }

    // header
    LiveView::drawLogo (g, { (float) headArea.getX() + 6.0f, (float) headArea.getY() - 6.0f }, "PERFORM", 28.0f);
    g.setColour (colours::textDim);
    g.setFont (LooperLookAndFeel::font (10.5f, true));
    g.drawFittedText (tr ("SHIFT + MASTER on Push 1 or the host parameter View switches views"), hintArea, juce::Justification::centredLeft, 1, 0.8f);
    LiveView::drawTransport (g, proc, transportArea.toFloat());

    for (int t = 0; t < numCards; ++t)
        paintCard (g, t);

    // performance FX area
    LooperLookAndFeel::drawGlassPanel (g, fxArea.toFloat(), 16.0f, 1);
    {
        auto t = fxArea.reduced (18, 12).removeFromTop (30).toFloat();
        g.setGradientFill (LooperLookAndFeel::brandGradient (t));
        g.fillRoundedRectangle (t.getX(), t.getCentreY() - 8.0f, 4.0f, 16.0f, 2.0f);
        g.setColour (colours::text);
        g.setFont (LooperLookAndFeel::font (15.0f, true));
        g.drawText (tr ("PERFORMANCE FX"), t.withTrimmedLeft (14.0f), juce::Justification::centredLeft);
        g.setColour (colours::textDim);
        g.setFont (LooperLookAndFeel::font (10.5f, true));
        g.drawText (tr ("TARGET"), targetCaption, juce::Justification::centredRight);
    }
    // column A captions
    g.setColour (colours::textDim);
    g.setFont (LooperLookAndFeel::font (10.5f, true));
    g.drawText (tr ("PUNCH AMOUNT"), amountCaption, juce::Justification::centredLeft);
    g.setFont (LooperLookAndFeel::font (10.0f, false));
    g.drawFittedText (tr ("Hold a punch FX pad, move in the field - release = off"), hintBottom, juce::Justification::centredLeft, 1, 0.8f);

    // filter
    paintSection (g, filterArea, tr ("FILTER"), juce::Colours::transparentBlack, tr ("BIPOLAR"));
    {
        auto c = filterCaption.toFloat();
        const float v = rawParam (proc, gPerfFilter);
        const auto kb = filterKnob.getBounds().toFloat();
        g.setFont (LooperLookAndFeel::font (10.5f, true));
        g.setColour (v < -0.01f ? colours::cyan : colours::textDim);
        g.drawText (tr ("LP"), juce::Rectangle<float> (kb.getX() - 34.0f, kb.getBottom() - 18.0f, 30.0f, 16.0f), juce::Justification::centredRight);
        g.setColour (v > 0.01f ? colours::cyan : colours::textDim);
        g.drawText (tr ("HP"), juce::Rectangle<float> (kb.getRight() + 4.0f, kb.getBottom() - 18.0f, 30.0f, 16.0f), juce::Justification::centredLeft);
        g.setColour (colours::textDim);
        g.drawText (tr ("CUTOFF"), c.removeFromTop (16.0f).withX (kb.getX() - 20.0f).withWidth (kb.getWidth() + 40.0f), juce::Justification::centred);
        g.setColour (colours::cyan);
        const auto txt = std::abs (v) < 0.01f ? tr ("Off") : ((v < 0.0f ? tr ("LP") : tr ("HP")) + " " + juce::String (juce::roundToInt (std::abs (v) * 100.0f)) + " %");
        g.drawText (txt, c.withX (kb.getX() - 20.0f).withWidth (kb.getWidth() + 40.0f), juce::Justification::centred);
        g.setColour (colours::textDim);
        g.setFont (LooperLookAndFeel::font (9.5f, false));
        g.drawFittedText (tr ("double-click"), centerBtn.getBounds().translated (0, 30).withHeight (14), juce::Justification::centred, 1, 0.7f);
    }

    // beat FX
    paintSection (g, beatArea, tr ("BEAT FX"), juce::Colours::transparentBlack);

    // column C captions
    g.setColour (colours::textDim);
    g.setFont (LooperLookAndFeel::font (10.5f, true));
    g.drawText (tr ("REPEAT RATE"), rateCaption, juce::Justification::centredLeft);
    g.setColour (colours::text);
    g.setFont (LooperLookAndFeel::font (12.0f, true));
    g.drawText (tr ("PUNCH FX"), punchCaption, juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (LooperLookAndFeel::font (10.0f, false));
    g.drawFittedText (tr ("hold = on - velocity / pressure = amount"), punchCaption.withTrimmedLeft (110), juce::Justification::centredRight, 1, 0.8f);

    // master
    paintSection (g, masterArea, tr ("MASTER"), juce::Colours::transparentBlack);
    {
        auto m = meterArea.toFloat();
        const float bw = (m.getWidth() - 6.0f) / 2.0f;
        for (int ch = 0; ch < 2; ++ch)
        {
            auto bar = juce::Rectangle<float> (m.getX() + (float) ch * (bw + 6.0f), m.getY(), bw, m.getHeight());
            g.setColour (juce::Colours::black.withAlpha (0.5f * theme::shade()));
            g.fillRoundedRectangle (bar, 3.0f);
            const float lvl = juce::jlimit (0.0f, 1.0f, ch == 0 ? meterL : meterR);
            const int segs = 14;
            const float sh = (bar.getHeight() - 4.0f) / (float) segs;
            for (int s = 0; s < segs; ++s)
            {
                const float pos = (float) (s + 1) / (float) segs;
                auto seg = juce::Rectangle<float> (bar.getX() + 2.0f, bar.getBottom() - 2.0f - (float) (s + 1) * sh + 1.0f, bar.getWidth() - 4.0f, sh - 2.0f);
                const auto col = pos > 0.9f ? colours::red : (pos > 0.72f ? colours::yellow : colours::green);
                g.setColour (pos <= lvl ? col : col.withAlpha (0.12f));
                g.fillRect (seg);
            }
        }
    }
}

void PerformView::paintCard (juce::Graphics& g, int t)
{
    auto& eng = proc.getEngine();
    const auto& st = eng.getTrackStatus (t);
    const auto state = (LoopTrack::State) st.state.load();
    const auto& parts = cardParts[(size_t) t];
    const auto c = cards[(size_t) t].toFloat();
    const auto col = colours::track (t);
    LooperLookAndFeel::drawGlassPanel (g, c, 14.0f, 2, col);

    const auto title = parts.title.toFloat();
    g.setColour (col);
    g.fillRoundedRectangle (title.getX() + 1.0f, title.getCentreY() - 7.0f, 4.0f, 14.0f, 2.0f);
    g.setColour (colours::text);
    g.setFont (LooperLookAndFeel::font (13.5f, true));
    g.drawFittedText (tr ("TRACK %1", juce::String (t + 1)), title.withTrimmedLeft (12.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
    if (! parts.length->isVisible())
    {
        bool hasLength = false;
        const auto len = LiveView::lengthText (proc, t, hasLength);
        g.setColour (colours::textDim);
        g.setFont (LooperLookAndFeel::font (10.5f, true));
        g.drawText (len, title, juce::Justification::centredRight);
    }

    juce::Colour sc;
    const auto word = LiveView::stateWord (proc, t, sc);
    const auto disp = parts.display.toFloat();
    LiveView::drawDisplayBox (g, disp, 8.0f);
    auto lt = disp.reduced (11.0f, 5.0f);
    const float wordW = juce::jmin (lt.getWidth() * 0.42f, 110.0f);
    auto wordArea = lt.removeFromLeft (wordW);
    g.setColour (sc);
    g.setFont (LooperLookAndFeel::font (juce::jlimit (15.0f, 26.0f, juce::jmin (disp.getHeight() * 0.5f, c.getWidth() * 0.11f)), true));
    g.drawFittedText (tr (word), wordArea.toNearestInt(), juce::Justification::centredLeft, 1, 0.55f);
    lt.removeFromLeft (6.0f);
    const float prog = state == LoopTrack::State::Empty ? 0.0f : juce::jlimit (0.0f, 1.0f, st.progress.load());
    bool hasLength = false;
    LiveView::lengthText (proc, t, hasLength);
    auto* strip = stripFn ? stripFn (t) : nullptr;
    if (areWaveformsShown() && strip != nullptr && strip->hasWaveform() && state != LoopTrack::State::Empty)
    {
        LoopWaveform wf;
        wf.peaks = strip->getWaveform().data();
        wf.numBins = TrackStrip::kWaveBins;
        wf.valid = true;
        wf.progress = prog;
        wf.ghost = st.ghostProgress.load();
        wf.state = state;
        wf.colour = col;
        wf.headWidth = 2.0f;
        wf.draw (g, lt.reduced (0.0f, 3.0f));
    }
    else
    {
        // loop position bar (waveforms off: the global setting)
        auto pb = lt.withSizeKeepingCentre (lt.getWidth(), 5.0f);
        g.setColour (colours::lcdText.withAlpha (0.12f));
        g.fillRoundedRectangle (pb, 2.5f);
        if (hasLength || state == LoopTrack::State::Recording)
        {
            g.setColour (state == LoopTrack::State::Recording ? colours::red : col);
            g.fillRoundedRectangle (pb.withWidth (pb.getWidth() * (state == LoopTrack::State::Recording ? 1.0f : prog)), 2.5f);
        }
    }

    g.setFont (LooperLookAndFeel::font (9.5f, true));
    g.setColour (colours::textDim);
    g.drawText (tr ("LEVEL"), parts.levelCaption, juce::Justification::centredLeft);
    g.setColour (colours::text);
    g.drawText (juce::String (juce::roundToInt (parts.level->getValue() * 100.0)) + " %", parts.levelCaption, juce::Justification::centredRight);
}
} // namespace livelooper
