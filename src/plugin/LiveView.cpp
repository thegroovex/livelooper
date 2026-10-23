// v1.7 Live view (branch v1.7-wip): large track cards, transport display, performance FX strip.
#include "LiveView.h"
#include "Parameters.h"

namespace livelooper
{
using namespace params;
using i18n::tr;

namespace
{
    constexpr int kHeadH = 78, kFootH = 122, kGap = 12;
}

void LiveView::drawDisplayBox (juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    // the same recessed LCD as on the full surface (dark in every theme)
    juce::ColourGradient bz (juce::Colour (0xff3e4049), 0.0f, r.getY(), juce::Colour (0xff0e0f12), 0.0f, r.getBottom(), false);
    g.setGradientFill (bz);
    g.fillRoundedRectangle (r, radius);
    const auto in = r.reduced (3.0f);
    juce::ColourGradient lg (juce::Colour (0xff0a1f2b), in.getCentreX(), in.getCentreY(), juce::Colour (0xff03090d), in.getX(), in.getY(), true);
    g.setGradientFill (lg);
    g.fillRoundedRectangle (in, radius - 2.0f);
    juce::ColourGradient ish (juce::Colours::black.withAlpha (0.7f), 0.0f, in.getY(), juce::Colours::black.withAlpha (0.0f), 0.0f, in.getY() + 10.0f, false);
    g.setGradientFill (ish);
    g.fillRoundedRectangle (in, radius - 2.0f);
}

juce::String LiveView::stateWord (LiveLooperProcessor& proc, int t, juce::Colour& sc)
{
    const auto& st = proc.getEngine().getTrackStatus (t);
    const auto state = (LoopTrack::State) st.state.load();
    const auto pending = (LooperEngine::Pending) st.pending.load();
    juce::String text;
    sc = colours::lcdText;
    switch (state)
    {
        case LoopTrack::State::Empty:       text = "EMPTY"; sc = colours::lcdText.withAlpha (0.55f); break;
        case LoopTrack::State::Recording:   text = "REC";   sc = colours::red; break;
        case LoopTrack::State::Overdubbing: text = "DUB";   sc = colours::orange; break;
        case LoopTrack::State::Playing:     text = st.stopping.load() ? "STOPPING" : "PLAY"; sc = colours::green; break;
        case LoopTrack::State::Stopped:     text = "STOP";  break;
    }
    if (pending == LooperEngine::Pending::Record)          { text = "REC WAIT";  sc = colours::red; }
    else if (pending == LooperEngine::Pending::Play)       { text = "PLAY WAIT"; sc = colours::green; }
    else if (pending == LooperEngine::Pending::AutoRecord) { text = "AUTO REC";  sc = colours::red; }
    else if (pending != LooperEngine::Pending::None)       { text = "REC END";   sc = colours::red; }
    return text;
}

juce::String LiveView::lengthText (LiveLooperProcessor& proc, int t, bool& hasLength)
{
    auto& eng = proc.getEngine();
    const auto& st = eng.getTrackStatus (t);
    const auto state = (LoopTrack::State) st.state.load();
    const double bars = st.lengthPpq.load() / juce::jmax (0.25, eng.getMeasurePpq());
    hasLength = bars > 0.0 && state != LoopTrack::State::Empty && state != LoopTrack::State::Recording;
    if (! hasLength)
        return "-";
    const bool whole = std::abs (bars - std::round (bars)) < 0.02;
    return tr (std::abs (bars - 1.0) < 0.02 ? "%1 BAR" : "%1 BARS", whole ? juce::String ((int) std::round (bars)) : juce::String (bars, 1));
}

float LiveView::drawLogo (juce::Graphics& g, juce::Point<float> p, const juce::String& badgeText, float size)
{
    const auto f = LooperLookAndFeel::font (size, true);
    const float tw = juce::GlyphArrangement::getStringWidth (f, "livelooper");
    g.setColour (colours::text);
    g.setFont (f);
    g.drawText ("livelooper", juce::Rectangle<float> (p.x, p.y + 8.0f, tw + 4.0f, size * 1.2f), juce::Justification::centredLeft);
    const auto bf = LooperLookAndFeel::font (11.0f, true);
    const auto badge = juce::Rectangle<float> (p.x + tw + 14.0f, p.y + 8.0f + size * 0.6f - 11.0f,
                                               juce::GlyphArrangement::getStringWidth (bf, badgeText) + 26.0f, 22.0f);
    g.setGradientFill (LooperLookAndFeel::brandGradient (badge));
    g.fillRoundedRectangle (badge, 11.0f);
    g.setColour (juce::Colours::white);
    g.setFont (bf);
    g.drawText (badgeText, badge, juce::Justification::centred);
    return badge.getRight();
}

std::unique_ptr<ChoiceChips> LiveView::makeViewSwitcher (LiveLooperProcessor& proc)
{
    // FULL . LIVE . LIVE - FX . PERFORM = the choices of the host parameter View (Push 1: SHIFT + MASTER)
    auto c = std::make_unique<ChoiceChips> (proc.getState(), gView, juce::StringArray { "FULL", "LIVE", "LIVE - FX", "PERFORM" });
    c->setBrandStyle (true);
    c->setGrouped (true);
    c->onSelect = [&proc] (int v) { proc.setView (v); };
    c->setTooltip (tr ("View: Full, Live, Live without FX, Performance - also SHIFT + MASTER on Push 1 or the host parameter View; Esc returns to the full surface"));
    return c;
}

//==============================================================================
LiveView::LiveView (LiveLooperProcessor& p, std::function<TrackStrip*(int)> strips)
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
        c.level = std::make_unique<LevelBar>();
        c.level->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        c.levelAtt = std::make_unique<APVTS::SliderAttachment> (st, trackId (t, kLevel), *c.level);
        setParamId (*c.level, trackId (t, kLevel));
        c.length = std::make_unique<LoopLengthPill> (proc, t);
        c.length->setLarge (true);
        addChildComponent (*c.length);
        addChildComponent (*c.pad);
        addChildComponent (*c.stop);
        addChildComponent (*c.level);
    }

    exitBtn.onClick = [this] { if (onExit) onExit(); };
    exitBtn.setTooltip ("Back to the full surface (Esc)");
    setParamId (exitBtn, gLiveView);
    tapBtn.onClick = [this] { proc.pressTrigger (gTap); };
    tapBtn.setTooltip ("Tap tempo (internal tempo, used when host sync is off)");
    setParamId (tapBtn, gTap);
    allBtn.onClick = [this] { proc.pressTrigger (gAllStartStop); };
    allBtn.setTooltip ("Start / stop all tracks");
    setParamId (allBtn, gAllStartStop);
    captureBtn.onClick = [this] { proc.pressTrigger (gCapture); };
    captureBtn.setTooltip ("Capture: the last bars you played become a loop on the next free track");
    setParamId (captureBtn, gCapture);
    bfxBtn.setClickingTogglesState (true);
    bfxAtt = std::make_unique<APVTS::ButtonAttachment> (st, gBfxOn, bfxBtn);
    setParamId (bfxBtn, gBfxOn);
    bfxBtn.setTooltip ("Beat FX on / off (type and beat fraction as on the full surface)");
    repeatBtn = std::make_unique<MomentaryButton> (st, gRepeatHold, "REPEAT", colours::red);
    repeatBtn->setTooltip ("Beat repeat while held");
    stutterBtn = std::make_unique<MomentaryButton> (st, gStutterHold, "STUTTER", colours::red);
    stutterBtn->setTooltip ("Stutter while held");
    fillBtn = std::make_unique<MomentaryButton> (st, gRhythmFill, "FILL", colours::orange);
    fillBtn->setTooltip ("Hold for a fill; on release the groove comes back on the next downbeat");
    filterKnob = std::make_unique<Knob> (st, gPerfFilter, "FILTER", colours::cyan);
    beatKnob = std::make_unique<Knob> (st, gBfxDepth, "BEAT FX", colours::orange);
    punchKnob = std::make_unique<Knob> (st, gPfxAmount, "PUNCH", colours::learn);
    masterKnob = std::make_unique<Knob> (st, gMaster, "MASTER", colours::green);

    LooperLookAndFeel::setEmphasis (exitBtn, 0);
    LooperLookAndFeel::setEmphasis (tapBtn, 0);
    LooperLookAndFeel::setEmphasis (allBtn, 2);
    for (juce::Component* c : { (juce::Component*) &exitBtn, (juce::Component*) &tapBtn })
        addAndMakeVisible (c);
    for (juce::Component* c : { (juce::Component*) &allBtn, (juce::Component*) &captureBtn, (juce::Component*) &bfxBtn,
                                (juce::Component*) repeatBtn.get(), (juce::Component*) stutterBtn.get(), (juce::Component*) fillBtn.get(),
                                (juce::Component*) filterKnob.get(), (juce::Component*) beatKnob.get(),
                                (juce::Component*) punchKnob.get(), (juce::Component*) masterKnob.get() })
        addAndMakeVisible (c);
    bfxBtn.setButtonText (beatFxText());
    viewChips = makeViewSwitcher (proc);
    addAndMakeVisible (*viewChips);
}

