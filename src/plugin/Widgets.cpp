#include "Widgets.h"

namespace livelooper
{
using namespace params;
using i18n::tr;

//============================================================================== helpers
void setParamId (juce::Component& c, const juce::String& paramId)
{
    c.getProperties().set ("paramId", paramId);
}

juce::String getParamId (const juce::Component& c)
{
    return c.getProperties()["paramId"].toString();
}

juce::Component* findMappable (juce::Component* c)
{
    while (c != nullptr)
    {
        if (getParamId (*c).isNotEmpty())
            return c;
        c = c->getParentComponent();
    }
    return nullptr;
}

bool handleMidiRightClick (juce::Component& c, const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        return false;
    if (auto* host = c.findParentComponentOfClass<MidiMenuHost>())
        if (auto* target = findMappable (&c))
            host->showMidiMenuFor (*target);
    return true;
}

//============================================================================== basic controls
LLSlider::LLSlider (juce::Slider::SliderStyle style, juce::Colour accent)
    : juce::Slider (style, juce::Slider::NoTextBox)
{
    setColour (juce::Slider::rotarySliderFillColourId, accent);
    setColour (juce::Slider::trackColourId, accent);
    setPopupDisplayEnabled (true, true, nullptr);
    setVelocityBasedMode (false);
}

void LLSlider::mouseDown (const juce::MouseEvent& e)
{
    if (! handleMidiRightClick (*this, e))
        juce::Slider::mouseDown (e);
}

juce::String LLSlider::getTooltip()
{
    const auto explicitTip = juce::Slider::getTooltip();
    if (explicitTip.isNotEmpty())
        return explicitTip;
    const auto id = getParamId (*this);
    return id.isNotEmpty() ? localisedName (id) : juce::String();
}

LLButton::LLButton (const juce::String& buttonText, juce::Colour led) : juce::TextButton (buttonText)
{
    getProperties().set ("ledColour", (juce::int64) led.getARGB());
}

void LLButton::mouseDown (const juce::MouseEvent& e)
{
    if (! handleMidiRightClick (*this, e))
        juce::TextButton::mouseDown (e);
}

//============================================================================== v1.6 record button
juce::String RecordButton::formatTime (double seconds)
{
    const int s = (int) juce::jlimit (0.0, 359999.0, std::isfinite (seconds) ? seconds : 0.0);
    return s >= 3600 ? juce::String::formatted ("%d:%02d:%02d", s / 3600, (s / 60) % 60, s % 60)
                     : juce::String::formatted ("%02d:%02d", s / 60, s % 60);
}

void RecordButton::setBeatPhase (double phase)
{
    const bool on = ! getToggleState() || (phase - std::floor (phase)) < 0.5;
    if (on != lit)
    {
        lit = on;
        repaint();
    }
}

void RecordButton::setElapsedSeconds (double seconds)
{
    const auto t = getToggleState() ? formatTime (seconds) : juce::String();
    if (t != timeText)
    {
        timeText = t;
        repaint();
    }
}

void RecordButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto b = getLocalBounds().toFloat();
    const float d = juce::jmin (b.getHeight(), 30.0f) - 2.0f;
    const auto circle = juce::Rectangle<float> (b.getRight() - d - 1.0f, b.getCentreY() - d * 0.5f, d, d);
    const bool on = getToggleState();
    const juce::Colour red (0xffff3b3b);

    // ring
    g.setColour (juce::Colour (0xff2a2d33));
    g.fillEllipse (circle);
    g.setColour (on ? red.withAlpha (0.9f) : juce::Colour (0xff4a4e57).brighter (over ? 0.3f : 0.0f));
    g.drawEllipse (circle.reduced (0.75f), 1.5f);
    // the red dot: dim when idle, bright (with a glow) on the beat while recording
    const auto dot = circle.reduced (d * 0.2f);
    if (on && lit)
    {
        g.setColour (red.withAlpha (0.35f));
        g.fillEllipse (dot.expanded (d * 0.12f));
    }
    g.setColour (on ? (lit ? red : red.darker (0.9f)) : red.darker (over ? 0.15f : 0.35f));
    g.fillEllipse (down ? dot.reduced (1.0f) : dot);

    // caption left of the circle: REC, while recording the elapsed time
    auto text = b.withRight (circle.getX() - 6.0f);
    g.setFont (LooperLookAndFeel::font (12.0f, true));
    g.setColour (on ? red : colours::textDim.withAlpha (over ? 1.0f : 0.8f));
    g.drawText (on ? tr ("REC") + " " + timeText : tr ("REC"), text, juce::Justification::centredRight);
}

MomentaryButton::MomentaryButton (APVTS& state, const juce::String& paramId, const juce::String& text, juce::Colour led)
    : LLButton (text, led), param (state.getParameter (paramId))
{
    setParamId (*this, paramId);
    startTimerHz (20);
}

MomentaryButton::~MomentaryButton() { stopTimer(); }

void MomentaryButton::set (bool on)
{
    if (param == nullptr)
        return;
    param->beginChangeGesture();
    param->setValueNotifyingHost (on ? 1.0f : 0.0f);
    param->endChangeGesture();
}

void MomentaryButton::mouseDown (const juce::MouseEvent& e)
{
    if (handleMidiRightClick (*this, e))
        return;
    down = true;
    set (true);
    setToggleState (true, juce::dontSendNotification);
}

void MomentaryButton::mouseUp (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);
    if (! down)
        return;
    down = false;
    set (false);
}

void MomentaryButton::timerCallback()
{
    const bool on = param != nullptr && param->getValue() >= 0.5f;
    if (getToggleState() != on)
        setToggleState (on, juce::dontSendNotification);
}

LLCombo::LLCombo()  { i18n::addListener (this); }
LLCombo::~LLCombo() { i18n::removeListener (this); }

void LLCombo::mouseDown (const juce::MouseEvent& e)
{
    if (! handleMidiRightClick (*this, e))
        juce::ComboBox::mouseDown (e);
}

void LLCombo::setEnglishItems (const juce::StringArray& items)
{
    englishItems = items;
    clear (juce::dontSendNotification);
    addItemList (translated (items), 1);
}

void LLCombo::languageChanged()
{
    // read the selection first: getSelectedId() compares the shown text with the item text
    const int id = getSelectedId();
    for (int i = 0; i < englishItems.size(); ++i)
        changeItemText (i + 1, tr (englishItems[i]));
    // refresh the text of the selected item
    setSelectedId (0, juce::dontSendNotification);
    setSelectedId (id, juce::dontSendNotification);
}

//============================================================================== Knob
Knob::Knob (APVTS& state, const juce::String& paramId, const juce::String& cap, juce::Colour accent)
    : slider (juce::Slider::RotaryHorizontalVerticalDrag, accent), caption (cap)
{
    addAndMakeVisible (slider);
    rebind (state, paramId);
}

void Knob::rebind (APVTS& state, const juce::String& paramId)
{
    attachment.reset();
    attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);
    setParamId (slider, paramId);
}

void Knob::resized()
{
    auto b = getLocalBounds();
    b.removeFromBottom (16);
    slider.setBounds (b);
}

void Knob::paint (juce::Graphics& g)
{
    g.setColour (colours::textDim);
    g.setFont (LooperLookAndFeel::font (10.5f, true));
    g.drawFittedText (i18n::upper (tr (caption)), getLocalBounds().removeFromBottom (16), juce::Justification::centred, 1);
}

//============================================================================== RecPlayButton
RecPlayButton::RecPlayButton (LiveLooperProcessor& p, int t) : proc (p), track (t)
{
    setParamId (*this, trackId (track, kRecPlay));
    setTooltip ("REC / DUB / PLAY  (right-click: MIDI learn)");
}

void RecPlayButton::mouseDown (const juce::MouseEvent& e)
{
    if (handleMidiRightClick (*this, e))
        return;
    pressed = true;
    proc.pressTrigger (trackId (track, kRecPlay));
    repaint();
}

