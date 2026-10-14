#pragma once

#include "BeatFx.h"
#include "Effects.h"
#include "LoopTrack.h"
#include "Performance.h"
#include "RhythmGenerator.h"

namespace livelooper
{
/**
    The complete looper: five tracks, input FX, track FX bus, rhythm and the timeline
    that quantizes everything to the host tempo (or the internal clock).

    Threading: process() and the command functions run on the audio thread. Loop
    import/export from the message thread goes through lockTrack()/unlockTrack(); the
    audio thread only ever try-locks, so it never blocks.
*/
class LooperEngine
{
public:
    enum class Command : int
    {
        RecPlay = 0, Stop, Clear, Undo, Redo,       // per track
        AllStartStop, AllStart, AllStop, ClearAll,  // global
        Capture,                                    // per track: retrospective capture (-1 = first empty track)
        Start,                                      // per track: play (quantized) when stopped, used by scenes
        // v1.4 (arg = extra argument of queueCommand)
        Cut,              // per track: jump to slice arg (0 .. kNumSlices-1) at the next 16th
        CutOff,           // per track: back to the running position (in sync)
        StutterOn,        // per track: repeat at the cut point while on
        StutterOff,
        Multiply,         // per track: loop x 2
        Halve,            // per track: loop / 2
        Copy,             // per track: copy the loop to track arg (-1 = first empty track)
        SetRecLength,     // per track: arg = bars for the pending / running first recording
        // v1.6
        LoopTrick         // per track: arg = LoopTrack::Trick, executed at the next quantize boundary
    };
    static constexpr int kNumSlices = 6;

    enum class Pending : int { None = 0, Record, FinishRecord, FinishRecordThenStop, Play, AutoRecord };

    struct TrackStatus
    {
        std::atomic<int>   state { 0 };
        std::atomic<int>   pending { 0 };
        std::atomic<float> progress { 0.0f };
        std::atomic<float> peak { 0.0f };
        std::atomic<int>   lengthSamples { 0 };
        std::atomic<double> lengthPpq { 0.0 };
        std::atomic<bool>  canUndo { false };
        std::atomic<bool>  canRedo { false };
        std::atomic<bool>  stopping { false };
        // meter: input level while recording / overdubbing, output level otherwise
        mutable std::atomic<float> meterL { 0.0f }, meterR { 0.0f };   // UI takes them with exchange()
        std::atomic<bool>  meterIsInput { false };
        std::atomic<float> ghostProgress { -1.0f };   // stopped track: where it would be now (0..1), -1 = none
        std::atomic<float> duckGain { 1.0f };
        // v1.4
        std::atomic<bool>  recordingSpan { false };   // the running first recording has a fixed length
        std::atomic<bool>  cut { false };             // loop cutting / stutter active
        mutable std::atomic<float> displayMeter { 0.0f };   // peak for the Push display (taken with exchange)
        // v1.6
        std::atomic<int>   pendingTrick { -1 };       // loop trick waiting for the quantize boundary (-1 = none)
    };

    /** Extra outputs (all optional): cue pair for the rhythm, direct output pair per track. */
    struct ExtraOutputs
    {
        float* cueL = nullptr;
        float* cueR = nullptr;
        std::array<float*, kNumTracks> directL {}, directR {};
    };

    LooperEngine();

    void prepare (double sampleRate, int maxBlockSize, double maxLoopSeconds);
    void reset();
    double getSampleRate() const noexcept { return sampleRate; }
    double getMaxLoopSeconds() const noexcept { return maxLoopSeconds; }

    // ---- settings (audio thread, once per block)
    void setGlobalSettings (const GlobalSettings& s) noexcept   { global = s; }
    void setTrackSettings (int track, const TrackSettings& s) noexcept { tracks[(size_t) track].setSettings (s); }
    void setInputFx (int slot, const FxSettings& s) noexcept     { inputFx.setSlot (slot, s); }
    void setTrackFx (int slot, const FxSettings& s) noexcept     { trackFx.setSlot (slot, s); }
    void setRhythm (bool on, int pattern, float level) noexcept;
    void setClickSound (int sound) noexcept                     { rhythm.setClickSound (sound); metronome.setClickSound (sound); }
    /** kit index, swing 0.5 (straight) ... 0.75, humanize 0 ... 1 */
    void setRhythmFeel (int kit, float swing, float humanize) noexcept
    {
        rhythm.setKit (kit);
        rhythm.setSwing (swing);
        rhythm.setHumanize (humanize);
    }
    void setFollowHostTransport (bool f) noexcept               { followHostTransport = f; }