LiveView::~LiveView() = default;

void LiveView::setShowWaveforms (bool on)
{
    waveforms = on;
    repaint();
}

void LiveView::setShowFxStrip (bool on)
{
    if (fxStrip == on)
        return;
    fxStrip = on;
    resized();
    repaint();
}

juce::String LiveView::beatFxText() const
{
    // "ECHO 1/8": the beat FX type and its beat fraction (both in the current UI language)
    auto* type = proc.getState().getParameter (gBfxType);
    auto* beat = proc.getState().getParameter (gBfxBeat);
    juce::String t = type != nullptr ? i18n::upper (tr (type->getCurrentValueAsText())) : juce::String ("BEAT FX");
    if (beat != nullptr)
        t << " " << beat->getCurrentValueAsText();
    return t;
}

//============================================================================== layout
void LiveView::resized()
{
    auto b = getLocalBounds().reduced (18, 14);
    headArea = b.removeFromTop (kHeadH);
    {
        auto head = headArea;
        exitBtn.setBounds (head.removeFromRight (128).withSizeKeepingCentre (128, 34));
        head.removeFromRight (10);
        tapBtn.setBounds (head.removeFromRight (80).withSizeKeepingCentre (80, 34));
        const int tw = juce::jmin (500, juce::jmax (300, getWidth() - 2 * 360));
        transportArea = juce::Rectangle<int> (tw, kHeadH).withCentre ({ getWidth() / 2, headArea.getCentreY() });
        // v1.7 views: the view switcher under the logo (where the Esc hint was; the hint is its tooltip now)
        const int x = headArea.getX() + 6;
        viewChips->setBounds (x, headArea.getY() + 47, juce::jmax (120, juce::jmin (320, transportArea.getX() - x - 12)), 24);
    }
    b.removeFromTop (kGap);
    footArea = {};
    for (juce::Component* c : { (juce::Component*) &allBtn, (juce::Component*) &captureBtn, (juce::Component*) &bfxBtn,
                                (juce::Component*) repeatBtn.get(), (juce::Component*) stutterBtn.get(), (juce::Component*) fillBtn.get(),
                                (juce::Component*) filterKnob.get(), (juce::Component*) beatKnob.get(),
                                (juce::Component*) punchKnob.get(), (juce::Component*) masterKnob.get() })
        c->setVisible (fxStrip);
    if (fxStrip)
    {
        footArea = b.removeFromBottom (kFootH);
        b.removeFromBottom (14);
        auto f = footArea.reduced (16, 12);
        f.removeFromTop (24);
        auto right = f.removeFromRight (330).withSizeKeepingCentre (330, 52);
        captureBtn.setBounds (right.removeFromRight (110));
        right.removeFromRight (10);
        allBtn.setBounds (right);
        f.removeFromRight (18);
        const int kw = 78;
        for (auto* k : { filterKnob.get(), beatKnob.get(), punchKnob.get(), masterKnob.get() })
            k->setBounds (f.removeFromLeft (kw).withTrimmedTop (2));
        f.removeFromLeft (14);
        auto row = f.withSizeKeepingCentre (f.getWidth(), 76);
        auto r1 = row.removeFromTop (34);
        auto r2 = row.withTrimmedTop (8);
        const int bw = juce::jmin (116, (f.getWidth() - 8 - 16 - 120) / 2);
        bfxBtn.setBounds (r1.removeFromLeft (bw));
        r1.removeFromLeft (8);
        repeatBtn->setBounds (r1.removeFromLeft (bw));
        stutterBtn->setBounds (r2.removeFromLeft (bw));
        r2.removeFromLeft (8);
        fillBtn->setBounds (r2.removeFromLeft (bw));
        punchBar = f.withSizeKeepingCentre (f.getWidth(), 76).withLeft (fillBtn->getRight() + 24);
    }

    numCards = proc.getVisibleTracks();
    const int w = (b.getWidth() - (numCards - 1) * kGap) / juce::jmax (1, numCards);
    for (int t = 0; t < kNumTracks; ++t)
    {
        auto& c = cardParts[(size_t) t];
        const bool shown = t < numCards;
        c.pad->setVisible (shown);
        c.stop->setVisible (shown);
        c.level->setVisible (shown);
        if (! shown)
            c.length->setVisible (false);
        if (! shown)
        {
            cards[(size_t) t] = {};
            continue;
        }
        const auto card = juce::Rectangle<int> (b.getX() + t * (w + kGap), b.getY(), w, b.getHeight());
        cards[(size_t) t] = card;
        auto inner = card.reduced (14, 12);
        {
            // title; v1.7: the loop length pill on its right while the track is empty / recording
            auto title = inner.removeFromTop (30);
            const int ph = 24;
            const int pw = juce::jmin (c.length->getPreferredWidth (ph), title.getWidth() - 96);
            c.length->setBounds (title.removeFromRight (pw).withSizeKeepingCentre (pw, ph));
        }
        c.display = inner.removeFromTop (juce::jlimit (120, 205, inner.getHeight() * 205 / 560));
        inner.removeFromTop (8);
        c.stop->setBounds (inner.removeFromBottom (48));
        auto lv = inner.removeFromBottom (62).reduced (0, 8);
        c.levelCaption = lv.removeFromTop (16);
        c.level->setBounds (lv.withSizeKeepingCentre (lv.getWidth(), 20));
        const int d = juce::jmin (inner.getWidth() + 8, inner.getHeight());
        c.pad->setBounds (inner.withSizeKeepingCentre (d, d));
    }
}

