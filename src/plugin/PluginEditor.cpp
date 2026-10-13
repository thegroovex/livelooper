#include "PluginEditor.h"

namespace livelooper
{
using namespace params;
using i18n::tr;

namespace layout
{
    const juce::Rectangle<int> header       { 0, 0, 1180, 78 };
    const juce::Rectangle<int> lcd          { 300, 12, 548, 56 };
    const juce::Rectangle<int> inputFx      { 12, 86, 380, 122 };
    const juce::Rectangle<int> fxEditor     { 400, 86, 380, 122 };
    const juce::Rectangle<int> trackFx      { 788, 86, 380, 122 };
    const juce::Rectangle<int> tracks       { 12, 216, 850, 446 };
    const juce::Rectangle<int> rhythm       { 870, 216, 298, 190 };
    const juce::Rectangle<int> masterPanel  { 870, 412, 298, 250 };
    const juce::Rectangle<int> footer       { 0, 668, 1180, 52 };
}

LiveLooperEditor::LiveLooperEditor (LiveLooperProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lookAndFeel);
    auto& st = proc.getState();

    surface.painter = [this] (juce::Graphics& g) { paintSurface (g); };
    addAndMakeVisible (surface);

    // ---- header
    surface.addAndMakeVisible (lcd);
    lcd.onTempoClicked = [this] { showTempoEntry(); };

    tempoEntry.setJustification (juce::Justification::centredLeft);
    tempoEntry.setInputRestrictions (6, "0123456789.,");
    tempoEntry.setSelectAllWhenFocused (true);
    tempoEntry.setColour (juce::TextEditor::backgroundColourId, colours::lcdBack);
    tempoEntry.setColour (juce::TextEditor::textColourId, colours::yellow);
    tempoEntry.setColour (juce::TextEditor::outlineColourId, colours::yellow);
    tempoEntry.setColour (juce::TextEditor::focusedOutlineColourId, colours::yellow);
    tempoEntry.setColour (juce::TextEditor::highlightColourId, colours::yellow.withAlpha (0.3f));
    tempoEntry.setColour (juce::CaretComponent::caretColourId, colours::yellow);
    tempoEntry.setFont (LooperLookAndFeel::font (30.0f, true));
    tempoEntry.setTooltip ("Type in the tempo (40 - 300 BPM), Return sets it, Esc cancels");
    tempoEntry.onReturnKey = [this] { commitTempoEntry(); };
    tempoEntry.onEscapeKey = [this] { tempoEntry.setVisible (false); grabKeyboardFocus(); };
    tempoEntry.onFocusLost = [this] { if (tempoEntry.isVisible()) commitTempoEntry(); };
    surface.addChildComponent (tempoEntry);
    learnBtn.setClickingTogglesState (false);
    learnBtn.onClick = [this] { setLearnMode (! learnMode); };
    learnBtn.setTooltip ("MIDI learn: click a control, then move a knob / press a pad on your controller");
    mapBtn.onClick = [this] { showMidiMapMenu(); };
    mapBtn.setTooltip ("Show / remove MIDI assignments, load the default map");
    setupBtn.onClick = [this] { showSetupMenu(); };
    setupBtn.setTooltip ("Settings, language");
    for (auto* b : { &learnBtn, &mapBtn, &setupBtn })
        surface.addAndMakeVisible (b);

    // ---- FX slots + editor
    for (int s = 0; s < kNumFxSlots; ++s)
    {
        inputSlots[(size_t) s] = std::make_unique<FxSlot> (st, true, s, [this] (bool in, int sl) { selectFxSlot (in, sl); });
        trackSlots[(size_t) s] = std::make_unique<FxSlot> (st, false, s, [this] (bool in, int sl) { selectFxSlot (in, sl); });
        surface.addAndMakeVisible (*inputSlots[(size_t) s]);
        surface.addAndMakeVisible (*trackSlots[(size_t) s]);
    }
    fxType.setEnglishItems (fxTypeChoices());
    fxType.onChange = [this] { updateFxEditorLabels(); };
    surface.addAndMakeVisible (fxType);
    fxP1  = std::make_unique<Knob> (st, fxId (true, 0, kFxP1), "P1", colours::cyan);
    fxP2  = std::make_unique<Knob> (st, fxId (true, 0, kFxP2), "P2", colours::cyan);
    fxMix = std::make_unique<Knob> (st, fxId (true, 0, kFxMix), "Mix", colours::cyan);
    for (auto* k : { fxP1.get(), fxP2.get(), fxMix.get() })
        surface.addAndMakeVisible (k);

    // ---- tracks
    for (int t = 0; t < kNumTracks; ++t)
    {
        strips[(size_t) t] = std::make_unique<TrackStrip> (proc, t);
        surface.addAndMakeVisible (*strips[(size_t) t]);
    }

