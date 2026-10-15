#pragma once

#include "Parameters.h"
#include "engine/LooperEngine.h"
#include "engine/MidiMap.h"
#include "KeyMap.h"
#include "LinkSync.h"
#include "PerformanceRecorder.h"
#include "SoundPacks.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace livelooper
{
namespace push { class PushDevice; }
namespace controllers { class GridControllerDevice; }   // v1.6
namespace ko2 { class Device; }                         // v1.6 K.O. II

/** v1.6: colours a track can get (Push encoder page "Colour", GUI strip, pad colours). -1 = the default look. */
struct TrackColour { const char* name; int padBright; int padDim; juce::uint32 argb; };
inline constexpr int kNumTrackColours = 12;
const TrackColour& getTrackColourInfo (int index) noexcept;

/**
    The plug-in: owns the parameters (APVTS), the MIDI-learn map and the engine.

    Every function of the looper is a host parameter, so it can be automated and mapped
    with the host's own MIDI mapping (e.g. MIDI learn / MIDI map mode of your DAW).
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

    //============================================================================== v1.5 Ableton Link
    bool isLinkAvailable() const noexcept { return LinkSync::isAvailable(); }
    /** "Link off" / "Link: 2 peers, 124.0 BPM" (localised). */
    juce::String getLinkStatus() const;
    int  getLinkPeers() const noexcept;
    /** True while Link drives the timeline (Link on and HOST SYNC off / standalone). */
    bool isLinkDriving() const noexcept { return linkDriving.load(); }
    /** Standalone: latency of the audio device output, for the Link phase (seconds). */
    void setOutputLatencySeconds (double s) noexcept { outputLatency.store (s); }

    //============================================================================== v1.5 global settings
    /** The looper's configuration without loops (parameters, MIDI / keyboard map, tracks, routing, Push). */
    std::unique_ptr<juce::XmlElement> createSettingsSnapshot();
    void applySettingsSnapshot (const juce::XmlElement& snapshot);
    /** Every setting back to its default (the loops stay). */
    void applyDefaultSettings();
    /** Applies the last used settings if this instance has not loaded a project / set yet. */
    void applyStartupSettingsIfFresh();
    /** Writes the snapshot to the user settings file (when it changed). */
    void saveLastSettings();
    bool hasLoadedState() const noexcept { return stateLoaded; }

    //============================================================================== looper sets
    /** Saves the complete looper (loops, settings, FX, rhythm) to a set file (.llset).
        MIDI mapping, keyboard mapping and the cue routing stay with the app, not the set. */
    bool saveSet (const juce::File& file);
    bool loadSet (const juce::File& file, juce::String& error);
    /** The set as XML (LIVELOOPER_SET, loops as FLAC inside) / loads such XML. */
    std::unique_ptr<juce::XmlElement> createSetXml();
    bool loadSetXml (std::unique_ptr<juce::XmlElement> xml, juce::String& error);
    static constexpr const char* kSetExtension = ".llset";

    //============================================================================== v1.6 features (branch v1.6-wip)
    /** Performance recorder: the master mix as WAV into PerformanceRecorder::getFolder().
        Also switched by the host parameter "perf_rec". */
    bool startPerformanceRecording();
    juce::File stopPerformanceRecording();
    bool isPerformanceRecording() const noexcept { return recorder.isRecording(); }
    double getPerformanceRecordingSeconds() const noexcept { return recorder.getElapsedSeconds(); }
    PerformanceRecorder& getRecorder() noexcept { return recorder; }
    /** Set to true by the "clip_export" parameter; the editor (or the timer) takes it. */
    bool takeClipExportRequest() noexcept { return clipExportRequested.exchange (false); }
    /** Saves the set with all audio as one shared file (.livelooper container). */
    bool saveSetContainer (const juce::File& file);
    bool loadSetContainer (const juce::File& file, juce::String& error);
    static constexpr const char* kContainerExtension = ".livelooper";
    /** Demo set: running groove + pre-recorded loops (embedded, self-made, CC0). */
    bool loadDemoSet();
    /** Empty set: every loop cleared, settings back to the defaults. */
    void loadEmptySet();
    /** Grid controller templates (Launchpad, APC mini, Launchkey); null in tests. */
    controllers::GridControllerDevice* getGridControllers() noexcept { return gridControllers.get(); }
    void createGridControllers();
    /** Called from the timer when the user switches perf_rec; also notifies listeners. */
    std::function<void (const juce::File&)> onRecordingFinished;
    // ---- end v1.6 features
    //============================================================================== v1.6 K.O. II + controller audio
    /** The K.O. II on its USB MIDI port (null in unit tests until createKo2Device()). */
    ko2::Device* getKo2Device() noexcept { return ko2Device.get(); }
    void createKo2Device();
    bool isKo2Connected() const noexcept;
    /** Role from the host parameter ctl_audio_mode: 0 = controller only, 1 = record K.O. II. */
    int  getControllerAudioMode() const noexcept;
    void setControllerAudioMode (int mode);
    /** SETUP > Controllers > K.O. II (saved for all instances): control groups (bits A=1 .. D=8, -1 = default
        of the role), send clock, MIDI channel (0 = any), latency offset in ms (-1 = automatic). */
    int  getKo2Groups() const;
    void setKo2Groups (int bits);
    bool getKo2ClockOut() const;
    void setKo2ClockOut (bool on);
    int  getKo2Channel() const;
    void setKo2Channel (int ch);
    int  getKo2OffsetMs() const;
    void setKo2OffsetMs (int ms);
    /** Offset actually used (automatic = reported input latency + one block). */
    int  getKo2EffectiveOffsetSamples() const;
    /** Resample: records capture-length bars of the input into the first empty track, starting on a bar.
        Returns the track, -1 when no track is free. Also the host parameter ko2_resample. */
    int  performKo2Resample();
    int  getResampleTrack() const noexcept { return resampleTrack; }
    /** Short messages for the editor (toasts): "No free track" ... taken once. */
    juce::String takeControllerNotice();
    void postControllerNotice (const juce::String& text);
    /** Standalone app: switch the audio device to / from the K.O. II (record = role 1 and connected). */
    std::function<void (bool recordFromKo2)> onControllerAudioChanged;
    /** Tests / screenshots: a simulated K.O. II / Push (no hardware). */
    void simulateControllers (bool ko2, bool push);
    bool isPushConnected() const noexcept;
    push::PushDevice* getPushDevice() noexcept { return pushDevice.get(); }
    /** Track colours (-1 = default), saved with the state and sets. */
    int  getTrackColour (int track) const noexcept { return trackColours[(size_t) juce::jlimit (0, kNumTracks - 1, track)].load(); }
    void setTrackColour (int track, int colour) noexcept
    {
        if (juce::isPositiveAndBelow (track, kNumTracks))
            trackColours[(size_t) track].store (juce::jlimit (-1, kNumTrackColours - 1, colour));
    }
    //============================================================================== v1.6 rhythm (branch v1.6-rhythm)
    /** BEATS: the 16 user pattern slots ("User 1 .. 16" in the pattern selector), saved with the set. */
    BeatPatternBank& getUserPatterns() noexcept { return userPatterns; }
    juce::String getUserPatternName (int slot) const;
    void setUserPatternName (int slot, const juce::String& name);
    void storeUserPattern (int slot, const BeatPattern& p, const juce::String& name);
    int  getBeatsEditSlot() const noexcept { return beatsEditSlot; }
    void setBeatsEditSlot (int slot) noexcept { beatsEditSlot = juce::jlimit (0, BeatPatternBank::kNumSlots - 1, slot); }
    /** Plays user slot 'slot' (sets the pattern parameter; the switch follows PATTERN SWITCH). */
    void selectUserPattern (int slot);
    /** The slot BEATS edits: when a preset plays, it is copied into the edit slot first and that slot plays. */
    int  prepareEditSlot();
    /** Names for the UI: kit choice (pack slots show their pack kit), pattern choice (user slots their name),
        voice of the current kit ("Conga"). Localised. */
    juce::String getKitDisplayName (int kitIndex) const;
    juce::String getPatternDisplayName (int patternIndex) const;
    juce::String getVoiceDisplayName (int voice) const;
    int  getCurrentKit() const noexcept;
    int  getCurrentPattern() const noexcept;
    /** Sound packs: rescans the packs folder / imports a pack folder or .zip (message thread). */
    void refreshSoundPacks();
    bool importSoundPack (const juce::File& folderOrZip, juce::String& error);
    /** v1.6 beat library: a beat with its name, steps, velocities, length, swing and row mixer
        (level / pitch / mute / solo) as a .llpattern file - in Music/livelooper/Patterns or anywhere. */
    SoundPacks::BeatSettings captureBeatSettings() const;
    void applyBeatSettings (const SoundPacks::BeatSettings& s);
    /** Saves user slot 'slot' (target empty = into the library under 'name'); returns the file or empty. */
    juce::File saveBeat (int slot, const juce::String& name, const juce::File& target = {});
    /** The slot a loaded beat goes to: the edit slot when it is empty (or replaceCurrent), otherwise the first
        free slot; when all 16 are used, the edit slot. */
    int  slotForLoading (bool replaceCurrent) const;
    /** Loads a beat into 'slot' (steps + name + swing / mixer when saved), makes it the edit slot and plays it. */
    void loadBeat (const SoundPacks::NamedPattern& beat, int slot);
    bool loadBeatFile (const juce::File& file, int slot, juce::String& error);
    /** Host parameters "Next / Previous Library Beat": steps through the library (wraps around). The first
        step picks a slot with slotForLoading(false); later steps reuse it while it still holds that beat.
        Returns the index in the library, -1 when it is empty. */
    int  browseLibraryBeat (int delta);
    int  takeBeatLibraryRequest() noexcept { return beatLibRequest.exchange (0); }
    /** v1.6 BEATS: the user's own sample on a row (drag & drop or "Load sample..."). It replaces the row's
        voice in every kit until the row is reset; level / pitch / mute / solo of the row still apply.
        The audio (mono, at most kMaxRowSampleSeconds) is saved with the set and the plug-in state, the
        original path too. Message thread. */
    static constexpr double kMaxRowSampleSeconds = 10.0;
    bool loadRowSample (int row, const juce::File& file, juce::String& error);
    /** Same from audio already in memory (state / set restore, tests). */
    bool setRowSampleAudio (int row, const juce::AudioBuffer<float>& audio, double sampleRate,
                            const juce::String& name, const juce::File& file);
    void clearRowSample (int row);
    bool hasRowSample (int row) const noexcept;
    juce::String getRowSampleName (int row) const;
    juce::File getRowSampleFile (int row) const;
    /** File types the rows accept (wildcard of the audio formats this build reads). */
    static juce::String getRowSampleWildcard();
    static bool isRowSampleFile (const juce::File& f);
    int  getRowSampleVersion() const noexcept { return rowSampleVersion; }
    const DrumBank* getCurrentDrumBank() const noexcept { return currentBank.get(); }

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
    /** v1.4 (Push DUPLICATE + two pads): copies the loop of track src to the empty track dst. */
    void requestCopy (int src, int dst) noexcept { pendingCopy.store (src * 16 + dst + 1); }
    /** v1.4: a slice pad held this long (seconds) turns from a jump into a stutter that ends on release. */
    static constexpr double kSliceHoldSeconds = 0.25;

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
        int arg = 0;            // extra command argument (copy: -1 = first empty track)
        int octave = 0;         // v1.4: octave up / down trigger (changes the pitch parameter)
        int special = 0;        // v1.6: 1 = clip export, 2 / 3 = next / previous library beat (message thread)
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
        // v1.4
        std::array<std::atomic<float>*, params::kNumSlices> slices {};
        std::atomic<float>* mute {}; std::atomic<float>* solo {}; std::atomic<float>* recLen {};
    };

    /** v1.4 slice pads (audio thread): tap = jump, hold = stutter that returns in sync on release. */
    struct SliceState
    {
        int held = -1;                  // slice whose pad is held (the latest press)
        long long heldSamples = 0;
        bool stutter = false;
        std::array<bool, params::kNumSlices> last {};
        int lastRecLen = 0;
    };
    void handleSlicePads (int numSamples) noexcept;

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
    std::atomic<float>* tapeStopParam {}; std::atomic<float>* perfFilterParam {};
    // v1.5
    std::atomic<float>* bfxOn {}; std::atomic<float>* bfxHold {}; std::atomic<float>* bfxType {};
    std::atomic<float>* bfxBeat {}; std::atomic<float>* bfxDepth {}; std::atomic<float>* bfxTarget {};
    std::atomic<float>* colorType {}; std::array<std::atomic<float>*, kNumTracks> colorKnob {};
    std::atomic<float>* repeatHold {}; std::atomic<float>* repeatLatch {}; std::atomic<float>* repeatRate {};
    std::atomic<float>* stutterHold {}; std::atomic<float>* stutterLatch {}; std::atomic<float>* stutterRate {};
    std::atomic<float>* metroOn {}; std::atomic<float>* metroLevel {}; std::atomic<float>* metroAccent {};
    std::atomic<float>* linkOn {}; std::atomic<float>* linkStartStop {};
    std::atomic<float>* rhythmGroove {}; std::atomic<float>* rhythmFills {};
    // ---- v1.6 rhythm (branch v1.6-rhythm)
    std::atomic<float>* rhythmVariation {}; std::atomic<float>* rhythmFill {}; std::atomic<float>* beatSwitch {};
    std::atomic<float>* drumsBfx {}; std::atomic<float>* drumsRepeat {};
    std::array<std::atomic<float>*, params::kNumBeatRows> beatMute {}, beatSolo {}, beatVol {}, beatPitch {};
    BeatPatternBank userPatterns;
    std::array<juce::String, BeatPatternBank::kNumSlots> userPatternNames;
    int beatsEditSlot = 0;
    std::atomic<int> beatLibRequest { 0 };       // v1.6: +n / -n library steps requested by the host parameters
    int browseSlot = -1, browseIndex = -1;       // v1.6: where library browsing loads to, and what it loaded
    BeatPattern browseLoaded;
    std::shared_ptr<const DrumBank> currentBank;
    std::vector<std::shared_ptr<const DrumBank>> retiredBanks;   // kept alive while hits may still play
    int soundPackVersion = -1;
    struct RowSample
    {
        juce::String name;
        juce::File file;
        juce::MemoryBlock flac;          // mono FLAC as saved with the set
        DrumBank::Sample sample;
    };
    std::array<RowSample, params::kNumBeatRows> rowSamples;
    int rowSampleVersion = 0;
    void writeRowSamples (juce::XmlElement& root) const;
    void readRowSamples (const juce::XmlElement& root);
    void writeBeatsState (juce::XmlElement& root) const;
    void readBeatsState (const juce::XmlElement& root);
    void updateDrumBank();
    // ---- end v1.6 rhythm
    std::unique_ptr<LinkSync> link;
    std::atomic<LinkSync*> linkPtr { nullptr };
    std::atomic<bool> linkDriving { false };
    std::atomic<double> outputLatency { 0.0 };
    float lastTempoParam = -1.0f;
    bool lastEngineRunning = false;
    bool stateLoaded = false, startupHandled = false;
    int settingsTimer = 0;
    juce::String lastSnapshot;
    void updateLink();
    std::array<SliceState, kNumTracks> sliceState {};
    std::array<std::atomic<bool>, kNumTracks> recLenFired {};
    std::array<std::atomic<int>, kNumTracks> pendingOctave {};
    std::atomic<int> pendingCopy { 0 };
    bool trackBusesAvailable = false;
    std::array<std::atomic<int>, kNumTracks> trackOutPair {};
    std::atomic<int> visibleTracks { kDefaultVisibleTracks };
    std::array<std::atomic<int>, params::kNumScenes> sceneMasks {};
    std::atomic<int> lastScene { -1 };
    std::atomic<bool> sceneStoreDone { false };
    std::unique_ptr<push::PushDevice> pushDevice;
    // v1.6 punch FX / send FX / K.O. II
    std::atomic<float>* sfxType {}; std::atomic<float>* sfxX {}; std::atomic<float>* sfxY {}; std::atomic<float>* sfxLevel {};
    std::array<std::atomic<float>*, params::kNumPunchHolds> pfxHold {};
    std::atomic<float>* pfxAmount {}; std::atomic<float>* pfxX {}; std::atomic<float>* pfxY {};
    std::atomic<float>* pfxTarget {}; std::atomic<float>* pfxLatch {}; std::atomic<float>* ctlAudioMode {};
    std::array<bool, params::kNumPunchHolds> pfxLast {}, pfxLatched {};
    std::unique_ptr<ko2::Device> ko2Device;
    std::atomic<bool> resampleRequested { false };
    int resampleTrack = -1;
    int lastAudioRecord = -1;
    std::array<int, kNumTracks> lastTrackState {};
    std::array<std::atomic<int>, kNumTracks> trackColours {};
    juce::StringArray controllerNotices;
    juce::CriticalSection noticeLock;
    void updateControllers();
    bool pushEnabledSetting = true;

    // v1.6 features
    PerformanceRecorder recorder;
    std::unique_ptr<controllers::GridControllerDevice> gridControllers;
    std::atomic<float>* perfRecParam {};
    std::atomic<bool> clipExportRequested { false };
    void updatePerformanceRecorder();

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
