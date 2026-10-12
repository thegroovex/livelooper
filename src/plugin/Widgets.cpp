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
    g.drawText (tr ("DUB / PLAY"), juce::Rectangle<float> (centre.x - padR, centre.y + padR * 0.25f, padR * 2.0f, padR * 0.3f),
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
    down = false;
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
    b.removeFromTop (24);                             // title
    lcdArea = b.removeFromTop (52);
    b.removeFromTop (6);

    auto row = b.removeFromTop (24);
    const int bw = row.getWidth() / 3;
    reverse.setBounds (row.removeFromLeft (bw).reduced (2, 0));
    oneShot.setBounds (row.removeFromLeft (bw).reduced (2, 0));
    fx.setBounds (row.reduced (2, 0));
    b.removeFromTop (8);

    auto mid = b.removeFromTop (150);
    level.setBounds (mid.removeFromLeft (46));
    mid.removeFromLeft (6);
    pan.setBounds (mid.removeFromTop (54).withSizeKeepingCentre (54, 54));
    mid.removeFromTop (14);                           // "PAN" caption
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
    auto bar = text.removeFromBottom (4.0f);
    g.setColour (colours::lcdText.withAlpha (0.15f));
    g.fillRect (bar);
    g.setColour (stateColour);
    g.fillRect (bar.withWidth (bar.getWidth() * prog));

    // captions
    g.setColour (colours::textDim);
    g.setFont (LooperLookAndFeel::font (11.0f, true));
    g.drawText (tr ("PAN"), pan.getBounds().translated (0, pan.getHeight() - 2).withHeight (14), juce::Justification::centred);

    // level meter next to the fader
    const float peak = juce::jlimit (0.0f, 1.0f, st.peak.load());
    auto meter = juce::Rectangle<float> ((float) level.getRight() + 1.0f, (float) level.getY(), 3.0f, (float) level.getHeight());
    g.setColour (juce::Colours::black);
    g.fillRect (meter);
    g.setColour (peak > 0.98f ? colours::red : colours::green);
    g.fillRect (meter.withTop (meter.getBottom() - meter.getHeight() * peak));
}

void TrackStrip::refresh (bool blinkPhase)
{
    const auto& st = proc.getEngine().getTrackStatus (track);
    undo.setEnabled (st.canUndo.load());
    redo.setEnabled (st.canRedo.load());
    recPlay.setBlinkPhase (blinkPhase);
    recPlay.repaint();
    repaint (lcdArea);
    repaint (level.getBounds().withWidth (level.getWidth() + 6));
}

//============================================================================== TrackSettingsPanel
TrackSettingsPanel::TrackSettingsPanel (LiveLooperProcessor& p, MidiMenuHost& host, int t) : proc (p), track (t)
{
    auto& st = proc.getState();
    struct Def { const char* id; const char* caption; juce::StringArray choices; };
    const Def defs[] = {
        { kInput, "Input", inputChoices() }, { kStartMode, "Start", startModeChoices() },
        { kStopMode, "Stop", stopModeChoices() }, { kDubMode, "Dub mode", dubModeChoices() },
        { kMeasure, "Measures", measureChoices() }
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

    setSize (300, 44 + (int) rows.size() * 30 + 76);
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