    // ---- rhythm / master
    rhythmOn.setClickingTogglesState (true);
    rhythmOnAtt = std::make_unique<APVTS::ButtonAttachment> (st, gRhythmOn, rhythmOn);
    setParamId (rhythmOn, gRhythmOn);
    rhythmPattern.setEnglishItems (rhythmChoices());
    rhythmPatternAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, gRhythmPattern, rhythmPattern);
    setParamId (rhythmPattern, gRhythmPattern);
    rhythmLevel = std::make_unique<Knob> (st, gRhythmLevel, "Level", colours::orange);
    tempo = std::make_unique<Knob> (st, gTempo, "Tempo", colours::yellow);
    tap.onClick = [this] { proc.pressTrigger (gTap); };
    setParamId (tap, gTap);
    tap.setTooltip ("Tap tempo (internal tempo, used when host sync is off)");

    sync.setClickingTogglesState (true);
    syncAtt = std::make_unique<APVTS::ButtonAttachment> (st, gSync, sync);
    setParamId (sync, gSync);
    sync.setTooltip ("Follow the tempo, bar grid and transport of the host");
    quantize.setEnglishItems (quantizeChoices());
    quantizeAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, gQuantize, quantize);
    setParamId (quantize, gQuantize);
    quantize.setTooltip ("Quantize: start / end recordings and starts on the beat or bar");
    master = std::make_unique<Knob> (st, gMaster, "Master", colours::green);
    monitor = std::make_unique<Knob> (st, gMonitor, "Input", colours::green);
    fade = std::make_unique<Knob> (st, gFade, "Fade", colours::cyan);

    allStartStop.onClick = [this] { proc.pressTrigger (gAllStartStop); };
    setParamId (allStartStop, gAllStartStop);
    allStartStop.setTooltip ("Start / stop all tracks");
    clearAll.setTooltip ("Clear all tracks");
    clearAll.onClick = [this] { confirmClearAll(); };
    setParamId (clearAll, gClearAll);

    capture.onClick = [this] { proc.pressTrigger (gCapture); };
    setParamId (capture, gCapture);
    capture.tooltipFn = [this] {
        return tr ("Capture: turns the last %1 bars you played into a loop on the first empty track (no need to press record)",
                   captureBarsChoices()[juce::roundToInt (proc.getState().getRawParameterValue (gCaptureBars)->load())]);
    };

    for (int i = 0; i < kNumScenes; ++i)
    {
        auto b = std::make_unique<LLButton> (juce::String (i + 1), colours::yellow);
        b->onClick = [this, i] { proc.pressTrigger (sceneId (i)); };
        setParamId (*b, sceneId (i));
        b->tooltipFn = [this, i] {
            return tr (proc.sceneIsStored (i) ? "Scene %1: starts its tracks and stops the others (quantized)"
                                              : "Scene %1 is empty: click to store which tracks are playing now",
                       juce::String (i + 1));
        };
        surface.addAndMakeVisible (*b);
        sceneBtns[(size_t) i] = std::move (b);
    }
    sceneStore.setClickingTogglesState (true);
    sceneStoreAtt = std::make_unique<APVTS::ButtonAttachment> (st, gSceneStore, sceneStore);
    setParamId (sceneStore, gSceneStore);
    sceneStore.setTooltip ("Store: the next scene button you press stores which tracks are playing now");
    surface.addAndMakeVisible (sceneStore);

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &rhythmOn, &rhythmPattern, rhythmLevel.get(), tempo.get(), &tap, &sync, &quantize,
             master.get(), monitor.get(), fade.get(), &allStartStop, &clearAll, &capture })
        surface.addAndMakeVisible (c);

    surface.addChildComponent (overlay);
    tempo->slider.setTooltip ("Internal tempo - right-click to type in the BPM, or click the tempo on the display");

    selectFxSlot (true, 0);
    updateKeyboardFocusPolicy();
    proc.addChangeListener (this);
    i18n::addListener (this);

    setResizable (true, true);
    applyVisibleTracks();
    startTimerHz (30);
}

LiveLooperEditor::~LiveLooperEditor()
{
    proc.cancelMidiLearn();
    proc.removeChangeListener (this);
    i18n::removeListener (this);
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void LiveLooperEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
}

void LiveLooperEditor::applyVisibleTracks()
{
    const int n = proc.getVisibleTracks();
    if (n == shownTracks)
        return;
    const float scale = shownTracks < 0 || getHeight() <= 0 ? 1.0f : (float) getHeight() / (float) kBaseHeight;
    shownTracks = n;
    for (int t = 0; t < kNumTracks; ++t)
        strips[(size_t) t]->setVisible (t < n);
    const int w = surfaceWidth();
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (0.0);      // drop the old ratio before the limits change
    setResizeLimits (w * 6 / 10, kBaseHeight * 6 / 10, w * 2, kBaseHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) w / (double) kBaseHeight);
    setSize (juce::roundToInt ((float) w * scale), juce::roundToInt ((float) kBaseHeight * scale));
    resized();
}

void LiveLooperEditor::resized()
{
    const float scale = (float) getHeight() / (float) kBaseHeight;
    surface.setBounds (0, 0, surfaceWidth(), kBaseHeight);
    surface.setTransform (juce::AffineTransform::scale (scale));
    layoutSurface();
}

void LiveLooperEditor::layoutSurface()
{
    const int dx = surfaceWidth() - kBaseWidth;          // extra width for tracks 6 - 8
    lcd.setBounds (layout::lcd.translated (dx / 2, 0));
    tempoEntry.setBounds (lcd.getBounds().getX() + lcd.getTempoArea().getX() - 4, lcd.getY() + 9,
                          lcd.getTempoArea().getWidth() - 14, lcd.getHeight() - 18);
    learnBtn.setBounds (868 + dx, 22, 104, 36);
    mapBtn.setBounds (978 + dx, 22, 92, 36);
    setupBtn.setBounds (1076 + dx, 22, 92, 36);

    auto slotRow = [] (juce::Rectangle<int> panel) { return panel.reduced (8).withTrimmedTop (22); };
    auto in = slotRow (layout::inputFx), tr = slotRow (layout::trackFx.translated (dx, 0));
    const int sw = in.getWidth() / kNumFxSlots;
    for (int s = 0; s < kNumFxSlots; ++s)
    {
        inputSlots[(size_t) s]->setBounds (in.removeFromLeft (sw));
        trackSlots[(size_t) s]->setBounds (tr.removeFromLeft (sw));
    }

    auto ed = layout::fxEditor.translated (dx / 2, 0).reduced (8).withTrimmedTop (22);
    fxType.setBounds (ed.removeFromLeft (120).withSizeKeepingCentre (116, 28));
    const int kw = ed.getWidth() / 3;
    fxP1->setBounds (ed.removeFromLeft (kw));
    fxP2->setBounds (ed.removeFromLeft (kw));
    fxMix->setBounds (ed);

    auto tracks = layout::tracks.withWidth (layout::tracks.getWidth() + dx);
    const int visible = proc.getVisibleTracks();
    const int stripW = tracks.getWidth() / visible;
    for (int t = 0; t < visible; ++t)
        strips[(size_t) t]->setBounds (tracks.removeFromLeft (stripW).reduced (2, 0));

    auto rh = layout::rhythm.translated (dx, 0).reduced (10).withTrimmedTop (22);
    auto row = rh.removeFromTop (32);
    rhythmOn.setBounds (row.removeFromLeft (100).reduced (0, 2));
    row.removeFromLeft (8);
    rhythmPattern.setBounds (row.reduced (0, 3));
    rh.removeFromTop (8);
    row = rh;
    const int third = row.getWidth() / 3;
    rhythmLevel->setBounds (row.removeFromLeft (third).reduced (4));
    tempo->setBounds (row.removeFromLeft (third).reduced (4));
    tap.setBounds (row.withSizeKeepingCentre (row.getWidth() - 12, 44));

    auto ms = layout::masterPanel.translated (dx, 0).reduced (10).withTrimmedTop (22);
    row = ms.removeFromTop (32);
    quantize.setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (0, 3).withTrimmedRight (4));
    sync.setBounds (row.reduced (0, 2).withTrimmedLeft (4));
    ms.removeFromTop (6);
    row = ms.removeFromTop (84);
    const int kn = row.getWidth() / 3;
    master->setBounds (row.removeFromLeft (kn).reduced (4));
    monitor->setBounds (row.removeFromLeft (kn).reduced (4));
    fade->setBounds (row.reduced (4));
    ms.removeFromTop (8);
    {
        auto bottom = ms.removeFromBottom (30).reduced (0, 1);
        capture.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2).withTrimmedRight (3));
        clearAll.setBounds (bottom.withTrimmedLeft (3));
    }
    ms.removeFromBottom (6);
    allStartStop.setBounds (ms);

    // scenes in the footer
    auto sc = layout::footer.reduced (20, 12).withTrimmedLeft (64);
    for (auto& b : sceneBtns)
    {
        b->setBounds (sc.removeFromLeft (34).reduced (1, 0));
        sc.removeFromLeft (2);
    }
    sc.removeFromLeft (4);
    sceneStore.setBounds (sc.removeFromLeft (66).reduced (1, 0));

    overlay.setBounds (surface.getLocalBounds());
}