    // ---- v1.5 performance FX (audio thread, once per block)
    /** Beat FX + beat repeat / stutter on the target channel (FxTarget, Track1 + n = track n + 1).
        repeatOn: held or latched; stutter: chop every repeat; rate: index into kRepeatPpq. */
    void setPerformanceFx (const BeatFxSettings& s, int target, bool repeatOn, int repeatRate, bool stutter) noexcept
    {
        beatFx.setSettings (s);
        fxTarget = std::clamp (target, 0, kNumFxTargets - 1);
        beatRepeat.set (repeatOn, repeatRate, stutter);
    }
    /** Colour FX: one type for all channels (like a DJ mixer), one knob per track (-1 .. 1). */
    void setColorFx (ColorFxType type, int track, float amount) noexcept
    {
        colorType = type;
        colorAmount[(size_t) std::clamp (track, 0, kNumTracks - 1)] = amount;
    }
    /** v1.5 metronome, independent of the rhythm. */
    void setMetronome (bool on, float level, bool accent) noexcept
    {
        metronomeOn = on;
        metronome.setPattern (0);
        metronome.setLevel (level);
        metronome.setClickAccent (accent);
    }
    /** v1.5 rhythm: drum samples (message thread, before playing), groove template, fills. */
    void setDrumBank (std::shared_ptr<const DrumBank> b) noexcept { rhythm.setDrumBank (std::move (b)); }
    void setRhythmGroove (int groove, int fills) noexcept { rhythm.setGroove (groove); rhythm.setFills (fills); }
    const RhythmGenerator& getRhythm() const noexcept { return rhythm; }
    const BeatFx& getBeatFx() const noexcept { return beatFx; }
    const BeatRepeat& getBeatRepeat() const noexcept { return beatRepeat; }
    int getFxTarget() const noexcept { return fxTarget; }

    // ---- commands (audio thread). Executed at the start of the next process() call.
    void queueCommand (Command c, int track = -1, int arg = 0) noexcept;
    /** v1.4 loop bus performance: tape stop depth 0 .. 1, filter -1 (low-pass) .. 0 (off) .. 1 (high-pass). */
    void setPerformance (float tapeStopDepth, float filter) noexcept { tapeStop.setDepth (tapeStopDepth); perfFilter.setAmount (filter); }
    bool isTapeStopActive() const noexcept { return tapeStop.isActive(); }
    /** Start of slice s (0 .. kNumSlices-1) of track t in loop samples, snapped to 16th notes of the loop. */
    int sliceStart (int t, int slice) const noexcept;
    /** Length of the stutter repeat for track t (a 16th, 8th or quarter note, at most a slice). */
    int stutterLength (int t) const noexcept;

    // ---- processing
    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples, const TimeInfo& time) noexcept
    {
        process (inL, inR, outL, outR, nullptr, nullptr, numSamples, time);
    }
    /** cueL / cueR (may be null): separate output for the rhythm / metronome (GlobalSettings::clickRoute).
        Without a cue output the rhythm always goes to the main output. */
    void process (const float* inL, const float* inR, float* outL, float* outR, float* cueL, float* cueR,
                  int numSamples, const TimeInfo& time) noexcept
    {
        ExtraOutputs x;
        x.cueL = cueL;
        x.cueR = cueR;
        process (inL, inR, outL, outR, x, numSamples, time);
    }
    void process (const float* inL, const float* inR, float* outL, float* outR, const ExtraOutputs& extra,
                  int numSamples, const TimeInfo& time) noexcept;

    /** Tracks that hold memory (5 or 8). Call from the message thread; allocates. */
    void setActiveTracks (int n);
    int  getActiveTracks() const noexcept { return activeTracks.load(); }
    /** Seconds of input kept for the retrospective capture. */
    double getCaptureSecondsAvailable() const noexcept { return (double) ringFilled.load() / sampleRate; }

    // ---- status for the UI (any thread)
    const TrackStatus& getTrackStatus (int t) const noexcept { return status[(size_t) t]; }
    double getTimelinePpq() const noexcept   { return statusPpq.load(); }
    double getCurrentBpm() const noexcept    { return statusBpm.load(); }
    bool   isTimelineRunning() const noexcept { return statusRunning.load(); }
    bool   isHostDriven() const noexcept     { return statusHost.load(); }
    double getMeasurePpq() const noexcept    { return statusMeasurePpq.load(); }
    /** Peak of the main output since the last call (left / right), resets it. */
    void   takeMasterMeter (float& l, float& r) noexcept { l = masterPeakL.exchange (0.0f); r = masterPeakR.exchange (0.0f); }
    /** Peak of the cue output since the last call, resets it. */
    float  takeCueMeter() noexcept           { return cuePeak.exchange (0.0f); }
    /** Peak of the main output for the Push display (separate from the editor's meter), resets it. */
    float  takeDisplayMasterMeter() noexcept { return displayMasterPeak.exchange (0.0f); }

