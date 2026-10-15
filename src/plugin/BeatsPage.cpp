#include "BeatsPage.h"
#include "SoundPacks.h"

namespace livelooper
{
using namespace params;
using i18n::tr;

BeatPattern BeatsPage::clipboard;
bool BeatsPage::clipboardFull = false;

namespace
{
    // brand gradient #FF7A18 -> #FF2D6F -> #B14BFF
    juce::Colour brand (float t)
    {
        const juce::Colour a (0xffff7a18), b (0xffff2d6f), c (0xffb14bff);
        t = juce::jlimit (0.0f, 1.0f, t);
        return t < 0.5f ? a.interpolatedWith (b, t * 2.0f) : b.interpolatedWith (c, (t - 0.5f) * 2.0f);
    }
}

BeatsPage::BeatsPage (LiveLooperProcessor& p) : proc (p)
{
    auto& st = proc.getState();
    i18n::addListener (this);

    addAndMakeVisible (backBtn);
    backBtn.onClick = [this] { if (onClose) onClose(); };
    backBtn.setTooltip ("Back to the looper");

    addAndMakeVisible (slotBox);
    slotBox.onChange = [this] { if (slotBox.getSelectedId() > 0) setEditSlot (slotBox.getSelectedId() - 1); };
    slotBox.setTooltip ("The user pattern slot you edit (User 1 - 16 in the pattern selector)");
    addAndMakeVisible (nameEdit);
    nameEdit.setFont (LooperLookAndFeel::font (14.0f, true));
    nameEdit.setColour (juce::TextEditor::backgroundColourId, colours::lcdBack);
    nameEdit.setColour (juce::TextEditor::textColourId, colours::lcdText);
    nameEdit.setColour (juce::TextEditor::outlineColourId, colours::sectionEdge);
    nameEdit.setTextToShowWhenEmpty (tr ("Pattern name"), colours::textDim);
    nameEdit.onTextChange = [this] { proc.setUserPatternName (getEditSlot(), nameEdit.getText()); };
    addAndMakeVisible (playBtn);
    playBtn.onClick = [this] { proc.selectUserPattern (getEditSlot()); };
    playBtn.setTooltip ("Play this pattern (switches on the next beat / bar, see PATTERN SWITCH)");

    kitBox.setEnglishItems (kitChoices());
    kitAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, gRhythmKit, kitBox);
    setParamId (kitBox, gRhythmKit);
    addAndMakeVisible (kitBox);
    stepsBox.addItem ("16", 1);
    stepsBox.addItem ("32", 2);
    stepsBox.onChange = [this] { setResolution (stepsBox.getSelectedId() == 2 ? 32 : 16, proc.getUserPatterns().getBars (getEditSlot())); };
    stepsBox.setTooltip ("Steps per bar: 16th or 32nd notes");
    addAndMakeVisible (stepsBox);
    for (int b = 1; b <= BeatPattern::kMaxBars; ++b)
        barsBox.addItem (b == 1 ? tr ("1 bar") : tr ("%1 bars", juce::String (b)), b);
    barsBox.onChange = [this] { setResolution (proc.getUserPatterns().getStepsPerBar (getEditSlot()), juce::jmax (1, barsBox.getSelectedId())); };
    barsBox.setTooltip ("Length of the pattern");
    addAndMakeVisible (barsBox);
    switchBox.setEnglishItems (beatSwitchChoices());
    switchAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, gBeatSwitch, switchBox);
    setParamId (switchBox, gBeatSwitch);
    switchBox.setTooltip ("Pattern switch: when a new pattern starts playing");
    addAndMakeVisible (switchBox);

    for (auto* b : { &loadBtn, &saveBtn, &clearBtn, &copyBtn, &pasteBtn, &randomBtn, &importBtn, &packsBtn })
        addAndMakeVisible (*b);
    loadBtn.onClick = [this] { showLoadMenu(); };
    loadBtn.setTooltip ("LOAD: a saved beat from your library, a beat file, a groove or a sound-pack pattern");
    saveBtn.onClick = [this] { showSaveDialog(); };
    saveBtn.setTooltip ("SAVE: store this beat under a name in your beat library (Music/livelooper/Patterns) or as a file anywhere");
    clearBtn.onClick = [this] { clearPattern(); };
    copyBtn.onClick = [this] { copyPattern(); };
    pasteBtn.onClick = [this] { pastePattern(); };
    randomBtn.onClick = [this] { randomiseLight(); };
    randomBtn.setTooltip ("Light variation: a few ghost notes and velocity changes, the groove stays");
    importBtn.onClick = [this] { importPack(); };
    importBtn.setTooltip ("Import a sound pack (folder or .zip with livelooper-pack.json)");
    packsBtn.onClick = [this] { showPackMenu(); };

    for (int b = 0; b < BeatPattern::kMaxBars; ++b)
    {
        barBtns[(size_t) b] = std::make_unique<LLButton> (juce::String (b + 1), colours::yellow);
        barBtns[(size_t) b]->onClick = [this, b] { setBarPage (b); };
        barBtns[(size_t) b]->tooltipFn = [b] { return tr ("Show bar %1", juce::String (b + 1)); };
        addAndMakeVisible (*barBtns[(size_t) b]);
    }
    fillBtn = std::make_unique<MomentaryButton> (st, gRhythmFill, "FILL", colours::orange);
    fillBtn->setTooltip ("Hold for a fill; on release the groove comes back on the next downbeat");
    addAndMakeVisible (*fillBtn);

    for (int r = 0; r < BeatPattern::kRows; ++r)
    {
        auto& m = muteBtns[(size_t) r];
        m = std::make_unique<LLButton> ("M", colours::yellow);
        m->setClickingTogglesState (true);
        muteAtts[(size_t) r] = std::make_unique<APVTS::ButtonAttachment> (st, beatRowId (kBeatMute, r), *m);
        setParamId (*m, beatRowId (kBeatMute, r));
        addAndMakeVisible (*m);
        auto& s = soloBtns[(size_t) r];
        s = std::make_unique<LLButton> ("S", colours::cyan);
        s->setClickingTogglesState (true);
        soloAtts[(size_t) r] = std::make_unique<APVTS::ButtonAttachment> (st, beatRowId (kBeatSolo, r), *s);
        setParamId (*s, beatRowId (kBeatSolo, r));
        addAndMakeVisible (*s);
        volKnobs[(size_t) r] = std::make_unique<Knob> (st, beatRowId (kBeatVol, r), "Lvl", colours::orange);
        pitchKnobs[(size_t) r] = std::make_unique<Knob> (st, beatRowId (kBeatPitch, r), "Pitch", colours::learn);
        addAndMakeVisible (*volKnobs[(size_t) r]);
        addAndMakeVisible (*pitchKnobs[(size_t) r]);
        rowNameBtns[(size_t) r] = std::make_unique<RowNameButton> (*this, r);
        rowNameBtns[(size_t) r]->onClick = [this, r] { showRowSampleMenu (r); };
        addAndMakeVisible (*rowNameBtns[(size_t) r]);
    }

    rhythmOnBtn.setClickingTogglesState (true);
    rhythmOnAtt = std::make_unique<APVTS::ButtonAttachment> (st, gRhythmOn, rhythmOnBtn);
    setParamId (rhythmOnBtn, gRhythmOn);
    drumsBfxBtn.setClickingTogglesState (true);
    drumsBfxAtt = std::make_unique<APVTS::ButtonAttachment> (st, gDrumsBfx, drumsBfxBtn);
    setParamId (drumsBfxBtn, gDrumsBfx);
    drumsBfxBtn.setTooltip ("The drums go through the beat FX too (off: the drums stay dry)");
    drumsRepBtn.setClickingTogglesState (true);
    drumsRepAtt = std::make_unique<APVTS::ButtonAttachment> (st, gDrumsRepeat, drumsRepBtn);
    setParamId (drumsRepBtn, gDrumsRepeat);
    drumsRepBtn.setTooltip ("The drums go through beat repeat / stutter too (off: the drums stay dry)");
    grooveBox.setEnglishItems (grooveChoices());
    grooveAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, gRhythmGroove, grooveBox);
    setParamId (grooveBox, gRhythmGroove);
    grooveBox.setTooltip ("Groove: micro-timing and accents");
    fillsBox.setEnglishItems (fillChoices());
    fillsAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, gRhythmFills, fillsBox);
    setParamId (fillsBox, gRhythmFills);
    fillsBox.setTooltip ("Automatic fills");
    swingKnob = std::make_unique<Knob> (st, gRhythmSwing, "Swing", colours::orange);
    feelKnob = std::make_unique<Knob> (st, gRhythmHuman, "Feel", colours::orange);
    variationKnob = std::make_unique<Knob> (st, gRhythmVariation, "Variation", colours::learn);
    variationKnob->slider.setTooltip ("Variation: other samples, layers, pitch and decay on every hit - no machine gun");
    levelKnob = std::make_unique<Knob> (st, gRhythmLevel, "Level", colours::orange);
    for (juce::Component* c : { (juce::Component*) &rhythmOnBtn, (juce::Component*) &drumsBfxBtn, (juce::Component*) &drumsRepBtn,
                                (juce::Component*) &grooveBox, (juce::Component*) &fillsBox, (juce::Component*) swingKnob.get(),
                                (juce::Component*) feelKnob.get(), (juce::Component*) variationKnob.get(), (juce::Component*) levelKnob.get() })
        addAndMakeVisible (c);

    addAndMakeVisible (grid);
    refreshTexts();
    startTimerHz (30);
}

