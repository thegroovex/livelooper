#pragma once

#include "Widgets.h"

namespace livelooper
{
/**
    v1.6 BEATS page: a step sequencer for the drum kit.

    Rows = the eight voices of the current kit (kick, snare, clap, closed / open hat, rim,
    shaker, tom - the Afro Latin kit and sound packs name them after their instruments),
    16 or 32 steps per bar, 1 - 4 bars. Every step has a velocity (ghost, normal, accent or
    anything in between). Per row: mute, solo, level and pitch (host parameters, so they can be
    automated and MIDI mapped). The pattern is one of the 16 user slots that the pattern selector
    lists as "User 1 .. 16"; editing a slot makes it play (the switch follows PATTERN SWITCH).

    Mouse: click = step on / off (drag along the row to paint), SHIFT + click = accent,
    ALT + click = ghost note, right-click = next velocity (ghost > normal > accent), mouse wheel
    over a step = fine velocity.

    Own samples: drop an audio file on a row, or click the row name > "Load sample...". The row
    then plays that sample in every kit (name shown under the voice) until "Reset to kit sample".
*/
class BeatsPage : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer, private i18n::Listener
{
public:
    explicit BeatsPage (LiveLooperProcessor& p);
    ~BeatsPage() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

    /** Called when the page should close (LOOPER button). */
    std::function<void()> onClose;
    /** Shows a short message in the editor's status line. */
    std::function<void (const juce::String&)> onStatus;

    // ---- actions (public for tests)
    int  getEditSlot() const noexcept { return proc.getBeatsEditSlot(); }
    void setEditSlot (int slot);
    void toggleStep (int row, int step, juce::ModifierKeys mods);
    void cycleVelocity (int row, int step);
    void clearPattern();
    void copyPattern();
    void pastePattern();
    void randomiseLight (juce::int64 seed = juce::Time::currentTimeMillis());
    void setResolution (int stepsPerBar, int bars);
    void loadPreset (int presetIndex);
    void loadPattern (const BeatPattern& p, const juce::String& name);
    juce::File saveToLibrary();
    /** v1.6 beat library: SAVE (name dialog -> library, or "Save as file..." anywhere) and LOAD (library list,
        "Open file...", grooves, packs, other slots). A loaded beat goes into the edit slot when it is empty,
        otherwise into a free slot (or always into the edit slot with "Replace this slot"). */
    juce::File saveBeat (const juce::String& name, const juce::File& target = {});
    int  loadLibraryBeat (int index);
    bool openBeatFile (const juce::File& file);
    int  getLoadTargetSlot() const;
    void setReplaceOnLoad (bool r) noexcept { replaceOnLoad = r; }
    void showSaveDialog();
    void showSaveAsChooser (const juce::String& name);
    int  getBarPage() const noexcept { return barPage; }
    void setBarPage (int b);
    /** v1.6 own samples per row (public for tests). */
    bool loadRowSample (int row, const juce::File& file);
    void resetRowSample (int row);
    void chooseRowSample (int row);
    void showRowSampleMenu (int row);
    int  rowAtY (int y) const;
    juce::Button* getRowSampleButton (int row) noexcept { return juce::isPositiveAndBelow (row, BeatPattern::kRows) ? rowNameBtns[(size_t) row].get() : nullptr; }
    // drag & drop of audio files onto a row
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragMove (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void paintOverChildren (juce::Graphics& g) override;

    /** Velocity steps of a click: 40 ghost, 96 normal, 127 accent. */
    static int nextVelocity (int v) noexcept { return v == 0 ? 40 : v < 96 ? 96 : v < 127 ? 127 : 40; }

private:
    class Grid : public juce::Component
    {
    public:
        explicit Grid (BeatsPage& o) : owner (o) {}
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
        bool cellAt (juce::Point<int> p, int& row, int& step) const;
        juce::Rectangle<float> cellBounds (int row, int stepInPage) const;
    private:
        BeatsPage& owner;
        int paintRow = -1, paintValue = 0, lastStep = -1;
    };

    /** Row name: voice name, the own sample under it, click = sample menu. */
    class RowNameButton : public juce::Button
    {
    public:
        RowNameButton (BeatsPage& o, int r) : juce::Button ("row"), owner (o), row (r) {}
        void paintButton (juce::Graphics& g, bool over, bool down) override;
        juce::String getTooltip() override;
    private:
        BeatsPage& owner;
        int row;
    };

    void timerCallback() override;
    void languageChanged() override;
    void refreshTexts();
    void showLoadMenu();
    void showPackMenu();
    void importPack();
    void status (const juce::String& s) { if (onStatus) onStatus (s); }
    void ensurePlaying();
    int  visibleSteps() const;
    int  playingCell() const;

    LiveLooperProcessor& proc;
    Grid grid { *this };

    juce::ComboBox slotBox;
    juce::TextEditor nameEdit;
    LLButton playBtn { "PLAY", colours::green }, backBtn { "LOOPER", colours::cyan };
    LLCombo kitBox, stepsBox, barsBox, switchBox, grooveBox, fillsBox;
    std::unique_ptr<APVTS::ComboBoxAttachment> kitAtt, switchAtt, grooveAtt, fillsAtt;
    LLButton loadBtn { "LOAD", colours::cyan }, saveBtn { "SAVE", colours::cyan }, clearBtn { "CLEAR", colours::red };
    LLButton copyBtn { "COPY", colours::yellow }, pasteBtn { "PASTE", colours::yellow }, randomBtn { "VARY", colours::orange };
    LLButton importBtn { "IMPORT PACK", colours::learn }, packsBtn { "PACKS", colours::learn };
    std::unique_ptr<MomentaryButton> fillBtn;
    LLButton rhythmOnBtn { "RHYTHM", colours::orange }, drumsBfxBtn { "DRUMS > BEAT FX", colours::orange },
             drumsRepBtn { "DRUMS > REPEAT", colours::red };
    std::unique_ptr<APVTS::ButtonAttachment> rhythmOnAtt, drumsBfxAtt, drumsRepAtt;
    std::array<std::unique_ptr<LLButton>, BeatPattern::kMaxBars> barBtns;
    std::array<std::unique_ptr<LLButton>, BeatPattern::kRows> muteBtns, soloBtns;
    std::array<std::unique_ptr<APVTS::ButtonAttachment>, BeatPattern::kRows> muteAtts, soloAtts;
    std::array<std::unique_ptr<Knob>, BeatPattern::kRows> volKnobs, pitchKnobs;
    std::array<std::unique_ptr<RowNameButton>, BeatPattern::kRows> rowNameBtns;
    int dragRow = -1, lastRowSampleVersion = -1;
    std::unique_ptr<Knob> swingKnob, feelKnob, variationKnob, levelKnob;

    juce::Rectangle<int> topBar, toolBar, rowHeader, bottomBar, captionArea;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::AlertWindow> saveDialog;
    bool replaceOnLoad = false;
    int barPage = 0;
    int lastVersion = -1, lastCell = -2, lastKit = -1, lastPattern = -1, lastPackVersion = -1;
    static BeatPattern clipboard;
    static bool clipboardFull;
};
} // namespace livelooper
