#include "LooperEngine.h"

#include <algorithm>

namespace livelooper
{
namespace
{
    /** Pressing finish within this fraction of a grid step after a boundary ends the loop
        at that boundary (retroactively) instead of waiting for the next one. */
    constexpr double kRetroWindow = 0.15;
}

LooperEngine::LooperEngine() = default;

void LooperEngine::prepare (double newSampleRate, int maxBlockSize, double newMaxLoopSeconds)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    maxBlock = std::max (32, maxBlockSize);
    maxLoopSeconds = std::max (1.0, newMaxLoopSeconds);

    for (int t = 0; t < kNumTracks; ++t)
    {
        const juce::SpinLock::ScopedLockType lock (trackLocks[(size_t) t]);
        auto& tr = tracks[(size_t) t];
        tr.prepare (sampleRate);
        const int maxLen = (int) (sampleRate * maxLoopSeconds);
        if (tr.getMaxLength() != std::max (LoopTrack::kChunkSize, maxLen))
            tr.allocate (maxLen);      // keeps existing loops when only the block size changed
        pending[(size_t) t] = {};
    }

    inputFx.prepare (sampleRate);
    trackFx.prepare (sampleRate);
    rhythm.prepare (sampleRate);

    for (auto* v : { &inProcL, &inProcR, &fxBusL, &fxBusR })
        v->assign ((size_t) maxBlock, 0.0f);

    numQueued = 0;
}

void LooperEngine::reset()
{
    inputFx.reset();
    trackFx.reset();
    rhythm.reset();
}

void LooperEngine::setRhythm (bool on, int pattern, float level) noexcept
{
    rhythmOn = on;
    rhythm.setPattern (pattern);
    rhythm.setLevel (level);
}

void LooperEngine::queueCommand (Command c, int track) noexcept
{
    if (numQueued < (int) commandQueue.size())
        commandQueue[(size_t) numQueued++] = { c, track };
}

//==============================================================================
double LooperEngine::gridPpq() const noexcept
{
    switch (global.quantize)
    {
        case Quantize::Beat:    return measurePpq / std::max (1, beatsPerMeasure);
        case Quantize::Measure: return measurePpq;
        case Quantize::Off:     break;
    }
    return 0.0;
}

double LooperEngine::nextBoundary (double grid) const noexcept
{
    return std::ceil (blockPpq / grid - 1.0e-6) * grid;
}

int LooperEngine::autoFinishSamples (int t) const noexcept
{
    const int m = tracks[(size_t) t].getSettings().measures;
    if (m <= 0)
        return 0;
    return (int) std::llround (m * measurePpq * samplesPerPpq);
}

bool LooperEngine::anythingNeedsClock() const noexcept
{
    if (rhythmOn)
        return true;
    for (int t = 0; t < kNumTracks; ++t)
        if (tracks[(size_t) t].isRunning() || pending[(size_t) t].type != Pending::None)
            return true;
    return false;
}

void LooperEngine::ensureClockRunning() noexcept
{
    if (running)
        return;
    internalRunning = true;
    internalPpq = 0.0;
    blockPpq = 0.0;
    running = true;
    for (auto& tr : tracks)
        tr.invalidateAnchor();
}

//==============================================================================
void LooperEngine::finishRecordingQuantized (int t, bool thenStop) noexcept
{
    auto& tr = tracks[(size_t) t];
    auto& pa = pending[(size_t) t];
    const auto next = thenStop ? RecAction::RecPlay : global.recAction;

    if (global.quantize == Quantize::Off || ! running)
    {
        tr.finishRecording (next);
        if (thenStop)
            tr.stop (false);
        return;
    }

    const double grid = gridPpq();
    const double anchor = tr.getAnchorPpq();
    const double prev = std::floor ((blockPpq + 1.0e-9) / grid) * grid;

    if (prev > anchor + 1.0e-6 && blockPpq - prev < kRetroWindow * grid)
    {
        const int expectedLen = (int) std::llround ((prev - anchor) * tr.getSamplesPerPpq());
        const int trim = tr.getRecordedSamples() - expectedLen;
        if (trim >= 0)
        {
            tr.finishRecording (next, trim);
            if (thenStop)
                tr.stop (false);
            return;
        }
    }

    double target = nextBoundary (grid);
    if (target <= anchor + 1.0e-6)
        target += grid;
    pa = { thenStop ? Pending::FinishRecordThenStop : Pending::FinishRecord, target };
}