BeatsPage::~BeatsPage()
{
    i18n::removeListener (this);
}

void BeatsPage::languageChanged()
{
    barsBox.clear (juce::dontSendNotification);
    for (int b = 1; b <= BeatPattern::kMaxBars; ++b)
        barsBox.addItem (b == 1 ? tr ("1 bar") : tr ("%1 bars", juce::String (b)), b);
    lastVersion = -1;
    lastKit = -1;
    refreshTexts();
}

void BeatsPage::refreshTexts()
{
    const int slot = getEditSlot();
    slotBox.clear (juce::dontSendNotification);
    for (int s = 0; s < BeatPatternBank::kNumSlots; ++s)
        slotBox.addItem (proc.getPatternDisplayName (RhythmGenerator::getNumPatterns() + s), s + 1);
    slotBox.setSelectedId (slot + 1, juce::dontSendNotification);
    if (! nameEdit.hasKeyboardFocus (true))
        nameEdit.setText (proc.getUserPatternName (slot), juce::dontSendNotification);
    auto& bank = proc.getUserPatterns();
    stepsBox.setSelectedId (bank.getStepsPerBar (slot) >= 32 ? 2 : 1, juce::dontSendNotification);
    barsBox.setSelectedId (bank.getBars (slot), juce::dontSendNotification);
    for (int k = 0; k < kitBox.getNumItems(); ++k)
        kitBox.changeItemText (k + 1, proc.getKitDisplayName (k));
    barPage = juce::jlimit (0, bank.getBars (slot) - 1, barPage);
    for (int b = 0; b < BeatPattern::kMaxBars; ++b)
    {
        barBtns[(size_t) b]->setEnabled (b < bank.getBars (slot));
        barBtns[(size_t) b]->setToggleState (b == barPage, juce::dontSendNotification);
    }
    repaint();
}

