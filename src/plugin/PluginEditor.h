#pragma once

#include "Widgets.h"
#include "KeyMapEditor.h"
#include "BeatsPage.h"

namespace livelooper { class GuidedTour;
class WelcomeScreen; }

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
    static constexpr int kBaseHeight = 856;   // v1.5: + performance FX row
    static constexpr int kStripWidth = 170;
    /** Width of the surface: wider when 8 tracks are shown. */
    int surfaceWidth() const noexcept { return kBaseWidth + (proc.getVisibleTracks() - kDefaultVisibleTracks) * kStripWidth; }

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

    //============================================================================== v1.6 features (branch v1.6-wip)
    /** Guided tour (5 steps); shown once after the first start, always in the help menu. */
    void startTour();
    bool isTourVisible() const;
    GuidedTour* getTour() noexcept { return tour.get(); }
    /** First start: the welcome screen (demo set or empty set), then the tour - once (force: also in tests). */
    void handleFirstRun (bool force = false);
    /** v1.6 welcome screen (also SETUP > Welcome / demo set ...). */
    void showWelcome (bool firstStart);
    bool isWelcomeVisible() const noexcept;
    WelcomeScreen* getWelcome() noexcept { return welcome.get(); }
    /** What the welcome buttons do; asks first when the current set holds loops (confirm = false: tests skip the question). */
    void chooseDemoSet (bool confirm = true);
    void chooseEmptySet (bool confirm = true);
    bool currentSetHasLoops() const;
    /** 9:16 clip of the last 20 s (or the last performance recording) in the background. */
    void exportClip (bool fromRecording);
    bool isClipExportRunning() const noexcept { return clipThread != nullptr; }
    void saveSharedSet();
    void addFeatures16Menu (juce::PopupMenu& menu);
    bool handleFeatures16Menu (int result);
    juce::Button& getPerfRecButton() noexcept { return perfRecBtn; }
    // ---- end v1.6 features
    // ---- v1.6 rhythm (branch v1.6-rhythm): BEATS page
    void showBeatsPage (bool show);
    bool isBeatsPageVisible() const noexcept { return beatsPage != nullptr && beatsPage->isVisible(); }
    BeatsPage* getBeatsPage() noexcept { return beatsPage.get(); }

private:
    // ---- v1.6 features
    void initFeatures16();
    void layoutFeatures16 (int dx);
    void timerFeatures16();
    void showHelpMenu();
    LLButton perfRecBtn { "REC", colours::red };
    std::unique_ptr<APVTS::ButtonAttachment> perfRecAtt;
    std::unique_ptr<GuidedTour> tour;
    std::unique_ptr<WelcomeScreen> welcome;
    void closeWelcome (bool startTourAfter);
    std::unique_ptr<juce::Thread> clipThread;
    std::atomic<float> clipProgress { -1.0f };
    // ---- end v1.6 features

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
    // v1.5
    void showHelp();
    void showAbout();
    void askStartupSettingsOnce();
    void checkForUpdates (bool userInitiated);
    void layoutPerformanceRow (int dx);
    void updatePerformanceLabels();
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
    LLButton helpBtn { "?", colours::cyan };

    // v1.5 performance row: beat FX, beat repeat / stutter, colour FX, metronome
    LLCombo bfxType, bfxBeat, bfxTarget, repeatRate, stutterRate, clickSoundBox;
    std::unique_ptr<APVTS::ComboBoxAttachment> bfxTypeAtt, bfxBeatAtt, bfxTargetAtt, repeatRateAtt, stutterRateAtt, clickSoundAtt;
    LLButton bfxOn { "ON", colours::orange }, beatDown { "<", colours::cyan }, beatUp { ">", colours::cyan };
    std::unique_ptr<MomentaryButton> bfxHold, repeatHold, stutterHold;
    LLButton repeatLatch { "LATCH", colours::yellow }, stutterLatch { "LATCH", colours::yellow };
    std::unique_ptr<Knob> bfxDepth, metroLevel;
    std::array<std::unique_ptr<LLButton>, 6> colorBtns;
    LLButton metroOn { "METRONOME", colours::yellow }, metroAccent { "ACCENT", colours::yellow };
    std::unique_ptr<APVTS::ButtonAttachment> bfxOnAtt, repeatLatchAtt, stutterLatchAtt, metroOnAtt, metroAccentAtt;
    juce::Rectangle<int> beatFxArea, repeatArea, colorArea, metroArea;

    std::array<std::unique_ptr<FxSlot>, kNumFxSlots> inputSlots, trackSlots;
    LLCombo fxType;
    std::unique_ptr<APVTS::ComboBoxAttachment> fxTypeAtt;
    std::unique_ptr<Knob> fxP1, fxP2, fxMix;
    bool selectedInput = true;
    int selectedSlot = 0;

    std::array<std::unique_ptr<TrackStrip>, kNumTracks> strips;

    LLButton allStartStop { "ALL START / STOP", colours::green }, clearAll { "CLEAR ALL", colours::red };
    LLButton capture { "CAPTURE", colours::red };
    std::array<std::unique_ptr<LLButton>, params::kNumScenes> sceneBtns;
    LLButton sceneStore { "STORE", colours::yellow };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> sceneStoreAtt;
    int shownTracks = -1;
    void applyVisibleTracks();
    void exportStems();
    LLButton tap { "TAP", colours::yellow }, rhythmOn { "RHYTHM", colours::orange }, sync { "HOST SYNC", colours::cyan };
    LLCombo quantize, rhythmPattern, rhythmKit;
    std::unique_ptr<APVTS::ButtonAttachment> rhythmOnAtt, syncAtt;
    std::unique_ptr<APVTS::ComboBoxAttachment> quantizeAtt, rhythmPatternAtt, rhythmKitAtt;
    std::unique_ptr<Knob> tempo, rhythmLevel, rhythmSwing, rhythmHuman, master, monitor, fade;

    // ---- v1.6 rhythm (branch v1.6-rhythm)
    std::unique_ptr<BeatsPage> beatsPage;
    LLButton beatsBtn { "BEATS", colours::learn };
    std::unique_ptr<MomentaryButton> fillBtn;
    LLButton drumsBfxBtn { "DRUMS", colours::orange }, drumsRepBtn { "DRUMS", colours::red };
    std::unique_ptr<APVTS::ButtonAttachment> drumsBfxAtt, drumsRepAtt;
    int lastPatternNamesVersion = -1;
    void layoutRhythm16 (int dx);
    void refreshPatternNames();
    // ---- end v1.6 rhythm

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
