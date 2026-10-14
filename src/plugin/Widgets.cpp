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
    g.setFont (LooperLookAndFeel::font (11.0f, true));
    g.drawFittedText (tr (caption).toUpperCase(), getLocalBounds().removeFromBottom (16), juce::Justification::centred, 1);
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

    juce::Colour colour = colours::textDim.withAlpha (0.25f);
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
    const float outerR = size * 0.5f - 2.0f;
    const float ringR = outerR - 5.0f;

    // bezel
    g.setColour (juce::Colour (0xff101113));
    g.fillEllipse (area.reduced (1.0f));

    // LED ring: 24 segments
    constexpr int numSegments = 24;
    const int lit = (int) std::floor (progress * numSegments + 0.0001f);
    for (int i = 0; i < numSegments; ++i)
    {
        const float a0 = juce::MathConstants<float>::twoPi * (float) i / numSegments + 0.04f;
        const float a1 = juce::MathConstants<float>::twoPi * (float) (i + 1) / numSegments - 0.04f;
        juce::Path seg;
        seg.addCentredArc (centre.x, centre.y, ringR, ringR, 0.0f, a0, a1, true);

        juce::Colour c = juce::Colour (0xff2b2e33);
        if (state != LoopTrack::State::Empty || pending != LooperEngine::Pending::None)
        {
            if (fullRing)       c = active || pending == LooperEngine::Pending::None ? colour : colour.withAlpha (0.25f);
            else if (dimRing)   c = colour.withAlpha (0.3f);
            else if (active)    c = (i <= lit) ? colour : colour.withAlpha (0.18f);
        }
        if (fullRing && ! active && pending != LooperEngine::Pending::None)
            c = colour.withAlpha (0.2f);
        g.setColour (c);
        g.strokePath (seg, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    }

    // rubber pad
    const float padR = ringR - 8.0f;
    juce::ColourGradient pad (juce::Colour (pressed ? 0xff2a2d31 : 0xff464b52), centre.x, centre.y - padR,
                              juce::Colour (0xff1f2125), centre.x, centre.y + padR, false);
    g.setGradientFill (pad);
    g.fillEllipse (centre.x - padR, centre.y - padR, padR * 2.0f, padR * 2.0f);
    if (active && state != LoopTrack::State::Empty)
    {
        g.setColour (colour.withAlpha (0.18f));
        g.fillEllipse (centre.x - padR, centre.y - padR, padR * 2.0f, padR * 2.0f);
    }
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawEllipse (centre.x - padR, centre.y - padR, padR * 2.0f, padR * 2.0f, 1.2f);

    // legend
    g.setColour (colours::text.withAlpha (0.85f));
    g.setFont (LooperLookAndFeel::font (padR * 0.24f, true));
    g.drawText (tr ("REC"), juce::Rectangle<float> (centre.x - padR, centre.y - padR * 0.55f, padR * 2.0f, padR * 0.4f),
                juce::Justification::centred);
    g.setColour (colours::red);
    g.fillEllipse (centre.x - padR * 0.32f, centre.y - padR * 0.02f, padR * 0.2f, padR * 0.2f);
    juce::Path tri;
    tri.addTriangle (centre.x + padR * 0.08f, centre.y - padR * 0.02f, centre.x + padR * 0.08f, centre.y + padR * 0.18f,
                     centre.x + padR * 0.28f, centre.y + padR * 0.08f);
    g.setColour (colours::green);
    g.fillPath (tri);
    g.setColour (colours::text.withAlpha (0.6f));
    g.setFont (LooperLookAndFeel::font (padR * 0.17f, true));
    const bool recPlayMode = juce::roundToInt (proc.getState().getRawParameterValue (gRecAction)->load()) == 1;
    g.drawText (tr (recPlayMode ? "PLAY / DUB" : "DUB / PLAY"), juce::Rectangle<float> (centre.x - padR, centre.y + padR * 0.25f, padR * 2.0f, padR * 0.3f),
                juce::Justification::centred);
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
    juce::ColourGradient grad (juce::Colour (down ? 0xff2a2d31 : 0xff42464d), b.getX(), b.getY(),
                               juce::Colour (0xff222428), b.getX(), b.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (b, 6.0f, 1.0f);

    if (keyHold > 0.0f && ! (down && ! cleared))
    {
        g.setColour (colours::red.withAlpha (0.35f));
        g.fillRoundedRectangle (b.withWidth (b.getWidth() * juce::jmin (1.0f, keyHold)), 6.0f);
    }
    if (down && ! cleared)
    {
        const float held = juce::jlimit (0.0f, 1.0f, (float) (juce::Time::getMillisecondCounter() - downTime) / 1500.0f);
        g.setColour (colours::red.withAlpha (0.35f));
        g.fillRoundedRectangle (b.withWidth (b.getWidth() * held), 6.0f);
    }
    if (cleared)
    {
        g.setColour (colours::red.withAlpha (0.5f));
        g.fillRoundedRectangle (b, 6.0f);
    }

    g.setColour (colours::text);
    const float s = b.getHeight() * 0.32f;
    g.fillRect (b.getX() + 12.0f, b.getCentreY() - s * 0.5f, s, s);
    g.setFont (LooperLookAndFeel::font (b.getHeight() * 0.4f, true));
    g.drawText (tr (cleared ? "CLEARED" : "STOP"), b.withTrimmedLeft (12.0f + s), juce::Justification::centred);
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
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRect (area);
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
    auto content = getLocalBounds().toFloat().reduced (3.0f).reduced (10.0f, 4.0f);
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
    g.setColour (juce::Colour (0xff0d0e10));
    g.fillRoundedRectangle (b, 6.0f);
    auto inner = b.reduced (3.0f);
    juce::ColourGradient grad (colours::lcdBack.brighter (0.08f), inner.getX(), inner.getY(),
                               colours::lcdBack, inner.getX(), inner.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (inner, 4.0f);

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
        g.setColour (accentColour);
        g.setFont (LooperLookAndFeel::font (juce::jmin (bigArea.getHeight() * 0.62f, 34.0f), true));
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
    g.setColour (midi ? colours::yellow : colours::lcdText.withAlpha (0.15f));
    g.fillEllipse (dot);
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
    onButton.setBounds (b.removeFromTop (b.getHeight() - 20).reduced (4, 2));
}

void FxSlot::paint (juce::Graphics& g)
{
    auto label = getLocalBounds().removeFromBottom (20).reduced (2, 1);
    if (selected)
    {
        g.setColour ((input ? colours::cyan : colours::orange).withAlpha (0.2f));
        g.fillRoundedRectangle (label.toFloat(), 3.0f);
    }
    juce::String typeName = "?";
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (fxId (input, slot, kFxType))))
        typeName = tr (fxTypeChoices()[p->getIndex()]);
    g.setColour (selected ? colours::text : colours::textDim);
    g.setFont (LooperLookAndFeel::font (11.0f, true));
    g.drawFittedText (typeName.toUpperCase(), label, juce::Justification::centred, 1, 0.7f);
}