void BeatsPage::visibilityChanged()
{
    if (! isVisible())
        return;
    // the page follows the pattern that plays; an empty slot starts from the playing groove
    const int current = proc.getCurrentPattern();
    if (RhythmGenerator::isUserPattern (current))
        proc.setBeatsEditSlot (RhythmGenerator::userSlotOf (current));
    else if (current > 0 && proc.getUserPatterns().isEmpty (getEditSlot()))
        proc.storeUserPattern (getEditSlot(), RhythmGenerator::presetToPattern (current), tr (RhythmGenerator::getPatternName (current)));
    refreshTexts();
}

void BeatsPage::setEditSlot (int slot)
{
    proc.setBeatsEditSlot (slot);
    barPage = 0;
    refreshTexts();
}

void BeatsPage::ensurePlaying()
{
    const int idx = RhythmGenerator::getNumPatterns() + getEditSlot();
    if (proc.getCurrentPattern() != idx)
        proc.selectUserPattern (getEditSlot());
}

int BeatsPage::visibleSteps() const
{
    return proc.getUserPatterns().getStepsPerBar (getEditSlot());
}

void BeatsPage::toggleStep (int row, int step, juce::ModifierKeys mods)
{
    auto& bank = proc.getUserPatterns();
    const int slot = getEditSlot();
    const int v = bank.getCell (slot, row, step);
    const int nv = v > 0 ? 0 : (mods.isShiftDown() ? 127 : mods.isAltDown() ? 40 : 96);
    bank.setCell (slot, row, step, nv);
    ensurePlaying();
    grid.repaint();
}

void BeatsPage::cycleVelocity (int row, int step)
{
    auto& bank = proc.getUserPatterns();
    const int slot = getEditSlot();
    bank.setCell (slot, row, step, nextVelocity (bank.getCell (slot, row, step)));
    ensurePlaying();
    grid.repaint();
}

void BeatsPage::clearPattern()
{
    auto p = proc.getUserPatterns().load (getEditSlot());
    for (auto& r : p.cells) r.fill (0);
    proc.storeUserPattern (getEditSlot(), p, proc.getUserPatternName (getEditSlot()));
    refreshTexts();
}

void BeatsPage::copyPattern()
{
    clipboard = proc.getUserPatterns().load (getEditSlot());
    clipboard.name = proc.getUserPatternName (getEditSlot()).toStdString();
    clipboardFull = true;
    status (tr ("Pattern copied"));
}

void BeatsPage::pastePattern()
{
    if (! clipboardFull)
        return;
    proc.storeUserPattern (getEditSlot(), clipboard, juce::String (clipboard.name));
    ensurePlaying();
    refreshTexts();
}

void BeatsPage::randomiseLight (juce::int64 seed)
{
    // a light touch: ghost notes on hats / snare off-steps, velocity nuances, the kick may move one pickup
    juce::Random rnd (seed);
    auto p = proc.getUserPatterns().load (getEditSlot());
    const int n = p.numSteps(), sub = p.stepsPerBar / 16;
    for (int r = 0; r < BeatPattern::kRows; ++r)
    {
        bool rowUsed = false;
        for (int s = 0; s < n; ++s)
            rowUsed = rowUsed || p.get (r, s) > 0;
        if (! rowUsed)
            continue;
        for (int s = 0; s < n; ++s)
        {
            const int v = p.get (r, s);
            if (v > 0)
                p.set (r, s, juce::jlimit (20, 127, v + rnd.nextInt (juce::Range<int> (-12, 13))));
            else if ((r == RhythmGenerator::Snare || r == RhythmGenerator::HatClosed || r == RhythmGenerator::Shaker)
                     && (s / sub) % 2 == 1 && rnd.nextFloat() < 0.12f)
                p.set (r, s, 36 + rnd.nextInt (12));
        }
    }
    // kick: maybe one extra pickup 16th before a beat
    if (rnd.nextFloat() < 0.5f)
    {
        const int beat = 1 + rnd.nextInt (juce::jmax (1, n / (4 * sub) - 1));
        const int s = beat * 4 * sub - sub;
        if (s > 0 && s < n && p.get (RhythmGenerator::Kick, s) == 0)
            p.set (RhythmGenerator::Kick, s, 70);
    }
    proc.storeUserPattern (getEditSlot(), p, proc.getUserPatternName (getEditSlot()));
    ensurePlaying();
    refreshTexts();
}

void BeatsPage::setResolution (int stepsPerBar, int bars)
{
    auto p = proc.getUserPatterns().load (getEditSlot());
    if (p.stepsPerBar == stepsPerBar && p.bars == bars)
        return;
    p.resize (stepsPerBar, bars);
    proc.storeUserPattern (getEditSlot(), p, proc.getUserPatternName (getEditSlot()));
    refreshTexts();
}

void BeatsPage::loadPreset (int presetIndex)
{
    loadPattern (RhythmGenerator::presetToPattern (presetIndex), tr (RhythmGenerator::getPatternName (presetIndex)));
}