void RecPlayButton::paint (juce::Graphics& g)
{
    const auto& st = proc.getEngine().getTrackStatus (track);
    const auto state = (LoopTrack::State) st.state.load();
    const auto pending = (LooperEngine::Pending) st.pending.load();
    const float progress = st.progress.load();

    juce::Colour colour = colours::track (track);
    bool active = false, fullRing = false, dimRing = false;

    switch (state)
    {
        case LoopTrack::State::Recording:   colour = colours::red;    active = true; fullRing = progress <= 0.0f; break;
        case LoopTrack::State::Overdubbing: colour = colours::orange; active = true; break;
        case LoopTrack::State::Playing:     colour = colours::green;  active = true; break;
        case LoopTrack::State::Stopped:     colour = colours::green;  dimRing = true; break;
        case LoopTrack::State::Empty:       break;
    }
    if (pending == LooperEngine::Pending::Record)
    {
        colour = colours::red; fullRing = true; active = blink;
    }
    else if (pending == LooperEngine::Pending::Play)
    {
        colour = colours::green; fullRing = true; active = blink;
    }
    else if (pending == LooperEngine::Pending::FinishRecord || pending == LooperEngine::Pending::FinishRecordThenStop)
        fullRing = blink;
    if (st.stopping.load() && blink)
        colour = colour.withMultipliedAlpha (0.5f);

    const auto bounds = getLocalBounds().toFloat();
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto area = juce::Rectangle<float> (size, size).withCentre (bounds.getCentre());
    const auto centre = area.getCentre();
    const float outerR = size * 0.5f - 6.0f;
    const float ringR = outerR - 7.0f;
    const float padR = ringR - 10.0f;

    // static parts (shadow, bezel, pad body with its lighting) are cached per look
    const bool lit = active && state != LoopTrack::State::Empty;
    const juce::Colour padColour = lit ? colour : colours::track (track);
    const auto key = juce::String ((int) size) + "/" + juce::String::toHexString ((int) padColour.getARGB()) + (lit ? "L" : "") + (pressed ? "P" : "") + "/" + juce::String (theme::version());
    if (key != cacheKey || cache.isNull())
    {
        cacheKey = key;
        const float sc = 2.0f;        // rendered at 2x: stays sharp when the window is enlarged
        cache = juce::Image (juce::Image::ARGB, juce::roundToInt (size * sc), juce::roundToInt (size * sc), true);
        juce::Graphics cg (cache);
        cg.addTransform (juce::AffineTransform::scale (sc));
        const juce::Point<float> c (size * 0.5f, size * 0.5f);
        // drop shadow
        juce::ColourGradient sh (juce::Colours::black.withAlpha (theme::isLight() ? 0.25f : 0.6f), c.x, c.y + outerR * 0.15f, juce::Colours::black.withAlpha (0.0f), c.x, c.y + outerR + 6.0f, true);
        cg.setGradientFill (sh);
        cg.fillEllipse (c.x - outerR - 5.0f, c.y - outerR + 1.0f, (outerR + 5.0f) * 2.0f, (outerR + 6.0f) * 2.0f);
        // bezel: machined ring, lit from above
        juce::ColourGradient bez (theme::pick (0xff3a3c44, 0xffffffff), c.x, c.y - outerR, theme::pick (0xff0c0d10, 0xffb9bdc6), c.x, c.y + outerR, false);
        cg.setGradientFill (bez);
        cg.fillEllipse (c.x - outerR, c.y - outerR, outerR * 2.0f, outerR * 2.0f);
        cg.setColour (theme::pick (0xff0b0c0e, 0xffd5d8df));
        cg.fillEllipse (c.x - outerR + 2.5f, c.y - outerR + 2.5f, (outerR - 2.5f) * 2.0f, (outerR - 2.5f) * 2.0f);
        // pad recess
        cg.setColour (theme::pick (0xff000000, 0xffa9aeb8));
        cg.fillEllipse (c.x - padR - 3.0f, c.y - padR - 2.0f, (padR + 3.0f) * 2.0f, (padR + 3.0f) * 2.0f);
        // the pad: soft silicone, tinted by the track colour (idle) or glowing in the state colour
        const auto base = lit ? padColour : (theme::isLight() ? padColour.withMultipliedSaturation (0.45f).withMultipliedBrightness (0.62f)
                                                              : padColour.withMultipliedSaturation (0.55f).withMultipliedBrightness (0.42f));
        juce::ColourGradient body (base.brighter (lit ? 0.35f : 0.25f), c.x - padR * 0.35f, c.y - padR * 0.55f,
                                   base.darker (lit ? 0.55f : 0.8f), c.x + padR * 0.4f, c.y + padR, true);
        if (pressed)
            body.multiplyOpacity (0.85f);
        cg.setGradientFill (body);
        cg.fillEllipse (c.x - padR, c.y - padR, padR * 2.0f, padR * 2.0f);
        // inner shadow at the bottom edge, specular highlight on top
        juce::ColourGradient innerSh (juce::Colours::black.withAlpha (0.0f), c.x, c.y + padR * 0.3f, juce::Colours::black.withAlpha (0.45f), c.x, c.y + padR, false);
        cg.setGradientFill (innerSh);
        cg.fillEllipse (c.x - padR, c.y - padR, padR * 2.0f, padR * 2.0f);
        if (! pressed)
        {
            juce::ColourGradient spec (juce::Colours::white.withAlpha (lit ? 0.42f : 0.22f), c.x, c.y - padR,
                                       juce::Colours::white.withAlpha (0.0f), c.x, c.y - padR * 0.1f, false);
            cg.setGradientFill (spec);
            cg.fillEllipse (c.x - padR * 0.66f, c.y - padR * 0.94f, padR * 1.32f, padR * 0.8f);
        }
        cg.setColour (juce::Colours::white.withAlpha (0.10f));
        cg.drawEllipse (c.x - padR, c.y - padR, padR * 2.0f, padR * 2.0f, 1.0f);
    }
    g.setOpacity (1.0f);
    g.drawImage (cache, area, juce::RectanglePlacement::stretchToFit);

    // glow of a lit pad
    if (lit)
    {
        juce::ColourGradient glow (colour.withAlpha (0.0f), centre.x, centre.y, colour.withAlpha (0.0f), centre.x, centre.y + outerR + 4.0f, true);
        glow.addColour (0.72, colour.withAlpha (0.0f));
        glow.addColour (0.86, colour.withAlpha (0.22f));
        g.setGradientFill (glow);
        g.fillEllipse (area.reduced (1.0f));
    }

    // LED ring: 24 segments with a soft glow
    constexpr int numSegments = 24;
    const int litSeg = (int) std::floor (progress * numSegments + 0.0001f);
    for (int i = 0; i < numSegments; ++i)
    {
        const float a0 = juce::MathConstants<float>::twoPi * (float) i / numSegments + 0.05f;
        const float a1 = juce::MathConstants<float>::twoPi * (float) (i + 1) / numSegments - 0.05f;
        juce::Path seg;
        seg.addCentredArc (centre.x, centre.y, ringR, ringR, 0.0f, a0, a1, true);

        juce::Colour c = theme::pick (0xff24262c, 0xffc4c8d0);
        bool on = false;
        if (state != LoopTrack::State::Empty || pending != LooperEngine::Pending::None)
        {
            if (fullRing)       { c = active || pending == LooperEngine::Pending::None ? colour : colour.withAlpha (0.25f); on = c.getAlpha() > 200; }
            else if (dimRing)   c = colour.withAlpha (0.3f);
            else if (active)    { on = i <= litSeg; c = on ? colour : colour.withAlpha (0.16f); }
        }
        if (fullRing && ! active && pending != LooperEngine::Pending::None)
        {
            c = colour.withAlpha (0.2f);
            on = false;
        }
        if (on)
        {
            g.setColour (c.withAlpha (0.28f));
            g.strokePath (seg, juce::PathStrokeType (11.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        }
        g.setColour (c);
        g.strokePath (seg, juce::PathStrokeType (5.5f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    }

    // legend: what the next press does
    g.setColour (juce::Colours::white.withAlpha (lit ? 0.95f : 0.75f));
    g.setFont (LooperLookAndFeel::font (padR * 0.25f, true));
    g.drawText (tr ("REC"), juce::Rectangle<float> (centre.x - padR, centre.y - padR * 0.5f, padR * 2.0f, padR * 0.4f),
                juce::Justification::centred);
    g.setColour (lit ? juce::Colours::white : colours::red);
    g.fillEllipse (centre.x - padR * 0.30f, centre.y - padR * 0.02f, padR * 0.19f, padR * 0.19f);
    juce::Path tri;
    tri.addTriangle (centre.x + padR * 0.08f, centre.y - padR * 0.02f, centre.x + padR * 0.08f, centre.y + padR * 0.17f,
                     centre.x + padR * 0.27f, centre.y + padR * 0.075f);
    g.setColour (lit ? juce::Colours::white : colours::green);
    g.fillPath (tri);
    // stage mode: the second legend line is larger and brighter (readable from a distance)
    const bool stage = theme::isStage();
    g.setColour (juce::Colours::white.withAlpha (stage ? 0.95f : (lit ? 0.8f : 0.55f)));
    g.setFont (LooperLookAndFeel::font (padR * (stage ? 0.19f : 0.15f), true));
    const bool recPlayMode = juce::roundToInt (proc.getState().getRawParameterValue (gRecAction)->load()) == 1;
    g.drawFittedText (tr (recPlayMode ? "PLAY / DUB" : "DUB / PLAY"),
                      juce::Rectangle<float> (centre.x - padR * 0.8f, centre.y + padR * 0.26f, padR * 1.6f, padR * (stage ? 0.36f : 0.3f)).toNearestInt(),
                      juce::Justification::centred, 1, 0.8f);
}

//============================================================================== StopButton
StopButton::StopButton (LiveLooperProcessor& p, int t) : proc (p), track (t)
{
    setParamId (*this, trackId (track, kStop));
    setTooltip ("STOP  (hold 1.5 s: clear track, right-click: MIDI learn)");
}

void StopButton::mouseDown (const juce::MouseEvent& e)
{
    if (handleMidiRightClick (*this, e))
        return;
    down = true;
    cleared = false;
    downTime = juce::Time::getMillisecondCounter();
    proc.pressTrigger (trackId (track, kStop));
    startTimerHz (30);
    repaint();
}

void StopButton::mouseUp (const juce::MouseEvent&)
{
    // after a clear the button returns to STOP as soon as it is released
    down = false;
    cleared = false;
    stopTimer();
    repaint();
}

void StopButton::timerCallback()
{
    if (down && ! cleared && juce::Time::getMillisecondCounter() - downTime > 1500)
    {
        cleared = true;
        proc.pressTrigger (trackId (track, kClear));
    }
    repaint();
}

void StopButton::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (2.0f);
    const float corner = juce::jmin (10.0f, b.getHeight() * 0.32f);
    if (! down)
        LooperLookAndFeel::drawSoftShadow (g, b, corner, 0.7f, 4.0f, 3.0f);
    const bool light = theme::isLight();
    juce::ColourGradient grad (juce::Colour (light ? (down ? 0xffdfe2e7 : 0xffffffff) : (down ? 0xff26272d : 0xff42444d)), b.getX(), b.getY(),
                               juce::Colour (light ? 0xffe4e6eb : (down ? 0xff2c2d33 : 0xff26272d)), b.getX(), b.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (b, corner);
    g.setColour (juce::Colours::black.withAlpha (light ? 0.18f : 0.7f));
    g.drawRoundedRectangle (b, corner, 1.0f);
    if (! down)
    {
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.drawLine (b.getX() + corner, b.getY() + 1.2f, b.getRight() - corner, b.getY() + 1.2f, 1.0f);
    }

    if (keyHold > 0.0f && ! (down && ! cleared))
    {
        g.setColour (colours::red.withAlpha (0.35f));
        g.fillRoundedRectangle (b.withWidth (b.getWidth() * juce::jmin (1.0f, keyHold)), corner);
    }
    if (down && ! cleared)
    {
        const float held = juce::jlimit (0.0f, 1.0f, (float) (juce::Time::getMillisecondCounter() - downTime) / 1500.0f);
        g.setColour (colours::red.withAlpha (0.35f));
        g.fillRoundedRectangle (b.withWidth (b.getWidth() * held), corner);
    }
    if (cleared)
    {
        g.setColour (colours::red.withAlpha (0.5f));
        g.fillRoundedRectangle (b, corner);
    }

    const auto label = tr (cleared ? "CLEARED" : "STOP");
    const auto f = LooperLookAndFeel::font (juce::jmin (15.0f, b.getHeight() * 0.40f), true);
    const float s = b.getHeight() * 0.28f;
    const float tw = juce::jmin (b.getWidth() - s - 24.0f, juce::GlyphArrangement::getStringWidth (f, label));
    const float x0 = b.getCentreX() - (s + 8.0f + tw) * 0.5f;
    g.setColour (colours::text);
    g.fillRoundedRectangle (x0, b.getCentreY() - s * 0.5f, s, s, 1.5f);
    g.setFont (f);
    g.drawFittedText (label, juce::Rectangle<float> (x0 + s + 8.0f, b.getY(), tw + 2.0f, b.getHeight()).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
}

//============================================================================== meters
void MeterBallistics::push (float peak, float frameRateHz) noexcept
{
    const float fall = std::pow (10.0f, -26.0f / 20.0f / frameRateHz);   // about 26 dB per second
    level = std::max (peak, level * fall);
    if (level < 1.0e-5f)
        level = 0.0f;
    if (peak >= hold)
    {
        hold = peak;
        holdFrames = (int) (1.2f * frameRateHz);
    }
    else if (--holdFrames <= 0)
        hold = std::max (level, hold * fall);
}

float MeterBallistics::toPosition (float gain) noexcept
{
    if (gain <= 0.001f)
        return 0.0f;
    const float db = 20.0f * std::log10 (gain);
    return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
}

void drawMeterBar (juce::Graphics& g, juce::Rectangle<float> area, const MeterBallistics& m, bool vertical, juce::Colour base)
{
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (area, 1.5f);
    const float pos = MeterBallistics::toPosition (m.level);
    // v1.5: green up to -12 dB, yellow up to -3 dB, orange up to 0 dBFS, red above
    const float yellowPos = MeterBallistics::toPosition (juce::Decibels::decibelsToGain (-12.0f));
    const float orangePos = MeterBallistics::toPosition (juce::Decibels::decibelsToGain (-3.0f));
    const float redPos = MeterBallistics::toPosition (1.0f);
    auto segment = [&] (float from, float to, juce::Colour c) {
        const float a = juce::jmin (from, pos), b = juce::jmin (to, pos);
        if (b <= a)
            return;
        g.setColour (c);
        if (vertical)
            g.fillRect (area.getX(), area.getBottom() - area.getHeight() * b, area.getWidth(), area.getHeight() * (b - a));
        else
            g.fillRect (area.getX() + area.getWidth() * a, area.getY(), area.getWidth() * (b - a), area.getHeight());
    };
    segment (0.0f, yellowPos, base);
    segment (yellowPos, orangePos, colours::yellow);
    segment (orangePos, redPos, colours::orange);
    segment (redPos, 1.0f, colours::red);

    const float hp = MeterBallistics::toPosition (m.hold);
    if (hp > 0.0f)
    {
        g.setColour (hp >= redPos ? colours::red : (hp >= orangePos ? colours::orange : colours::text.withAlpha (0.8f)));
        if (vertical)
            g.fillRect (area.getX(), area.getBottom() - area.getHeight() * hp, area.getWidth(), 1.5f);
        else
            g.fillRect (area.getX() + area.getWidth() * hp - 1.0f, area.getY(), 1.5f, area.getHeight());
    }
}

//============================================================================== Lcd
void Lcd::setContent (const juce::String& big, const juce::StringArray& small, juce::Colour accent, bool midiActivity)
{
    if (big == bigText && small == lines && accent == accentColour && midiActivity == midi)
        return;
    bigText = big;
    lines = small;
    accentColour = accent;
    midi = midiActivity;
    repaint();
}

void Lcd::pushMasterMeter (float peakL, float peakR)
{
    master[0].push (peakL);
    master[1].push (peakR);
    repaint (getLocalBounds().removeFromTop (28));
}

juce::Rectangle<int> Lcd::getTempoArea() const
{
    auto content = getLocalBounds().toFloat().reduced (5.0f).reduced (3.0f, 0.0f).reduced (10.0f, 4.0f);
    content.removeFromTop (22.0f);   // master meter row with dB scale
    return content.removeFromLeft (content.getWidth() * 0.36f).toNearestInt();
}

void Lcd::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isLeftButtonDown() && getTempoArea().contains (e.getPosition()) && onTempoClicked)
        onTempoClicked();
}

void Lcd::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (getTempoArea().contains (e.getPosition()) ? juce::MouseCursor::IBeamCursor : juce::MouseCursor::NormalCursor);
}

juce::String Lcd::getTooltip()
{
    return tr ("Click the tempo to type in the BPM");
}

void Lcd::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    auto inner = b.reduced (5.0f);
    // bezel + recessed glass (static: cached at 2x)
    if (bezel.isNull() || bezel.getWidth() != getWidth() * 2 || bezel.getHeight() != getHeight() * 2)
    {
        bezel = juce::Image (juce::Image::ARGB, getWidth() * 2, getHeight() * 2, true);
        juce::Graphics cg (bezel);
        cg.addTransform (juce::AffineTransform::scale (2.0f));
        // machined bezel, lit from above
        juce::ColourGradient bz (juce::Colour (0xff4a4c55), 0.0f, b.getY(), juce::Colour (0xff101115), 0.0f, b.getBottom(), false);
        cg.setGradientFill (bz);
        cg.fillRoundedRectangle (b, 12.0f);
        cg.setColour (juce::Colours::black);
        cg.fillRoundedRectangle (b.reduced (2.0f), 10.0f);
        // the LCD: deep blue-black with a faint backlight in the middle
        juce::ColourGradient lcd (juce::Colour (0xff0b2230), inner.getCentreX(), inner.getCentreY(),
                                  juce::Colour (0xff030a0f), inner.getX(), inner.getY(), true);
        cg.setGradientFill (lcd);
        cg.fillRoundedRectangle (inner, 8.0f);
        // scan lines
        cg.setColour (juce::Colours::black.withAlpha (0.18f));
        for (float y = inner.getY() + 1.0f; y < inner.getBottom(); y += 2.0f)
            cg.fillRect (inner.getX() + 2.0f, y, inner.getWidth() - 4.0f, 0.5f);
        // inner shadow along the top and left edge (the glass sits below the bezel)
        juce::ColourGradient ish (juce::Colours::black.withAlpha (0.75f), 0.0f, inner.getY(), juce::Colours::black.withAlpha (0.0f), 0.0f, inner.getY() + 12.0f, false);
        cg.setGradientFill (ish);
        cg.fillRoundedRectangle (inner, 8.0f);
        cg.setColour (juce::Colours::white.withAlpha (0.08f));
        cg.drawRoundedRectangle (inner.reduced (0.5f), 8.0f, 1.0f);
    }
    if (glass.isNull() || glass.getWidth() != getWidth() * 2 || glass.getHeight() != getHeight() * 2)
    {
        glass = juce::Image (juce::Image::ARGB, getWidth() * 2, getHeight() * 2, true);
        juce::Graphics cg (glass);
        cg.addTransform (juce::AffineTransform::scale (2.0f));
        // a diagonal reflection across the upper half of the glass
        juce::Path refl;
        refl.startNewSubPath (inner.getX(), inner.getY());
        refl.lineTo (inner.getX() + inner.getWidth() * 0.62f, inner.getY());
        refl.lineTo (inner.getX() + inner.getWidth() * 0.42f, inner.getY() + inner.getHeight() * 0.48f);
        refl.lineTo (inner.getX(), inner.getY() + inner.getHeight() * 0.48f);
        refl.closeSubPath();
        juce::Path clip;
        clip.addRoundedRectangle (inner, 8.0f);
        cg.reduceClipRegion (clip);
        juce::ColourGradient rg (juce::Colours::white.withAlpha (0.07f), inner.getX(), inner.getY(), juce::Colours::white.withAlpha (0.0f), inner.getX(), inner.getY() + inner.getHeight() * 0.5f, false);
        cg.setGradientFill (rg);
        cg.fillPath (refl);
        cg.setColour (juce::Colours::white.withAlpha (0.10f));
        cg.fillRect (inner.getX() + 10.0f, inner.getY() + 1.0f, inner.getWidth() - 20.0f, 1.0f);
    }
    g.setOpacity (1.0f);
    g.drawImage (bezel, b, juce::RectanglePlacement::stretchToFit);
    inner = inner.reduced (3.0f, 0.0f);

    auto content = inner.reduced (10.0f, 4.0f);

    // master meter: top row of the display, with a dB scale and the peak (hold) in numbers (v1.5)
    {
        auto row = content.removeFromTop (21.0f).withTrimmedRight (14.0f);
        g.setColour (colours::lcdText.withAlpha (0.55f));
        g.setFont (LooperLookAndFeel::font (8.5f, true));
        const float labelW = 46.0f;
        g.drawText (tr ("MASTER"), row.removeFromLeft (labelW).withHeight (10.0f), juce::Justification::centredLeft);

        const float holdGain = juce::jmax (master[0].hold, master[1].hold);
        const float holdDb = holdGain > 1.0e-5f ? juce::Decibels::gainToDecibels (holdGain) : -100.0f;
        auto readout = row.removeFromRight (52.0f);
        g.setColour (holdDb >= 0.0f ? colours::red : (holdDb >= -3.0f ? colours::orange : (holdDb >= -12.0f ? colours::yellow : colours::lcdText)));
        g.setFont (LooperLookAndFeel::font (12.0f, true));
        g.drawText (holdDb <= -99.0f ? juce::String ("-inf") : (holdDb > 0.0f ? "+" : "") + juce::String (holdDb, 1),
                    readout.withTrimmedLeft (4.0f).withHeight (12.0f), juce::Justification::centredRight);
        g.setColour (colours::lcdText.withAlpha (0.45f));
        g.setFont (LooperLookAndFeel::font (7.5f, true));
        g.drawText ("dBFS", readout.withTrimmedTop (12.0f), juce::Justification::centredRight);

        auto bars = row.removeFromTop (10.0f).reduced (0.0f, 0.5f);
        drawMeterBar (g, bars.removeFromTop (bars.getHeight() * 0.5f).reduced (0.0f, 0.5f), master[0], false, colours::lcdText);
        drawMeterBar (g, bars.reduced (0.0f, 0.5f), master[1], false, colours::lcdText);

        // scale: ticks and labels at -60 ... +6 dBFS
        const auto scale = row.withTrimmedTop (1.0f);
        g.setFont (LooperLookAndFeel::font (7.5f, true));
        for (int db : { -60, -48, -36, -24, -18, -12, -6, -3, 0, 6 })
        {
            const float x = scale.getX() + scale.getWidth() * MeterBallistics::toPosition (juce::Decibels::decibelsToGain ((float) db));
            const auto c2 = db >= 0 ? colours::red : (db >= -3 ? colours::orange : (db >= -12 ? colours::yellow : colours::lcdText));
            g.setColour (c2.withAlpha (0.7f));
            g.fillRect (x - 0.5f, scale.getY(), 1.0f, 2.5f);
            if (db != -18)
                g.drawText ((db > 0 ? "+" : "") + juce::String (db), juce::Rectangle<float> (x - 12.0f, scale.getY() + 2.0f, 24.0f, 8.0f),
                            juce::Justification::centred);
        }
        content.removeFromTop (1.0f);
    }
    const bool hasBig = bigText.isNotEmpty();
    if (hasBig)
    {
        auto bigArea = content.removeFromLeft (content.getWidth() * 0.36f);
        g.setFont (LooperLookAndFeel::font (juce::jmin (bigArea.getHeight() * 0.62f, 34.0f), true));
        // LCD glow: the digits light the glass around them
        g.setColour (accentColour.withAlpha (0.16f));
        for (auto o : { juce::Point<int> (-1, 0), juce::Point<int> (1, 0), juce::Point<int> (0, -1), juce::Point<int> (0, 1), juce::Point<int> (2, 1), juce::Point<int> (-2, -1) })
            g.drawFittedText (bigText, bigArea.toNearestInt().translated (o.x, o.y), juce::Justification::centredLeft, 1, 0.7f);
        g.setColour (accentColour);
        g.drawFittedText (bigText, bigArea.toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
    }

    const int n = juce::jmax (1, lines.size());
    const float lh = content.getHeight() / (float) n;
    g.setFont (LooperLookAndFeel::font (juce::jmin (lh * 0.72f, 15.0f), true));
    for (int i = 0; i < lines.size(); ++i)
    {
        g.setColour (i == 0 && ! hasBig ? accentColour : colours::lcdText.withAlpha (i == 0 ? 1.0f : 0.8f));
        g.drawFittedText (lines[i], content.withY (content.getY() + lh * (float) i).withHeight (lh).toNearestInt(),
                          juce::Justification::centredLeft, 1, 0.75f);
    }

    // MIDI activity
    const auto dot = juce::Rectangle<float> (7.0f, 7.0f).withPosition (inner.getRight() - 12.0f, inner.getY() + 5.0f);
    if (midi)
    {
        g.setColour (colours::yellow.withAlpha (0.3f));
        g.fillEllipse (dot.expanded (3.0f));
    }
    g.setColour (midi ? colours::yellow : colours::lcdText.withAlpha (0.15f));
    g.fillEllipse (dot);
    g.setOpacity (1.0f);
    g.drawImage (glass, b, juce::RectanglePlacement::stretchToFit);
}

//============================================================================== FxSlot
FxSlot::FxSlot (APVTS& s, bool inputFx, int sl, std::function<void (bool, int)> select)
    : onButton (juce::String::charToString ((juce::juce_wchar) ('A' + sl)), inputFx ? colours::cyan : colours::orange),
      state (s), input (inputFx), slot (sl), onSelect (std::move (select))
{
    onButton.setClickingTogglesState (true);
    attachment = std::make_unique<APVTS::ButtonAttachment> (state, fxId (input, slot, kFxOn), onButton);
    setParamId (onButton, fxId (input, slot, kFxOn));
    onButton.tooltipFn = [this] {
        return tr (input ? "Input FX %1 on/off" : "Track FX %1 on/off", juce::String::charToString ((juce::juce_wchar) ('A' + slot)));
    };
    onButton.onClick = [this] { if (onSelect) onSelect (input, slot); };
    addAndMakeVisible (onButton);
}

void FxSlot::resized()
{
    auto b = getLocalBounds();
    onButton.setBounds (b.removeFromTop (b.getHeight() - 22).reduced (5, 3));
    LooperLookAndFeel::setEmphasis (onButton, 2);
}

void FxSlot::paint (juce::Graphics& g)
{
    auto label = getLocalBounds().removeFromBottom (20).reduced (5, 1);
    if (selected)
    {
        g.setColour ((input ? colours::cyan : colours::orange).withAlpha (0.22f));
        g.fillRoundedRectangle (label.toFloat(), label.getHeight() * 0.5f);
    }
    juce::String typeName = "?";
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (fxId (input, slot, kFxType))))
        typeName = tr (fxTypeChoices()[p->getIndex()]);
    g.setColour (selected ? colours::text : colours::textDim);
    g.setFont (LooperLookAndFeel::font (11.0f, true));
    g.drawFittedText (i18n::upper (typeName), label, juce::Justification::centred, 1, 0.7f);
}