void LooperEngine::trackPlayQuantized (int t) noexcept
{
    auto& tr = tracks[(size_t) t];
    if (! running)
    {
        ensureClockRunning();
        tr.play (blockPpq, true);
        return;
    }
    if (global.quantize == Quantize::Off)
    {
        tr.play (blockPpq, true);
        return;
    }
    const double target = nextBoundary (gridPpq());
    if (target - blockPpq < 1.0e-6)
        tr.play (blockPpq, true);
    else
        pending[(size_t) t] = { Pending::Play, target };
}

void LooperEngine::trackRecPlay (int t) noexcept
{
    auto& tr = tracks[(size_t) t];
    auto& pa = pending[(size_t) t];

    if (pa.type == Pending::Record || pa.type == Pending::Play)
    {
        pa.type = Pending::None;      // second press while waiting cancels
        return;
    }
    if (pa.type == Pending::FinishRecord || pa.type == Pending::FinishRecordThenStop)
        return;

    switch (tr.getState())
    {
        case LoopTrack::State::Empty:
        {
            if (! running)
                ensureClockRunning();
            else if (global.quantize != Quantize::Off)
            {
                const double target = nextBoundary (gridPpq());
                if (target - blockPpq > 1.0e-6)
                {
                    pa = { Pending::Record, target };
                    return;
                }
            }
            tr.startRecording (blockPpq, samplesPerPpq, autoFinishSamples (t), global.recAction);
            break;
        }
        case LoopTrack::State::Recording:   finishRecordingQuantized (t, false); break;
        case LoopTrack::State::Overdubbing: tr.endOverdub(); break;
        case LoopTrack::State::Playing:
            if (tr.isStopping()) tr.play (blockPpq, true);
            else                 tr.startOverdub();
            break;
        case LoopTrack::State::Stopped:     trackPlayQuantized (t); break;
    }
}

void LooperEngine::trackStop (int t) noexcept
{
    auto& tr = tracks[(size_t) t];
    auto& pa = pending[(size_t) t];

    if (pa.type == Pending::Record || pa.type == Pending::Play)
    {
        pa.type = Pending::None;
        return;
    }
    if (pa.type == Pending::FinishRecord)
    {
        pa.type = Pending::FinishRecordThenStop;
        return;
    }

    switch (tr.getState())
    {
        case LoopTrack::State::Recording:
            finishRecordingQuantized (t, true);
            break;
        case LoopTrack::State::Playing:
        case LoopTrack::State::Overdubbing:
            if (tr.getSettings().stopMode == StopMode::LoopEnd)
            {
                tr.endOverdub();
                tr.stopAtLoopEnd();
            }
            else
                tr.stop (true);
            break;
        case LoopTrack::State::Empty:
        case LoopTrack::State::Stopped:
            break;
    }
}

void LooperEngine::executePending (int t) noexcept
{
    auto& tr = tracks[(size_t) t];
    auto& pa = pending[(size_t) t];
    const auto type = pa.type;
    pa.type = Pending::None;

    switch (type)
    {
        case Pending::Record:
            tr.startRecording (blockPpq, samplesPerPpq, autoFinishSamples (t), global.recAction);
            break;
        case Pending::FinishRecord:
            tr.finishRecording (global.recAction);
            break;
        case Pending::FinishRecordThenStop:
            tr.finishRecording (RecAction::RecPlay);
            tr.stop (false);
            break;
        case Pending::Play:
            tr.play (blockPpq, true);
            break;
        case Pending::None:
            break;
    }
}

