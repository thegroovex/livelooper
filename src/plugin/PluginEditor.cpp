#include "PluginEditor.h"
#include "SoundPacks.h"
#include "AppSettings.h"
#include "InfoWindows.h"
#include "Updates.h"
#include "GuidedTour.h"   // v1.6
#include "WelcomeScreen.h" // v1.6

namespace livelooper
{
using namespace params;
using i18n::tr;

namespace layout
{
    // v1.6: loop tracks first (largest, brightest), FX second, everything else subdued around them
    const juce::Rectangle<int> header       { 0, 0, 1180, 88 };
    const juce::Rectangle<int> lcd          { 318, 10, 530, 72 };
    const juce::Rectangle<int> tracks       { 12, 96, 850, 512 };
    const juce::Rectangle<int> fxPanel      { 12, 618, 850, 180 };
    const juce::Rectangle<int> rhythm       { 870, 96, 298, 234 };
    const juce::Rectangle<int> metronome    { 870, 338, 298, 98 };
    const juce::Rectangle<int> masterPanel  { 870, 444, 298, 354 };
    const juce::Rectangle<int> footer       { 0, 804, 1180, 52 };
    // kept for the BEATS page (covers FX + tracks) and older references
    const juce::Rectangle<int> inputFx      { 12, 96, 380, 122 };
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
    setupBtn.setTooltip ("Settings, sets, Link, updates, about, language");
    helpBtn.onClick = [this] { showHelpMenu(); };      // v1.6: help + guided tour
    helpBtn.setTooltip ("Help: every function explained (searchable), guided tour");
    for (auto* b : { &learnBtn, &mapBtn, &setupBtn, &helpBtn })
    {
        LooperLookAndFeel::setEmphasis (*b, 0);
        surface.addAndMakeVisible (b);
    }

    // ---- v1.6 FX panel: one group at a time
    {
        const char* names[] = { "INPUT FX", "TRACK FX", "BEAT FX", "REPEAT / STUTTER", "COLOR FX" };
        const juce::Colour leds[] = { colours::cyan, colours::orange, colours::orange, colours::red, colours::blue };
        for (int i = 0; i < numFxTabs; ++i)
        {
            fxTabs[(size_t) i] = std::make_unique<FxTabButton> (names[i], leds[i]);
            fxTabs[(size_t) i]->onClick = [this, i] { setFxTab (i); };
            fxTabs[(size_t) i]->setTooltip ("Show this group of effects");
            surface.addAndMakeVisible (*fxTabs[(size_t) i]);
        }
        auto anyOn = [this] (bool input) {
            for (int sl = 0; sl < kNumFxSlots; ++sl)
                if (proc.getState().getRawParameterValue (fxId (input, sl, kFxOn))->load() > 0.5f)
                    return true;
            return false;
        };
        auto param = [this] (const juce::String& id) { return proc.getState().getRawParameterValue (id)->load(); };
        fxTabs[tabInput]->activeFn  = [anyOn] { return anyOn (true); };
        fxTabs[tabTrack]->activeFn  = [anyOn] { return anyOn (false); };
        fxTabs[tabBeat]->activeFn   = [param] { return param (gBfxOn) > 0.5f || param (gBfxHold) > 0.5f; };
        fxTabs[tabRepeat]->activeFn = [param] { return param (gRepeatLatch) > 0.5f || param (gStutterLatch) > 0.5f
                                                       || param (gRepeatHold) > 0.5f || param (gStutterHold) > 0.5f; };
        fxTabs[tabColor]->activeFn  = [param] { return juce::roundToInt (param (gColorType)) != 0; };
    }
    TrackStrip::showMeters = appsettings::getValue ("trackMeters") == "1";