void FxSlot::mouseDown (const juce::MouseEvent&)
{
    if (onSelect)
        onSelect (input, slot);
}

//============================================================================== TrackStrip
//============================================================================== v1.7 loop length pill
LoopLengthPill::LoopLengthPill (LiveLooperProcessor& p, int t) : proc (p), track (t)
{
    setParamId (*this, trackId (track, kMeasure));
    setRepaintsOnMouseActivity (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

int LoopLengthPill::getChoice() const
{
    auto* v = proc.getState().getRawParameterValue (trackId (track, kMeasure));
    return v != nullptr ? juce::jlimit (0, (int) kMeasureChoices.size() - 1, juce::roundToInt (v->load())) : 0;
}

void LoopLengthPill::setChoice (int index)
{
    index = juce::jlimit (0, (int) kMeasureChoices.size() - 1, index);
    if (auto* p = proc.getState().getParameter (trackId (track, kMeasure)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
        p->endChangeGesture();
    }
    repaint();
}

juce::String LoopLengthPill::choiceText (int index)
{
    const int bars = kMeasureChoices[(size_t) juce::jlimit (0, (int) kMeasureChoices.size() - 1, index)];
    if (bars <= 0)
        return tr ("AUTO");
    return tr (bars == 1 ? "%1 BAR" : "%1 BARS", juce::String (bars));
}

bool LoopLengthPill::isInactive() const
{
    const auto state = (LoopTrack::State) proc.getEngine().getTrackStatus (track).state.load();
    return state != LoopTrack::State::Empty && state != LoopTrack::State::Recording;
}

juce::String LoopLengthPill::getText() const
{
    const auto& st = proc.getEngine().getTrackStatus (track);
    const auto state = (LoopTrack::State) st.state.load();
    const int rb = st.recBars.load();
    if (rb > 0 && (state == LoopTrack::State::Empty || state == LoopTrack::State::Recording))
        return tr (rb == 1 ? "%1 BAR" : "%1 BARS", juce::String (rb));
    return choiceText (getChoice());
}

int LoopLengthPill::getPreferredWidth (int height) const
{
    const auto f = LooperLookAndFeel::font (large ? 12.5f : 10.0f, true);
    float w = 0.0f;
    // the widest text of both UI languages, so the layout stays put when the language changes
    for (const auto& t : { juce::String ("16 BARS"), juce::String ("16 TAKTE"), juce::String ("AUTO"), choiceText ((int) kMeasureChoices.size() - 1) })
        w = juce::jmax (w, juce::GlyphArrangement::getStringWidth (f, t));
    return juce::roundToInt (w) + height + 6;      // text + loop symbol + padding
}

void LoopLengthPill::paint (juce::Graphics& g)
{
    const auto& st = proc.getEngine().getTrackStatus (track);
    const auto state = (LoopTrack::State) st.state.load();
    const int rb = st.recBars.load();
    const bool recording = state == LoopTrack::State::Recording;
    const bool fixedRec = recording && rb > 0;
    const bool once = state == LoopTrack::State::Empty && rb > 0;
    const bool inactive = isInactive();
    const int choice = getChoice();
    const bool fixed = choice > 0 || rb > 0;

    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float rad = r.getHeight() * 0.5f;
    const float alpha = inactive ? 0.45f : 1.0f;
    const auto accent = once ? colours::orange : (fixedRec ? colours::red : (fixed ? colours::orange : colours::textDim));

    g.setColour (theme::pick (0xff0d0e11, 0xffdfe2e8).withAlpha (0.85f * alpha));
    g.fillRoundedRectangle (r, rad);
    if (fixedRec)
    {
        // progress of the fixed-length recording
        const float prog = juce::jlimit (0.0f, 1.0f, st.progress.load());
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip;
        clip.addRoundedRectangle (r, rad);
        g.reduceClipRegion (clip);
        g.setColour (colours::red.withAlpha (0.35f));
        g.fillRect (r.withWidth (r.getWidth() * prog));
    }
    else if (fixed && ! inactive)
    {
        g.setColour (accent.withAlpha (0.16f));
        g.fillRoundedRectangle (r, rad);
    }
    const bool over = isMouseOver (true) && ! isMouseButtonDown();
    g.setColour (accent.withAlpha ((fixed ? 0.85f : 0.45f) * alpha * (over ? 1.0f : 0.8f) + (over ? 0.1f : 0.0f)));
    if (once)
    {
        // one-shot length: dashed outline
        juce::Path outline;
        outline.addRoundedRectangle (r, rad);
        const float dash[] = { 3.0f, 2.0f };
        juce::PathStrokeType (1.2f).createDashedStroke (outline, outline, dash, 2);
        g.fillPath (outline);
    }
    else
        g.drawRoundedRectangle (r, rad, over ? 1.4f : 1.0f);

    // loop symbol: an open circle with an arrow head
    const float d = r.getHeight() * 0.48f;
    const auto icon = juce::Rectangle<float> (r.getX() + rad * 0.55f, r.getCentreY() - d * 0.5f, d, d);
    const auto ink = (fixed ? accent : colours::text).withAlpha ((fixed ? 1.0f : 0.75f) * alpha);
    {
        juce::Path arc;
        arc.addCentredArc (icon.getCentreX(), icon.getCentreY(), d * 0.5f, d * 0.5f, 0.0f,
                           juce::MathConstants<float>::pi * 0.35f, juce::MathConstants<float>::pi * 2.0f, true);
        g.setColour (ink);
        g.strokePath (arc, juce::PathStrokeType (juce::jmax (1.1f, d * 0.14f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        juce::Path head;
        const float hx = icon.getCentreX(), hy = icon.getY(), a = d * 0.3f;
        head.addTriangle (hx + a * 0.9f, hy, hx - a * 0.3f, hy - a * 0.75f, hx - a * 0.3f, hy + a * 0.75f);
        g.fillPath (head);
    }

    g.setColour ((fixed ? colours::text : colours::text.withAlpha (0.8f)).withAlpha (alpha));
    g.setFont (LooperLookAndFeel::font (large ? 12.5f : 10.0f, true));
    auto text = r.withTrimmedLeft (icon.getRight() - r.getX() + 3.0f).withTrimmedRight (rad * 0.6f);
    g.drawFittedText (getText(), text.toNearestInt(), juce::Justification::centred, 1, 0.75f);
}

void LoopLengthPill::refresh()
{
    const auto& st = proc.getEngine().getTrackStatus (track);
    const auto state = st.state.load();
    juce::String key;
    key << getChoice() << '/' << state << '/' << st.recBars.load() << '/' << (i18n::isGerman() ? 1 : 0);
    if (state == (int) LoopTrack::State::Recording && st.recBars.load() > 0)
        key << '/' << juce::roundToInt (st.progress.load() * 100.0f);
    if (key != shownKey)
    {
        shownKey = key;
        repaint();
    }
}

void LoopLengthPill::mouseDown (const juce::MouseEvent& e)
{
    dragged = false;
    if (handleMidiRightClick (*this, e))
    {
        dragged = true;           // no menu on the release of a right-click
        return;
    }
    dragStartChoice = getChoice();
    dragStartY = e.getPosition().y;
}

void LoopLengthPill::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;
    const int dy = dragStartY - e.getPosition().y;
    if (! dragged && std::abs (dy) < 5)
        return;
    dragged = true;
    const int c = juce::jlimit (0, (int) kMeasureChoices.size() - 1, dragStartChoice + dy / 14);
    if (c != getChoice())
        setChoice (c);
}

void LoopLengthPill::mouseUp (const juce::MouseEvent& e)
{
    if (dragged || e.mods.isPopupMenu() || ! getLocalBounds().contains (e.getPosition()))
        return;
    juce::PopupMenu m;
    m.addSectionHeader (tr ("Loop length - track %1", juce::String (track + 1)));
    const int cur = getChoice();
    for (int i = 0; i < (int) kMeasureChoices.size(); ++i)
    {
        const int bars = kMeasureChoices[(size_t) i];
        m.addItem (i + 1, bars <= 0 ? tr ("Auto (ends with the next press)") : (bars == 1 ? tr ("1 bar") : tr ("%1 bars", juce::String (bars))),
                   true, i == cur);
    }
    if (isInactive())
    {
        m.addSeparator();
        m.addItem (-1, tr ("Applies to the next new recording (after CLEAR)"), false, false);
    }
    juce::Component::SafePointer<LoopLengthPill> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r) {
        if (safe != nullptr && r > 0)
            safe->setChoice (r - 1);
    });
}

void LoopLengthPill::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    wheelAcc += (std::abs (w.deltaY) > std::abs (w.deltaX) ? w.deltaY : -w.deltaX) * (w.isReversed ? -1.0f : 1.0f);
    if (std::abs (wheelAcc) < 0.12f)
        return;
    setChoice (getChoice() + (wheelAcc > 0.0f ? 1 : -1));
    wheelAcc = 0.0f;
}

juce::String LoopLengthPill::getTooltip()
{
    juce::String t = tr ("Loop length: the first recording ends by itself after this many bars (AUTO = ends with your next press). Click: choose, scroll / drag: change, right-click: MIDI learn");
    const auto& st = proc.getEngine().getTrackStatus (track);
    const auto state = (LoopTrack::State) st.state.load();
    if (st.recBars.load() > 0 && state == LoopTrack::State::Empty)
        t << "\n" << tr ("Next recording only: %1 (set on the controller); the setting stays %2", getText(), choiceText (getChoice()));
    else if (isInactive())
        t << "\n" << tr ("The track holds a loop: the length applies to the next new recording (after CLEAR)");
    return t;
}

TrackStrip::TrackStrip (LiveLooperProcessor& p, int t)
    : proc (p), track (t), recPlay (p, t), stop (p, t), lengthPill (p, t)
{
    auto& st = proc.getState();
    for (auto* b : { &reverse, &oneShot, &fx })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (b);
    }
    reverseAtt = std::make_unique<APVTS::ButtonAttachment> (st, trackId (track, kReverse), reverse);
    oneShotAtt = std::make_unique<APVTS::ButtonAttachment> (st, trackId (track, kOneShot), oneShot);
    fxAtt      = std::make_unique<APVTS::ButtonAttachment> (st, trackId (track, kTrackFx), fx);
    setParamId (reverse, trackId (track, kReverse));
    setParamId (oneShot, trackId (track, kOneShot));
    setParamId (fx, trackId (track, kTrackFx));
    reverse.setTooltip ("Reverse playback");
    oneShot.setTooltip ("One shot: play once, then stop");
    fx.setTooltip ("Send this track through the track FX");

    level.setPopupDisplayEnabled (true, true, nullptr);
    levelAtt = std::make_unique<APVTS::SliderAttachment> (st, trackId (track, kLevel), level);
    panAtt   = std::make_unique<APVTS::SliderAttachment> (st, trackId (track, kPan), pan);
    level.setDoubleClickReturnValue (true, 1.0);
    pan.setDoubleClickReturnValue (true, 0.0);
    setParamId (level, trackId (track, kLevel));
    setParamId (pan, trackId (track, kPan));
    addAndMakeVisible (level);
    addAndMakeVisible (pan);
    // v1.5 colour FX knob (centre = off, left / right = more of the effect chosen in COLOR FX)
    colorAtt = std::make_unique<APVTS::SliderAttachment> (st, trackId (track, kColor), color);
    color.setDoubleClickReturnValue (true, 0.0);
    setParamId (color, trackId (track, kColor));
    color.setTooltip ("Color FX of this track: centre = off, turn left / right for more (effect chosen in COLOR FX); double-click = off");
    addAndMakeVisible (color);

    setParamId (undo, trackId (track, kUndo));
    setParamId (redo, trackId (track, kRedo));
    undo.onClick = [this] { proc.pressTrigger (trackId (track, kUndo)); };
    redo.onClick = [this] { proc.pressTrigger (trackId (track, kRedo)); };
    settings.onClick = [this] {
        if (auto* host = findParentComponentOfClass<MidiMenuHost>())
            host->showTrackSettings (track, settings);
    };
    settings.setTooltip ("Track settings, clear, import / export");
    undo.setTooltip ("Undo the last recording / overdub");
    redo.setTooltip ("Redo");
    for (auto* b : { &undo, &redo, &settings })
        addAndMakeVisible (b);

    addAndMakeVisible (recPlay);
    addAndMakeVisible (stop);
    addAndMakeVisible (lengthPill);     // v1.7
}

bool TrackStrip::showWaveforms = false;    // v1.7: SETUP > Display > Show waveforms (default off)
bool TrackStrip::layersWanted = false;
bool TrackStrip::showMeters = true;     // v1.6 final: IN / OUT meters by default (SETUP can hide them)

void TrackStrip::resized()
{
    // v1.6 final: the classic order - display, REV / 1SHOT / FX, fader + meter + PAN / COLOR + UNDO / REDO / SETUP,
    // the big REC / PLAY pad, STOP at the bottom
    auto b = getLocalBounds().reduced (10, 8);
    auto title = b.removeFromTop (26);
    ledArea = title.removeFromRight (16).withSizeKeepingCentre (10, 10);
    {
        // v1.7 loop length pill between the track name and the LED
        title.removeFromRight (4);
        const int ph = 20;
        const int pw = juce::jmin (lengthPill.getPreferredWidth (ph), title.getWidth() - 70);
        lengthPill.setBounds (title.removeFromRight (pw).withSizeKeepingCentre (pw, ph));
    }
    lcdArea = b.removeFromTop (60);
    b.removeFromTop (8);

    auto row = b.removeFromTop (24);
    const int bw = row.getWidth() / 3;
    reverse.setBounds (row.removeFromLeft (bw).reduced (2, 0));
    oneShot.setBounds (row.removeFromLeft (bw).reduced (2, 0));
    fx.setBounds (row.reduced (2, 0));
    b.removeFromTop (8);

    auto mix = b.removeFromTop (juce::jmin (136, b.getHeight() * 42 / 100));
    level.setBounds (mix.removeFromLeft (30));
    if (showMeters)
        meterArea = mix.removeFromLeft (12).withTrimmedTop (6).withTrimmedBottom (16);   // stereo meter + IN / OUT caption
    else
        meterArea = {};
    mix.removeFromLeft (6);
    {
        auto knobs = mix.removeFromTop (44);
        const int k = juce::jmin (44, knobs.getWidth() / 2);
        pan.setBounds (knobs.removeFromLeft (knobs.getWidth() / 2).withSizeKeepingCentre (k, k));
        color.setBounds (knobs.withSizeKeepingCentre (k, k));
    }
    mix.removeFromTop (14);                           // "PAN" / "COLOR" captions
    const int bh = juce::jmin (24, (mix.getHeight() - 6) / 3);
    const int gap = (mix.getHeight() - 3 * bh) / 3;
    mix.removeFromTop (gap);
    undo.setBounds (mix.removeFromTop (bh));
    mix.removeFromTop (gap);
    redo.setBounds (mix.removeFromTop (bh));
    mix.removeFromTop (gap);
    settings.setBounds (mix.removeFromTop (bh));
    for (auto* btn : { &undo, &redo, &settings })
        LooperLookAndFeel::setEmphasis (*btn, 0);

    b.removeFromTop (8);
    stop.setBounds (b.removeFromBottom (38));
    b.removeFromBottom (6);
    recPlay.setBounds (b);
}

void TrackStrip::paint (juce::Graphics& g)
{
    // the glass panel behind the strip is part of the editor's cached surface
    // title with the track colour
    {
        auto t = getLocalBounds().reduced (10, 8).removeFromTop (26).toFloat();
        g.setColour (colours::track (track));
        g.fillRoundedRectangle (t.getX() + 1.0f, t.getCentreY() - 6.0f, 4.0f, 12.0f, 2.0f);
        g.setColour (colours::text);
        g.setFont (LooperLookAndFeel::font (15.0f, true));
        g.drawText (tr ("TRACK %1", juce::String (track + 1)), t.withTrimmedLeft (12.0f).withRight ((float) lengthPill.getX() - 3.0f), juce::Justification::centredLeft);
    }

    // mini LCD
    const auto& st = proc.getEngine().getTrackStatus (track);
    const auto state = (LoopTrack::State) st.state.load();
    const auto pending = (LooperEngine::Pending) st.pending.load();

    juce::String stateText;
    juce::Colour stateColour = colours::lcdText;
    switch (state)
    {
        case LoopTrack::State::Empty:       stateText = "EMPTY"; stateColour = colours::lcdText.withAlpha (0.5f); break;
        case LoopTrack::State::Recording:   stateText = "REC";   stateColour = colours::red; break;
        case LoopTrack::State::Overdubbing: stateText = "DUB";   stateColour = colours::orange; break;
        case LoopTrack::State::Playing:     stateText = st.stopping.load() ? "STOPPING" : "PLAY"; stateColour = colours::green; break;
        case LoopTrack::State::Stopped:     stateText = "STOP";  break;
    }
    if (pending == LooperEngine::Pending::Record)      { stateText = "REC WAIT";  stateColour = colours::red; }
    else if (pending == LooperEngine::Pending::Play)   { stateText = "PLAY WAIT"; stateColour = colours::green; }
    else if (pending == LooperEngine::Pending::AutoRecord) { stateText = "AUTO REC"; stateColour = colours::red; }
    else if (pending != LooperEngine::Pending::None)   { stateText = "REC END"; stateColour = colours::red; }

    const double measure = proc.getEngine().getMeasurePpq();
    const double bars = measure > 0.0 ? st.lengthPpq.load() / measure : 0.0;
    juce::String lengthText = "--";
    if (state != LoopTrack::State::Empty && state != LoopTrack::State::Recording && bars > 0.0)
    {
        const bool whole = std::abs (bars - std::round (bars)) < 0.02;
        lengthText = tr (std::abs (bars - 1.0) < 0.02 ? "%1 BAR" : "%1 BARS",
                         whole ? juce::String ((int) std::round (bars)) : juce::String (bars, 1));
    }

    auto lcd = lcdArea.toFloat();
    {
        // recessed glass display
        juce::ColourGradient bz (juce::Colour (0xff3e4049), 0.0f, lcd.getY(), juce::Colour (0xff0e0f12), 0.0f, lcd.getBottom(), false);
        g.setGradientFill (bz);
        g.fillRoundedRectangle (lcd, 9.0f);
        const auto in = lcd.reduced (2.0f);
        juce::ColourGradient lg (juce::Colour (0xff0a1f2b), in.getCentreX(), in.getCentreY(), juce::Colour (0xff03090d), in.getX(), in.getY(), true);
        g.setGradientFill (lg);
        g.fillRoundedRectangle (in, 7.5f);
        juce::ColourGradient ish (juce::Colours::black.withAlpha (0.7f), 0.0f, in.getY(), juce::Colours::black.withAlpha (0.0f), 0.0f, in.getY() + 8.0f, false);
        g.setGradientFill (ish);
        g.fillRoundedRectangle (in, 7.5f);
    }
    if (showWaveforms)
    {
        // v1.7 "Show waveforms": compact header (state, length, level), a tall waveform in the track colour
        auto text = lcd.reduced (9.0f, 5.0f);
        auto head = text.removeFromTop (16.0f);
        g.setFont (LooperLookAndFeel::font (14.5f, true));
        g.setColour (stateColour);
        g.drawText (tr (stateText), head, juce::Justification::centredLeft);
        g.setColour (colours::lcdText.withAlpha (0.8f));
        g.setFont (LooperLookAndFeel::font (10.5f, true));
        const auto right = (state == LoopTrack::State::Empty || lengthText == "--" ? juce::String ("-") : lengthText)
                           + "   " + tr ("LVL %1", juce::String (juce::roundToInt (level.getValue() * 100.0)));
        g.drawText (right, head.withTrimmedLeft (g.getCurrentFont().getHeight() * 3.0f), juce::Justification::centredRight);
        text.removeFromTop (2.0f);
        LoopWaveform wf;
        wf.peaks = wave.data();
        wf.layer = layerValid ? layer.data() : nullptr;
        wf.numBins = kWaveBins;
        wf.valid = waveValid && state != LoopTrack::State::Empty;
        wf.progress = state == LoopTrack::State::Empty ? 0.0f : juce::jlimit (0.0f, 1.0f, st.progress.load());
        wf.ghost = st.ghostProgress.load();
        wf.state = state;
        wf.colour = colours::track (track);
        wf.draw (g, text);
    }
    else
    {
        auto text = lcd.reduced (9.0f, 5.0f);
        g.setFont (LooperLookAndFeel::font (16.5f, true));
        if (state != LoopTrack::State::Empty || pending != LooperEngine::Pending::None)
        {
            g.setColour (stateColour.withAlpha (0.18f));
            g.drawText (tr (stateText), text.withHeight (18.0f).translated (1.0f, 0.0f), juce::Justification::centredLeft);
            g.drawText (tr (stateText), text.withHeight (18.0f).translated (-1.0f, 0.0f), juce::Justification::centredLeft);
        }
        g.setColour (stateColour);
        g.drawText (tr (stateText), text.removeFromTop (18.0f), juce::Justification::centredLeft);
        g.setColour (colours::lcdText.withAlpha (0.8f));
        g.setFont (LooperLookAndFeel::font (11.5f, true));
        auto line2 = text.removeFromTop (14.0f);
        g.drawText (lengthText, line2, juce::Justification::centredLeft);
        g.drawText (tr ("LVL %1", juce::String (juce::roundToInt (level.getValue() * 100.0))), line2, juce::Justification::centredRight);

        const float prog = state == LoopTrack::State::Empty ? 0.0f : juce::jlimit (0.0f, 1.0f, st.progress.load());
        auto waveArea = text.removeFromBottom (juce::jmin (18.0f, text.getHeight()));
        if (waveValid)
        {
            // waveform overview: played part bright, the rest dimmed, play head line
            const float w = waveArea.getWidth() / (float) kWaveBins;
            const float mid = waveArea.getCentreY();
            const bool recording = state == LoopTrack::State::Recording;
            for (int i = 0; i < kWaveBins; ++i)
            {
                const float v = std::sqrt (juce::jlimit (0.0f, 1.0f, wave[(size_t) i]));   // sqrt: quiet parts stay visible
                const float h = juce::jmax (1.0f, v * waveArea.getHeight() * 0.5f);
                const bool played = recording || ((float) i + 0.5f) / (float) kWaveBins <= prog;
                g.setColour (stateColour.withAlpha (played ? 0.95f : 0.35f));
                g.fillRect (waveArea.getX() + w * (float) i, mid - h, juce::jmax (1.0f, w - 0.6f), h * 2.0f);
            }
            const float ghost = st.ghostProgress.load();
            if (state == LoopTrack::State::Stopped && ghost >= 0.0f)
            {
                // ghost play head: where the stopped loop would be now, so it can be started on cue
                g.setColour (juce::Colours::white.withAlpha (0.45f));     // the display is dark in every theme
                const float x = waveArea.getX() + waveArea.getWidth() * juce::jlimit (0.0f, 1.0f, ghost);
                for (float y = waveArea.getY(); y < waveArea.getBottom(); y += 4.0f)
                    g.fillRect (x - 0.5f, y, 1.0f, 2.0f);
            }
            else if (! recording && state != LoopTrack::State::Empty)
            {
                g.setColour (juce::Colours::white);
                g.fillRect (waveArea.getX() + waveArea.getWidth() * prog - 0.75f, waveArea.getY(), 1.5f, waveArea.getHeight());
            }
        }
        else
        {
            auto bar = waveArea.withSizeKeepingCentre (waveArea.getWidth(), 4.0f);
            g.setColour (colours::lcdText.withAlpha (0.15f));
            g.fillRect (bar);
            g.setColour (stateColour);
            g.fillRect (bar.withWidth (bar.getWidth() * prog));
        }
    }

    // captions
    g.setColour (colours::textDim);
    g.setFont (LooperLookAndFeel::font (10.0f, true));
    g.drawText (tr ("PAN"), pan.getBounds().translated (0, pan.getHeight() - 1).withHeight (13), juce::Justification::centred);
    g.drawText (tr ("COLOR"), color.getBounds().translated (0, color.getHeight() - 1).withHeight (13).expanded (8, 0), juce::Justification::centred);

    // stereo meter next to the fader (optional, SETUP): input while recording / overdubbing, output otherwise
    if (showMeters && ! meterArea.isEmpty())
    {
        auto m = meterArea.toFloat();
        const float bw = (m.getWidth() - 2.0f) * 0.5f;
        const auto base = meterInput ? colours::orange : colours::green;
        drawMeterBar (g, m.withWidth (bw), meter[0], true, base);
        drawMeterBar (g, m.withTrimmedLeft (bw + 2.0f), meter[1], true, base);
        g.setColour (meterInput ? colours::orange : colours::textDim);
        g.setFont (LooperLookAndFeel::font (9.0f, true));
        g.drawText (tr (meterInput ? "IN" : "OUT"), meterArea.withY (meterArea.getBottom() + 2).withHeight (12).expanded (6, 0),
                    juce::Justification::centred);
    }

    // clip LED: green = the track holds a loop, red = first recording, blinking = waiting
    {
        auto led = ledArea.toFloat();
        juce::Colour c = theme::pick (0xff2b2e33, 0xffc4c8d0);
        bool lit = false;
        if (state == LoopTrack::State::Recording)                           { c = colours::red; lit = true; }
        else if (state != LoopTrack::State::Empty)                          { c = colours::green; lit = true; }
        if (pending == LooperEngine::Pending::Record && blink)              { c = colours::red; lit = true; }
        if (lit)
        {
            g.setColour (c.withAlpha (0.25f));
            g.fillEllipse (led.expanded (3.0f));
        }
        g.setColour (c);
        g.fillEllipse (led);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawEllipse (led, 1.0f);
    }
}

void TrackStrip::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (lcdArea.contains (e.getPosition()) && canDragOut() ? juce::MouseCursor::DraggingHandCursor
                                                                         : juce::MouseCursor::NormalCursor);
}

void TrackStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (dragOutStarted || ! lcdArea.contains (e.getMouseDownPosition()) || e.getDistanceFromDragStart() < 6 || ! canDragOut())
        return;
    dragOutStarted = true;
    const auto file = proc.exportLoopForDrag (track);
    if (file.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}

void TrackStrip::mouseUp (const juce::MouseEvent&)
{
    dragOutStarted = false;
}

juce::String TrackStrip::getTooltip()
{
    const auto pos = getMouseXYRelative();
    if (lcdArea.contains (pos) && canDragOut())
        return tr ("Drag the waveform onto an audio track in your DAW to export the loop");
    return {};
}

void TrackStrip::refresh (bool blinkPhase)
{
    const auto& st = proc.getEngine().getTrackStatus (track);
    undo.setEnabled (st.canUndo.load());
    redo.setEnabled (st.canRedo.load());
    recPlay.setBlinkPhase (blinkPhase);
    recPlay.repaint();
    blink = blinkPhase;

    auto& eng = proc.getEngine();
    meter[0].push (st.meterL.exchange (0.0f));
    meter[1].push (st.meterR.exchange (0.0f));
    meterInput = st.meterIsInput.load();

    // waveform: refreshed about 10 times per second (every frame while recording / overdubbing)
    const auto state = (LoopTrack::State) st.state.load();
    if (++waveCounter >= 3 || state == LoopTrack::State::Recording || state == LoopTrack::State::Overdubbing)
    {
        waveCounter = 0;
        auto& tr = eng.getTrack (track);
        const int span = tr.getRecordingSpan();
        waveValid = tr.getWaveform (wave.data(), kWaveBins, span);
        // v1.7: the last overdub layer, computed here on the message thread from the two loop layers
        layerValid = waveValid && (showWaveforms || layersWanted) && tr.getOverdubLayer (layer.data(), kWaveBins);
    }

    repaint (lcdArea);
    repaint (ledArea.expanded (4));
    lengthPill.refresh();
    if (showMeters && ! meterArea.isEmpty())
        repaint (meterArea.expanded (8, 0).withBottom (meterArea.getBottom() + 14));
}