void LooperEngine::executeCommand (const QueuedCommand& c) noexcept
{
    const int t = c.track;
    const bool validTrack = t >= 0 && t < kNumTracks;

    switch (c.command)
    {
        case Command::RecPlay: if (validTrack) trackRecPlay (t); break;
        case Command::Stop:    if (validTrack) trackStop (t); break;
        case Command::Clear:
            if (validTrack)
            {
                pending[(size_t) t].type = Pending::None;
                tracks[(size_t) t].clear();
            }
            break;
        case Command::Undo: if (validTrack) tracks[(size_t) t].undo(); break;
        case Command::Redo: if (validTrack) tracks[(size_t) t].redo(); break;

        case Command::AllStartStop:
        {
            bool anyActive = false;
            for (int i = 0; i < kNumTracks; ++i)
            {
                const auto& tr = tracks[(size_t) i];
                if ((tr.isRunning() && ! tr.isStopping()) || pending[(size_t) i].type == Pending::Play)
                    anyActive = true;
            }
            executeCommand ({ anyActive ? Command::AllStop : Command::AllStart, -1 });
            break;
        }
        case Command::AllStart:
            for (int i = 0; i < kNumTracks; ++i)
            {
                const auto& tr = tracks[(size_t) i];
                if (pending[(size_t) i].type != Pending::None)
                    continue;
                if (tr.getState() == LoopTrack::State::Stopped || (tr.getState() == LoopTrack::State::Playing && tr.isStopping()))
                    trackPlayQuantized (i);
            }
            break;
        case Command::AllStop:
            for (int i = 0; i < kNumTracks; ++i)
            {
                if (pending[(size_t) i].type == Pending::Play || pending[(size_t) i].type == Pending::Record)
                    pending[(size_t) i].type = Pending::None;
                if (tracks[(size_t) i].isRunning())
                    trackStop (i);
            }
            break;
        case Command::ClearAll:
            for (int i = 0; i < kNumTracks; ++i)
            {
                pending[(size_t) i].type = Pending::None;
                tracks[(size_t) i].clear();
            }
            break;
    }
}

//==============================================================================
void LooperEngine::process (const float* inL, const float* inR, float* outL, float* outR, int numSamples, const TimeInfo& time) noexcept
{
    int done = 0;
    TimeInfo t = time;
    while (done < numSamples)
    {
        const int n = std::min (maxBlock, numSamples - done);
        processChunk (inL + done, inR + done, outL + done, outR + done, n, t);
        done += n;
        if (t.hostPlaying)
            t.ppq += n * (t.bpm / (60.0 * sampleRate));
    }
}

