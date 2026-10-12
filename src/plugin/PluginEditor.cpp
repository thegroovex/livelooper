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
    clearAll.setTooltip ("Clear all five tracks");
    clearAll.onClick = [this] {
        auto opts = juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon)
                        .withTitle (tr ("Clear all tracks?")).withMessage (tr ("All five loops will be deleted."))
                        .withButton (tr ("Clear all")).withButton (tr ("Cancel")).withAssociatedComponent (this);
        juce::AlertWindow::showAsync (opts, [this] (int r) { if (r == 1) proc.pressTrigger (gClearAll); });
    };
    setParamId (clearAll, gClearAll);

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &rhythmOn, &rhythmPattern, rhythmLevel.get(), tempo.get(), &tap, &sync, &quantize,
             master.get(), monitor.get(), fade.get(), &allStartStop, &clearAll })
        surface.addAndMakeVisible (c);

    surface.addChildComponent (overlay);

    selectFxSlot (true, 0);
    proc.addChangeListener (this);
    i18n::addListener (this);

    setResizable (true, true);
    setResizeLimits (kBaseWidth * 6 / 10, kBaseHeight * 6 / 10, kBaseWidth * 2, kBaseHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) kBaseWidth / (double) kBaseHeight);
    setSize (kBaseWidth, kBaseHeight);
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

void LiveLooperEditor::resized()
{
    const float scale = (float) getWidth() / (float) kBaseWidth;
    surface.setBounds (0, 0, kBaseWidth, kBaseHeight);
    surface.setTransform (juce::AffineTransform::scale (scale));
    layoutSurface();
}

void LiveLooperEditor::layoutSurface()
{
    lcd.setBounds (layout::lcd);
    learnBtn.setBounds (868, 22, 104, 36);
    mapBtn.setBounds (978, 22, 92, 36);
    setupBtn.setBounds (1076, 22, 92, 36);

    auto slotRow = [] (juce::Rectangle<int> panel) { return panel.reduced (8).withTrimmedTop (22); };
    auto in = slotRow (layout::inputFx), tr = slotRow (layout::trackFx);
    const int sw = in.getWidth() / kNumFxSlots;
    for (int s = 0; s < kNumFxSlots; ++s)
    {
        inputSlots[(size_t) s]->setBounds (in.removeFromLeft (sw));
        trackSlots[(size_t) s]->setBounds (tr.removeFromLeft (sw));
    }

    auto ed = layout::fxEditor.reduced (8).withTrimmedTop (22);
    fxType.setBounds (ed.removeFromLeft (120).withSizeKeepingCentre (116, 28));
    const int kw = ed.getWidth() / 3;
    fxP1->setBounds (ed.removeFromLeft (kw));
    fxP2->setBounds (ed.removeFromLeft (kw));
    fxMix->setBounds (ed);

    auto tracks = layout::tracks;
    const int stripW = tracks.getWidth() / kNumTracks;
    for (int t = 0; t < kNumTracks; ++t)
        strips[(size_t) t]->setBounds (tracks.removeFromLeft (stripW).reduced (2, 0));

    auto rh = layout::rhythm.reduced (10).withTrimmedTop (22);
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

    auto ms = layout::masterPanel.reduced (10).withTrimmedTop (22);
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
    clearAll.setBounds (ms.removeFromBottom (30).reduced (0, 1));
    ms.removeFromBottom (6);
    allStartStop.setBounds (ms);

    overlay.setBounds (surface.getLocalBounds());
}

void LiveLooperEditor::paintSurface (juce::Graphics& g)
{
    // brushed panel
    juce::ColourGradient grad (juce::Colour (0xff2b2e33), 0.0f, 0.0f, juce::Colour (0xff17191c), 0.0f, (float) kBaseHeight, false);
    g.setGradientFill (grad);
    g.fillAll();
    g.setColour (juce::Colours::white.withAlpha (0.015f));
    for (int y = 0; y < kBaseHeight; y += 3)
        g.drawHorizontalLine (y, 0.0f, (float) kBaseWidth);

    // logo
    g.setColour (colours::text);
    g.setFont (LooperLookAndFeel::font (34.0f, true));
    g.drawText ("livelooper", 22, 14, 260, 40, juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (LooperLookAndFeel::font (11.0f, true));
    g.drawText (tr ("5 TRACK LOOP STATION"), 24, 50, 260, 16, juce::Justification::centredLeft);
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
    section (layout::fxEditor, fxTitle, selectedInput ? colours::cyan : colours::orange);
    section (layout::trackFx, tr ("TRACK FX"), colours::orange);
    section (layout::rhythm, tr ("RHYTHM"), colours::orange);
    section (layout::masterPanel, tr ("MASTER"), colours::green);

    // footer
    g.setColour (colours::textDim);
    g.setFont (LooperLookAndFeel::font (12.0f));
    const auto footer = layout::footer.reduced (20, 12);
    g.drawFittedText (tr ("Right-click any control for MIDI learn   |   hold STOP to clear   |   drop audio files on a track to import"),
                      footer.withTrimmedRight (130), juce::Justification::centredLeft, 1, 0.75f);
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
    surface.repaint (layout::fxEditor);
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
        s->refresh (blink);
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

    updateLcd();
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
        lines.add (tr ("MEMORY %1 S / TRACK", juce::String ((int) proc.getMaxLoopSeconds())));

    lcd.setContent (juce::String (bpm, 1), lines, accent, midiLedFrames > 0);
}

void LiveLooperEditor::showStatus (const juce::String& text)
{
    statusText = text;
    statusTime = juce::Time::getMillisecondCounter();
}

void LiveLooperEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
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

    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&c), [safe, id, learningThis] (int r) {
        if (safe == nullptr)
            return;
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

    juce::PopupMenu menu;
    menu.addSectionHeader (tr ("livelooper setup"));
    menu.addSubMenu (tr ("Record action"), rec);
    menu.addItem (3, tr ("Follow host transport (start / stop with the host)"), true, follow->get());
    menu.addSubMenu (tr ("Maximum loop length"), memory);
    menu.addItem (4, tr ("Save loops with the project"), true, proc.getSaveLoopsWithProject());
    menu.addSubMenu ("Language / Sprache", language);

    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&setupBtn),
                        [safe, recAction, follow, lengths] (int r) {
        if (safe == nullptr || r == 0)
            return;
        if (r == 1 || r == 2)
            recAction->setValueNotifyingHost (recAction->convertTo0to1 ((float) (r - 1)));
        else if (r == 3)
            follow->setValueNotifyingHost (follow->get() ? 0.0f : 1.0f);
        else if (r == 4)
            safe->proc.setSaveLoopsWithProject (! safe->proc.getSaveLoopsWithProject());
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
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;ogg;mp3"))
            return true;
    return false;
}

void LiveLooperEditor::filesDropped (const juce::StringArray& files, int x, int y)
{
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
