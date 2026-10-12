#pragma once

#include "Widgets.h"
#include "KeyMapEditor.h"

namespace livelooper
{
/**
    The livelooper surface: display and MIDI controls on top, input FX / FX editor /
    track FX below, five track strips and the rhythm + master section.

    The surface is laid out at a fixed base size and scaled as a whole when the window
    is resized, so it keeps its hardware proportions.
*/
class LiveLooperEditor : public juce::AudioProcessorEditor,
                         public MidiMenuHost,
                         public juce::FileDragAndDropTarget,
                         private juce::Timer,
                         private juce::ChangeListener,
                         private i18n::Listener
{
public:
    static constexpr int kBaseWidth = 1180;
    static constexpr int kBaseHeight = 720;

    explicit LiveLooperEditor (LiveLooperProcessor&);
    ~LiveLooperEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // MidiMenuHost
    void showMidiMenuFor (juce::Component& c) override;
    void exportTrack (int track) override;
    void importTrack (int track) override;
    void showTrackSettings (int track, juce::Component& anchor) override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    // for tests / screenshots
    void setLearnMode (bool on);
    bool isLearnMode() const noexcept { return learnMode; }
    void selectFxSlot (bool inputFx, int slot);
    void showStatus (const juce::String& text);

    // computer keyboard (standalone app)
    bool keyPressed (const juce::KeyPress& key) override;
    bool keyStateChanged (bool isKeyDown) override;
    void performKeyAction (const juce::String& action);
    /** Opens the text field on the display to type in the tempo. */
    void showTempoEntry();
    /** Looper sets (.llset): file dialogs and loading (also by drag and drop). */
    void saveSet();
    void loadSet();
    void loadSetFile (const juce::File& file);
    bool isTempoEntryVisible() const { return tempoEntry.isVisible(); }
    juce::TextEditor& getTempoEntry() noexcept { return tempoEntry; }
    /** A track clear on the computer keyboard must be held like the STOP button (1.5 s). */
    static constexpr juce::uint32 kClearHoldMs = 1500;
    int getPendingKeyClearTrack() const noexcept { return keyClear.track; }
    /** Checks a held clear key; called from the timer (public for tests). */
    void updateKeyClearHold (juce::uint32 now, bool keyStillDown);

private:
    class Surface : public juce::Component
    {
    public:
        std::function<void (juce::Graphics&)> painter;
        void paint (juce::Graphics& g) override { if (painter) painter (g); }
    };

    class LearnOverlay : public juce::Component
    {
    public:
        explicit LearnOverlay (LiveLooperEditor& e) : editor (e) {}
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
    private:
        LiveLooperEditor& editor;
    };

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void languageChanged() override;
    void paintSurface (juce::Graphics& g);
    void layoutSurface();
    void updateLcd();
    void updateFxEditorLabels();
    void showMidiMapMenu();
    void showSetupMenu();
    void confirmClearAll();
    void commitTempoEntry();
    void updateKeyboardFocusPolicy();
    void collectMappables (juce::Component& parent, juce::Array<juce::Component*>& out);
    juce::String paramName (const juce::String& id) const;

    LiveLooperProcessor& proc;
    LooperLookAndFeel lookAndFeel;
    Surface surface;
    LearnOverlay overlay { *this };
    juce::TooltipWindow tooltips { this, 600 };

    Lcd lcd;
    LLButton learnBtn { "MIDI LEARN", colours::learn }, mapBtn { "MIDI MAP", colours::cyan }, setupBtn { "SETUP", colours::cyan };

    std::array<std::unique_ptr<FxSlot>, kNumFxSlots> inputSlots, trackSlots;
    LLCombo fxType;
    std::unique_ptr<APVTS::ComboBoxAttachment> fxTypeAtt;
    std::unique_ptr<Knob> fxP1, fxP2, fxMix;
    bool selectedInput = true;
    int selectedSlot = 0;

    std::array<std::unique_ptr<TrackStrip>, kNumTracks> strips;

    LLButton allStartStop { "ALL START / STOP", colours::green }, clearAll { "CLEAR ALL", colours::red };
    LLButton tap { "TAP", colours::yellow }, rhythmOn { "RHYTHM", colours::orange }, sync { "HOST SYNC", colours::cyan };
    LLCombo quantize, rhythmPattern;
    std::unique_ptr<APVTS::ButtonAttachment> rhythmOnAtt, syncAtt;
    std::unique_ptr<APVTS::ComboBoxAttachment> quantizeAtt, rhythmPatternAtt;
    std::unique_ptr<Knob> tempo, rhythmLevel, master, monitor, fade;

    juce::TextEditor tempoEntry;
    juce::Array<int> heldKeys;
    struct KeyClear { int track = -1; int keyCode = 0; juce::uint32 start = 0; } keyClear;
    int lastRecAction = -1;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::String statusText;
    juce::uint32 statusTime = 0;
    bool learnMode = false;
    int blinkCounter = 0;
    int lastMidiActivity = 0;
    int midiLedFrames = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveLooperEditor)
};
} // namespace livelooper