void LiveLooperEditor::paintSurface (juce::Graphics& g)
{
    // brushed panel
    const int dx = surfaceWidth() - kBaseWidth;
    juce::ColourGradient grad (juce::Colour (0xff2b2e33), 0.0f, 0.0f, juce::Colour (0xff17191c), 0.0f, (float) kBaseHeight, false);
    g.setGradientFill (grad);
    g.fillAll();
    g.setColour (juce::Colours::white.withAlpha (0.015f));
    for (int y = 0; y < kBaseHeight; y += 3)
        g.drawHorizontalLine (y, 0.0f, (float) surfaceWidth());

    // logo
    g.setColour (colours::text);
    g.setFont (LooperLookAndFeel::font (34.0f, true));
    g.drawText ("livelooper", 22, 14, 260, 40, juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (LooperLookAndFeel::font (11.0f, true));
    g.drawText (tr ("%1 TRACK LOOP STATION", juce::String (proc.getVisibleTracks())), 24, 50, 260, 16, juce::Justification::centredLeft);
    const juce::Colour dots[] = { colours::red, colours::orange, colours::green, colours::cyan, colours::learn };
    for (int i = 0; i < 5; ++i)
    {
        g.setColour (dots[i]);
        g.fillEllipse (196.0f + (float) i * 16.0f, 30.0f, 9.0f, 9.0f);
    }

    auto section = [&g] (juce::Rectangle<int> r, const juce::String& title, juce::Colour accent)
    {
        g.setColour (colours::section.withAlpha (0.85f));
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (colours::sectionEdge);
        g.drawRoundedRectangle (r.toFloat(), 8.0f, 1.0f);
        g.setColour (accent);
        g.fillRoundedRectangle ((float) r.getX() + 10.0f, (float) r.getY() + 11.0f, 4.0f, 12.0f, 2.0f);
        g.setColour (colours::text);
        g.setFont (LooperLookAndFeel::font (13.0f, true));
        g.drawText (title, r.getX() + 20, r.getY() + 6, r.getWidth() - 30, 22, juce::Justification::centredLeft);
    };

    section (layout::inputFx, tr ("INPUT FX"), colours::cyan);
    const auto fxTitle = tr (selectedInput ? "INPUT FX" : "TRACK FX") + " " + juce::String::charToString ((juce::juce_wchar) ('A' + selectedSlot))
                         + "  " + tr ("EDIT");
    section (layout::fxEditor.translated (dx / 2, 0), fxTitle, selectedInput ? colours::cyan : colours::orange);
    section (layout::trackFx.translated (dx, 0), tr ("TRACK FX"), colours::orange);
    section (layout::rhythm.translated (dx, 0), tr ("RHYTHM"), colours::orange);
    section (layout::masterPanel.translated (dx, 0), tr ("MASTER"), colours::green);

    // footer: scenes, tips, version
    const auto footer = layout::footer.withWidth (surfaceWidth()).reduced (20, 12);
    g.setColour (colours::text);
    g.setFont (LooperLookAndFeel::font (12.0f, true));
    g.drawText (tr ("SCENES"), footer.withWidth (62), juce::Justification::centredLeft);
    const int last = proc.getLastScene();
    for (int i = 0; i < kNumScenes; ++i)
    {
        // LED under each scene button: stored = dim, last recalled = bright
        const auto b = sceneBtns[(size_t) i]->getBounds();
        g.setColour (proc.sceneIsStored (i) ? (i == last ? colours::yellow : colours::yellow.withAlpha (0.35f))
                                            : juce::Colour (0xff2b2e33));
        g.fillRect (b.getX() + 6, b.getBottom() + 2, b.getWidth() - 12, 3);
    }
    g.setColour (colours::textDim);
    g.setFont (LooperLookAndFeel::font (12.0f));
    g.drawFittedText (tr ("Right-click any control for MIDI learn   |   hold STOP to clear   |   drop audio files on a track to import"),
                      footer.withTrimmedLeft (sceneStore.getRight() - footer.getX() + 18).withTrimmedRight (130),
                      juce::Justification::centredLeft, 1, 0.6f);
    g.drawText ("livelooper v" LIVELOOPER_VERSION, footer, juce::Justification::centredRight);
}

//==============================================================================
void LiveLooperEditor::selectFxSlot (bool inputFx, int slot)
{
    selectedInput = inputFx;
    selectedSlot = slot;
    auto& st = proc.getState();

    fxTypeAtt.reset();
    fxTypeAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, fxId (inputFx, slot, kFxType), fxType);
    setParamId (fxType, fxId (inputFx, slot, kFxType));
    fxP1->rebind (st, fxId (inputFx, slot, kFxP1));
    fxP2->rebind (st, fxId (inputFx, slot, kFxP2));
    fxMix->rebind (st, fxId (inputFx, slot, kFxMix));
    const auto accent = inputFx ? colours::cyan : colours::orange;
    for (auto* k : { fxP1.get(), fxP2.get(), fxMix.get() })
        k->slider.setColour (juce::Slider::rotarySliderFillColourId, accent);

    for (int s = 0; s < kNumFxSlots; ++s)
    {
        inputSlots[(size_t) s]->setSelected (inputFx && s == slot);
        trackSlots[(size_t) s]->setSelected (! inputFx && s == slot);
    }
    updateFxEditorLabels();
    surface.repaint (layout::fxEditor.translated ((surfaceWidth() - kBaseWidth) / 2, 0));
}