void BeatsPage::loadPattern (const BeatPattern& p, const juce::String& name)
{
    const int slot = getLoadTargetSlot();
    SoundPacks::NamedPattern beat;
    beat.pattern = p;
    beat.name = name;
    proc.loadBeat (beat, slot);            // no saved swing / mixer: those stay as they are
    barPage = 0;
    status (tr ("Beat \"%1\" loaded into User %2", name, juce::String (slot + 1)));
    refreshTexts();
}

juce::File BeatsPage::saveToLibrary()
{
    return saveBeat (proc.getUserPatternName (getEditSlot()));
}

juce::File BeatsPage::saveBeat (const juce::String& name, const juce::File& target)
{
    const auto f = proc.saveBeat (getEditSlot(), name, target);
    status (f != juce::File() ? tr ("Beat saved: %1", f.getFileNameWithoutExtension()) : tr ("Could not save the beat"));
    refreshTexts();
    return f;
}

void BeatsPage::showSaveDialog()
{
    auto name = proc.getUserPatternName (getEditSlot());
    if (name.isEmpty())
        name = tr ("User %1", juce::String (getEditSlot() + 1));
    saveDialog = std::make_unique<juce::AlertWindow> (tr ("Save beat"),
                                                      tr ("Name of the beat - it is saved in your beat library (Music/livelooper/Patterns) "
                                                          "with steps, velocities, length, swing and the row mixer."),
                                                      juce::MessageBoxIconType::NoIcon, this);
    saveDialog->addTextEditor ("name", name, tr ("Name"));
    saveDialog->addButton (tr ("Save"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton (tr ("Save as file..."), 2);
    saveDialog->addButton (tr ("Cancel"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int r) {
        if (saveDialog == nullptr)
            return;
        const auto chosen = saveDialog->getTextEditorContents ("name").trim();
        saveDialog.reset();
        if (r == 2)
            showSaveAsChooser (chosen);
        else if (r == 1 && chosen.isNotEmpty())
        {
            const auto existing = SoundPacks::get().getLibraryFileFor (chosen);
            if (! existing.existsAsFile())
            {
                saveBeat (chosen);
                return;
            }
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, tr ("Save beat"),
                                                tr ("A beat named \"%1\" is already in your library. Replace it?", chosen),
                                                tr ("Replace"), tr ("Cancel"), this,
                                                juce::ModalCallbackFunction::create ([this, chosen] (int ok) { if (ok != 0) saveBeat (chosen); }));
        }
    }), false);
}

void BeatsPage::showSaveAsChooser (const juce::String& name)
{
    const auto start = SoundPacks::get().getLibraryFileFor (name.isNotEmpty() ? name : juce::String ("Beat"));
    chooser = std::make_unique<juce::FileChooser> (tr ("Save beat as file"), start, juce::String ("*") + SoundPacks::kPatternExtension);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, name] (const juce::FileChooser& fc) {
                              const auto f = fc.getResult();
                              if (f != juce::File())
                                  saveBeat (name.isNotEmpty() ? name : f.getFileNameWithoutExtension(), f);
                          });
}

int BeatsPage::getLoadTargetSlot() const
{
    return proc.slotForLoading (replaceOnLoad);
}

int BeatsPage::loadLibraryBeat (int index)
{
    const auto lib = SoundPacks::get().getUserPatterns();
    if (! juce::isPositiveAndBelow (index, (int) lib.size()))
        return -1;
    const int slot = getLoadTargetSlot();
    proc.loadBeat (lib[(size_t) index], slot);
    barPage = 0;
    status (tr ("Beat \"%1\" loaded into User %2", lib[(size_t) index].name, juce::String (slot + 1)));
    refreshTexts();
    return slot;
}

bool BeatsPage::openBeatFile (const juce::File& file)
{
    const int slot = getLoadTargetSlot();
    juce::String error;
    if (! proc.loadBeatFile (file, slot, error))
    {
        status (error);
        return false;
    }
    barPage = 0;
    status (tr ("Beat \"%1\" loaded into User %2", proc.getUserPatternName (slot), juce::String (slot + 1)));
    refreshTexts();
    return true;
}

void BeatsPage::setBarPage (int b)
{
    barPage = juce::jlimit (0, proc.getUserPatterns().getBars (getEditSlot()) - 1, b);
    refreshTexts();
}