void FxSlot::mouseDown (const juce::MouseEvent&)
{
    if (onSelect)
        onSelect (input, slot);
}

//============================================================================== TrackStrip
TrackStrip::TrackStrip (LiveLooperProcessor& p, int t)
    : proc (p), track (t), recPlay (p, t), stop (p, t)
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
}

void TrackStrip::resized()
{
    auto b = getLocalBounds().reduced (8, 6);
    ledArea = b.removeFromTop (24).removeFromRight (16).withSizeKeepingCentre (10, 10).translated (0, -2);   // clip LED in the title row
    lcdArea = b.removeFromTop (66);
    b.removeFromTop (6);

    auto row = b.removeFromTop (24);
    const int bw = row.getWidth() / 3;
    reverse.setBounds (row.removeFromLeft (bw).reduced (2, 0));
    oneShot.setBounds (row.removeFromLeft (bw).reduced (2, 0));
    fx.setBounds (row.reduced (2, 0));
    b.removeFromTop (8);

    auto mid = b.removeFromTop (150);
    level.setBounds (mid.removeFromLeft (38));
    meterArea = mid.removeFromLeft (12).withTrimmedTop (6).withTrimmedBottom (16);   // stereo meter + IN / OUT caption
    mid.removeFromLeft (4);
    {
        // v1.5: PAN and the track's COLOR FX knob side by side
        auto knobs = mid.removeFromTop (48);
        pan.setBounds (knobs.removeFromLeft (knobs.getWidth() / 2).withSizeKeepingCentre (46, 46));
        color.setBounds (knobs.withSizeKeepingCentre (46, 46));
    }
    mid.removeFromTop (14);                           // "PAN" / "COLOR" captions
    mid.removeFromTop (6);
    undo.setBounds (mid.removeFromTop (24).reduced (0, 1));
    mid.removeFromTop (3);
    redo.setBounds (mid.removeFromTop (24).reduced (0, 1));
    mid.removeFromTop (3);
    settings.setBounds (mid.removeFromTop (24).reduced (0, 1));

    b.removeFromTop (6);
    stop.setBounds (b.removeFromBottom (34));
    b.removeFromBottom (4);
    recPlay.setBounds (b);
}