    // ---- v1.5 performance row: beat FX
    bfxType.setEnglishItems (beatFxChoices());
    bfxTypeAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, gBfxType, bfxType);
    setParamId (bfxType, gBfxType);
    bfxType.setTooltip ("Beat FX type (synced to the tempo)");
    bfxType.onChange = [this] { updatePerformanceLabels(); };
    bfxBeat.setEnglishItems (beatFractionChoices());
    bfxBeatAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, gBfxBeat, bfxBeat);
    setParamId (bfxBeat, gBfxBeat);
    bfxBeat.setTooltip ("Beat fraction of the beat FX in beats: 1/32 beat ... 16 beats (4 bars)");
    bfxTarget.setEnglishItems (fxTargetChoices());
    bfxTargetAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, gBfxTarget, bfxTarget);
    setParamId (bfxTarget, gBfxTarget);
    bfxTarget.setTooltip ("FX channel: where beat FX, beat repeat and stutter work - master, all loops, the input (recorded with the effect) or one track");
    auto stepBeat = [this] (int delta) {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (proc.getState().getParameter (gBfxBeat)))
        {
            const int i = juce::jlimit (0, p->choices.size() - 1, p->getIndex() + delta);
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 ((float) i));
            p->endChangeGesture();
        }
    };
    beatDown.onClick = [stepBeat] { stepBeat (-1); };
    beatUp.onClick = [stepBeat] { stepBeat (1); };
    beatDown.setTooltip ("Shorter beat fraction");
    beatUp.setTooltip ("Longer beat fraction");
    bfxOn.setClickingTogglesState (true);
    bfxOnAtt = std::make_unique<APVTS::ButtonAttachment> (st, gBfxOn, bfxOn);
    setParamId (bfxOn, gBfxOn);
    bfxOn.setTooltip ("Beat FX on / off");
    bfxHold = std::make_unique<MomentaryButton> (st, gBfxHold, "HOLD", colours::orange);
    bfxHold->setTooltip ("Beat FX while held (momentary) - map it to a pad");
    bfxDepth = std::make_unique<Knob> (st, gBfxDepth, "Level", colours::orange);

    // beat repeat / stutter
    repeatRate.setEnglishItems (repeatRateChoices());
    repeatRateAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, gRepeatRate, repeatRate);
    setParamId (repeatRate, gRepeatRate);
    repeatRate.setTooltip ("Beat repeat timing (note value)");
    stutterRate.setEnglishItems (repeatRateChoices());
    stutterRateAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, gStutterRate, stutterRate);
    setParamId (stutterRate, gStutterRate);
    stutterRate.setTooltip ("Stutter timing (note value)");
    repeatHold = std::make_unique<MomentaryButton> (st, gRepeatHold, "REPEAT", colours::red);
    repeatHold->setTooltip ("Beat repeat while held: repeats the next 1/2 ... 1/32 note from the next grid line, back in sync on release");
    stutterHold = std::make_unique<MomentaryButton> (st, gStutterHold, "STUTTER", colours::red);
    stutterHold->setTooltip ("Stutter while held: chopped repeats of the timing, back in sync on release");
    for (auto* b : { &repeatLatch, &stutterLatch })
        b->setClickingTogglesState (true);
    repeatLatchAtt = std::make_unique<APVTS::ButtonAttachment> (st, gRepeatLatch, repeatLatch);
    stutterLatchAtt = std::make_unique<APVTS::ButtonAttachment> (st, gStutterLatch, stutterLatch);
    setParamId (repeatLatch, gRepeatLatch);
    setParamId (stutterLatch, gStutterLatch);
    repeatLatch.setTooltip ("Beat repeat latched on (until pressed again)");
    stutterLatch.setTooltip ("Stutter latched on (until pressed again)");

    // colour FX: one type for all tracks, one knob per track (in the track strips)
    for (int i = 0; i < (int) colorBtns.size(); ++i)
    {
        auto b = std::make_unique<LLButton> (i18n::upper (juce::String (getColorFxName ((ColorFxType) i))), colours::blue);
        b->onClick = [this, i] {
            if (auto* p = proc.getState().getParameter (gColorType))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->convertTo0to1 ((float) i));
                p->endChangeGesture();
            }
        };
        setParamId (*b, gColorType);
        b->setTooltip ("Color FX type for every track's COLOR knob");
        surface.addAndMakeVisible (*b);
        colorBtns[(size_t) i] = std::move (b);
    }

    // metronome (its own block since 1.5)
    for (auto* b : { &metroOn, &metroAccent })
        b->setClickingTogglesState (true);
    metroOnAtt = std::make_unique<APVTS::ButtonAttachment> (st, gMetroOn, metroOn);
    metroAccentAtt = std::make_unique<APVTS::ButtonAttachment> (st, gMetroAccent, metroAccent);
    setParamId (metroOn, gMetroOn);
    setParamId (metroAccent, gMetroAccent);
    metroOn.setTooltip ("Metronome on / off (independent of the rhythm)");
    metroAccent.setTooltip ("Accent on the first beat of every bar");
    clickSoundBox.setEnglishItems (clickSoundChoices());
    clickSoundAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, gClickSound, clickSoundBox);
    setParamId (clickSoundBox, gClickSound);
    clickSoundBox.setTooltip ("Metronome sound");
    metroLevel = std::make_unique<Knob> (st, gMetroLevel, "Level", colours::yellow);

    for (juce::Component* comp : std::initializer_list<juce::Component*> {
             &bfxType, &bfxBeat, &bfxTarget, &beatDown, &beatUp, &bfxOn, bfxHold.get(), bfxDepth.get(),
             &repeatRate, &stutterRate, repeatHold.get(), stutterHold.get(), &repeatLatch, &stutterLatch,
             &metroOn, &metroAccent, &clickSoundBox, metroLevel.get() })
        surface.addAndMakeVisible (comp);

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
    rhythmKit.setEnglishItems (kitChoices());
    rhythmKitAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, gRhythmKit, rhythmKit);
    setParamId (rhythmKit, gRhythmKit);
    rhythmKit.setTooltip ("Drum kit: Studio, 808, 909 or Lo-Fi");
    rhythmSwing = std::make_unique<Knob> (st, gRhythmSwing, "Swing", colours::orange);
    rhythmSwing->slider.setTooltip ("Swing: 50 % = straight, 66 % = triplet feel");
    rhythmHuman = std::make_unique<Knob> (st, gRhythmHuman, "Feel", colours::orange);
    rhythmHuman->slider.setTooltip ("Humanize: small variations in timing and velocity");
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
             &rhythmOn, &rhythmPattern, &rhythmKit, rhythmLevel.get(), rhythmSwing.get(), rhythmHuman.get(), tempo.get(), &tap, &sync, &quantize,
             master.get(), monitor.get(), fade.get(), &allStartStop, &clearAll, &capture })
        surface.addAndMakeVisible (c);

    initFeatures16();      // v1.6
    // v1.6 rhythm: BEATS page, FILL, drums through the performance FX
    beatsBtn.onClick = [this] { showBeatsPage (! isBeatsPageVisible()); };
    beatsBtn.setTooltip ("BEATS: step sequencer for the drums - build your own patterns");
    surface.addAndMakeVisible (beatsBtn);
    fillBtn = std::make_unique<MomentaryButton> (st, gRhythmFill, "FILL", colours::orange);
    fillBtn->setTooltip ("Hold for a fill; on release the groove comes back on the next downbeat");
    surface.addAndMakeVisible (*fillBtn);
    drumsBfxBtn.setClickingTogglesState (true);
    drumsBfxAtt = std::make_unique<APVTS::ButtonAttachment> (st, gDrumsBfx, drumsBfxBtn);
    setParamId (drumsBfxBtn, gDrumsBfx);
    drumsBfxBtn.setTooltip ("The drums go through the beat FX too (off: the drums stay dry)");
    surface.addAndMakeVisible (drumsBfxBtn);
    drumsRepBtn.setClickingTogglesState (true);
    drumsRepAtt = std::make_unique<APVTS::ButtonAttachment> (st, gDrumsRepeat, drumsRepBtn);
    setParamId (drumsRepBtn, gDrumsRepeat);
    drumsRepBtn.setTooltip ("The drums go through beat repeat / stutter too (off: the drums stay dry)");
    surface.addAndMakeVisible (drumsRepBtn);
    beatsPage = std::make_unique<BeatsPage> (proc);
    beatsPage->onClose = [this] { showBeatsPage (false); };
    beatsPage->onStatus = [this] (const juce::String& t) { showStatus (t); };
    surface.addChildComponent (*beatsPage);
    rhythmKit.setTooltip ("Drum kit: the built-in kits and the sound-pack slots");

    surface.addChildComponent (overlay);
    tempo->slider.setTooltip ("Internal tempo - right-click to type in the BPM, or click the tempo on the display");

    selectFxSlot (true, 0);
    updateKeyboardFocusPolicy();
    proc.addChangeListener (this);
    i18n::addListener (this);

    setResizable (true, true);
    applyVisibleTracks();
    updatePerformanceLabels();
    startTimerHz (30);

    // v1.5: ask once how livelooper should start; check for updates when wanted
    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    juce::Timer::callAfterDelay (600, [safe] {
        if (safe == nullptr)
            return;
        safe->askStartupSettingsOnce();
        safe->handleFirstRun();          // v1.6: demo set + guided tour on the very first start
        if (safe->proc.wrapperType != juce::AudioProcessor::wrapperType_Undefined && appsettings::getCheckUpdatesOnStart())
            safe->checkForUpdates (false);
    });
}