//============================================================================== LoopWaveform (v1.7)
void LoopWaveform::draw (juce::Graphics& g, juce::Rectangle<float> r) const
{
    const bool recording = state == LoopTrack::State::Recording;
    const bool stopped = state == LoopTrack::State::Stopped;
    g.setColour (colours::lcdText.withAlpha (0.10f));
    g.fillRect (r.getX(), r.getCentreY() - 0.5f, r.getWidth(), 1.0f);
    if (! valid || peaks == nullptr || numBins <= 0)
    {
        g.setColour (colours::lcdText.withAlpha (0.35f));
        g.setFont (LooperLookAndFeel::font (juce::jlimit (9.0f, 13.0f, r.getHeight() * 0.3f), true));
        g.drawText (i18n::tr ("NO LOOP"), r, juce::Justification::centred);
        return;
    }
    const float bw = r.getWidth() / (float) numBins;
    const float mid = r.getCentreY(), half = r.getHeight() * 0.48f;
    const auto layerColour = colour.interpolatedWith (juce::Colours::white, 0.65f);
    for (int i = 0; i < numBins; ++i)
    {
        const float v = std::sqrt (juce::jlimit (0.0f, 1.0f, peaks[i]));        // sqrt: quiet parts stay visible
        const float h = juce::jmax (1.0f, v * half);
        const bool played = recording || ((float) i + 0.5f) / (float) numBins <= progress;
        const float x = r.getX() + bw * (float) i, w = juce::jmax (1.0f, bw - 0.6f);
        g.setColour (stopped ? colour.withAlpha (played ? 0.6f : 0.32f) : (played ? colour.brighter (0.25f) : colour.withAlpha (0.38f)));
        g.fillRect (x, mid - h, w, h * 2.0f);
        if (layer != nullptr && layer[i] > 1.0e-4f)
        {
            // the last overdub layer: a lighter tint inside the loop
            const float h2 = juce::jmin (h, juce::jmax (1.0f, std::sqrt (juce::jlimit (0.0f, 1.0f, layer[i])) * half));
            g.setColour (layerColour.withAlpha (stopped ? 0.45f : (played ? 0.95f : 0.45f)));
            g.fillRect (x, mid - h2, w, h2 * 2.0f);
        }
    }
    if (stopped)
    {
        if (ghost < 0.0f)
            return;
        // dashed play head: where the stopped loop would be now (start it on cue)
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        const float x = r.getX() + r.getWidth() * juce::jlimit (0.0f, 1.0f, ghost);
        for (float y = r.getY(); y < r.getBottom(); y += 4.0f + headWidth)
            g.fillRect (x - headWidth * 0.4f, y, headWidth * 0.8f, 2.0f + headWidth * 0.5f);
        return;
    }
    // white play head (at the right edge of the recording so far while recording)
    const float x = r.getX() + r.getWidth() * (recording ? 1.0f : juce::jlimit (0.0f, 1.0f, progress));
    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.fillRect (x - headWidth * 1.4f, r.getY(), headWidth * 2.8f, r.getHeight());
    g.setColour (juce::Colours::white);
    g.fillRect (x - headWidth * 0.5f, r.getY(), headWidth, r.getHeight());
}