void BeatsPage::showLoadMenu()
{
    juce::PopupMenu menu, presets, packs, slots;
    const int target = getLoadTargetSlot();
    menu.addSectionHeader (tr ("Load into User %1", juce::String (target + 1))
                           + (proc.getUserPatterns().isEmpty (target) ? " " + tr ("(free)") : juce::String()));
    // the library first: the beats you saved
    const auto user = SoundPacks::get().getUserPatterns();
    if (user.empty())
        menu.addItem (-1, tr ("No saved beats yet - SAVE stores the beat you build here"), false);
    for (int i = 0; i < (int) user.size(); ++i)
        menu.addItem (2000 + i, user[(size_t) i].name);
    menu.addItem (5, tr ("Open file..."));
    menu.addSeparator();
    for (int i = 1; i < RhythmGenerator::getNumPatterns(); ++i)
        presets.addItem (1000 + i, tr (RhythmGenerator::getPatternName (i)));
    const auto packPatterns = SoundPacks::get().getPackPatterns();
    for (int i = 0; i < (int) packPatterns.size(); ++i)
        packs.addItem (3000 + i, packPatterns[(size_t) i].source + " / " + packPatterns[(size_t) i].name);
    menu.addSubMenu (tr ("Grooves"), presets);
    menu.addSubMenu (tr ("Sound packs"), packs, ! packPatterns.empty());
    for (int s = 0; s < BeatPatternBank::kNumSlots; ++s)
        if (s != getEditSlot())
            slots.addItem (4000 + s, proc.getPatternDisplayName (RhythmGenerator::getNumPatterns() + s), ! proc.getUserPatterns().isEmpty (s));
    menu.addSubMenu (tr ("Other slot of this set"), slots);
    menu.addSeparator();
    menu.addItem (6, tr ("Replace this slot (otherwise: a free slot)"), true, replaceOnLoad);
    menu.addItem (7, tr ("Show beat library folder"));
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&loadBtn),
                        [this, user, packPatterns] (int r) {
                            if (r >= 4000)
                                loadPattern (proc.getUserPatterns().load (r - 4000), proc.getUserPatternName (r - 4000));
                            else if (r >= 3000)
                                loadPattern (packPatterns[(size_t) (r - 3000)].pattern, packPatterns[(size_t) (r - 3000)].name);
                            else if (r >= 2000)
                            {
                                const int slot = getLoadTargetSlot();
                                proc.loadBeat (user[(size_t) (r - 2000)], slot);
                                barPage = 0;
                                status (tr ("Beat \"%1\" loaded into User %2", user[(size_t) (r - 2000)].name, juce::String (slot + 1)));
                                refreshTexts();
                            }
                            else if (r >= 1000)
                                loadPreset (r - 1000);
                            else if (r == 5)
                            {
                                chooser = std::make_unique<juce::FileChooser> (tr ("Open beat file"), SoundPacks::get().getPatternFolder(),
                                                                               juce::String ("*") + SoundPacks::kPatternExtension);
                                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                                      [this] (const juce::FileChooser& fc) {
                                                          if (fc.getResult() != juce::File())
                                                              openBeatFile (fc.getResult());
                                                      });
                            }
                            else if (r == 6)
                                replaceOnLoad = ! replaceOnLoad;
                            else if (r == 7)
                            {
                                auto f = SoundPacks::get().getPatternFolder();
                                f.createDirectory();
                                f.revealToUser();
                            }
                        });
}

void BeatsPage::showPackMenu()
{
    juce::PopupMenu menu;
    const auto packs = SoundPacks::get().getPacks();
    if (packs.empty())
        menu.addItem (-1, tr ("No sound packs installed"), false);
    for (const auto& p : packs)
        menu.addItem (-1, p.name + (p.author.isNotEmpty() ? "  (" + p.author + ")" : juce::String()) + "  - "
                              + tr ("%1 kits, %2 patterns", juce::String (p.kitNames.size()), juce::String (p.numPatterns)), false);
    menu.addSeparator();
    menu.addItem (1, tr ("Import pack..."));
    menu.addItem (2, tr ("Rescan packs folder"));
    menu.addItem (3, tr ("Show packs folder"));
    menu.addItem (4, tr ("Show user patterns folder"));
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&packsBtn), [this] (int r) {
        if (r == 1) importPack();
        else if (r == 2) { proc.refreshSoundPacks(); refreshTexts(); status (tr ("Sound packs rescanned")); }
        else if (r == 3) { auto f = SoundPacks::get().getPackFolder(); f.createDirectory(); f.revealToUser(); }
        else if (r == 4) { auto f = SoundPacks::get().getPatternFolder(); f.createDirectory(); f.revealToUser(); }
    });
}

void BeatsPage::importPack()
{
    chooser = std::make_unique<juce::FileChooser> (tr ("Import sound pack (folder or .zip)"),
                                                   juce::File::getSpecialLocation (juce::File::userHomeDirectory), "*.zip");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectDirectories,
                          [this] (const juce::FileChooser& fc) {
                              const auto f = fc.getResult();
                              if (f == juce::File())
                                  return;
                              juce::String error;
                              if (proc.importSoundPack (f, error))
                                  status (tr ("Sound pack imported: %1", f.getFileNameWithoutExtension()));
                              else
                                  status (tr ("Import failed: %1", error));
                              refreshTexts();
                          });
}

int BeatsPage::playingCell() const
{
    const int slot = getEditSlot();
    if (proc.getEngine().getRhythm().getActivePattern() != RhythmGenerator::getNumPatterns() + slot)
        return -1;
    const auto step = proc.getEngine().getRhythm().getCurrentStep();
    if (step < 0)
        return -1;
    auto& bank = proc.getUserPatterns();
    const long long len16 = 16LL * bank.getBars (slot);
    const int s16 = (int) (((step % len16) + len16) % len16);
    return s16 * (bank.getStepsPerBar (slot) / 16);
}

void BeatsPage::timerCallback()
{
    if (! isShowing())
        return;
    const int v = proc.getUserPatterns().getVersion();
    const int kit = proc.getCurrentKit(), pat = proc.getCurrentPattern();
    if (v != lastVersion || kit != lastKit || pat != lastPattern || SoundPacks::get().getVersion() != lastPackVersion)
    {
        lastVersion = v;
        lastKit = kit;
        lastPattern = pat;
        lastPackVersion = SoundPacks::get().getVersion();
        refreshTexts();
    }
    if (proc.getRowSampleVersion() != lastRowSampleVersion)
    {
        lastRowSampleVersion = proc.getRowSampleVersion();
        for (auto& b : rowNameBtns)
            b->repaint();
    }
    const int cell = playingCell();
    if (cell != lastCell)
    {
        lastCell = cell;
        grid.repaint();
        repaint (topBar);
    }
    const bool playingThis = proc.getEngine().getRhythm().getActivePattern() == RhythmGenerator::getNumPatterns() + getEditSlot();
    playBtn.setToggleState (playingThis, juce::dontSendNotification);
}