LiveLooperEditor::~LiveLooperEditor()
{
    if (clipThread != nullptr)          // v1.6
        clipThread->stopThread (10000);
    proc.onRecordingFinished = nullptr;
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
    // header buttons (quiet): MIDI LEARN, MIDI MAP, SETUP, ? - the round REC sits right below them
    learnBtn.setBounds (874 + dx, 14, 98, 32);
    mapBtn.setBounds (976 + dx, 14, 86, 32);
    setupBtn.setBounds (1066 + dx, 14, 66, 32);
    helpBtn.setBounds (1136 + dx, 14, 32, 32);
    layoutPerformanceRow (dx);
    layoutFeatures16 (dx);      // v1.6
    layoutRhythm16 (dx);

    auto tracks = layout::tracks.withWidth (layout::tracks.getWidth() + dx);
    const int visible = proc.getVisibleTracks();
    const int stripW = tracks.getWidth() / visible;
    for (int t = 0; t < visible; ++t)
        strips[(size_t) t]->setBounds (tracks.removeFromLeft (stripW).reduced (3, 0));

    // RHYTHM
    rhythmArea = layout::rhythm.translated (dx, 0);
    auto rh = rhythmArea.reduced (14, 12).withTrimmedTop (30);
    auto row = rh.removeFromTop (30);
    rhythmOn.setBounds (row.removeFromLeft (98));
    row.removeFromLeft (8);
    rhythmPattern.setBounds (row);
    rh.removeFromTop (6);
    row = rh.removeFromTop (28);
    tap.setBounds (row.removeFromRight (70));
    row.removeFromRight (8);
    rhythmKit.setBounds (row);
    rh.removeFromTop (6);
    row = rh;
    const int quarter = row.getWidth() / 4;
    rhythmLevel->setBounds (row.removeFromLeft (quarter).reduced (3));
    rhythmSwing->setBounds (row.removeFromLeft (quarter).reduced (3));
    rhythmHuman->setBounds (row.removeFromLeft (quarter).reduced (3));
    tempo->setBounds (row.reduced (3));

    // MASTER
    masterArea = layout::masterPanel.translated (dx, 0);
    auto ms = masterArea.reduced (14, 12).withTrimmedTop (30);
    row = ms.removeFromTop (30);
    quantize.setBounds (row.removeFromLeft (row.getWidth() / 2).withTrimmedRight (4));
    sync.setBounds (row.withTrimmedLeft (4));
    ms.removeFromTop (8);
    row = ms.removeFromTop (84);
    const int kn = row.getWidth() / 3;
    master->setBounds (row.removeFromLeft (kn).reduced (4));
    monitor->setBounds (row.removeFromLeft (kn).reduced (4));
    fade->setBounds (row.reduced (4));
    ms.removeFromTop (12);
    {
        auto bottom = ms.removeFromBottom (30);
        capture.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2).withTrimmedRight (4));
        clearAll.setBounds (bottom.withTrimmedLeft (4));
    }
    ms.removeFromBottom (12);
    allStartStop.setBounds (ms);
    LooperLookAndFeel::setEmphasis (allStartStop, 2);
    for (auto* b2 : { &capture, &clearAll, &tap, &sync })
        LooperLookAndFeel::setEmphasis (*b2, 0);

    // scenes in the footer
    auto sc = layout::footer.reduced (20, 11).withTrimmedLeft (70);
    for (auto& b : sceneBtns)
    {
        b->setBounds (sc.removeFromLeft (34).reduced (1, 0));
        sc.removeFromLeft (3);
    }
    sc.removeFromLeft (6);
    sceneStore.setBounds (sc.removeFromLeft (76).reduced (1, 0));

    overlay.setBounds (surface.getLocalBounds());
    surfaceCacheKey = {};
}