void LiveLooperEditor::updateFxEditorLabels()
{
    const auto type = (FxType) juce::jlimit (0, (int) FxType::NumTypes - 1, fxType.getSelectedItemIndex());
    fxP1->setCaption (getFxParam1Name (type));
    fxP2->setCaption (getFxParam2Name (type));
    fxMix->setCaption (type == FxType::Delay || type == FxType::Reverb ? "Level" : "Mix");
}

//==============================================================================
void LiveLooperEditor::timerCallback()
{
    blinkCounter = (blinkCounter + 1) % 16;
    const bool blink = blinkCounter < 8;
    for (auto& s : strips)
        if (s->isVisible())
            s->refresh (blink);
    if (blinkCounter % 4 == 0)
        surface.repaint (layout::footer.withWidth (surfaceWidth()));
    for (int s = 0; s < kNumFxSlots; ++s)
    {
        inputSlots[(size_t) s]->repaint();
        trackSlots[(size_t) s]->repaint();
    }

    const int activity = proc.getMidiActivityCounter();
    if (activity != lastMidiActivity)
    {
        lastMidiActivity = activity;
        midiLedFrames = 4;
    }
    else if (midiLedFrames > 0)
        --midiLedFrames;

    const bool learning = proc.getMidiLearnParamId().isNotEmpty();
    learnBtn.setToggleState (learnMode || learning, juce::dontSendNotification);
    if (learnMode || learning)
        overlay.repaint();

    // REC button mode changed (setup menu, host automation, MIDI): show it on the display
    const int recMode = juce::roundToInt (proc.getState().getRawParameterValue (gRecAction)->load());
    if (lastRecAction >= 0 && recMode != lastRecAction)
        showStatus (tr ("REC BUTTON: %1", tr (recMode == 1 ? "REC > PLAY > DUB" : "REC > DUB > PLAY")));
    lastRecAction = recMode;

    {
        float ml = 0.0f, mr = 0.0f;
        proc.getEngine().takeMasterMeter (ml, mr);
        lcd.pushMasterMeter (ml, mr);
    }

    if (keyClear.track >= 0)
        updateKeyClearHold (juce::Time::getMillisecondCounter(), juce::KeyPress::isKeyCurrentlyDown (keyClear.keyCode));

    updateLcd();

    // computer keyboard: forget keys that were released, keep the focus on the surface
    for (int i = heldKeys.size(); --i >= 0;)
        if (! juce::KeyPress::isKeyCurrentlyDown (heldKeys[i]))
            heldKeys.remove (i);
    if (proc.isKeyboardControlActive() && isShowing() && ! tempoEntry.isVisible()
        && juce::Component::getCurrentlyFocusedComponent() == nullptr && juce::Process::isForegroundProcess())
        grabKeyboardFocus();
}

void LiveLooperEditor::updateLcd()
{
    auto& eng = proc.getEngine();
    const double bpm = eng.getCurrentBpm();
    const bool host = eng.isHostDriven();
    const bool running = eng.isTimelineRunning();
    const double measure = juce::jmax (0.25, eng.getMeasurePpq());
    const double ppq = eng.getTimelinePpq();

    juce::String bar = tr ("BAR  -- . --");
    if (running)
    {
        const int b = (int) std::floor (ppq / measure) + 1;
        const int beat = (int) std::floor (positiveModulo (ppq, measure)) + 1;
        bar = tr ("BAR %1 . %2", juce::String (b), juce::String (beat));
    }

    const auto syncOn = proc.getState().getRawParameterValue (gSync)->load() >= 0.5f;
    const auto quant = quantizeChoices()[juce::roundToInt (proc.getState().getRawParameterValue (gQuantize)->load())];
    juce::StringArray lines;
    lines.add (tr (host ? "BPM   HOST" : (syncOn ? "BPM   HOST (STOPPED) / INT CLOCK" : "BPM   INTERNAL")));
    lines.add (bar + "     " + tr ("QUANTIZE %1", tr (quant).toUpperCase()));

    juce::Colour accent = colours::lcdText;
    const auto learnId = proc.getMidiLearnParamId();
    if (learnId.isNotEmpty())
    {
        lines.add (tr ("LEARN: %1  >  send note / CC", paramName (learnId)));
        accent = colours::learn;
    }
    else if (learnMode)
    {
        lines.add (tr ("MIDI LEARN: click a control"));
        accent = colours::learn;
    }
    else if (statusText.isNotEmpty() && juce::Time::getMillisecondCounter() - statusTime < 4000)
        lines.add (statusText);
    else
    {
        const int recMode = juce::roundToInt (proc.getState().getRawParameterValue (gRecAction)->load());
        lines.add (tr (recMode == 1 ? "REC > PLAY > DUB" : "REC > DUB > PLAY") + "     "
                   + tr ("MEMORY %1 S / TRACK", juce::String ((int) proc.getMaxLoopSeconds())));
    }

    lcd.setContent (juce::String (bpm, 1), lines, accent, midiLedFrames > 0);
}

void LiveLooperEditor::showStatus (const juce::String& text)
{
    statusText = text;
    statusTime = juce::Time::getMillisecondCounter();
}

void LiveLooperEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    applyVisibleTracks();
    overlay.repaint();
    selectFxSlot (selectedInput, selectedSlot);
}

juce::String LiveLooperEditor::paramName (const juce::String& id) const
{
    return localisedName (id);
}

void LiveLooperEditor::languageChanged()
{
    updateFxEditorLabels();
    repaint();
    surface.repaint();
}

//============================================================================== computer keyboard / tempo entry
void LiveLooperEditor::updateKeyboardFocusPolicy()
{
    // Clicking a button must not steal the keys from the surface (a focused button would
    // react to Space / Return itself). Only the tempo field takes the keyboard.
    const bool keys = proc.isKeyboardControlAvailable();
    std::function<void (juce::Component&)> walk = [&] (juce::Component& parent) {
        for (auto* c : parent.getChildren())
        {
            if (c == &tempoEntry)
                continue;
            if (keys)
                c->setWantsKeyboardFocus (false);
            walk (*c);
        }
    };
    walk (surface);
    setWantsKeyboardFocus (keys);
}