void BeatsPage::resized()
{
    auto r = getLocalBounds().reduced (10, 6);
    topBar = r.removeFromTop (36);
    {
        auto t = topBar;
        backBtn.setBounds (t.removeFromLeft (96).reduced (0, 3));
        t.removeFromLeft (10);
        slotBox.setBounds (t.removeFromLeft (220).reduced (0, 5));
        t.removeFromLeft (6);
        nameEdit.setBounds (t.removeFromLeft (170).reduced (0, 5));
        t.removeFromLeft (6);
        playBtn.setBounds (t.removeFromLeft (72).reduced (0, 3));
        switchBox.setBounds (t.removeFromRight (130).reduced (0, 5));
        t.removeFromRight (8);
        barsBox.setBounds (t.removeFromRight (88).reduced (0, 5));
        t.removeFromRight (6);
        stepsBox.setBounds (t.removeFromRight (64).reduced (0, 5));
        t.removeFromRight (8);
        kitBox.setBounds (t.removeFromRight (juce::jmin (210, t.getWidth() - 40)).reduced (0, 5));
    }
    r.removeFromTop (6);
    toolBar = r.removeFromTop (32);
    {
        auto t = toolBar;
        for (auto* b : { &loadBtn, &saveBtn, &clearBtn, &copyBtn, &pasteBtn, &randomBtn })
        {
            b->setBounds (t.removeFromLeft (b == &saveBtn || b == &loadBtn ? 104 : 76).reduced (0, 1));   // SAVE / LOAD: the big ones
            t.removeFromLeft (6);
        }
        t.removeFromLeft (6);
        importBtn.setBounds (t.removeFromLeft (112).reduced (0, 1));
        t.removeFromLeft (6);
        packsBtn.setBounds (t.removeFromLeft (70).reduced (0, 1));
        fillBtn->setBounds (t.removeFromRight (96).reduced (0, 1));
        t.removeFromRight (14);
        for (int b = BeatPattern::kMaxBars - 1; b >= 0; --b)
        {
            barBtns[(size_t) b]->setBounds (t.removeFromRight (34).reduced (0, 1));
            t.removeFromRight (4);
        }
    }
    r.removeFromTop (8);
    bottomBar = r.removeFromBottom (84);
    {
        auto b = bottomBar.reduced (0, 4);
        auto col = b.removeFromLeft (130);
        rhythmOnBtn.setBounds (col.removeFromTop (34).reduced (0, 1));
        b.removeFromLeft (12);
        captionArea = b.removeFromLeft (70);
        col = b.removeFromLeft (170);
        grooveBox.setBounds (col.removeFromTop (34).reduced (0, 3));
        col.removeFromTop (6);
        fillsBox.setBounds (col.removeFromTop (30).reduced (0, 2));
        b.removeFromLeft (10);
        auto right = b.removeFromRight (190);
        drumsBfxBtn.setBounds (right.removeFromTop (34).reduced (0, 1));
        right.removeFromTop (6);
        drumsRepBtn.setBounds (right.removeFromTop (34).reduced (0, 1));
        b.removeFromRight (10);
        const int kw = juce::jmin (90, b.getWidth() / 4);
        for (auto* k : { swingKnob.get(), feelKnob.get(), variationKnob.get(), levelKnob.get() })
            k->setBounds (b.removeFromLeft (kw).reduced (2));
    }
    r.removeFromBottom (6);
    rowHeader = r.removeFromLeft (262);
    grid.setBounds (r);
    const float rowH = (float) rowHeader.getHeight() / (float) BeatPattern::kRows;
    for (int row = 0; row < BeatPattern::kRows; ++row)
    {
        auto rr = juce::Rectangle<int> (rowHeader.getX(), rowHeader.getY() + juce::roundToInt (rowH * (float) row),
                                        rowHeader.getWidth(), juce::roundToInt (rowH));
        rowNameBtns[(size_t) row]->setBounds (rr.removeFromLeft (104).reduced (0, 1));      // name + own sample
        const int h = rr.getHeight();
        muteBtns[(size_t) row]->setBounds (rr.removeFromLeft (30).withSizeKeepingCentre (28, juce::jmin (26, h - 6)));
        soloBtns[(size_t) row]->setBounds (rr.removeFromLeft (30).withSizeKeepingCentre (28, juce::jmin (26, h - 6)));
        volKnobs[(size_t) row]->setBounds (rr.removeFromLeft (48).reduced (1));
        pitchKnobs[(size_t) row]->setBounds (rr.removeFromLeft (48).reduced (1));
    }
}

void BeatsPage::paint (juce::Graphics& g)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
    g.setColour (colours::section);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
    g.setColour (colours::sectionEdge);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8.0f, 1.0f);

    // row names: RowNameButton
    // bottom bar captions
    g.setColour (colours::textDim);
    g.setFont (LooperLookAndFeel::font (11.0f, true));
    g.drawText (tr ("GROOVE"), captionArea.withHeight (34), juce::Justification::centredRight);
    g.drawText (tr ("FILLS"), captionArea.withTrimmedTop (40).withHeight (30), juce::Justification::centredRight);
    g.setColour (colours::sectionEdge);
    g.drawHorizontalLine (bottomBar.getY() - 3, (float) bottomBar.getX(), (float) bottomBar.getRight());
}

