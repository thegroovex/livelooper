#pragma once

#include "Types.h"

#include <algorithm>
#include <memory>

namespace livelooper
{
/**
    One looper track: recording, overdubbing, playback, one level of undo/redo.

    Memory model
    ------------
    Two full-length stereo layers are preallocated. The loop is split into chunks of
    kChunkSize samples and every chunk records which layer currently holds its audio.
    The first time an overdub touches a chunk, the chunk is copied to the other layer
    and ownership flips; the old audio stays untouched in the previous layer. Undo and
    redo therefore just flip the ownership of the chunks the last overdub touched -
    real-time safe, no large copies on the audio thread.

    All methods except allocate()/prepare() and the loop import/export functions are
    called on the audio thread.
*/
class LoopTrack
{
public:
    enum class State : int { Empty = 0, Recording, Playing, Overdubbing, Stopped };
    enum class UndoKind : int { None = 0, Recording, Overdub, Length, Rewrite };
    /** v1.6 loop tricks: the loop audio is rewritten (undoable like an overdub). */
    enum class Trick : int { HalfSpeed = 0, DoubleSpeed, Reverse, HalveLength, DoubleLength, NumTricks };

    static constexpr int kChunkShift = 9;
    static constexpr int kChunkSize  = 1 << kChunkShift;

    //============================================================================== setup
    void allocate (int maxLengthSamples);
    void prepare (double sampleRate);
    int  getMaxLength() const noexcept { return maxLength; }

    LoopTrack();
    ~LoopTrack();
    LoopTrack (const LoopTrack&) = delete;
    LoopTrack& operator= (const LoopTrack&) = delete;

    void setSettings (const TrackSettings& s) noexcept { settings = s; }
    /** Samples per ppq of the loop divided by those of the current tempo (for time-stretch). */
    void setTempoRatio (double r) noexcept { tempoRatio = r; }
    /** True while the playback goes through the time-stretch / pitch shifter. */
    bool isStretching() const noexcept { return stretchWasActive; }
    const TrackSettings& getSettings() const noexcept  { return settings; }
    void setFadeSamples (int n) noexcept               { fadeSamples = n < 1 ? 1 : n; }

    //============================================================================== actions (executed immediately)
    /** Starts the first recording. autoFinishSamples > 0 finishes it by itself after that many samples. */
    void startRecording (double ppqNow, double samplesPerPpq, int autoFinishSamples, RecAction afterAutoFinish);

    /** Ends the first recording. trimSamples > 0 cuts that many samples from the end
        (retroactive quantize); the trimmed audio is moved to the loop start if the track
        continues in overdub. */
    void finishRecording (RecAction next, int trimSamples = 0);

    void startOverdub();
    void endOverdub();                                   // overdub -> play
    void play (double ppqNow, bool syncToTimeline);
    void stop (bool allowFade);                          // honours the stop mode (fade) of the settings
    void stopAtLoopEnd() noexcept                        { stopAtWrap = true; }
    void clear();
    bool undo();
    bool redo();

    //============================================================================== v1.4 performance moves (audio thread)
    /** Loop cutting: from now on the loop is heard from logicalStart on (the running position
        keeps counting underneath, so cutOff() returns to it in sync). Short crossfade. */
    void cutTo (int logicalStart) noexcept;
    /** Stutter: repeats the lengthSamples after the current cut point while on. */
    void setStutter (bool on, int lengthSamples) noexcept;
    /** Back to the running position (in sync), crossfaded. */
    void cutOff() noexcept;
    bool isCut() const noexcept               { return readMap.type != MapType::Straight; }
    bool isStuttering() const noexcept        { return readMap.type == MapType::Stutter; }
    /** Logical loop position that is heard now (differs from the running position while cut). */
    int  getAudiblePosition() const noexcept  { return length > 0 ? mapPosition (readMap, playPos, runCounter) : 0; }
    /** Multiply: the loop is duplicated (length x 2). Playing or stopped tracks only. Undoable. */
    bool multiply() noexcept;
    /** Halves the loop length (keeps the first half). Playing or stopped tracks only. Undoable. */
    bool halve() noexcept;
    /** v1.6 loop tricks (audio thread). HalfSpeed: the loop plays at half speed, an octave lower,
        and becomes twice as long (only the first half is kept when the memory is too short).
        DoubleSpeed: twice as fast, an octave higher, played twice so the length stays.
        Reverse: the recorded audio itself is reversed. HalveLength / DoubleLength = halve() /
        multiply(). Playing or stopped tracks only; one UNDO restores the loop. */
    bool applyTrick (Trick t) noexcept;
    /** Copies the loop of another track into this (empty) track; it ends up stopped, anchored like the source. */
    bool copyFrom (const LoopTrack& source) noexcept;
    /** Sets / changes the planned length of the running first recording. Returns true when the
        recording is already longer (the caller should finish it at the next boundary). */
    bool setRecordingSpan (int samples) noexcept;