bool LiveLooperEditor::keyPressed (const juce::KeyPress& key)
{
    if (! proc.isKeyboardControlActive() || tempoEntry.isVisible())
        return false;
    const auto action = proc.getKeyMap().getActionFor (key);
    if (action.isEmpty())
        return false;

    // a held key repeats: only tempo up / down follow the repeat
    if (! KeyMap::isRepeatingAction (action) && heldKeys.contains (key.getKeyCode()))
        return true;
    heldKeys.addIfNotAlreadyThere (key.getKeyCode());

    for (int t = 0; t < kNumTracks; ++t)
        if (action == trackId (t, kClear))
        {
            // like the STOP button: hold the key to clear, a short tap does nothing
            keyClear = { t, key.getKeyCode(), juce::Time::getMillisecondCounter() };
            showStatus (tr ("HOLD KEY TO CLEAR TRACK %1", juce::String (t + 1)));
            return true;
        }

    performKeyAction (action);
    if (! KeyMap::isSpecialAction (action) && action != gClearAll)
        showStatus (tr ("KEY %1 > %2", KeyMap::describeKey (key).toUpperCase(), paramName (action).toUpperCase()));
    return true;
}

bool LiveLooperEditor::keyStateChanged (bool isKeyDown)
{
    if (! isKeyDown)
        for (int i = heldKeys.size(); --i >= 0;)
            if (! juce::KeyPress::isKeyCurrentlyDown (heldKeys[i]))
                heldKeys.remove (i);
    return false;
}

void LiveLooperEditor::updateKeyClearHold (juce::uint32 now, bool keyStillDown)
{
    const int t = keyClear.track;
    if (t < 0)
        return;
    auto& stop = strips[(size_t) t]->getStopButton();
    const auto held = now - keyClear.start;
    if (! keyStillDown)
    {
        keyClear.track = -1;
        stop.setKeyHoldProgress (0.0f);
        showStatus (tr ("CLEAR CANCELLED - HOLD THE KEY 1.5 S"));
    }
    else if (held >= kClearHoldMs)
    {
        keyClear.track = -1;
        stop.setKeyHoldProgress (0.0f);
        proc.pressTrigger (trackId (t, kClear));
        showStatus (tr ("TRACK %1 CLEARED", juce::String (t + 1)));
    }
    else
        stop.setKeyHoldProgress ((float) held / (float) kClearHoldMs);
}

void LiveLooperEditor::performKeyAction (const juce::String& action)
{
    auto* tempoParam = proc.getState().getParameter (gTempo);
    auto nudge = [this, tempoParam] (double delta) {
        const double bpm = tempoParam->convertFrom0to1 (tempoParam->getValue()) + delta;
        proc.setInternalTempo (std::round (bpm * 10.0) / 10.0);
        showStatus (tr ("TEMPO %1 BPM", juce::String (tempoParam->convertFrom0to1 (tempoParam->getValue()), 1)));
    };

    if (action == KeyMap::actTempoUp)            nudge (1.0);
    else if (action == KeyMap::actTempoDown)     nudge (-1.0);
    else if (action == KeyMap::actTempoUpFine)   nudge (0.1);
    else if (action == KeyMap::actTempoDownFine) nudge (-0.1);
    else if (action == KeyMap::actEnterTempo)    showTempoEntry();
    else if (action == gClearAll)                confirmClearAll();
    else if (isTrigger (action))                 proc.pressTrigger (action);
    else if (auto* p = proc.getState().getParameter (action))
    {
        float v = 0.0f;
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (p))
            v = choice->convertTo0to1 ((float) ((choice->getIndex() + 1) % choice->choices.size()));
        else
            v = p->getValue() >= 0.5f ? 0.0f : 1.0f;
        p->beginChangeGesture();
        p->setValueNotifyingHost (v);
        p->endChangeGesture();
    }
}

void LiveLooperEditor::showTempoEntry()
{
    auto* p = proc.getState().getParameter (gTempo);
    tempoEntry.setText (juce::String (p->convertFrom0to1 (p->getValue()), 1), juce::dontSendNotification);
    tempoEntry.setVisible (true);
    tempoEntry.toFront (false);
    tempoEntry.grabKeyboardFocus();
    tempoEntry.selectAll();
    showStatus (tr ("TYPE IN TEMPO (40 - 300), RETURN"));
}

void LiveLooperEditor::commitTempoEntry()
{
    const auto text = tempoEntry.getText().replaceCharacter (',', '.').trim();
    tempoEntry.setVisible (false);
    if (text.isNotEmpty() && text.containsAnyOf ("0123456789"))
    {
        const double bpm = juce::jlimit (40.0, 300.0, text.getDoubleValue());
        proc.setInternalTempo (bpm);
        showStatus (proc.getEngine().isHostDriven() ? tr ("INTERNAL TEMPO %1 BPM (HOST TEMPO IN USE)", juce::String (bpm, 1))
                                                    : tr ("TEMPO %1 BPM", juce::String (bpm, 1)));
    }
    if (proc.isKeyboardControlAvailable())
        grabKeyboardFocus();
}

void LiveLooperEditor::confirmClearAll()
{
    auto opts = juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon)
                    .withTitle (tr ("Clear all tracks?")).withMessage (tr ("All five loops will be deleted."))
                    .withButton (tr ("Clear all")).withButton (tr ("Cancel")).withAssociatedComponent (this);
    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    juce::AlertWindow::showAsync (opts, [safe] (int r) {
        if (safe != nullptr && r == 1)
            safe->proc.pressTrigger (gClearAll);
    });
}

//============================================================================== MIDI learn
void LiveLooperEditor::setLearnMode (bool on)
{
    learnMode = on;
    if (! on)
        proc.cancelMidiLearn();
    overlay.setVisible (on);
    overlay.toFront (false);
    learnBtn.toFront (false);
    learnBtn.setToggleState (on, juce::dontSendNotification);
}

void LiveLooperEditor::collectMappables (juce::Component& parent, juce::Array<juce::Component*>& out)
{
    for (auto* c : parent.getChildren())
    {
        if (! c->isVisible() || c == &overlay)
            continue;
        if (getParamId (*c).isNotEmpty())
            out.add (c);
        collectMappables (*c, out);
    }
}