//============================================================================== grid
juce::Rectangle<float> BeatsPage::Grid::cellBounds (int row, int stepInPage) const
{
    const int n = owner.visibleSteps();
    const float w = (float) getWidth() / (float) n, h = (float) getHeight() / (float) BeatPattern::kRows;
    return { w * (float) stepInPage, h * (float) row, w, h };
}

bool BeatsPage::Grid::cellAt (juce::Point<int> p, int& row, int& step) const
{
    const int n = owner.visibleSteps();
    if (! getLocalBounds().contains (p))
        return false;
    row = juce::jlimit (0, BeatPattern::kRows - 1, p.y * BeatPattern::kRows / juce::jmax (1, getHeight()));
    step = owner.barPage * n + juce::jlimit (0, n - 1, p.x * n / juce::jmax (1, getWidth()));
    return true;
}

void BeatsPage::Grid::paint (juce::Graphics& g)
{
    auto& bank = owner.proc.getUserPatterns();
    const int slot = owner.getEditSlot();
    const int n = owner.visibleSteps();
    const int perBeat = n / 4;
    const int playing = owner.playingCell();
    for (int row = 0; row < BeatPattern::kRows; ++row)
        for (int i = 0; i < n; ++i)
        {
            const int step = owner.barPage * n + i;
            auto c = cellBounds (row, i).reduced (n > 16 ? 1.5f : 2.5f);
            const int v = bank.getCell (slot, row, step);
            const bool beatStart = (i / perBeat) % 2 == 0;
            g.setColour (beatStart ? colours::panelLight : colours::panelLight.darker (0.25f));
            g.fillRoundedRectangle (c, 3.0f);
            if (v > 0)
            {
                const float vel = (float) v / 127.0f;
                auto fill = brand ((float) i / (float) juce::jmax (1, n - 1)).withAlpha (0.35f + 0.65f * vel);
                g.setColour (fill);
                const float hh = c.getHeight() * (0.45f + 0.55f * vel);
                g.fillRoundedRectangle (c.withTop (c.getBottom() - hh), 3.0f);
                if (v >= 120)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.85f));
                    g.fillRect (c.withHeight (2.5f).translated (0.0f, 1.0f).reduced (3.0f, 0.0f));
                }
            }
            if (step == playing)
            {
                g.setColour (juce::Colours::white.withAlpha (0.22f));
                g.fillRoundedRectangle (c, 3.0f);
            }
        }
}

void BeatsPage::Grid::mouseDown (const juce::MouseEvent& e)
{
    int row, step;
    if (! cellAt (e.getPosition(), row, step))
        return;
    if (e.mods.isPopupMenu())
    {
        owner.cycleVelocity (row, step);
        paintRow = -1;
        return;
    }
    owner.toggleStep (row, step, e.mods);
    paintRow = row;
    paintValue = owner.proc.getUserPatterns().getCell (owner.getEditSlot(), row, step);
    lastStep = step;
}

void BeatsPage::Grid::mouseDrag (const juce::MouseEvent& e)
{
    int row, step;
    if (paintRow < 0 || ! cellAt (e.getPosition(), row, step) || step == lastStep)
        return;
    lastStep = step;
    owner.proc.getUserPatterns().setCell (owner.getEditSlot(), paintRow, step, paintValue);
    repaint();
}

void BeatsPage::Grid::mouseUp (const juce::MouseEvent&)
{
    paintRow = -1;
}

void BeatsPage::Grid::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    int row, step;
    if (! cellAt (e.getPosition(), row, step))
        return;
    auto& bank = owner.proc.getUserPatterns();
    const int v = bank.getCell (owner.getEditSlot(), row, step);
    if (v == 0)
        return;
    bank.setCell (owner.getEditSlot(), row, step, juce::jlimit (1, 127, v + (w.deltaY > 0 ? 6 : -6)));
    repaint();
}
//============================================================================== v1.6 own samples per row
void BeatsPage::RowNameButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat();
    if (over || down)
    {
        g.setColour (colours::text.withAlpha (down ? 0.14f : 0.07f));
        g.fillRoundedRectangle (r, 5.0f);
    }
    const bool own = owner.proc.hasRowSample (row);
    g.setColour (brand ((float) row / 7.0f));
    g.fillRoundedRectangle (r.getX(), r.getCentreY() - 7.0f, 4.0f, 14.0f, 2.0f);
    auto text = r.withTrimmedLeft (10.0f).withTrimmedRight (14.0f);
    g.setColour (colours::text);
    g.setFont (LooperLookAndFeel::font (own ? 12.0f : 13.0f, true));
    if (own)
    {
        g.drawFittedText (i18n::upper (owner.proc.getVoiceDisplayName (row)), text.removeFromTop (text.getHeight() * 0.5f).toNearestInt(),
                          juce::Justification::bottomLeft, 1, 0.8f);
        g.setColour (colours::cyan);
        g.setFont (LooperLookAndFeel::font (11.0f));
        g.drawFittedText (owner.proc.getRowSampleName (row), text.toNearestInt(), juce::Justification::topLeft, 1, 0.7f);
    }
    else
        g.drawFittedText (i18n::upper (owner.proc.getVoiceDisplayName (row)), text.toNearestInt(),
                          juce::Justification::centredLeft, 1, 0.8f);
    // menu marker
    juce::Path tri;
    const float cx = r.getRight() - 7.0f, cy = r.getCentreY();
    tri.addTriangle (cx - 3.5f, cy - 2.0f, cx + 3.5f, cy - 2.0f, cx, cy + 2.5f);
    g.setColour ((own ? colours::cyan : colours::textDim).withAlpha (over ? 1.0f : 0.7f));
    g.fillPath (tri);
}