void LiveView::refresh (bool blinkPhase)
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
        {
            // v1.7: the loop length matters until the first loop exists
            const auto& ts = proc.getEngine().getTrackStatus (t);
            const auto state = (LoopTrack::State) ts.state.load();
            const bool want = state == LoopTrack::State::Empty || state == LoopTrack::State::Recording;
            if (c.length->isVisible() != want)
                c.length->setVisible (want);
            if (want)
                c.length->refresh();
        }
    }
    if (proc.getVisibleTracks() != numCards)
        resized();
    const auto bfx = beatFxText();
    if (bfx != shownBfx)
    {
        shownBfx = bfx;
        bfxBtn.setButtonText (bfx);
    }
    if (shownTheme != theme::version())
    {
        shownTheme = theme::version();
        repaint();
    }
    repaint (transportArea);
    viewChips->refresh();
    if (fxStrip)
        repaint (punchBar);
}

bool LiveView::keyPressed (const juce::KeyPress& key)
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
void LiveView::paint (juce::Graphics& g)
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
        glow (w - 80.0f, 160.0f, 560.0f, colours::brand2, 0.10f);
    }

    // header: logo + LIVE badge (the view switcher sits below)
    drawLogo (g, { (float) headArea.getX() + 6.0f, (float) headArea.getY() - 4.0f }, "LIVE");
    paintTransport (g);
    for (int t = 0; t < numCards; ++t)
        paintCard (g, t);
    if (fxStrip)
        paintFxStrip (g);
}