    // ---- direct access for tests and import/export (lock the track first when not on the audio thread)
    LoopTrack& getTrack (int t) noexcept      { return tracks[(size_t) t]; }
    Pending getPending (int t) const noexcept { return pending[(size_t) t].type; }
    void lockTrack (int t) noexcept           { trackLocks[(size_t) t].enter(); }
    void unlockTrack (int t) noexcept         { trackLocks[(size_t) t].exit(); }

private:
    struct PendingAction { Pending type = Pending::None; double targetPpq = 0.0; };
    struct QueuedCommand { Command command; int track; int arg = 0; };
    struct PendingCut { bool active = false; double targetPpq = 0.0; int slice = 0; };

    void processChunk (const float* inL, const float* inR, float* outL, float* outR, const ExtraOutputs& extra,
                       int offset, int n, const TimeInfo& time) noexcept;
    void processTrack (int t, const float* srcL, const float* srcR, int n, double chunkStartPpq) noexcept;
    bool captureInto (int t) noexcept;
    void executeCommand (const QueuedCommand& c) noexcept;
    void trackRecPlay (int t) noexcept;
    void trackStop (int t) noexcept;
    void trackPlayQuantized (int t) noexcept;
    void executePending (int t) noexcept;
    void finishRecordingQuantized (int t, bool thenStop) noexcept;
    bool anythingNeedsClock() const noexcept;
    void ensureClockRunning() noexcept;
    double gridPpq() const noexcept;
    double nextBoundary (double grid) const noexcept;
    int autoFinishSamples (int t) const noexcept;
    void updateStatus() noexcept;
    void executeCut (int t, int slice) noexcept;
    // v1.6 loop tricks
    struct PendingTrick { bool active = false; double targetPpq = 0.0; int kind = 0; };
    std::array<PendingTrick, kNumTracks> trickPending {};
    /** Executes a loop trick; the track lock must be held. */
    void executeTrick (int t, int kind) noexcept;

    double sampleRate = 44100.0;
    int maxBlock = 512;
    double maxLoopSeconds = 120.0;

    GlobalSettings global;
    std::array<LoopTrack, kNumTracks> tracks;
    std::array<PendingAction, kNumTracks> pending;
    std::array<juce::SpinLock, kNumTracks> trackLocks;
    std::array<TrackStatus, kNumTracks> status;
    FxChain inputFx, trackFx;
    RhythmGenerator rhythm;
    bool rhythmOn = false;
    // v1.5
    RhythmGenerator metronome;
    bool metronomeOn = false;
    BeatFx beatFx;
    BeatRepeat beatRepeat;
    int fxTarget = 0;
    std::array<ColorFx, kNumTracks> colorFx;
    std::array<float, kNumTracks> colorAmount {};
    ColorFxType colorType = ColorFxType::Filter;
    double freePpq = 0.0;              // grid for the performance FX while the timeline stands still
    void applyPerformanceFx (float* l, float* r, int n, double ppq) noexcept
    {
        beatRepeat.process (l, r, n, ppq, ppqPerSample);
        beatFx.process (l, r, n, ppq, ppqPerSample);
    }
    bool followHostTransport = false;

    std::array<PendingCut, kNumTracks> cutPending {};
    std::array<bool, kNumTracks> stutterWanted {};
    std::array<int, kNumTracks> recLengthBars {};
    TapeStop tapeStop;
    PerformanceFilter perfFilter;
    std::vector<float> loopBusL, loopBusR;
    std::atomic<float> displayMasterPeak { 0.0f };

    std::array<QueuedCommand, 128> commandQueue {};
    int numQueued = 0;

    // timeline
    double blockPpq = 0.0;         // ppq of the sample currently being processed (chunk start)
    double ppqPerSample = 0.0;
    double samplesPerPpq = 0.0;
    double bpm = 120.0;
    double measurePpq = 4.0;
    int    beatsPerMeasure = 4;
    bool   running = false;
    bool   hostDriven = false;
    bool   internalRunning = false;
    double internalPpq = 0.0;
    bool   wasHostPlaying = false;
    double expectedHostPpq = 0.0;

    std::vector<float> inProcL, inProcR, fxBusL, fxBusR, clickBuf, kickBuf, duckEnvBuf;
    std::vector<float> tOutL, tOutR, tFxL, tFxR, resL, resR;
    std::vector<float> ringL, ringR;           // retrospective capture: the last seconds of input (after input FX)
    int ringWrite = 0;
    std::atomic<int> ringFilled { 0 };
    std::atomic<int> activeTracks { kDefaultVisibleTracks };
    float duckEnv = 0.0f, duckRelease = 0.999f;
    std::atomic<float> masterPeakL { 0.0f }, masterPeakR { 0.0f }, cuePeak { 0.0f };

    std::atomic<double> statusPpq { 0.0 }, statusBpm { 120.0 }, statusMeasurePpq { 4.0 };
    std::atomic<bool> statusRunning { false }, statusHost { false };
};
} // namespace livelooper