    /** Re-aligns the playback position with the timeline (after a transport jump). */
    void resyncToTimeline (double ppqNow);
    void invalidateAnchor() noexcept                     { anchorValid = false; }

    //============================================================================== processing
    /** Processes a block segment. out* receive the dry track signal (added), fx* receive the
        signal of tracks routed into the track FX bus (added). */
    void process (const float* inL, const float* inR,
                  float* outL, float* outR, float* fxL, float* fxR, int numSamples) noexcept;

    //============================================================================== state
    State    getState() const noexcept       { return state; }
    bool     isEmpty() const noexcept        { return state == State::Empty; }
    bool     isRunning() const noexcept      { return state == State::Recording || state == State::Playing || state == State::Overdubbing; }
    bool     isStopping() const noexcept     { return stopping; }
    int      getLength() const noexcept      { return length; }
    int      getRecordedSamples() const noexcept { return recPos; }
    /** Planned length of the running first recording (0 = open end). */
    int      getRecordingSpan() const noexcept { return state == State::Recording ? autoFinish : 0; }
    int      getPlayPosition() const noexcept { return playPos; }
    double   getSamplesPerPpq() const noexcept { return recSamplesPerPpq; }
    double   getAnchorPpq() const noexcept   { return anchorPpq; }
    bool     hasAnchor() const noexcept      { return anchorValid; }
    bool     canUndo() const noexcept        { return undoKind != UndoKind::None; }
    bool     canRedo() const noexcept        { return redoKind != UndoKind::None; }
    float    getPeak() noexcept              { auto p = peak; peak = 0.0f; return p; }
    /** Level for the track meter: the input while recording / overdubbing, else the output.
        Returns and resets the peaks since the last call. */
    void     takeMeter (float& l, float& r, bool& isInput) noexcept
    {
        isInput = state == State::Recording || state == State::Overdubbing;
        l = isInput ? inPeakL : outPeakL;
        r = isInput ? inPeakR : outPeakR;
        inPeakL = inPeakR = outPeakL = outPeakR = 0.0f;
    }

    /** Waveform overview for the display (any thread; reads may race harmlessly with the
        audio thread). Fills numBins peak values 0..1 over the loop (or the recording so far,
        spread over maxSpanSamples when > 0). Returns false when there is nothing to show. */
    bool getWaveform (float* bins, int numBins, int maxSpanSamples = 0) const noexcept;

    /** Normalised playback/record position 0..1 (recording: grows with recorded length). */
    float getProgress() const noexcept;

    //============================================================================== import / export (message thread, track locked)
    /** Copies the current loop (merged layers) into the destination (must hold getLength() samples). */
    void readLoop (float* dstL, float* dstR) const noexcept;
    /** Replaces the loop with the given audio; the track ends up stopped. */
    void loadLoop (const float* srcL, const float* srcR, int numSamples, double samplesPerPpq);
    /** Loads numSamples from a ring buffer starting at start (wrapping), crossfades the loop end
        into the audio that came before the start (xfade samples) and anchors it at anchorPpq. */
    void loadLoopFromRing (const float* ringL, const float* ringR, int ringSize, int start, int numSamples,
                           int xfade, double samplesPerPpq, double anchorPpq);