//============================================================================== TrackSettingsPanel
TrackSettingsPanel::TrackSettingsPanel (LiveLooperProcessor& p, MidiMenuHost& host, int t) : proc (p), track (t)
{
    auto& st = proc.getState();
    struct Def { const char* id; const char* caption; juce::StringArray choices; };
    const Def defs[] = {
        { kInput, "Input", inputChoices() }, { kStartMode, "Start", startModeChoices() },
        { kStopMode, "Stop", stopModeChoices() }, { kDubMode, "Dub mode", dubModeChoices() },
        { kMeasure, "Loop length", measureChoices() }, { kOutput, "Output", outputChoices() }
    };
    for (const auto& [id, caption, choices] : defs)
    {
        Row r;
        r.caption = caption;
        r.combo = std::make_unique<LLCombo>();
        r.combo->setEnglishItems (choices);
        r.attachment = std::make_unique<APVTS::ComboBoxAttachment> (st, trackId (track, id), *r.combo);
        setParamId (*r.combo, trackId (track, id));
        addAndMakeVisible (*r.combo);
        rows.push_back (std::move (r));
    }

    loopSync.setClickingTogglesState (true);
    loopSyncAtt = std::make_unique<APVTS::ButtonAttachment> (st, trackId (track, kLoopSync), loopSync);
    setParamId (loopSync, trackId (track, kLoopSync));
    loopSync.setTooltip ("Keep the loop locked to the bar grid of the timeline");
    addAndMakeVisible (loopSync);

    setParamId (clear, trackId (track, kClear));
    clear.onClick = [this] { proc.pressTrigger (trackId (track, kClear)); };
    exportBtn.onClick = [&host, this] { host.exportTrack (track); };
    importBtn.onClick = [&host, this] { host.importTrack (track); };
    for (auto* b : { &clear, &exportBtn, &importBtn })
        addAndMakeVisible (b);

    feedback = std::make_unique<Knob> (st, trackId (track, kFeedback), "Feedback", colours::orange);
    feedback->slider.setTooltip ("Feedback: how much of the loop remains with every overdub pass (100 % = all)");
    duck = std::make_unique<Knob> (st, trackId (track, kDuck), "Ducking", colours::green);
    duck->slider.setTooltip ("Ducking: lowers this track while the ducking source (SETUP) plays");
    pitch = std::make_unique<Knob> (st, trackId (track, kPitch), "Pitch", colours::cyan);
    pitch->slider.setTooltip ("Pitch in semitones (the length stays the same)");
    for (auto* k : { feedback.get(), duck.get(), pitch.get() })
        addAndMakeVisible (k);

    stretch.setClickingTogglesState (true);
    stretchAtt = std::make_unique<APVTS::ButtonAttachment> (st, trackId (track, kStretch), stretch);
    setParamId (stretch, trackId (track, kStretch));
    stretch.setTooltip ("Follow tempo: the loop follows tempo changes without changing its pitch (time-stretch)");
    setParamId (capture, trackId (track, kCapture));
    capture.setTooltip ("Capture: turns the last bars you played into a loop on this (empty) track");
    capture.onClick = [this] { proc.pressTrigger (trackId (track, kCapture)); };
    addAndMakeVisible (stretch);
    addAndMakeVisible (capture);

    setSize (300, 44 + (int) rows.size() * 30 + 76 + 90 + 36);
}