void LiveLooperEditor::renderSurfaceCache()
{
    const int w = surfaceWidth(), h = kBaseHeight;
    const float sc = 2.0f;        // 2x: sharp when the window is enlarged and on high-DPI screens
    surfaceCache = juce::Image (juce::Image::RGB, juce::roundToInt ((float) w * sc), juce::roundToInt ((float) h * sc), false);
    juce::Graphics g (surfaceCache);
    g.addTransform (juce::AffineTransform::scale (sc));
    const int dx = w - kBaseWidth;

    // deep background with a faint brand glow behind the glass
    juce::ColourGradient bg (juce::Colour (0xff1b1b21), 0.0f, 0.0f, juce::Colour (0xff0c0c10), 0.0f, (float) h, false);
    g.setGradientFill (bg);
    g.fillAll();
    auto glow = [&g] (float cx, float cy, float r, juce::Colour c, float a) {
        juce::ColourGradient rg (c.withAlpha (a), cx, cy, c.withAlpha (0.0f), cx + r, cy, true);
        g.setGradientFill (rg);
        g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    };
    glow (120.0f, 40.0f, 520.0f, colours::brand0, 0.10f);
    glow ((float) w * 0.45f, (float) h + 80.0f, 620.0f, colours::brand1, 0.08f);
    glow ((float) w - 80.0f, 160.0f, 560.0f, colours::brand2, 0.10f);

    // logo: livelooper in the brand gradient, quiet subtitle
    {
        const auto f = LooperLookAndFeel::font (32.0f, true);
        const juce::Rectangle<float> logo (24.0f, 14.0f, juce::GlyphArrangement::getStringWidth (f, "livelooper") + 2.0f, 40.0f);
        g.setGradientFill (LooperLookAndFeel::brandGradient (logo));
        g.setFont (f);
        g.drawText ("livelooper", logo, juce::Justification::centredLeft);
        g.setColour (colours::textDim);
        g.setFont (LooperLookAndFeel::font (11.0f, true));
        g.drawText (tr ("%1 TRACK LOOP STATION", juce::String (proc.getVisibleTracks())) + "  \xc2\xb7  designed by groovex",
                    26, 54, 290, 16, juce::Justification::centredLeft);
    }

    auto title = [&g] (juce::Rectangle<int> r, const juce::String& text, juce::Colour accent, bool quiet)
    {
        g.setColour (accent.withAlpha (quiet ? 0.7f : 1.0f));
        g.fillRoundedRectangle ((float) r.getX() + 14.0f, (float) r.getY() + 17.0f, 4.0f, 12.0f, 2.0f);
        g.setColour (quiet ? colours::textDim.brighter (0.2f) : colours::text);
        g.setFont (LooperLookAndFeel::font (quiet ? 12.0f : 13.0f, true));
        g.drawText (text, r.getX() + 24, r.getY() + 12, r.getWidth() - 40, 22, juce::Justification::centredLeft);
    };

    // tracks: the brightest glass, tinted with each track's colour
    for (int t = 0; t < proc.getVisibleTracks(); ++t)
        LooperLookAndFeel::drawGlassPanel (g, strips[(size_t) t]->getBounds().toFloat(), 16.0f, 2, colours::track (t));
    // FX: second level
    LooperLookAndFeel::drawGlassPanel (g, fxPanelArea.toFloat(), 16.0f, 1);
    title (fxPanelArea, tr ("FX"), colours::orange, false);
    if (fxTab == tabInput || fxTab == tabTrack)
    {
        // caption above the editor of the selected slot
        const auto ed = fxType.getBounds();
        g.setColour (colours::textDim);
        g.setFont (LooperLookAndFeel::font (11.0f, true));
        g.drawText (tr (selectedInput ? "INPUT FX" : "TRACK FX") + " " + juce::String::charToString ((juce::juce_wchar) ('A' + selectedSlot)) + "  " + tr ("EDIT"),
                    ed.getX(), ed.getY() - 22, 260, 18, juce::Justification::centredLeft);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillRect ((float) ed.getX() - 14.0f, (float) fxContentArea.getY() + 6.0f, 1.0f, (float) fxContentArea.getHeight() - 12.0f);
    }
    else if (fxTab == tabColor)
    {
        g.setColour (colours::textDim);
        g.setFont (LooperLookAndFeel::font (12.0f));
        g.drawFittedText (tr ("Turn COLOR on a track to blend in the chosen effect - centre = off."),
                          fxContentArea.withTrimmedTop (fxContentArea.getHeight() - 26), juce::Justification::centredLeft, 1, 0.8f);
    }
    // everything else: quiet
    LooperLookAndFeel::drawGlassPanel (g, rhythmArea.toFloat(), 16.0f, 0);
    title (rhythmArea, tr ("RHYTHM"), colours::orange, true);
    LooperLookAndFeel::drawGlassPanel (g, metroArea.toFloat(), 16.0f, 0);
    title (metroArea, tr ("METRONOME"), colours::yellow, true);
    LooperLookAndFeel::drawGlassPanel (g, masterArea.toFloat(), 16.0f, 0);
    title (masterArea, tr ("MASTER"), colours::green, true);

    // footer: scenes, tips, version
    const auto footer = layout::footer.withWidth (w).reduced (20, 12);
    g.setColour (colours::text);
    g.setFont (LooperLookAndFeel::font (12.0f, true));
    g.drawText (tr ("SCENES"), footer.withWidth (66), juce::Justification::centredLeft);
    g.setColour (colours::textDim.withAlpha (0.85f));
    g.setFont (LooperLookAndFeel::font (11.5f));
    g.drawFittedText (tr ("Right-click any control for MIDI learn   |   hold STOP to clear   |   drop audio files on a track to import"),
                      footer.withTrimmedLeft (sceneStore.getRight() - footer.getX() + 18).withTrimmedRight (120),
                      juce::Justification::centredLeft, 1, 0.6f);
    g.drawText ("livelooper v" LIVELOOPER_VERSION, footer, juce::Justification::centredRight);
    juce::ignoreUnused (dx);
}

void LiveLooperEditor::paintSurface (juce::Graphics& g)
{
    const auto key = juce::String (surfaceWidth()) + "/" + juce::String (proc.getVisibleTracks()) + "/" + (i18n::isGerman() ? "de" : "en")
                     + "/" + juce::String (fxTab) + "/" + juce::String ((int) selectedInput) + juce::String (selectedSlot)
                     + "/" + fxType.getBounds().toString() + strips[0]->getBounds().toString();
    if (key != surfaceCacheKey || surfaceCache.isNull())
    {
        surfaceCacheKey = key;
        renderSurfaceCache();
    }
    g.drawImage (surfaceCache, juce::Rectangle<float> (0.0f, 0.0f, (float) surfaceWidth(), (float) kBaseHeight));

    // scene LEDs: stored = dim, last recalled = bright
    const int last = proc.getLastScene();
    for (int i = 0; i < kNumScenes; ++i)
    {
        const auto b = sceneBtns[(size_t) i]->getBounds();
        const bool stored = proc.sceneIsStored (i);
        const auto c = stored ? (i == last ? colours::yellow : colours::yellow.withAlpha (0.35f)) : juce::Colour (0xff2b2d33);
        if (stored && i == last)
        {
            g.setColour (colours::yellow.withAlpha (0.25f));
            g.fillRoundedRectangle ((float) b.getX() + 4.0f, (float) b.getBottom() + 1.0f, (float) b.getWidth() - 8.0f, 5.0f, 2.5f);
        }
        g.setColour (c);
        g.fillRoundedRectangle ((float) b.getX() + 7.0f, (float) b.getBottom() + 2.0f, (float) b.getWidth() - 14.0f, 3.0f, 1.5f);
    }
}