void TrackStrip::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (colours::section);
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (colours::sectionEdge);
    g.drawRoundedRectangle (b, 8.0f, 1.0f);

    // title
    g.setColour (colours::text);
    g.setFont (LooperLookAndFeel::font (15.0f, true));
    g.drawText (tr ("TRACK %1", juce::String (track + 1)), getLocalBounds().reduced (12, 6).removeFromTop (22),
                juce::Justification::centredLeft);

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
    g.setColour (colours::lcdBack);
    g.fillRoundedRectangle (lcd, 4.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (lcd, 4.0f, 1.0f);
    auto text = lcd.reduced (7.0f, 4.0f);
    g.setColour (stateColour);
    g.setFont (LooperLookAndFeel::font (15.0f, true));
    g.drawText (tr (stateText), text.removeFromTop (18.0f), juce::Justification::centredLeft);
    g.setColour (colours::lcdText.withAlpha (0.8f));
    g.setFont (LooperLookAndFeel::font (12.0f, true));
    auto line2 = text.removeFromTop (15.0f);
    g.drawText (lengthText, line2, juce::Justification::centredLeft);
    g.drawText (tr ("LVL %1", juce::String (juce::roundToInt (level.getValue() * 100.0))), line2, juce::Justification::centredRight);

    const float prog = state == LoopTrack::State::Empty ? 0.0f : juce::jlimit (0.0f, 1.0f, st.progress.load());
    auto waveArea = text.removeFromBottom (juce::jmin (22.0f, text.getHeight()));
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
            g.setColour (colours::text.withAlpha (0.45f));
            const float x = waveArea.getX() + waveArea.getWidth() * juce::jlimit (0.0f, 1.0f, ghost);
            for (float y = waveArea.getY(); y < waveArea.getBottom(); y += 4.0f)
                g.fillRect (x - 0.5f, y, 1.0f, 2.0f);
        }
        else if (! recording && state != LoopTrack::State::Empty)
        {
            g.setColour (colours::text);
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

    // captions
    g.setColour (colours::textDim);
    g.setFont (LooperLookAndFeel::font (11.0f, true));
    g.drawText (tr ("PAN"), pan.getBounds().translated (0, pan.getHeight() - 2).withHeight (14), juce::Justification::centred);
    g.drawText (tr ("COLOR"), color.getBounds().translated (0, color.getHeight() - 2).withHeight (14).expanded (6, 0), juce::Justification::centred);

    // stereo meter next to the fader: input while recording / overdubbing, output otherwise
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
        juce::Colour c = juce::Colour (0xff2b2e33);
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
    }

    repaint (lcdArea);
    repaint (ledArea.expanded (4));
    repaint (meterArea.expanded (8, 0).withBottom (meterArea.getBottom() + 14));
}

//============================================================================== TrackSettingsPanel
TrackSettingsPanel::TrackSettingsPanel (LiveLooperProcessor& p, MidiMenuHost& host, int t) : proc (p), track (t)
{
    auto& st = proc.getState();
    struct Def { const char* id; const char* caption; juce::StringArray choices; };
    const Def defs[] = {
        { kInput, "Input", inputChoices() }, { kStartMode, "Start", startModeChoices() },
        { kStopMode, "Stop", stopModeChoices() }, { kDubMode, "Dub mode", dubModeChoices() },
        { kMeasure, "Measures", measureChoices() }, { kOutput, "Output", outputChoices() }
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
        g.drawText (tr (r.caption).toUpperCase(), b.removeFromTop (30).removeFromLeft (100), juce::Justification::centredLeft);
}
} // namespace livelooper