void LiveView::paintTransport (juce::Graphics& g)
{
    drawTransport (g, proc, transportArea.toFloat());
}

void LiveView::drawTransport (juce::Graphics& g, LiveLooperProcessor& proc, juce::Rectangle<float> box)
{
    auto& eng = proc.getEngine();
    const float k = juce::jlimit (0.5f, 1.5f, box.getHeight() / 78.0f);     // designed at 78 px height
    LooperLookAndFeel::drawSoftShadow (g, box, 12.0f, 0.8f);
    drawDisplayBox (g, box, 12.0f);
    auto l = box.reduced (22.0f * k, 6.0f);
    const float unit = l.getWidth() / 456.0f;          // proportions of the 500 px display
    const double bpm = eng.getCurrentBpm() > 0.0 ? eng.getCurrentBpm() : 120.0;
    g.setColour (colours::lcdText);
    g.setFont (LooperLookAndFeel::font (44.0f * k, true));
    g.drawFittedText (juce::String (bpm, 1), l.removeFromLeft (150.0f * unit).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
    g.setFont (LooperLookAndFeel::font (11.0f, true));
    g.setColour (colours::lcdText.withAlpha (0.7f));
    g.drawText ("BPM", l.removeFromLeft (44.0f * unit).withTrimmedTop (24.0f * k), juce::Justification::centredLeft);
    // bar . beat and the beat dots (first beat orange)
    const double measure = juce::jmax (0.25, eng.getMeasurePpq());
    const double ppq = juce::jmax (0.0, eng.getTimelinePpq());
    const int beats = juce::jlimit (1, 8, juce::roundToInt (measure));
    const int bar = (int) std::floor (ppq / measure) + 1;
    const int beat = juce::jlimit (1, beats, (int) std::floor (std::fmod (ppq, measure)) + 1);
    g.setColour (colours::lcdText);
    g.setFont (LooperLookAndFeel::font (44.0f * k, true));
    g.drawFittedText (juce::String (bar) + "." + juce::String (beat), l.removeFromLeft (130.0f * unit).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
    auto info = l;
    g.setFont (LooperLookAndFeel::font (10.5f, true));
    g.setColour (colours::lcdText.withAlpha (0.7f));
    g.drawFittedText (tr ("BAR . BEAT"), info.removeFromTop (info.getHeight() * 0.5f).toNearestInt(), juce::Justification::bottomLeft, 1, 0.7f);
    const float d = juce::jmax (3.0f, juce::jmin (12.0f * k, (info.getWidth() - 4.0f) / (float) beats - 6.0f));
    for (int i = 0; i < beats; ++i)
    {
        const auto dot = juce::Rectangle<float> (info.getX() + (float) i * (d + 6.0f), info.getY() + 7.0f * k, d, d);
        g.setColour (i < beat ? (i == 0 ? colours::orange : colours::lcdText) : colours::lcdText.withAlpha (0.15f));
        g.fillEllipse (dot);
    }
}

void LiveView::paintCard (juce::Graphics& g, int t)
{
    auto& eng = proc.getEngine();
    const auto& st = eng.getTrackStatus (t);
    const auto state = (LoopTrack::State) st.state.load();
    const auto pending = (LooperEngine::Pending) st.pending.load();
    const auto& parts = cardParts[(size_t) t];
    const auto c = cards[(size_t) t].toFloat();
    const auto col = colours::track (t);
    LooperLookAndFeel::drawGlassPanel (g, c, 16.0f, 2, col);

    auto in = c.reduced (14.0f, 12.0f);
    auto title = in.removeFromTop (30.0f);
    g.setColour (col);
    g.fillRoundedRectangle (title.getX() + 1.0f, title.getCentreY() - 7.0f, 4.0f, 14.0f, 2.0f);
    g.setColour (colours::text);
    g.setFont (LooperLookAndFeel::font (17.0f, true));
    g.drawText (tr ("TRACK %1", juce::String (t + 1)), title.withTrimmedLeft (13.0f), juce::Justification::centredLeft);

    juce::Colour sc;
    const auto text = stateWord (proc, t, sc);
    juce::ignoreUnused (pending);

    // the state on a large display
    const auto disp = parts.display.toFloat();
    drawDisplayBox (g, disp, 10.0f);
    auto lt = disp.reduced (15.0f, 11.0f);
    g.setColour (sc);
    g.setFont (LooperLookAndFeel::font (juce::jlimit (22.0f, 38.0f, c.getWidth() * 0.19f), true));
    g.drawFittedText (tr (text), lt.removeFromTop (42.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.6f);
    g.setFont (LooperLookAndFeel::font (11.5f, true));
    g.setColour (colours::lcdText.withAlpha (0.8f));
    const float prog = state == LoopTrack::State::Empty ? 0.0f : juce::jlimit (0.0f, 1.0f, st.progress.load());
    auto info = lt.removeFromTop (16.0f);
    bool hasLength = false;
    const auto lengthText = LiveView::lengthText (proc, t, hasLength);
    g.drawText (lengthText, info, juce::Justification::centredLeft);
    if (hasLength && (state == LoopTrack::State::Playing || state == LoopTrack::State::Overdubbing))
        g.drawText (juce::String (juce::roundToInt (prog * 100.0f)) + " %", info, juce::Justification::centredRight);
    auto waveArea = lt.withTrimmedTop (6.0f);
    auto* strip = stripFn ? stripFn (t) : nullptr;
    if (waveforms && strip != nullptr)
    {
        LoopWaveform wf;
        wf.peaks = strip->getWaveform().data();
        wf.layer = strip->hasOverdubLayer() ? strip->getOverdubLayer().data() : nullptr;
        wf.numBins = TrackStrip::kWaveBins;
        wf.valid = strip->hasWaveform() && state != LoopTrack::State::Empty;
        wf.progress = prog;
        wf.ghost = st.ghostProgress.load();
        wf.state = state;
        wf.colour = col;
        wf.headWidth = 2.5f;
        if (wf.valid)
            wf.draw (g, waveArea);
        else
        {
            auto pb = waveArea.withSizeKeepingCentre (waveArea.getWidth(), 6.0f);
            g.setColour (colours::lcdText.withAlpha (0.12f));
            g.fillRoundedRectangle (pb, 3.0f);
        }
    }
    else
    {
        // loop position bar (without waveforms)
        auto pb = waveArea.withSizeKeepingCentre (waveArea.getWidth(), 6.0f);
        g.setColour (colours::lcdText.withAlpha (0.12f));
        g.fillRoundedRectangle (pb, 3.0f);
        if (hasLength || state == LoopTrack::State::Recording)
        {
            g.setColour (state == LoopTrack::State::Recording ? colours::red : col);
            g.fillRoundedRectangle (pb.withWidth (pb.getWidth() * (state == LoopTrack::State::Recording ? 1.0f : prog)), 3.0f);
        }
    }

    // level caption above the level bar
    g.setFont (LooperLookAndFeel::font (10.5f, true));
    g.setColour (colours::textDim);
    g.drawText (tr ("LEVEL"), parts.levelCaption, juce::Justification::centredLeft);
    g.setColour (colours::text);
    g.drawText (juce::String (juce::roundToInt (parts.level->getValue() * 100.0)) + " %", parts.levelCaption, juce::Justification::centredRight);
}

void LiveView::paintFxStrip (juce::Graphics& g)
{
    LooperLookAndFeel::drawGlassPanel (g, footArea.toFloat(), 14.0f, 1);
    auto ft = footArea.reduced (16, 10).removeFromTop (20).toFloat();
    g.setColour (colours::orange);
    g.fillRoundedRectangle (ft.getX(), ft.getCentreY() - 6.0f, 3.0f, 12.0f, 1.5f);
    g.setColour (colours::text);
    g.setFont (LooperLookAndFeel::font (12.0f, true));
    g.drawText (tr ("PERFORMANCE FX"), ft.withTrimmedLeft (10.0f), juce::Justification::centredLeft);

    // punch FX amount: the held punch FX (live amount) or the selected one with its amount setting
    auto pb = punchBar.toFloat();
    if (pb.getWidth() < 40.0f)
        return;
    const auto& punch = proc.getEngine().getPunchFx();
    int shown = selectedPunch ? selectedPunch() : 0;
    for (int i = 0; i < kNumPunchFx; ++i)
        if (punch.isOn (i))
        {
            shown = i;
            break;
        }
    const float live = livePunchAmount ? livePunchAmount() : -1.0f;
    const float amount = live >= 0.0f ? live : proc.getState().getRawParameterValue (gPfxAmount)->load();
    g.setColour (colours::textDim);
    g.setFont (LooperLookAndFeel::font (10.5f, true));
    g.drawFittedText (tr ("PUNCH FX") + "  " + i18n::upper (tr (getPunchFxName (juce::jlimit (0, kNumPunchFx - 1, shown)))),
                      pb.removeFromTop (20.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
    auto bar = pb.removeFromTop (14.0f);
    g.setColour (juce::Colours::black.withAlpha (0.45f * theme::shade()));
    g.fillRoundedRectangle (bar, 7.0f);
    g.setColour (colours::learn.withAlpha (punch.isActive() || live >= 0.0f ? 1.0f : 0.75f));
    g.fillRoundedRectangle (bar.withWidth (juce::jmax (bar.getHeight(), bar.getWidth() * juce::jlimit (0.0f, 1.0f, amount))), 7.0f);
    g.setColour (colours::text);
    g.drawText (juce::String (juce::roundToInt (amount * 100.0f)) + " %", pb.removeFromTop (20.0f), juce::Justification::centredRight);
}

//============================================================================== level bar
void LiveView::LevelBar::paint (juce::Graphics& g)
{
    // a meter (green - yellow - red) with the level as a white mark; drag to change the level (0 .. 200 %)
    const auto r = getLocalBounds().toFloat().withSizeKeepingCentre ((float) getWidth(), 12.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f * theme::shade()));
    g.fillRoundedRectangle (r, 6.0f);
    if (meter > 0.001f)
    {
        juce::ColourGradient m (colours::green, r.getX(), 0.0f, colours::red, r.getRight(), 0.0f, false);
        m.addColour (0.75, colours::yellow);
        g.setGradientFill (m);
        g.fillRoundedRectangle (r.withWidth (juce::jmax (6.0f, r.getWidth() * juce::jlimit (0.0f, 1.0f, meter))), 6.0f);
    }
    const auto range = getRange();
    const float pos = (float) ((getValue() - range.getStart()) / juce::jmax (1.0e-6, range.getLength()));
    const float x = r.getX() + r.getWidth() * juce::jlimit (0.0f, 1.0f, pos);
    g.setColour (colours::text);
    g.fillRect (x - 1.0f, r.getY() - 4.0f, 2.0f, r.getHeight() + 8.0f);
}

//============================================================================== LIVE button
void LiveButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = getToggleState();
    g.setGradientFill (LooperLookAndFeel::brandGradient (r, on ? 1.0f : (over ? 0.95f : 0.8f)));
    g.fillRoundedRectangle (down ? r.reduced (0.5f) : r, r.getHeight() * 0.5f);
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
    g.setColour (juce::Colours::white);
    g.setFont (LooperLookAndFeel::font (10.5f, true));
    g.drawText ("LIVE", r, juce::Justification::centred);
}

void LiveButton::mouseDown (const juce::MouseEvent& e)
{
    if (handleMidiRightClick (*this, e))
        return;
    juce::Button::mouseDown (e);
}

//============================================================================== v1.7 views: choice chips
ChoiceChips::ChoiceChips (APVTS& state, const juce::String& paramId, const juce::StringArray& englishLabels,
                          std::vector<int> choices, juce::Colour acc)
    : param (state.getParameter (paramId)), labels (englishLabels), values (std::move (choices)), accent (acc)
{
    if (values.empty())
        for (int i = 0; i < labels.size(); ++i)
            values.push_back (i);
    setParamId (*this, paramId);
    setRepaintsOnMouseActivity (false);
}

int ChoiceChips::getSelectedChip() const
{
    if (param == nullptr)
        return -1;
    const int current = juce::roundToInt (param->convertFrom0to1 (param->getValue()));
    for (size_t i = 0; i < values.size(); ++i)
        if (values[i] == current)
            return (int) i;
    return -1;
}

juce::Rectangle<int> ChoiceChips::getChipBounds (int chip) const
{
    const int n = juce::jmax (1, labels.size());
    const int gap = grouped ? 0 : juce::jmax (4, getHeight() / 5);
    auto r = getLocalBounds();
    if (grouped)
        r = r.reduced (3);
    const float w = (float) (r.getWidth() - (n - 1) * gap) / (float) n;
    const int x0 = r.getX() + juce::roundToInt ((float) chip * (w + (float) gap));
    const int x1 = r.getX() + juce::roundToInt ((float) chip * (w + (float) gap) + w);
    return { x0, r.getY(), x1 - x0, r.getHeight() };
}

juce::String ChoiceChips::getChipText (int chip) const
{
    return i18n::upper (i18n::tr (labels[chip]));
}

void ChoiceChips::selectChip (int chip)
{
    if (! juce::isPositiveAndBelow (chip, (int) values.size()) || param == nullptr)
        return;
    const int v = values[(size_t) chip];
    if (onSelect)
        onSelect (v);
    else
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 ((float) v));
        param->endChangeGesture();
    }
    refresh();
}

void ChoiceChips::refresh()
{
    const int sel = getSelectedChip();
    if (sel != shown)
    {
        shown = sel;
        repaint();
    }
}

void ChoiceChips::mouseDown (const juce::MouseEvent& e)
{
    if (handleMidiRightClick (*this, e))
        return;
    for (int i = 0; i < labels.size(); ++i)
        if (getChipBounds (i).contains (e.getPosition()))
            selectChip (i);
}

void ChoiceChips::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (int i = 0; i < labels.size(); ++i)
        if (getChipBounds (i).contains (e.getPosition()))
            h = i;
    if (h != hover)
    {
        hover = h;
        repaint();
    }
}