void LiveLooperEditor::FxTabButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    // a segment of a glass segmented control: the selected one is lit
    auto& lf = getLookAndFeel();
    lf.drawButtonBackground (g, *this, {}, over, down);
    auto r = getLocalBounds();
    const bool active = activeFn && activeFn();
    const auto led = juce::Colour ((juce::uint32) (juce::int64) getProperties()["ledColour"]);
    if (active)
    {
        const auto dot = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ (float) r.getX() + 12.0f, (float) r.getCentreY() });
        g.setColour (led.withAlpha (0.35f));
        g.fillEllipse (dot.expanded (3.0f));
        g.setColour (led);
        g.fillEllipse (dot);
    }
    g.setFont (LooperLookAndFeel::font (12.0f, true));
    g.setColour (getToggleState() ? juce::Colours::white : colours::textDim.brighter (over ? 0.5f : 0.2f));
    g.drawFittedText (tr (getButtonText()), r.reduced (active ? 18 : 8, 0).withTrimmedLeft (active ? 4 : 0), juce::Justification::centred, 1, 0.7f);
}

void LiveLooperEditor::setFxTab (int tab)
{
    fxTab = juce::jlimit (0, (int) numFxTabs - 1, tab);
    for (int i = 0; i < numFxTabs; ++i)
        fxTabs[(size_t) i]->setToggleState (i == fxTab, juce::dontSendNotification);
    if (fxTab == tabInput && ! selectedInput)
        selectFxSlot (true, selectedSlot);
    else if (fxTab == tabTrack && selectedInput)
        selectFxSlot (false, selectedSlot);
    layoutFxPanel (surfaceWidth() - kBaseWidth);
    surface.repaint();
}

void LiveLooperEditor::layoutFxPanel (int dx)
{
    fxPanelArea = layout::fxPanel.withWidth (layout::fxPanel.getWidth() + dx);
    // tab bar right of the title
    auto bar = fxPanelArea.reduced (14, 0).withTrimmedTop (10).withHeight (28).withTrimmedLeft (54);
    const int widths[] = { 112, 112, 104, 168, 112 };
    for (int i = 0; i < numFxTabs; ++i)
    {
        fxTabs[(size_t) i]->setBounds (bar.removeFromLeft (widths[i]));
        bar.removeFromLeft (4);
        fxTabs[(size_t) i]->setToggleState (i == fxTab, juce::dontSendNotification);
        LooperLookAndFeel::setEmphasis (*fxTabs[(size_t) i], 1);
    }
    fxContentArea = fxPanelArea.reduced (16, 10).withTrimmedTop (36);
    const auto c = fxContentArea;

    auto show = [] (std::initializer_list<juce::Component*> comps, bool on) { for (auto* x : comps) x->setVisible (on); };
    const bool slotsTab = fxTab == tabInput || fxTab == tabTrack;
    for (int s = 0; s < kNumFxSlots; ++s)
    {
        inputSlots[(size_t) s]->setVisible (fxTab == tabInput);
        trackSlots[(size_t) s]->setVisible (fxTab == tabTrack);
    }
    show ({ &fxType, fxP1.get(), fxP2.get(), fxMix.get() }, slotsTab);
    show ({ &bfxType, &bfxTarget, &beatDown, &beatUp, &bfxBeat, &bfxOn, bfxHold.get(), bfxDepth.get(), &drumsBfxBtn }, fxTab == tabBeat);
    show ({ repeatHold.get(), stutterHold.get(), &repeatRate, &stutterRate, &repeatLatch, &stutterLatch, &drumsRepBtn }, fxTab == tabRepeat);
    for (auto& b : colorBtns)
        b->setVisible (fxTab == tabColor);

    // INPUT / TRACK FX: four slot pads, then the editor of the selected slot
    {
        auto r = c;
        auto slots = r.removeFromLeft (4 * 98);
        for (int s = 0; s < kNumFxSlots; ++s)
        {
            const auto sb = slots.removeFromLeft (98);
            inputSlots[(size_t) s]->setBounds (sb);
            trackSlots[(size_t) s]->setBounds (sb);
        }
        r.removeFromLeft (30);
        auto ed = r.withTrimmedTop (24);
        fxType.setBounds (ed.removeFromLeft (170).withHeight (30));
        ed.removeFromLeft (16);
        const int kw = juce::jmin (100, ed.getWidth() / 3);
        auto knobs = juce::Rectangle<int> (ed.getX(), c.getY() + 2, kw * 3, c.getHeight() - 4);
        fxP1->setBounds (knobs.removeFromLeft (kw));
        fxP2->setBounds (knobs.removeFromLeft (kw));
        fxMix->setBounds (knobs);
    }

    // BEAT FX: type / channel | beat | level | ON / HOLD, DRUMS
    {
        auto r = c.reduced (0, 4);
        auto col = r.removeFromLeft (200 + dx / 3);
        bfxType.setBounds (col.removeFromTop (32).reduced (0, 1));
        col.removeFromTop (12);
        bfxTarget.setBounds (col.removeFromTop (32).reduced (0, 1));
        r.removeFromLeft (20);
        col = r.removeFromLeft (150);
        {
            auto rowB = col.removeFromTop (32);
            beatDown.setBounds (rowB.removeFromLeft (30).reduced (0, 1));
            beatUp.setBounds (rowB.removeFromRight (30).reduced (0, 1));
            bfxBeat.setBounds (rowB.reduced (4, 1));
        }
        col.removeFromTop (12);
        drumsBfxBtn.setBounds (col.removeFromTop (32).reduced (0, 1));
        r.removeFromLeft (20);
        auto onCol = r.removeFromRight (130);
        bfxOn.setBounds (onCol.removeFromTop (54).reduced (0, 1));
        onCol.removeFromTop (8);
        bfxHold->setBounds (onCol.removeFromTop (40).reduced (0, 1));
        bfxDepth->setBounds (r.withSizeKeepingCentre (juce::jmin (100, r.getWidth()), r.getHeight()));
        LooperLookAndFeel::setEmphasis (bfxOn, 2);
    }

    // BEAT REPEAT / STUTTER: two rows, DRUMS on the right
    {
        auto r = c.reduced (0, 6);
        auto drums = r.removeFromRight (120);
        drumsRepBtn.setBounds (drums.withSizeKeepingCentre (110, 34));
        r.removeFromRight (20);
        for (int i = 0; i < 2; ++i)
        {
            auto rowR = r.removeFromTop (44);
            auto& hold = i == 0 ? *repeatHold : *stutterHold;
            auto& rate = i == 0 ? repeatRate : stutterRate;
            auto& latch = i == 0 ? repeatLatch : stutterLatch;
            hold.setBounds (rowR.removeFromLeft (180).reduced (0, 2));
            rowR.removeFromLeft (14);
            latch.setBounds (rowR.removeFromRight (110).reduced (0, 4));
            rowR.removeFromRight (14);
            rate.setBounds (rowR.removeFromLeft (160).reduced (0, 6));
            r.removeFromTop (12);
            LooperLookAndFeel::setEmphasis (hold, 2);
        }
    }

    // COLOR FX: six large type buttons
    {
        auto r = c.withTrimmedBottom (34);
        const int bw = r.getWidth() / (int) colorBtns.size();
        for (int i = 0; i < (int) colorBtns.size(); ++i)
            colorBtns[(size_t) i]->setBounds (r.removeFromLeft (bw).reduced (5, 6));
    }
}