void LiveLooperEditor::LearnOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.25f));
    juce::Array<juce::Component*> mappables;
    editor.collectMappables (editor.surface, mappables);
    const auto target = editor.proc.getMidiLearnParamId();
    const bool pulse = editor.blinkCounter < 8;

    for (auto* c : mappables)
    {
        const auto r = editor.surface.getLocalArea (c, c->getLocalBounds()).toFloat().expanded (1.0f);
        const auto id = getParamId (*c);
        const bool mapped = editor.proc.describeMidiMapping (id).isNotEmpty();
        if (id == target)
        {
            g.setColour (colours::learn.withAlpha (pulse ? 0.55f : 0.3f));
            g.fillRoundedRectangle (r, 4.0f);
        }
        g.setColour (mapped ? colours::learn : colours::learn.withAlpha (0.35f));
        g.drawRoundedRectangle (r, 4.0f, mapped ? 2.0f : 1.0f);
        if (mapped)
        {
            g.setColour (colours::learn);
            g.fillEllipse (r.getRight() - 7.0f, r.getY() - 1.0f, 8.0f, 8.0f);
        }
    }
}

void LiveLooperEditor::LearnOverlay::mouseDown (const juce::MouseEvent& e)
{
    const auto pos = e.getEventRelativeTo (&editor.surface).getPosition();
    setInterceptsMouseClicks (false, false);
    auto* hit = editor.surface.getComponentAt (pos);
    setInterceptsMouseClicks (true, false);

    if (hit == &editor.learnBtn)
    {
        editor.setLearnMode (false);
        return;
    }
    if (auto* target = findMappable (hit))
    {
        if (e.mods.isPopupMenu())
        {
            editor.showMidiMenuFor (*target);
            return;
        }
        const auto id = getParamId (*target);
        if (editor.proc.getMidiLearnParamId() == id)
            editor.proc.cancelMidiLearn();
        else
            editor.proc.startMidiLearn (id);
        repaint();
    }
}

void LiveLooperEditor::showMidiMenuFor (juce::Component& c)
{
    const auto id = getParamId (c);
    if (id.isEmpty())
        return;

    const auto mapping = proc.describeMidiMapping (id);
    const bool learningThis = proc.getMidiLearnParamId() == id;

    juce::PopupMenu menu;
    menu.addSectionHeader (paramName (id));
    menu.addItem (1, tr (learningThis ? "Cancel MIDI Learn" : "MIDI Learn"));
    if (mapping.isNotEmpty())
        menu.addItem (2, tr ("Remove MIDI mapping  (%1)", mapping));
    else
        menu.addItem (3, tr ("No MIDI mapping"), false);
    if (id == gTempo)
    {
        menu.addSeparator();
        menu.addItem (4, tr ("Type in tempo (BPM) ..."));
    }

    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&c), [safe, id, learningThis] (int r) {
        if (safe == nullptr)
            return;
        if (r == 4)
        {
            safe->showTempoEntry();
            return;
        }
        if (r == 1)
        {
            if (learningThis)
                safe->proc.cancelMidiLearn();
            else
            {
                safe->proc.startMidiLearn (id);
                safe->showStatus (tr ("LEARN: %1", safe->paramName (id)));
            }
        }
        else if (r == 2)
        {
            safe->proc.removeMidiMapping (id);
            safe->showStatus (tr ("MAPPING REMOVED: %1", safe->paramName (id)));
        }
        safe->overlay.repaint();
    });
}

void LiveLooperEditor::showMidiMapMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader (tr ("MIDI assignments (click to remove)"));
    const auto mappings = proc.getMidiMap().getAllMappings();
    std::vector<int> params;
    for (const auto& [key, param] : mappings)
    {
        params.push_back (param);
        menu.addItem ((int) params.size(), MidiMap::describe (key) + "   >   " + paramName (proc.getParamIdForIndex (param)));
    }
    if (mappings.empty())
        menu.addItem (9999, tr ("No assignments yet"), false);
    menu.addSeparator();
    menu.addItem (10001, tr ("Load default map (notes C1..B1, CC 20..25)"));
    menu.addItem (10002, tr ("Remove all assignments"), ! mappings.empty());

    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&mapBtn), [safe, params] (int r) {
        if (safe == nullptr || r == 0)
            return;
        if (r == 10001)
        {
            safe->proc.loadDefaultMidiMap();
            safe->showStatus (tr ("DEFAULT MIDI MAP LOADED"));
        }
        else if (r == 10002)
        {
            safe->proc.getMidiMap().clear();
            safe->showStatus (tr ("ALL MIDI ASSIGNMENTS REMOVED"));
        }
        else if (r >= 1 && r <= (int) params.size())
            safe->proc.removeMidiMapping (safe->proc.getParamIdForIndex (params[(size_t) r - 1]));
        safe->overlay.repaint();
    });
}

