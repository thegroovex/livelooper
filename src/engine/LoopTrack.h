#pragma once

#include "Types.h"

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
    enum class UndoKind : int { None = 0, Recording, Overdub };

    static constexpr int kChunkShift = 9;
    static constexpr int kChunkSize  = 1 << kChunkShift;

    //============================================================================== setup
    void allocate (int maxLengthSamples);
    void prepare (double sampleRate);
    int  getMaxLength() const noexcept { return maxLength; }

    void setSettings (const TrackSettings& s) noexcept { settings = s; }
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
    int      getPlayPosition() const noexcept { return playPos; }
    double   getSamplesPerPpq() const noexcept { return recSamplesPerPpq; }
    double   getAnchorPpq() const noexcept   { return anchorPpq; }
    bool     hasAnchor() const noexcept      { return anchorValid; }
    bool     canUndo() const noexcept        { return undoKind != UndoKind::None; }
    bool     canRedo() const noexcept        { return redoKind != UndoKind::None; }
    float    getPeak() noexcept              { auto p = peak; peak = 0.0f; return p; }

    /** Normalised playback/record position 0..1 (recording: grows with recorded length). */
    float getProgress() const noexcept;

    //============================================================================== import / export (message thread, track locked)
    /** Copies the current loop (merged layers) into the destination (must hold getLength() samples). */
    void readLoop (float* dstL, float* dstR) const noexcept;
    /** Replaces the loop with the given audio; the track ends up stopped. */
    void loadLoop (const float* srcL, const float* srcR, int numSamples, double samplesPerPpq);

    /** Expected playback position for a timeline position (loop sync). */
    int expectedPosition (double ppq) const noexcept;

private:
    inline float sampleAt (int ch, int index) const noexcept
    {
        return layers[chunkOwner[(size_t) (index >> kChunkShift)]][ch][(size_t) index];
    }
    void touchChunkForWrite (int index) noexcept;
    void setGainRamp (float target, int samples) noexcept;
    void finishStop() noexcept;
    void routeInput (float l, float r, float& outL, float& outR) const noexcept;

    std::vector<float> layers[2][2];          // [layer][channel]
    std::vector<uint8_t> chunkOwner;          // which layer holds the chunk
    std::vector<uint8_t> chunkTouched;        // chunk was modified by the current/last overdub

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

    float peak = 0.0f;
};
} // namespace livelooper
