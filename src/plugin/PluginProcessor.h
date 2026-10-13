#pragma once

#include "Parameters.h"
#include "engine/LooperEngine.h"
#include "engine/MidiMap.h"
#include "KeyMap.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace livelooper
{
namespace push { class PushDevice; }

/**
    The plug-in: owns the parameters (APVTS), the MIDI-learn map and the engine.

    Every function of the looper is a host parameter, so it can be automated and mapped
    with the host's own MIDI mapping (Ableton Live: MIDI Map Mode after "Configure").
    Additionally the plug-in accepts MIDI and has its own MIDI learn.
*/
class LiveLooperProcessor : public juce::AudioProcessor,
                            public juce::ChangeBroadcaster,
                            private juce::Timer
{
public:
    /** withCueOutput: adds the "Cue" output bus (outputs 3 - 16) for the rhythm / metronome.
        Used by the standalone app only; plug-ins keep a plain stereo effect layout. */
    explicit LiveLooperProcessor (bool withCueOutput = juce::PluginHostType::getPluginLoadedAs() == juce::AudioProcessor::wrapperType_Standalone,
                                  bool withTrackOutputs = juce::PluginHostType::getPluginLoadedAs() != juce::AudioProcessor::wrapperType_Standalone);
    ~LiveLooperProcessor() override;

    //============================================================================== cue output (standalone)
    static constexpr int kCueBusChannels = 14;          // physical outputs 3 .. 16
    static constexpr int kNumCuePairs = kCueBusChannels / 2;
    bool hasCueOutput() const noexcept { return cueBusAvailable; }
    /** 0 = off, 1 = outputs 3/4, 2 = 5/6 ... */
    int  getCuePair() const noexcept { return cuePair.load(); }
    void setCuePair (int p) noexcept { cuePair.store (juce::jlimit (0, kNumCuePairs, p)); }
    ClickRoute getClickRoute() const noexcept { return (ClickRoute) clickRoute.load(); }
    void setClickRoute (ClickRoute r) noexcept { clickRoute.store ((int) r); }
    static juce::String getCuePairName (int pair);

    //============================================================================== v1.3
    /** Direct output of a track. Standalone: a pair of the cue bus (0 = none). Plug-in: its own
        optional stereo bus ("Track N", enabled in the host). The track's OUTPUT setting decides
        whether it is heard there instead of, or as well as, on the main output. */
    int  getTrackOutputPair (int track) const noexcept { return trackOutPair[(size_t) juce::jlimit (0, kNumTracks - 1, track)].load(); }
    void setTrackOutputPair (int track, int pair) noexcept
    {
        if (juce::isPositiveAndBelow (track, kNumTracks))
            trackOutPair[(size_t) track].store (juce::jlimit (0, kNumCuePairs, pair));
    }
    bool hasTrackOutputBuses() const noexcept { return trackBusesAvailable; }

    /** 5 or 8 tracks. Hidden tracks hold no memory (their loops are cleared). */
    int  getVisibleTracks() const noexcept { return visibleTracks.load(); }
    void setVisibleTracks (int n);

    /** Scenes: which tracks play. An empty scene stores on its first press. */
    bool sceneIsStored (int scene) const noexcept { return sceneMasks[(size_t) juce::jlimit (0, params::kNumScenes - 1, scene)].load() != 0; }
    uint32_t getSceneMask (int scene) const noexcept { return (uint32_t) sceneMasks[(size_t) juce::jlimit (0, params::kNumScenes - 1, scene)].load() & 0xffu; }
    int  getLastScene() const noexcept { return lastScene.load(); }
    void clearScene (int scene) noexcept { sceneMasks[(size_t) juce::jlimit (0, params::kNumScenes - 1, scene)].store (0); }

    /** Writes every track with a loop as 24-bit WAV (with tempo / beat metadata) into the folder.
        Returns the number of files. */
    int exportStems (const juce::File& folder);
    /** Writes one track as a 24-bit WAV with tempo / beat info (ACID chunk), as the stem export does. */
    bool writeLoopWav (int track, const juce::File& file);
    /** Folder for loops dragged out of the plug-in (kept, because Live references the files). */
    static juce::File getDragExportFolder();
    /** Writes track 'track' to a new file in the drag folder for drag & drop into Live; returns {} when empty. */
    juce::File exportLoopForDrag (int track);

    //============================================================================== Ableton Push
    /** Direct control from Push (1) through its User port, in the plug-in and the standalone app. */
    bool isPushAvailable() const noexcept { return pushDevice != nullptr; }
    bool isPushEnabled() const noexcept;
    void setPushEnabled (bool e);
    juce::String getPushStatus() const;
    /** Tests / screenshot tool: create the Push connection although no host loaded us. */
    void createPushDevice();

    //============================================================================== looper sets
    /** Saves the complete looper (loops, settings, FX, rhythm) to a set file (.llset).
        MIDI mapping, keyboard mapping and the cue routing stay with the app, not the set. */
    bool saveSet (const juce::File& file);
    bool loadSet (const juce::File& file, juce::String& error);
    static constexpr const char* kSetExtension = ".llset";

    //============================================================================== AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "livelooper"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //============================================================================== looper API (message thread)
    juce::AudioProcessorValueTreeState& getState() noexcept { return apvts; }
    LooperEngine& getEngine() noexcept                      { return engine; }
    MidiMap& getMidiMap() noexcept                          { return midiMap; }
    KeyMap& getKeyMap() noexcept                            { return keyMap; }

    /** Computer-keyboard control is offered in the standalone app (in a DAW the host owns
        the keyboard). Tests can switch it on for the plug-in build. */
    bool isKeyboardControlAvailable() const noexcept { return keyboardAvailable; }
    void setKeyboardControlAvailable (bool a) noexcept { keyboardAvailable = a; }
    bool isKeyboardControlActive() const noexcept { return keyboardAvailable && keyMap.isEnabled(); }

    /** Sets the internal tempo (clamped to 40..300 BPM) as a host-visible change. */
    void setInternalTempo (double bpm);

    /** Presses a momentary function (sets the trigger parameter, so hosts can record it). */
    void pressTrigger (const juce::String& paramId);

    int  getParamIndex (const juce::String& paramId) const;
    juce::String getParamIdForIndex (int index) const;
    juce::RangedAudioParameter* getParamForIndex (int index) const;

    void startMidiLearn (const juce::String& paramId);
    void cancelMidiLearn();
    juce::String getMidiLearnParamId() const;
    void removeMidiMapping (const juce::String& paramId);
    juce::String describeMidiMapping (const juce::String& paramId) const;   // empty if none
    void loadDefaultMidiMap();
    int  getMidiActivityCounter() const noexcept { return midiActivity.load(); }

    bool exportTrack (int track, const juce::File& file);
    bool importTrack (int track, const juce::File& file, juce::String& error);

    double getMaxLoopSeconds() const noexcept { return maxLoopSeconds; }
    void setMaxLoopSeconds (double seconds);
    bool getSaveLoopsWithProject() const noexcept { return saveLoops; }
    void setSaveLoopsWithProject (bool s) noexcept { saveLoops = s; }

    // for tests: process MIDI learn/mapping as if it came from the host
    void handleMidiMessage (const juce::MidiMessage& m);
    static BusesProperties makeBuses (bool withCue, bool withTrackOutputs);

private:
    struct Trigger
    {
        std::atomic<float>* raw = nullptr;
        juce::RangedAudioParameter* param = nullptr;
        LooperEngine::Command command {};
        int track = -1;
        bool isTap = false;
        int scene = -1;
        bool last = false;
        std::atomic<bool> needsReset { false };
    };

    struct TrackParams
    {
        std::atomic<float>* level {}; std::atomic<float>* pan {}; std::atomic<float>* reverse {};
        std::atomic<float>* oneShot {}; std::atomic<float>* loopSync {}; std::atomic<float>* fx {};
        std::atomic<float>* input {}; std::atomic<float>* startMode {}; std::atomic<float>* stopMode {};
        std::atomic<float>* dubMode {}; std::atomic<float>* measure {};
        std::atomic<float>* feedback {}; std::atomic<float>* duck {}; std::atomic<float>* output {};
        std::atomic<float>* pitch {}; std::atomic<float>* stretch {};
    };

    struct FxParams
    {
        std::atomic<float>* on {}; std::atomic<float>* type {}; std::atomic<float>* p1 {};
        std::atomic<float>* p2 {}; std::atomic<float>* mix {};
    };

    struct StoredLoop
    {
        int track = 0;
        double sampleRate = 0.0;
        double samplesPerPpq = 0.0;
        juce::AudioBuffer<float> audio;
    };

    bool snapshotTrack (int track, juce::AudioBuffer<float>& audio, double* samplesPerPpq);
    void captureLoops();
    void applyPendingLoops();

    void timerCallback() override;
    void fireTrigger (Trigger& t) noexcept;
    void handleTap() noexcept;
    void pushSettingsToEngine() noexcept;
    void applyMidiAction (const MidiMap::Action& a) noexcept;
    void handleScene (int scene) noexcept;

    juce::AudioProcessorValueTreeState apvts;
    LooperEngine engine;
    MidiMap midiMap;
    KeyMap keyMap;
    bool keyboardAvailable = false;

    std::vector<std::unique_ptr<Trigger>> triggers;
    std::array<TrackParams, kNumTracks> trackParams;
    std::array<FxParams, kNumFxSlots> inputFxParams, trackFxParams;
    std::atomic<float>* master {}; std::atomic<float>* monitor {}; std::atomic<float>* quantize {};
    std::atomic<float>* recAction {}; std::atomic<float>* sync {}; std::atomic<float>* follow {};
    std::atomic<float>* tempo {}; std::atomic<float>* fade {}; std::atomic<float>* rhythmOn {};
    std::atomic<float>* rhythmLevel {}; std::atomic<float>* rhythmPattern {}; std::atomic<float>* clickSound {};
    std::atomic<float>* rhythmKit {}; std::atomic<float>* rhythmSwing {}; std::atomic<float>* rhythmHuman {};
    bool cueBusAvailable = false;
    std::atomic<int> cuePair { 0 }, clickRoute { 0 };
    std::atomic<float>* captureBars {}; std::atomic<float>* autoRec {}; std::atomic<float>* autoRecThresh {};
    std::atomic<float>* duckSource {}; std::atomic<float>* sceneStore {};
    bool trackBusesAvailable = false;
    std::array<std::atomic<int>, kNumTracks> trackOutPair {};
    std::atomic<int> visibleTracks { kDefaultVisibleTracks };
    std::array<std::atomic<int>, params::kNumScenes> sceneMasks {};
    std::atomic<int> lastScene { -1 };
    std::atomic<bool> sceneStoreDone { false };
    std::unique_ptr<push::PushDevice> pushDevice;
    bool pushEnabledSetting = true;

    juce::Array<juce::RangedAudioParameter*> paramList;
    juce::HashMap<juce::String, int> paramIndexById;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    double maxLoopSeconds = 60.0;
    bool saveLoops = true;
    bool prepared = false;

    std::vector<StoredLoop> pendingLoops;

    long long sampleCounter = 0;
    long long lastTapSample = -1;
    std::array<double, 4> tapIntervals {};
    int numTapIntervals = 0;
    std::atomic<float> pendingTapTempo { 0.0f };
    std::atomic<int> midiActivity { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveLooperProcessor)
};
} // namespace livelooper