void LiveLooperEditor::showSetupMenu()
{
    auto& st = proc.getState();
    auto* recAction = dynamic_cast<juce::AudioParameterChoice*> (st.getParameter (gRecAction));
    auto* follow = dynamic_cast<juce::AudioParameterBool*> (st.getParameter (gFollow));

    juce::PopupMenu rec;
    rec.addItem (1, "Rec > Dub > Play", true, recAction->getIndex() == 0);
    rec.addItem (2, "Rec > Play > Dub", true, recAction->getIndex() == 1);

    juce::PopupMenu memory;
    const int lengths[] = { 30, 60, 120, 240, 480 };
    for (int i = 0; i < 5; ++i)
        memory.addItem (20 + i, tr ("%1 seconds per track", juce::String (lengths[i])), true,
                        (int) proc.getMaxLoopSeconds() == lengths[i]);

    juce::PopupMenu language;
    const auto langSetting = i18n::getSetting();
    language.addItem (30, tr ("Automatic (system language)"), true, langSetting == i18n::Language::Automatic);
    language.addItem (31, "English", true, langSetting == i18n::Language::English);
    language.addItem (32, "Deutsch", true, langSetting == i18n::Language::German);
    language.addSeparator();
    language.addItem (39, tr ("Parameter names in the host change after reloading the plug-in"), false);

    auto* clickSound = dynamic_cast<juce::AudioParameterChoice*> (st.getParameter (gClickSound));
    juce::PopupMenu click;
    for (int i = 0; i < clickSound->choices.size(); ++i)
        click.addItem (40 + i, tr (clickSoundChoices()[i]), true, clickSound->getIndex() == i);

    juce::PopupMenu cue;
    if (proc.hasCueOutput())
    {
        cue.addSectionHeader (tr ("Cue output (outputs of the audio interface)"));
        for (int p = 0; p <= LiveLooperProcessor::kNumCuePairs; ++p)
            cue.addItem (50 + p, LiveLooperProcessor::getCuePairName (p), true, proc.getCuePair() == p);
        cue.addSectionHeader (tr ("Rhythm / metronome on"));
        const auto route = proc.getClickRoute();
        cue.addItem (60, tr ("Main output"), true, route == ClickRoute::Main);
        cue.addItem (61, tr ("Cue output only"), true, route == ClickRoute::Cue);
        cue.addItem (62, tr ("Main and cue output"), true, route == ClickRoute::MainAndCue);
        cue.addSeparator();
        cue.addItem (69, tr ("Enable the outputs in Options > Audio/MIDI Settings"), false);
    }

    // ---- v1.3: tracks, capture, auto record, ducking, direct outputs
    auto* captureBars = dynamic_cast<juce::AudioParameterChoice*> (st.getParameter (gCaptureBars));
    auto* autoRec = dynamic_cast<juce::AudioParameterBool*> (st.getParameter (gAutoRec));
    auto* autoRecThresh = dynamic_cast<juce::AudioParameterFloat*> (st.getParameter (gAutoRecThresh));
    auto* duckSource = dynamic_cast<juce::AudioParameterChoice*> (st.getParameter (gDuckSource));

    juce::PopupMenu tracksMenu;
    tracksMenu.addItem (100, tr ("5 tracks"), true, proc.getVisibleTracks() == 5);
    tracksMenu.addItem (101, tr ("8 tracks"), true, proc.getVisibleTracks() == 8);

    juce::PopupMenu captureMenu;
    for (int i = 0; i < captureBars->choices.size(); ++i)
        captureMenu.addItem (110 + i, tr (kCaptureBars[i] == 1 ? "%1 bar" : "%1 bars", juce::String (kCaptureBars[i])),
                             true, captureBars->getIndex() == i);

    juce::PopupMenu autoRecMenu;
    autoRecMenu.addItem (120, tr ("Auto record (armed tracks wait for the input)"), true, autoRec->get());
    autoRecMenu.addSectionHeader (tr ("Threshold"));
    const std::array<int, 5> thresholds { -50, -40, -30, -20, -12 };
    for (int i = 0; i < 5; ++i)
        autoRecMenu.addItem (121 + i, juce::String (thresholds[(size_t) i]) + " dB", true,
                             juce::roundToInt (autoRecThresh->get()) == thresholds[(size_t) i]);

    juce::PopupMenu duckMenu;
    for (int i = 0; i < duckSource->choices.size(); ++i)
        duckMenu.addItem (130 + i, tr (duckSourceChoices()[i]), true, duckSource->getIndex() == i);

    juce::PopupMenu directMenu;
    if (proc.hasCueOutput())
    {
        for (int t = 0; t < proc.getVisibleTracks(); ++t)
        {
            juce::PopupMenu pairs;
            for (int p = 0; p <= LiveLooperProcessor::kNumCuePairs; ++p)
                pairs.addItem (200 + t * 10 + p, LiveLooperProcessor::getCuePairName (p), true, proc.getTrackOutputPair (t) == p);
            directMenu.addSubMenu (tr ("Track %1", juce::String (t + 1)) + ": " + LiveLooperProcessor::getCuePairName (proc.getTrackOutputPair (t)), pairs);
        }
        directMenu.addSeparator();
        directMenu.addItem (199, tr ("Set the track's OUTPUT to Direct in its SETUP"), false);
    }

    juce::PopupMenu menu;
    menu.addSectionHeader (tr ("livelooper setup"));
    menu.addItem (8, tr ("Save set ..."));
    menu.addItem (9, tr ("Load set ..."));
    menu.addItem (10, tr ("Export stems (one WAV per track) ..."));
    menu.addSeparator();
    menu.addSubMenu (tr ("Tracks"), tracksMenu);
    menu.addSubMenu (tr ("Capture length"), captureMenu);
    menu.addSubMenu (tr ("Auto record"), autoRecMenu);
    menu.addSubMenu (tr ("Ducking source"), duckMenu);
    if (proc.hasCueOutput())
        menu.addSubMenu (tr ("Direct track outputs"), directMenu);
    menu.addSeparator();
    menu.addSubMenu (tr ("Record action"), rec);
    menu.addItem (3, tr ("Follow host transport (start / stop with the host)"), true, follow->get());
    menu.addSubMenu (tr ("Maximum loop length"), memory);
    menu.addItem (4, tr ("Save loops with the project"), true, proc.getSaveLoopsWithProject());
    menu.addSubMenu (tr ("Metronome sound"), click);
    if (proc.hasCueOutput())
        menu.addSubMenu (tr ("Cue output (click / rhythm)"), cue);
    if (proc.isKeyboardControlAvailable())
    {
        menu.addSeparator();
        menu.addItem (5, tr ("Computer keyboard control"), true, proc.getKeyMap().isEnabled());
        menu.addItem (6, tr ("Keyboard mapping ..."));
    }
    menu.addItem (7, tr ("Type in tempo (BPM) ..."));
    menu.addSeparator();
    menu.addSubMenu ("Language / Sprache", language);

    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&setupBtn),
                        [safe, recAction, follow, lengths, clickSound, captureBars, autoRec, autoRecThresh, duckSource, thresholds] (int r) {
        if (safe == nullptr || r == 0)
            return;
        auto setParam = [] (juce::RangedAudioParameter* p, float plain) {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (plain));
            p->endChangeGesture();
        };
        if (r == 10)
        {
            safe->exportStems();
            return;
        }
        if (r == 100 || r == 101)
        {
            safe->proc.setVisibleTracks (r == 100 ? 5 : 8);
            return;
        }
        if (r >= 110 && r < 114)
        {
            setParam (captureBars, (float) (r - 110));
            return;
        }
        if (r == 120)
        {
            setParam (autoRec, autoRec->get() ? 0.0f : 1.0f);
            safe->showStatus (tr (autoRec->get() ? "AUTO REC ON" : "AUTO REC OFF"));
            return;
        }
        if (r >= 121 && r <= 125)
        {
            setParam (autoRecThresh, (float) thresholds[(size_t) (r - 121)]);
            return;
        }
        if (r >= 130 && r < 132)
        {
            setParam (duckSource, (float) (r - 130));
            return;
        }
        if (r >= 200 && r < 200 + kNumTracks * 10)
        {
            safe->proc.setTrackOutputPair ((r - 200) / 10, (r - 200) % 10);
            return;
        }
        if (r == 1 || r == 2)
            recAction->setValueNotifyingHost (recAction->convertTo0to1 ((float) (r - 1)));
        else if (r == 3)
            follow->setValueNotifyingHost (follow->get() ? 0.0f : 1.0f);
        else if (r == 4)
            safe->proc.setSaveLoopsWithProject (! safe->proc.getSaveLoopsWithProject());
        else if (r == 5)
        {
            auto& km = safe->proc.getKeyMap();
            km.setEnabled (! km.isEnabled());
            safe->showStatus (tr (km.isEnabled() ? "KEYBOARD CONTROL ON" : "KEYBOARD CONTROL OFF"));
            if (km.isEnabled())
                safe->grabKeyboardFocus();
        }
        else if (r == 6)
            KeyMapEditor::show (safe->proc, *safe);
        else if (r == 7)
            safe->showTempoEntry();
        else if (r == 8)
            safe->saveSet();
        else if (r == 9)
            safe->loadSet();
        else if (r >= 40 && r < 40 + clickSound->choices.size())
        {
            clickSound->beginChangeGesture();
            clickSound->setValueNotifyingHost (clickSound->convertTo0to1 ((float) (r - 40)));
            clickSound->endChangeGesture();
            safe->showStatus (tr ("METRONOME: %1", tr (clickSoundChoices()[r - 40]).toUpperCase()));
        }
        else if (r >= 50 && r <= 50 + LiveLooperProcessor::kNumCuePairs)
        {
            safe->proc.setCuePair (r - 50);
            safe->showStatus (tr ("CUE: %1", LiveLooperProcessor::getCuePairName (r - 50).toUpperCase()));
        }
        else if (r >= 60 && r <= 62)
            safe->proc.setClickRoute ((ClickRoute) (r - 60));
        else if (r >= 20 && r < 25)
        {
            safe->proc.setMaxLoopSeconds (lengths[r - 20]);
            safe->showStatus (tr ("MAX LOOP LENGTH %1 S", juce::String (lengths[r - 20])));
        }
        else if (r >= 30 && r <= 32)
        {
            i18n::setSetting ((i18n::Language) (r - 30));
            safe->showStatus (tr ("LANGUAGE: ENGLISH"));
        }
    });
}