juce::String BeatsPage::RowNameButton::getTooltip()
{
    if (owner.proc.hasRowSample (row))
        return tr ("Own sample: %1 - click to load another one or to go back to the kit sample", owner.proc.getRowSampleName (row));
    return tr ("Click: load your own sample for this row. Or drop an audio file (WAV, AIFF, FLAC, MP3 ...) on the row");
}

bool BeatsPage::loadRowSample (int row, const juce::File& file)
{
    juce::String error;
    if (! proc.loadRowSample (row, file, error))
    {
        status (error);
        return false;
    }
    status (tr ("%1: own sample %2", i18n::upper (proc.getVoiceDisplayName (row)), proc.getRowSampleName (row)));
    lastRowSampleVersion = -1;
    timerCallback();
    if (auto* b = getRowSampleButton (row))
        b->repaint();
    return true;
}

void BeatsPage::resetRowSample (int row)
{
    proc.clearRowSample (row);
    status (tr ("%1: kit sample again", i18n::upper (proc.getVoiceDisplayName (row))));
    if (auto* b = getRowSampleButton (row))
        b->repaint();
}

void BeatsPage::chooseRowSample (int row)
{
    auto start = proc.getRowSampleFile (row);
    chooser = std::make_unique<juce::FileChooser> (tr ("Load sample for %1", i18n::upper (proc.getVoiceDisplayName (row))),
                                                   start.existsAsFile() ? start.getParentDirectory()
                                                                        : juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   LiveLooperProcessor::getRowSampleWildcard());
    juce::Component::SafePointer<BeatsPage> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe, row] (const juce::FileChooser& fc) {
                              if (safe != nullptr && fc.getResult().existsAsFile())
                                  safe->loadRowSample (row, fc.getResult());
                          });
}

void BeatsPage::showRowSampleMenu (int row)
{
    juce::PopupMenu menu;
    const bool own = proc.hasRowSample (row);
    menu.addSectionHeader (i18n::upper (proc.getVoiceDisplayName (row)) + (own ? ": " + proc.getRowSampleName (row) : juce::String()));
    menu.addItem (1, tr ("Load sample..."));
    menu.addItem (2, tr ("Reset to kit sample"), own);
    menu.addSeparator();
    menu.addItem (3, tr ("Tip: drop an audio file on the row"), false);
    juce::Component::SafePointer<BeatsPage> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (getRowSampleButton (row)), [safe, row] (int result) {
        if (safe == nullptr)
            return;
        if (result == 1)
            safe->chooseRowSample (row);
        else if (result == 2)
            safe->resetRowSample (row);
    });
}

int BeatsPage::rowAtY (int y) const
{
    if (rowHeader.getHeight() <= 0 || y < rowHeader.getY() || y >= rowHeader.getBottom())
        return -1;
    return juce::jlimit (0, BeatPattern::kRows - 1, (y - rowHeader.getY()) * BeatPattern::kRows / rowHeader.getHeight());
}

bool BeatsPage::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (LiveLooperProcessor::isRowSampleFile (juce::File (f)))
            return true;
    return false;
}

void BeatsPage::fileDragEnter (const juce::StringArray& files, int x, int y) { fileDragMove (files, x, y); }

void BeatsPage::fileDragMove (const juce::StringArray&, int, int y)
{
    const int r = rowAtY (y);
    if (r != dragRow)
    {
        dragRow = r;
        repaint();
    }
}

void BeatsPage::fileDragExit (const juce::StringArray&)
{
    dragRow = -1;
    repaint();
}

void BeatsPage::filesDropped (const juce::StringArray& files, int, int y)
{
    dragRow = -1;
    repaint();
    const int r = rowAtY (y);
    if (r < 0)
    {
        status (tr ("Drop the sample on one of the rows"));
        return;
    }
    for (const auto& f : files)
        if (LiveLooperProcessor::isRowSampleFile (juce::File (f)))
        {
            loadRowSample (r, juce::File (f));
            return;
        }
}

void BeatsPage::paintOverChildren (juce::Graphics& g)
{
    if (dragRow < 0)
        return;
    const float rowH = (float) rowHeader.getHeight() / (float) BeatPattern::kRows;
    auto area = juce::Rectangle<float> ((float) rowHeader.getX(), (float) rowHeader.getY() + rowH * (float) dragRow,
                                        (float) (grid.getRight() - rowHeader.getX()), rowH).reduced (1.0f);
    g.setColour (colours::cyan.withAlpha (0.16f));
    g.fillRoundedRectangle (area, 6.0f);
    g.setColour (colours::cyan);
    g.drawRoundedRectangle (area, 6.0f, 2.0f);
    g.setFont (LooperLookAndFeel::font (14.0f, true));
    g.drawText (tr ("Drop: own sample for %1", i18n::upper (proc.getVoiceDisplayName (dragRow))),
                area.withTrimmedLeft (110.0f).toNearestInt(), juce::Justification::centred);
}
} // namespace livelooper