void TrackSettingsPanel::resized()
{
    auto b = getLocalBounds().reduced (12);
    b.removeFromTop (28);
    for (auto& r : rows)
    {
        auto row = b.removeFromTop (30);
        row.removeFromLeft (100);
        r.combo->setBounds (row.reduced (0, 3));
    }
    b.removeFromTop (6);
    auto row = b.removeFromTop (30);
    loopSync.setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (2, 2));
    clear.setBounds (row.reduced (2, 2));
    row = b.removeFromTop (32);
    exportBtn.setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (2, 3));
    importBtn.setBounds (row.reduced (2, 3));
    b.removeFromTop (6);
    row = b.removeFromTop (84);
    const int kw = row.getWidth() / 3;
    feedback->setBounds (row.removeFromLeft (kw));
    duck->setBounds (row.removeFromLeft (kw));
    pitch->setBounds (row);
    b.removeFromTop (4);
    row = b.removeFromTop (32);
    stretch.setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (2, 3));
    capture.setBounds (row.reduced (2, 3));
}

void TrackSettingsPanel::paint (juce::Graphics& g)
{
    g.fillAll (colours::section);
    auto b = getLocalBounds().reduced (12);
    g.setColour (colours::text);
    g.setFont (LooperLookAndFeel::font (15.0f, true));
    g.drawText (tr ("TRACK %1 SETUP", juce::String (track + 1)), b.removeFromTop (24), juce::Justification::centredLeft);
    b.removeFromTop (4);
    g.setFont (LooperLookAndFeel::font (12.0f, true));
    g.setColour (colours::textDim);
    for (auto& r : rows)
        g.drawText (i18n::upper (tr (r.caption)), b.removeFromTop (30).removeFromLeft (100), juce::Justification::centredLeft);
}
} // namespace livelooper