void LooperEngine::processChunk (const float* inL, const float* inR, float* outL, float* outR, int n, const TimeInfo& time) noexcept
{
    // ---------------------------------------------------------------- timeline
    bpm = (global.syncToHost && time.bpm > 0.0) ? time.bpm : global.internalTempo;
    bpm = std::clamp (bpm, 20.0, 400.0);
    const int num = std::max (1, time.numerator);
    const int den = std::max (1, time.denominator);
    measurePpq = global.syncToHost ? num * 4.0 / den : 4.0;
    beatsPerMeasure = global.syncToHost ? num : 4;
    ppqPerSample = bpm / (60.0 * sampleRate);
    samplesPerPpq = 1.0 / ppqPerSample;

    const bool hostPlaying = global.syncToHost && time.hostPlaying;
    bool hostStarted = false, hostStopped = false, jumped = false;

    if (hostPlaying)
    {
        if (! wasHostPlaying)
            hostStarted = true;
        else if (std::abs (time.ppq - expectedHostPpq) > std::max (0.02, ppqPerSample * 16.0))
            jumped = true;
        blockPpq = time.ppq;
        hostDriven = true;
        running = true;
        internalRunning = false;
    }
    else
    {
        if (wasHostPlaying)
        {
            hostStopped = true;
            internalPpq = expectedHostPpq;     // continue seamlessly on the internal clock
            internalRunning = ! followHostTransport && anythingNeedsClock();
        }
        hostDriven = false;
        blockPpq = internalPpq;
        running = internalRunning;
    }
    wasHostPlaying = hostPlaying;

    const int fadeSamples = (int) (global.fadeMs * 0.001 * sampleRate);
    for (auto& tr : tracks)
        tr.setFadeSamples (fadeSamples);

    if (hostStarted || jumped)
        for (auto& tr : tracks)
            tr.resyncToTimeline (blockPpq);

    if (followHostTransport)
    {
        if (hostStarted)
            for (int i = 0; i < kNumTracks; ++i)
                if (tracks[(size_t) i].getState() == LoopTrack::State::Stopped)
                    tracks[(size_t) i].play (blockPpq, true);
        if (hostStopped)
            for (int i = 0; i < kNumTracks; ++i)
            {
                pending[(size_t) i].type = Pending::None;
                if (tracks[(size_t) i].isRunning())
                    tracks[(size_t) i].stop (false);
            }
    }

    // ---------------------------------------------------------------- commands
    for (int i = 0; i < numQueued; ++i)
        executeCommand (commandQueue[(size_t) i]);
    numQueued = 0;

    if (! hostDriven && internalRunning && ! anythingNeedsClock())
    {
        bool fading = false;
        for (const auto& tr : tracks)
            fading = fading || tr.isStopping();
        if (! fading)
            internalRunning = running = false;
    }
    if (! hostDriven && ! internalRunning && rhythmOn)
        ensureClockRunning();

    // ---------------------------------------------------------------- input + input FX
    std::copy (inL, inL + n, inProcL.data());
    std::copy (inR, inR + n, inProcR.data());
    inputFx.process (inProcL.data(), inProcR.data(), n, blockPpq, ppqPerSample, bpm);

    std::fill (outL, outL + n, 0.0f);
    std::fill (outR, outR + n, 0.0f);
    std::fill (fxBusL.begin(), fxBusL.begin() + n, 0.0f);
    std::fill (fxBusR.begin(), fxBusR.begin() + n, 0.0f);

    // ---------------------------------------------------------------- tracks
    const double chunkStartPpq = blockPpq;
    for (int t = 0; t < kNumTracks; ++t)
    {
        auto& lock = trackLocks[(size_t) t];
        if (! lock.tryEnter())
            continue;

        auto& tr = tracks[(size_t) t];
        int done = 0;
        while (done < n)
        {
            int segEnd = n;
            const auto& pa = pending[(size_t) t];
            if (pa.type != Pending::None)
            {
                if (! running)
                    segEnd = done;
                else
                {
                    const double offset = (pa.targetPpq - (chunkStartPpq + done * ppqPerSample)) * samplesPerPpq;
                    segEnd = std::clamp (done + (int) std::ceil (offset - 1.0e-6), done, n);
                }
            }

            if (segEnd > done)
                tr.process (inProcL.data() + done, inProcR.data() + done, outL + done, outR + done,
                            fxBusL.data() + done, fxBusR.data() + done, segEnd - done);

            done = segEnd;
            if (done < n && pending[(size_t) t].type != Pending::None)
            {
                blockPpq = chunkStartPpq + done * ppqPerSample;
                executePending (t);
                blockPpq = chunkStartPpq;
            }
        }
        lock.exit();
    }

    // ---------------------------------------------------------------- track FX, rhythm, monitor, master
    trackFx.process (fxBusL.data(), fxBusR.data(), n, chunkStartPpq, ppqPerSample, bpm);
    rhythm.process (outL, outR, n, chunkStartPpq, ppqPerSample,
                    (int) std::lround (measurePpq), running && rhythmOn);

    const float mon = global.monitorLevel;
    const float master = global.masterLevel;
    for (int i = 0; i < n; ++i)
    {
        outL[i] = (outL[i] + fxBusL[(size_t) i] + inProcL[(size_t) i] * mon) * master;
        outR[i] = (outR[i] + fxBusR[(size_t) i] + inProcR[(size_t) i] * mon) * master;
    }

    // ---------------------------------------------------------------- advance
    const double endPpq = chunkStartPpq + n * ppqPerSample;
    if (hostDriven)
        expectedHostPpq = endPpq;
    else if (internalRunning)
        internalPpq = endPpq;
    blockPpq = running ? endPpq : blockPpq;

    updateStatus();
}

void LooperEngine::updateStatus() noexcept
{
    for (int t = 0; t < kNumTracks; ++t)
    {
        auto& tr = tracks[(size_t) t];
        auto& st = status[(size_t) t];
        st.state.store ((int) tr.getState());
        st.pending.store ((int) pending[(size_t) t].type);
        st.progress.store (tr.getProgress());
        st.peak.store (std::max (st.peak.load() * 0.9f, tr.getPeak()));
        st.lengthSamples.store (tr.getLength());
        st.lengthPpq.store (tr.getSamplesPerPpq() > 0.0 ? tr.getLength() / tr.getSamplesPerPpq() : 0.0);
        st.canUndo.store (tr.canUndo());
        st.canRedo.store (tr.canRedo());
        st.stopping.store (tr.isStopping());
    }
    statusPpq.store (running ? blockPpq : 0.0);
    statusBpm.store (bpm);
    statusRunning.store (running);
    statusHost.store (hostDriven);
    statusMeasurePpq.store (measurePpq);
}
} // namespace livelooper