//============================================================================== stems
void LiveLooperEditor::exportStems()
{
    const auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    chooser = std::make_unique<juce::FileChooser> (tr ("Export stems to folder"), dir);
    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [safe] (const juce::FileChooser& fc) {
        if (safe == nullptr || fc.getResult() == juce::File())
            return;
        const int n = safe->proc.exportStems (fc.getResult());
        safe->showStatus (n > 0 ? tr ("%1 STEMS EXPORTED", juce::String (n)) : tr ("NOTHING TO EXPORT"));
    });
}

//============================================================================== looper sets
void LiveLooperEditor::saveSet()
{
    const auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    chooser = std::make_unique<juce::FileChooser> (tr ("Save set"), dir.getChildFile ("livelooper-set.llset"), "*.llset");
    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe] (const juce::FileChooser& fc) {
        if (safe == nullptr || fc.getResult() == juce::File())
            return;
        const auto file = fc.getResult().withFileExtension ("llset");
        const bool ok = safe->proc.saveSet (file);
        safe->showStatus (ok ? tr ("SET SAVED: %1", file.getFileNameWithoutExtension().toUpperCase()) : tr ("SET COULD NOT BE SAVED"));
    });
}

void LiveLooperEditor::loadSet()
{
    const auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    chooser = std::make_unique<juce::FileChooser> (tr ("Load set"), dir, "*.llset");
    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc) {
        if (safe != nullptr && fc.getResult() != juce::File())
            safe->loadSetFile (fc.getResult());
    });
}

void LiveLooperEditor::loadSetFile (const juce::File& file)
{
    juce::String error;
    const bool ok = proc.loadSet (file, error);
    showStatus (ok ? tr ("SET LOADED: %1", file.getFileNameWithoutExtension().toUpperCase()) : tr ("LOAD FAILED: %1", error));
}

//============================================================================== track settings / files
void LiveLooperEditor::showTrackSettings (int track, juce::Component& anchor)
{
    auto panel = std::make_unique<TrackSettingsPanel> (proc, *this, track);
    const auto area = getLocalArea (&anchor, anchor.getLocalBounds());
    juce::CallOutBox::launchAsynchronously (std::move (panel), area, this);
}

void LiveLooperEditor::exportTrack (int track)
{
    const auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    chooser = std::make_unique<juce::FileChooser> (tr ("Export track %1", juce::String (track + 1)),
                                                   dir.getChildFile ("livelooper-track" + juce::String (track + 1) + ".wav"), "*.wav");
    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe, track] (const juce::FileChooser& fc) {
        if (safe == nullptr || fc.getResult() == juce::File())
            return;
        const auto file = fc.getResult().withFileExtension ("wav");
        safe->showStatus (safe->proc.exportTrack (track, file) ? tr ("EXPORTED %1", file.getFileName())
                                                               : tr ("EXPORT FAILED (TRACK EMPTY?)"));
    });
}

void LiveLooperEditor::importTrack (int track)
{
    chooser = std::make_unique<juce::FileChooser> (tr ("Import audio to track %1", juce::String (track + 1)),
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe, track] (const juce::FileChooser& fc) {
        if (safe == nullptr || ! fc.getResult().existsAsFile())
            return;
        juce::String error;
        const bool ok = safe->proc.importTrack (track, fc.getResult(), error);
        safe->showStatus (ok ? tr ("IMPORTED TO TRACK %1", juce::String (track + 1)) : tr ("IMPORT FAILED: %1", tr (error)));
    });
}

bool LiveLooperEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;ogg;mp3;llset"))
            return true;
    return false;
}

void LiveLooperEditor::filesDropped (const juce::StringArray& files, int x, int y)
{
    if (juce::File (files[0]).hasFileExtension ("llset"))
    {
        loadSetFile (juce::File (files[0]));
        return;
    }
    const auto pos = surface.getLocalPoint (this, juce::Point<int> (x, y));
    for (auto& s : strips)
        if (s->getBounds().contains (pos))
        {
            juce::String error;
            const bool ok = proc.importTrack (s->getTrackIndex(), juce::File (files[0]), error);
            showStatus (ok ? tr ("IMPORTED TO TRACK %1", juce::String (s->getTrackIndex() + 1)) : tr ("IMPORT FAILED: %1", tr (error)));
            return;
        }
}
} // namespace livelooper