//============================================================================== v1.5 performance row
void LiveLooperEditor::layoutPerformanceRow (int dx)
{
    // v1.6: beat FX, beat repeat / stutter and colour FX live in the tabbed FX panel
    layoutFxPanel (dx);
    beatFxArea = repeatArea = colorArea = fxPanelArea;

    // metronome (quiet, right column): on / accent / sound | level
    metroArea = layout::metronome.translated (dx, 0);
    auto m = metroArea.reduced (14, 10).withTrimmedTop (30);
    metroLevel->setBounds (m.removeFromRight (64).withTrimmedTop (-30).reduced (0, 2));
    m.removeFromRight (8);
    auto rowM = m.removeFromTop (24);
    metroOn.setBounds (rowM.removeFromLeft (rowM.getWidth() * 3 / 5).withTrimmedRight (3));
    metroAccent.setBounds (rowM.withTrimmedLeft (3));
    m.removeFromTop (5);
    clickSoundBox.setBounds (m.removeFromTop (24));
    for (auto* b2 : { &metroOn, &metroAccent })
        LooperLookAndFeel::setEmphasis (*b2, 0);
}

void LiveLooperEditor::updatePerformanceLabels()
{
    const auto t = (BeatFxType) juce::jlimit (0, (int) BeatFxType::NumTypes - 1, bfxType.getSelectedItemIndex());
    bfxDepth->setCaption (getBeatFxDepthName (t));
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
    if (fxTab <= tabTrack && fxTabs[0] != nullptr && fxTab != (inputFx ? tabInput : tabTrack))
        setFxTab (inputFx ? tabInput : tabTrack);
    surface.repaint (fxPanelArea);
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
    timerFeatures16();     // v1.6
    refreshPatternNames();      // v1.6
    blinkCounter = (blinkCounter + 1) % 16;
    const bool blink = blinkCounter < 8;
    for (auto& s : strips)
        if (s->isVisible())
            s->refresh (blink);
    if (blinkCounter % 4 == 0)
    {
        surface.repaint (layout::footer.withWidth (surfaceWidth()));
        for (auto& t : fxTabs)
            t->repaint();
    }
    for (int s = 0; s < kNumFxSlots; ++s)
    {
        inputSlots[(size_t) s]->repaint();
        trackSlots[(size_t) s]->repaint();
    }

    {
        const int ct = juce::roundToInt (proc.getState().getRawParameterValue (gColorType)->load());
        for (int i = 0; i < (int) colorBtns.size(); ++i)
            colorBtns[(size_t) i]->setToggleState (i == ct, juce::dontSendNotification);
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
    {
        // v1.5: Link status (peers) on the display
        const bool linkOn = proc.getState().getRawParameterValue (gLinkOn)->load() >= 0.5f;
        juce::String l1;
        if (linkOn && proc.isLinkDriving())
        {
            const int peers = proc.getLinkPeers();
            l1 << "BPM   " << tr (peers == 1 ? "LINK 1 PEER" : "LINK %1 PEERS", juce::String (peers));
        }
        else
        {
            l1 = proc.isLinkDriving() ? tr ("BPM   LINK") : tr (host ? "BPM   HOST" : (syncOn ? "BPM   HOST (STOPPED) / INT CLOCK" : "BPM   INTERNAL"));
            if (linkOn)
                l1 << "   " << tr ("LINK (HOST SYNC HAS PRIORITY)");
        }
        lines.add (l1);
    }
    lines.add (bar + "     " + tr ("QUANTIZE %1", i18n::upper (tr (quant))));

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
    updatePerformanceLabels();
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
        showStatus (tr ("KEY %1 > %2", i18n::upper (KeyMap::describeKey (key)), i18n::upper (paramName (action))));
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
        // v1.6: controls hidden under the BEATS page are not offered for MIDI learn
        if (&parent == &surface && isBeatsPageVisible() && c != beatsPage.get() && beatsPage->getBounds().contains (c->getBounds()))
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

    // v1.5 rhythm groove (micro-timing feel) and fills
    auto* groove = dynamic_cast<juce::AudioParameterChoice*> (st.getParameter (gRhythmGroove));
    auto* fills = dynamic_cast<juce::AudioParameterChoice*> (st.getParameter (gRhythmFills));
    juce::PopupMenu grooveMenu, fillsMenu;
    for (int i = 0; i < groove->choices.size(); ++i)
        grooveMenu.addItem (500 + i, tr (grooveChoices()[i]), true, groove->getIndex() == i);
    for (int i = 0; i < fills->choices.size(); ++i)
        fillsMenu.addItem (520 + i, tr (fillChoices()[i]), true, fills->getIndex() == i);

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
        cue.addItem (69, tr ("Enable the outputs in Audio/MIDI Settings (button above the looper)"), false);
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

    juce::PopupMenu pushMenu;
    if (proc.isPushAvailable())
    {
        pushMenu.addItem (140, tr ("Control from Ableton Push (User port)"), true, proc.isPushEnabled());
        pushMenu.addItem (149, proc.getPushStatus(), false);
        pushMenu.addSeparator();
        pushMenu.addItem (148, tr ("Press USER on Push to switch between your DAW and livelooper"), false);
    }

    // ---- v1.5: recent sets, Link, start settings, updates
    juce::PopupMenu recentMenu;
    const auto recent = appsettings::getRecentSets();
    for (int i = 0; i < recent.size(); ++i)
        recentMenu.addItem (300 + i, juce::File (recent[i]).getFileNameWithoutExtension() + "   (" + juce::File (recent[i]).getParentDirectory().getFileName() + ")");
    if (recent.isEmpty())
        recentMenu.addItem (298, tr ("No sets used yet"), false);
    recentMenu.addSeparator();
    recentMenu.addItem (299, tr ("Clear list"), ! recent.isEmpty());

    auto* linkOn = dynamic_cast<juce::AudioParameterBool*> (st.getParameter (gLinkOn));
    auto* linkStartStop = dynamic_cast<juce::AudioParameterBool*> (st.getParameter (gLinkStartStop));
    juce::PopupMenu linkMenu;
    linkMenu.addItem (400, tr ("Ableton Link (tempo and beat with other apps)"), proc.isLinkAvailable(), linkOn->get());
    linkMenu.addItem (401, tr ("Sync start / stop with Link"), proc.isLinkAvailable(), linkStartStop->get());
    linkMenu.addSeparator();
    linkMenu.addItem (409, proc.getLinkStatus(), false);
    if (proc.wrapperType != juce::AudioProcessor::wrapperType_Standalone)
        linkMenu.addItem (408, tr ("In a DAW, HOST SYNC has priority - switch it off to follow Link"), false);

    juce::PopupMenu startMenu;
    const auto mode = appsettings::getStartupMode();
    startMenu.addItem (410, tr ("Start with the last used settings"), true, mode == appsettings::StartupMode::LastSettings);
    startMenu.addItem (411, tr ("Start with the default settings"), true, mode == appsettings::StartupMode::Defaults);
    startMenu.addSeparator();
    startMenu.addItem (412, tr ("Reset all settings to the defaults now (keeps the loops)"));

    juce::PopupMenu updateMenu;
    updateMenu.addItem (420, tr ("Check for updates now ..."));
    updateMenu.addItem (421, tr ("Check automatically on start"), true, appsettings::getCheckUpdatesOnStart());
    updateMenu.addItem (422, tr ("Install updates automatically"), true, appsettings::getAutoInstallUpdates());
    updateMenu.addSeparator();
    updateMenu.addItem (429, tr (proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone
                                     ? "App: replaced when you quit livelooper"
                                     : "Plug-in: downloaded and prepared, you install it with your DAW closed"), false);

    juce::PopupMenu menu;
    menu.addSectionHeader (tr ("livelooper setup"));
    menu.addItem (8, tr ("Save set ..."));
    menu.addItem (9, tr ("Load set ..."));
    menu.addSubMenu (tr ("Last used sets"), recentMenu);
    menu.addItem (10, tr ("Export stems (one WAV per track) ..."));
    menu.addSeparator();
    addFeatures16Menu (menu);      // v1.6
    menu.addSubMenu (tr ("Tracks"), tracksMenu);
    menu.addSubMenu (tr ("Capture length"), captureMenu);
    menu.addSubMenu (tr ("Auto record"), autoRecMenu);
    menu.addSubMenu (tr ("Ducking source"), duckMenu);
    if (proc.hasCueOutput())
        menu.addSubMenu (tr ("Direct track outputs"), directMenu);
    if (proc.isPushAvailable())
        menu.addSubMenu (tr ("Ableton Push"), pushMenu);
    menu.addSeparator();
    menu.addSubMenu (tr ("Record action"), rec);
    menu.addItem (3, tr ("Follow host transport (start / stop with the host)"), true, follow->get());
    menu.addSubMenu (tr ("Maximum loop length"), memory);
    menu.addItem (4, tr ("Save loops with the project"), true, proc.getSaveLoopsWithProject());
    menu.addSubMenu (tr ("Metronome sound"), click);
    menu.addSubMenu (tr ("Rhythm groove"), grooveMenu);
    menu.addSubMenu (tr ("Rhythm fills"), fillsMenu);
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
    menu.addSubMenu (tr ("Ableton Link"), linkMenu);
    menu.addSubMenu (tr ("Settings on start"), startMenu);
    menu.addSubMenu (tr ("Updates"), updateMenu);
    menu.addSubMenu ("Language / Sprache", language);
    menu.addSeparator();
    menu.addItem (430, tr ("Help ..."));
    menu.addItem (431, tr ("About livelooper ..."));

    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&setupBtn),
                        [safe, recAction, follow, lengths, clickSound, captureBars, autoRec, autoRecThresh, duckSource, thresholds,
                         recent, linkOn, linkStartStop, groove, fills] (int r) {
        if (safe == nullptr || r == 0)
            return;
        if (safe->handleFeatures16Menu (r))      // v1.6
            return;
        auto setParam = [] (juce::RangedAudioParameter* p, float plain) {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (plain));
            p->endChangeGesture();
        };
        // ---- v1.5
        if (r >= 500 && r < 500 + groove->choices.size())
        {
            setParam (groove, (float) (r - 500));
            safe->showStatus (tr ("GROOVE: %1", i18n::upper (tr (grooveChoices()[r - 500]))));
            return;
        }
        if (r >= 520 && r < 520 + fills->choices.size())
        {
            setParam (fills, (float) (r - 520));
            safe->showStatus (tr ("FILLS: %1", i18n::upper (tr (fillChoices()[r - 520]))));
            return;
        }
        if (r >= 300 && r < 300 + recent.size())
        {
            safe->loadSetFile (juce::File (recent[r - 300]));
            return;
        }
        switch (r)
        {
            case 299: appsettings::clearRecentSets(); return;
            case 400:
                setParam (linkOn, linkOn->get() ? 0.0f : 1.0f);
                safe->showStatus (tr (linkOn->get() ? "LINK ON" : "LINK OFF"));
                return;
            case 401: setParam (linkStartStop, linkStartStop->get() ? 0.0f : 1.0f); return;
            case 410: appsettings::setStartupMode (appsettings::StartupMode::LastSettings); return;
            case 411: appsettings::setStartupMode (appsettings::StartupMode::Defaults); return;
            case 412:
                safe->proc.applyDefaultSettings();
                safe->showStatus (tr ("SETTINGS RESET TO DEFAULTS"));
                return;
            case 420: safe->checkForUpdates (true); return;
            case 421: appsettings::setCheckUpdatesOnStart (! appsettings::getCheckUpdatesOnStart()); return;
            case 422: appsettings::setAutoInstallUpdates (! appsettings::getAutoInstallUpdates()); return;
            case 430: safe->showHelp(); return;
            case 431: safe->showAbout(); return;
            default: break;
        }
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
        if (r >= 110 && r < 110 + kNumCaptureChoices)
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
        if (r == 140)
        {
            safe->proc.setPushEnabled (! safe->proc.isPushEnabled());
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
            safe->showStatus (tr ("METRONOME: %1", i18n::upper (tr (clickSoundChoices()[r - 40]))));
        }
        else if (r >= 50 && r <= 50 + LiveLooperProcessor::kNumCuePairs)
        {
            safe->proc.setCuePair (r - 50);
            safe->showStatus (tr ("CUE: %1", i18n::upper (LiveLooperProcessor::getCuePairName (r - 50))));
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
        if (ok)
            appsettings::addRecentSet (file);
        safe->showStatus (ok ? tr ("SET SAVED: %1", i18n::upper (file.getFileNameWithoutExtension())) : tr ("SET COULD NOT BE SAVED"));
    });
}