void ChoiceChips::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

void ChoiceChips::paint (juce::Graphics& g)
{
    const int sel = getSelectedChip();
    shown = sel;
    const bool light = theme::isLight();
    if (grouped)
    {
        // one dark pill around the chips (the view switcher of the mockup)
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (light ? juce::Colour (0xffe4e6ec) : juce::Colour (0xff0d0e12));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (colours::text.withAlpha (light ? 0.18f : 0.14f));
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
    }
    const float fh = juce::jlimit (9.0f, 13.0f, (float) getHeight() * 0.42f);
    for (int i = 0; i < labels.size(); ++i)
    {
        const auto r = getChipBounds (i).toFloat().reduced (grouped ? 0.0f : 0.5f);
        const float rad = grouped ? r.getHeight() * 0.5f : juce::jmin (8.0f, r.getHeight() * 0.3f);
        const bool on = i == sel;
        if (on && brand)
        {
            g.setGradientFill (LooperLookAndFeel::brandGradient (r));
            g.fillRoundedRectangle (r, rad);
        }
        else if (on)
        {
            g.setColour (accent.withAlpha (light ? 0.22f : 0.18f));
            g.fillRoundedRectangle (r, rad);
            g.setColour (accent);
            g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.5f);
        }
        else if (! grouped)
        {
            g.setColour (light ? juce::Colour (0xfff7f8fb) : juce::Colour (0xff1c1d23));
            g.fillRoundedRectangle (r, rad);
            g.setColour (colours::text.withAlpha (i == hover ? 0.4f : 0.18f));
            g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
        }
        else if (i == hover)
        {
            g.setColour (colours::text.withAlpha (0.08f));
            g.fillRoundedRectangle (r, rad);
        }
        g.setColour (on ? (brand ? juce::Colours::white : (light ? accent.darker (0.5f) : accent)) : colours::text.withAlpha (0.85f));
        g.setFont (LooperLookAndFeel::font (fh, true));
        g.drawFittedText (getChipText (i), r.reduced (3.0f, 0.0f).toNearestInt(), juce::Justification::centred, 1, 0.6f);
    }
}