    /** Expected playback position for a timeline position (loop sync). */
    int expectedPosition (double ppq) const noexcept;

private:
    enum class MapType : int { Straight = 0, Offset, Stutter };
    /** How the running position maps to the heard loop position (loop cutting / stutter). */
    struct ReadMap
    {
        MapType type = MapType::Straight;
        int offset = 0;              // Offset: heard = running + offset
        int base = 0, len = 1;       // Stutter: heard = base + (counter - counter0) % len
        long long counter0 = 0;
        int runPos0 = 0;             // running position at counter0 (stretch path)
    };
    int  mapPosition (const ReadMap& m, int pos, long long counter) const noexcept;
    /** position for the time-stretch reader, which runs ahead of the running position */
    int  mapReadPosition (const ReadMap& m, int pos) const noexcept;
    void changeMap (const ReadMap& next) noexcept;
    void resetMap() noexcept;
    void applyMuteGain (float& g) noexcept;

    inline float sampleAt (int ch, int index) const noexcept
    {
        return layers[chunkOwner[(size_t) (index >> kChunkShift)]][ch][(size_t) index];
    }
    void touchChunkForWrite (int index) noexcept;
    void setGainRamp (float target, int samples) noexcept;
    void finishStop() noexcept;
    void routeInput (float l, float r, float& outL, float& outR) const noexcept;
    inline void notePeak (int layer, int index, float l, float r) noexcept
    {
        auto& a = chunkPeak[layer][(size_t) (index >> kChunkShift)];
        const float v = std::max (std::abs (l), std::abs (r));
        if (v > a.load (std::memory_order_relaxed))
            a.store (v, std::memory_order_relaxed);
    }
    void recomputeChunkPeak (int layer, size_t chunk) noexcept;
    void flipTouchedChunks() noexcept;   // v1.6
    bool wantsStretch() const noexcept;
    /** Renders numSamples of stretched / pitched loop audio into stretchOutL/R. */
    void renderStretched (int numSamples) noexcept;

    struct Stretch;
    std::unique_ptr<Stretch> stretchImpl;
    double tempoRatio = 1.0;
    double playFrac = 0.0;          // fractional part of the play position (ratio != 1)
    double readPos = 0.0;           // input position of the stretcher in the loop (ahead by its latency)
    int    expectedPlayPos = -1;    // detects jumps of the play position (re-seek)
    bool   stretchWasActive = false;
    float  lastPitch = 0.0f;

    std::vector<float> layers[2][2];          // [layer][channel]
    std::vector<uint8_t> chunkOwner;          // which layer holds the chunk
    std::vector<uint8_t> chunkTouched;        // chunk was modified by the current/last overdub
    std::unique_ptr<std::atomic<float>[]> chunkPeak[2];   // waveform overview per layer and chunk
    size_t numChunks = 0;

    TrackSettings settings;
    State state = State::Empty;

    int    maxLength = 0;
    int    length = 0;
    int    recPos = 0;
    int    playPos = 0;
    int    autoFinish = 0;
    int    savedLength = 0;                   // for redo of an undone first recording
    double sampleRate = 44100.0;
    double recSamplesPerPpq = 0.0;
    double anchorPpq = 0.0;
    bool   anchorValid = false;
    RecAction pendingRecAction = RecAction::RecDub;

    UndoKind undoKind = UndoKind::None;
    UndoKind redoKind = UndoKind::None;

    // gain ramp (fades and declicking)
    float gain = 0.0f, gainTarget = 0.0f, gainStep = 0.0f;
    int   gainRampLeft = 0;
    int   fadeSamples = 22050;
    bool  stopping = false;
    bool  stopAtWrap = false;

    // v1.4: loop cutting, mute
    ReadMap readMap, prevMap;
    long long runCounter = 0;        // counts every sample the running position advances
    long long cutCounter = 0;        // runCounter at the last cut
    int   mapFadeLeft = 0, mapFadeTotal = 1;
    float muteGain = 1.0f;
    int   altLength = 0;             // length to restore by undo / redo of multiply / halve

    float peak = 0.0f;
    float inPeakL = 0.0f, inPeakR = 0.0f, outPeakL = 0.0f, outPeakR = 0.0f;
};
} // namespace livelooper
