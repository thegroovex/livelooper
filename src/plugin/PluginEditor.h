#pragma once

#include "Widgets.h"
#include "KeyMapEditor.h"
#include "BeatsPage.h"

namespace livelooper { class GuidedTour;
class WelcomeScreen;
class ControllerView; }

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
    /** FX groups: 0 input FX, 1 track FX, 2 beat FX, 3 beat repeat / stutter, 4 colour FX, 5 punch FX + send FX.
        v1.6 final layout: every group is visible at once in the two FX rows at the top (no tabs); the "tab" only
        remembers the group last focused (slot selection, tour) - kept for MIDI / tests. */
    enum FxTab { tabInput = 0, tabTrack, tabBeat, tabRepeat, tabColor, tabPunch, numFxTabs };
    void setFxTab (int tab);
    int getFxTab() const noexcept { return fxTab; }
    /** Panel of an FX group in the FX rows (surface coordinates). */
    juce::Rectangle<int> getFxGroupArea (int group) const;
    juce::Rectangle<int> getRightColumnArea() const { return rhythmArea.getUnion (masterArea); }

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
    /** part 0: sets, 1: recording / clip export, 2: controller templates, 3: guided tour. */
    void addFeatures16Menu (juce::PopupMenu& menu, int part);
    /** v1.6 SETUP: the menu structure (section headers = pages of the settings window) and its actions. */
    juce::PopupMenu buildSetupMenu (juce::Array<int>& toggleIds);
    void handleSetupResult (int result);
    /** The settings window (Apple System Settings style); also used by the screenshot tool. */
    std::unique_ptr<juce::Component> createSettingsPanel (const juce::String& page = {});
    void showSettingsWindow();
    bool handleFeatures16Menu (int result);
    RecordButton& getPerfRecButton() noexcept { return perfRecBtn; }
    // ---- end v1.6 features
    // ---- v1.6 K.O. II, punch FX, Controller GUI (EditorKo2.cpp)
    /** SETUP > Controllers > GUI: Standard / Controller (saved); the controller view shows only while a
        K.O. II or Push 1 is connected. */
    void setControllerGui (bool on);
    bool isControllerGuiShown() const noexcept;
    ControllerView* getControllerView() noexcept { return controllerView.get(); }
    /** The header chip (visible only while a K.O. II is connected). */
    juce::Component& getKo2Chip() noexcept { return *ko2Chip; }
    /** One-time question when a K.O. II is plugged in (force: tests / screenshots). */
    void showKo2Toast (bool force = false);
    bool isKo2ToastVisible() const noexcept;
    juce::Component* getKo2Toast() noexcept { return ko2Toast.get(); }
    /** SETUP > Controllers: K.O. II and GUI groups / their actions (ids 700 - 799). */
    void addControllerMenu (juce::PopupMenu& menu, juce::Array<int>& toggles);
    bool handleControllerMenu (int result);
    /** Punch FX tab: the punch FX the XY field plays (last pad clicked). */
    int getSelectedPunch() const noexcept { return selectedPunch; }
    void updateKo2Ui();          // timer (public for tests)
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
    RecordButton perfRecBtn;        // v1.6: round record button under MIDI LEARN / MIDI MAP / SETUP / ?
    double recBlinkStartMs = 0.0, recBlinkBeat0 = 0.0;
    std::unique_ptr<APVTS::ButtonAttachment> perfRecAtt;
    std::unique_ptr<GuidedTour> tour;
    juce::Component::SafePointer<juce::DialogWindow> settingsWindow;
    std::unique_ptr<WelcomeScreen> welcome;
    void closeWelcome (bool startTourAfter);
    std::unique_ptr<juce::Thread> clipThread;
    std::atomic<float> clipProgress { -1.0f };
    // ---- end v1.6 features
    // ---- v1.6 K.O. II / punch FX / Controller GUI
    void initKo2();
    void layoutKo2 (int dx);
    void layoutPunchTab (juce::Rectangle<int> content);
    void showKo2ChipMenu();
    void applyKo2Choice (int choice, bool remember);     // 0 controller, 1 record, 2 ignore
    std::array<std::unique_ptr<MomentaryButton>, 12> punchBtns;
    std::unique_ptr<juce::MouseListener> punchSelectListener;
    std::unique_ptr<Knob> pfxAmountKnob, sfxXKnob, sfxYKnob, sfxLevelKnob;
    LLCombo pfxTargetBox, sfxTypeBox;
    std::unique_ptr<APVTS::ComboBoxAttachment> pfxTargetAtt, sfxTypeAtt;
    std::unique_ptr<LLButton> pfxLatchBtn;
    std::unique_ptr<APVTS::ButtonAttachment> pfxLatchAtt;
    std::unique_ptr<juce::Component> punchXY;
    int selectedPunch = 0;
    float shownLivePunch = -1.0f;       // v1.6 K.O. II hold ramp: live amount shown in the PUNCH FX title row
    juce::Rectangle<int> livePunchBadgeArea() const;
    std::unique_ptr<juce::Button> ko2Chip;
    std::unique_ptr<juce::Component> ko2Toast;
    std::unique_ptr<ControllerView> controllerView;
    bool ko2WasConnected = false;
    std::array<int, 8> shownTrackColours { -2, -2, -2, -2, -2, -2, -2, -2 };
    juce::Rectangle<int> punchCaption, sendCaption;

    class Surface : public juce::Component
    {
    public:
        std::function<void (juce::Graphics&)> painter;
        void paint (juce::Graphics& g) override { if (painter) painter (g); }
    };

    /** Segment of the FX panel's tab bar; a small LED shows that something in the group is on. */
    class FxTabButton : public LLButton
    {
    public:
        FxTabButton (const juce::String& text, juce::Colour led) : LLButton (text, led) {}
        std::function<bool()> activeFn;
        void paintButton (juce::Graphics& g, bool over, bool down) override;
    };
    std::array<std::unique_ptr<FxTabButton>, numFxTabs> fxTabs;
    int fxTab = tabInput;
    juce::Rectangle<int> fxPanelArea, fxContentArea, rhythmArea, masterArea;
    juce::Rectangle<int> inputFxArea, fxEditArea, trackFxArea, punchArea, sendArea;   // v1.6 FX rows
    void layoutFxPanel (int dx);
    // cached static surface (background, glass panels, titles) - rebuilt only when the layout changes
    juce::Image surfaceCache;
    juce::String surfaceCacheKey;
    void renderSurfaceCache();

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