//============================================================================== v1.7 Performance view: pad tiles
StagePad::StagePad (APVTS& state, const juce::String& paramId, const juce::String& englishTitle, juce::Colour c, Mode m)
    : param (state.getParameter (paramId)), title (englishTitle), colour (c), mode (m)
{
    setParamId (*this, paramId);
}

bool StagePad::isLit() const
{
    if (mode == Mode::trigger)
        return down;
    return param != nullptr && param->getValue() >= 0.5f;
}

void StagePad::refresh()
{
    const bool lit = isLit();
    const auto sub = subtitle ? subtitle() : juce::String();
    if (lit != shownLit || sub != shownSub || (lit && amount))
    {
        shownLit = lit;
        shownSub = sub;
        repaint();
    }
}

void StagePad::mouseDown (const juce::MouseEvent& e)
{
    if (handleMidiRightClick (*this, e))
        return;
    down = true;
    auto set = [this] (float v) {
        if (param == nullptr)
            return;
        param->beginChangeGesture();
        param->setValueNotifyingHost (v);
        param->endChangeGesture();
    };
    if (mode == Mode::momentary)
        set (1.0f);
    else if (mode == Mode::toggle)
        set (isLit() ? 0.0f : 1.0f);
    if (onPress)
        onPress();
    repaint();
}