void LiveLooperEditor::loadSet()
{
    const auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    chooser = std::make_unique<juce::FileChooser> (tr ("Load set"), dir, "*.llset;*.livelooper");
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
    if (ok)
        appsettings::addRecentSet (file);
    showStatus (ok ? tr ("SET LOADED: %1", i18n::upper (file.getFileNameWithoutExtension())) : tr ("LOAD FAILED: %1", error));
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
    // a loop dragged out of livelooper itself is not imported again
    for (const auto& f : files)
        if (juce::File (f).isAChildOf (LiveLooperProcessor::getDragExportFolder()))
            return false;
    for (const auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;ogg;mp3;llset;livelooper"))
            return true;
    return false;
}

void LiveLooperEditor::filesDropped (const juce::StringArray& files, int x, int y)
{
    if (juce::File (files[0]).hasFileExtension ("llset;livelooper"))
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
//============================================================================== v1.5 help, about, start settings, updates
void LiveLooperEditor::showHelp()
{
    HelpWindow::show (*this);
}

void LiveLooperEditor::showAbout()
{
    AboutWindow::show (*this, proc, false);
}

void LiveLooperEditor::askStartupSettingsOnce()
{
    if (appsettings::getStartupMode() != appsettings::StartupMode::Ask
        || proc.wrapperType == juce::AudioProcessor::wrapperType_Undefined)     // tests / screenshot tool
        return;
    auto opts = juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::QuestionIcon)
                    .withTitle (tr ("Welcome to livelooper"))
                    .withMessage (tr ("How should livelooper start from now on?\n\n"
                                      "Last settings: every start (and every new plug-in without a saved project) uses the settings you "
                                      "used last - effects, rhythm, metronome, MIDI and keyboard mapping, tracks, Push.\n"
                                      "Default settings: always start fresh.\n\nYou can change this any time in SETUP > Settings on start."))
                    .withButton (tr ("Use last settings")).withButton (tr ("Use default settings")).withAssociatedComponent (this);
    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    juce::AlertWindow::showAsync (opts, [safe] (int r) {
        const bool last = r != 0;      // 1 = first button; closing the window keeps the last settings
        appsettings::setStartupMode (last ? appsettings::StartupMode::LastSettings : appsettings::StartupMode::Defaults);
        if (safe != nullptr && last)
            safe->proc.applyStartupSettingsIfFresh();
    });
}

void LiveLooperEditor::checkForUpdates (bool userInitiated)
{
    const bool standalone = proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
    if (userInitiated)
    {
        AboutWindow::show (*this, proc, true);
        return;
    }
    // automatic check (once per session): quiet unless there is something new
    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    UpdateManager::getInstance()->checkOnStart (standalone, [safe] (const juce::String& message) {
        if (safe != nullptr)
            safe->showStatus (message);
    });
}
//============================================================================== v1.6 rhythm (branch v1.6-rhythm)
void LiveLooperEditor::layoutRhythm16 (int dx)
{
    // header buttons: BEATS + FILL in the rhythm panel, DRUMS in the beat FX and beat repeat panels
    // (DRUMS buttons: in the BEAT FX and REPEAT tabs of the FX panel, see layoutFxPanel)
    const auto rh = layout::rhythm.translated (dx, 0);
    beatsBtn.setBounds (rh.getRight() - 14 - 72, rh.getY() + 11, 72, 24);
    fillBtn->setBounds (rh.getRight() - 14 - 72 - 6 - 60, rh.getY() + 11, 60, 24);
    if (beatsPage != nullptr)
        beatsPage->setBounds (juce::Rectangle<int> (12, layout::tracks.getY(), surfaceWidth() - 24,
                                                    layout::fxPanel.getBottom() - layout::tracks.getY()));
}

void LiveLooperEditor::showBeatsPage (bool show)
{
    if (beatsPage == nullptr)
        return;
    beatsPage->setVisible (show);
    if (show)
        beatsPage->toFront (false);
    overlay.toFront (false);
    beatsBtn.setToggleState (show, juce::dontSendNotification);
}

void LiveLooperEditor::refreshPatternNames()
{
    // the pattern / kit selectors show the names of the user patterns and the sound-pack kits
    const int v = proc.getUserPatterns().getVersion() * 31 + SoundPacks::get().getVersion() * 7 + (i18n::isGerman() ? 1 : 0);
    if (v == lastPatternNamesVersion && blinkCounter % 15 != 0)
        return;
    lastPatternNamesVersion = v;
    const int presets = RhythmGenerator::getNumPatterns();
    for (int i = presets; i < rhythmPattern.getNumItems(); ++i)
    {
        const auto name = proc.getPatternDisplayName (i);
        if (rhythmPattern.getItemText (i) != name)
            rhythmPattern.changeItemText (i + 1, name);
    }
    for (int k = RhythmGenerator::getNumKits(); k < rhythmKit.getNumItems(); ++k)
    {
        const auto name = proc.getKitDisplayName (k);
        if (rhythmKit.getItemText (k) != name)
            rhythmKit.changeItemText (k + 1, name);
    }
}
} // namespace livelooper