void StagePad::mouseUp (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() || ! down)
        return;
    down = false;
    if (mode == Mode::momentary && param != nullptr)
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (0.0f);
        param->endChangeGesture();
    }
    repaint();
}

void StagePad::paint (juce::Graphics& g)
{
    const bool lit = isLit();
    shownLit = lit;
    const bool light = theme::isLight();
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const float rad = juce::jmin (12.0f, r.getHeight() * 0.18f);
    if (lit)
    {
        if (brandLit)
            g.setGradientFill (LooperLookAndFeel::brandGradient (r));
        else
            g.setGradientFill (juce::ColourGradient (colour.brighter (0.25f), 0.0f, r.getY(), colour.darker (0.25f), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, rad);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.2f);
    }
    else
    {
        LooperLookAndFeel::drawGlassPanel (g, r, rad, 1, neutral ? juce::Colours::transparentBlack : colour.withAlpha (0.10f));
        g.setColour (neutral ? colours::text.withAlpha (light ? 0.25f : 0.16f) : colour.withAlpha (light ? 0.75f : 0.55f));
        g.drawRoundedRectangle (r.reduced (0.5f), rad, neutral ? 1.0f : 1.4f);
    }
    if (mark == 1)
    {
        g.setColour (lit ? juce::Colours::white.withAlpha (0.9f) : colour);
        g.fillRoundedRectangle (r.getX() + 12.0f, r.getY() + 9.0f, juce::jmin (16.0f, r.getWidth() * 0.18f), 3.0f, 1.5f);
    }
    else if (mark == 2 && ! lit)
    {
        g.setColour (colour);
        g.fillRoundedRectangle (r.getX() + r.getWidth() * 0.1f, r.getBottom() - 10.0f, r.getWidth() * 0.8f, 4.0f, 2.0f);
    }
    const auto sub = subtitle ? subtitle() : juce::String();
    shownSub = sub;
    const float th = juce::jlimit (10.0f, 22.0f, juce::jmin (r.getHeight() * (sub.isEmpty() ? 0.32f : 0.27f), r.getWidth() * 0.16f));
    auto content = r.reduced (6.0f, 4.0f);
    const float a = lit && amount ? amount() : -1.0f;
    if (a >= 0.0f)
        content.removeFromBottom (12.0f);
    const auto textCol = lit ? juce::Colours::white : (light ? colours::text : colours::text);
    if (sub.isNotEmpty())
    {
        auto top = content.removeFromTop (content.getHeight() * 0.58f);
        g.setColour (textCol);
        g.setFont (LooperLookAndFeel::font (th, true));
        g.drawFittedText (i18n::upper (getTitle()), top.toNearestInt(), juce::Justification::centredBottom, 1, 0.6f);
        g.setColour (lit ? juce::Colours::white.withAlpha (0.9f) : colours::textDim);
        g.setFont (LooperLookAndFeel::font (juce::jmax (9.0f, th * 0.5f), true));
        g.drawFittedText (sub, content.withTrimmedTop (3.0f).toNearestInt(), juce::Justification::centredTop, 1, 0.7f);
    }
    else
    {
        g.setColour (textCol);
        g.setFont (LooperLookAndFeel::font (th, true));
        g.drawFittedText (i18n::upper (getTitle()), content.toNearestInt(), juce::Justification::centred, 2, 0.6f);
    }
    if (a >= 0.0f)
    {
        auto bar = juce::Rectangle<float> (r.getX() + 12.0f, r.getBottom() - 14.0f, r.getWidth() - 24.0f, 6.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (bar.withWidth (juce::jmax (6.0f, bar.getWidth() * juce::jlimit (0.0f, 1.0f, a))), 3.0f);
    }
}
} // namespace livelooper
