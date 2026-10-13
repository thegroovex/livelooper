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
        const int maxLen = t < activeTracks.load() ? (int) (sampleRate * maxLoopSeconds) : 0;
        if (tr.getMaxLength() != std::max (LoopTrack::kChunkSize, maxLen))
            tr.allocate (maxLen);      // keeps existing loops when only the block size changed
        pending[(size_t) t] = {};
    }

    inputFx.prepare (sampleRate);
    trackFx.prepare (sampleRate);
    rhythm.prepare (sampleRate);

    for (auto* v : { &inProcL, &inProcR, &fxBusL, &fxBusR, &clickBuf, &kickBuf, &duckEnvBuf,
                     &tOutL, &tOutR, &tFxL, &tFxR, &resL, &resR })
        v->assign ((size_t) maxBlock, 0.0f);

    const auto ringSize = (size_t) (sampleRate * maxLoopSeconds);
    if (ringL.size() != ringSize)
    {
        ringL.assign (ringSize, 0.0f);
        ringR.assign (ringSize, 0.0f);
        ringWrite = 0;
        ringFilled.store (0);
    }
    duckRelease = (float) std::exp (-1.0 / (0.18 * sampleRate));

    numQueued = 0;
}

void LooperEngine::setActiveTracks (int n)
{
    n = std::clamp (n, 1, kNumTracks);
    activeTracks.store (n);
    const int maxLen = (int) (sampleRate * maxLoopSeconds);
    for (int t = 0; t < kNumTracks; ++t)
    {
        auto& tr = tracks[(size_t) t];
        const int want = std::max (LoopTrack::kChunkSize, t < n ? maxLen : 0);
        if (tr.getMaxLength() == want)
            continue;
        const juce::SpinLock::ScopedLockType lock (trackLocks[(size_t) t]);
        pending[(size_t) t] = {};
        tr.allocate (t < n ? maxLen : 0);       // hidden tracks give their memory back (and lose their loop)
    }
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

    if (pa.type == Pending::Record || pa.type == Pending::Play || pa.type == Pending::AutoRecord)
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
            if (t >= activeTracks.load())
                return;
            if (global.autoRecord)
            {
                pa = { Pending::AutoRecord, 0.0 };   // waits for the input to exceed the threshold
                return;
            }
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
        case Pending::AutoRecord:
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
                if (pending[(size_t) i].type == Pending::Play || pending[(size_t) i].type == Pending::Record
                    || pending[(size_t) i].type == Pending::AutoRecord)
                    pending[(size_t) i].type = Pending::None;
                if (tracks[(size_t) i].isRunning())
                    trackStop (i);
            }
            break;
        case Command::Capture:
        {
            int target = t;
            if (! validTrack)
                for (int i = 0; i < activeTracks.load() && target < 0; ++i)
                    if (tracks[(size_t) i].isEmpty() && pending[(size_t) i].type == Pending::None)
                        target = i;
            if (target >= 0 && target < activeTracks.load())
                captureInto (target);
            break;
        }
        case Command::Start:
            if (validTrack && pending[(size_t) t].type == Pending::None)
            {
                const auto& tr = tracks[(size_t) t];
                if (tr.getState() == LoopTrack::State::Stopped || (tr.getState() == LoopTrack::State::Playing && tr.isStopping()))
                    trackPlayQuantized (t);
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
bool LooperEngine::captureInto (int t) noexcept
{
    auto& tr = tracks[(size_t) t];
    if (! tr.isEmpty() || ringL.empty())
        return false;

    const int ringSize = (int) ringL.size();
    const int filled = ringFilled.load (std::memory_order_relaxed);
    const int xfade = std::min (512, (int) (0.005 * sampleRate));
    const double barSamples = measurePpq * samplesPerPpq;
    int bars = std::max (1, global.captureBars);

    // samples from the last bar line to now: the captured loop ends on a bar line and
    // continues in phase when the timeline is already running
    const double prevBar = running ? std::floor ((blockPpq + 1.0e-9) / measurePpq) * measurePpq : 0.0;
    const int sinceBar = running ? (int) std::llround ((blockPpq - prevBar) * samplesPerPpq) : 0;

    auto lengthFor = [barSamples] (int b) { return (int) std::llround (b * barSamples); };
    while (bars > 1 && (lengthFor (bars) + sinceBar + xfade > filled || lengthFor (bars) > tr.getMaxLength()))
        bars /= 2;
    const int len = lengthFor (bars);
    if (len + sinceBar + xfade > filled || len > tr.getMaxLength() || len < 64)
        return false;

    const int start = ((ringWrite - sinceBar - len) % ringSize + ringSize) % ringSize;
    const juce::SpinLock::ScopedTryLockType lock (trackLocks[(size_t) t]);
    if (! lock.isLocked())
        return false;

    if (! running)
        ensureClockRunning();
    const double anchor = running ? prevBar - bars * measurePpq : 0.0;
    tr.loadLoopFromRing (ringL.data(), ringR.data(), ringSize, start, len, xfade, samplesPerPpq, anchor);
    tr.play (blockPpq, true);
    return true;
}

void LooperEngine::processTrack (int t, const float* srcL, const float* srcR, int n, double chunkStartPpq) noexcept
{
    auto& lock = trackLocks[(size_t) t];
    if (! lock.tryEnter())
        return;

    auto& tr = tracks[(size_t) t];
    int done = 0;
    while (done < n)
    {
        int segEnd = n;
        const auto& pa = pending[(size_t) t];
        if (pa.type == Pending::AutoRecord)
        {
            // starts with the first sample above the threshold
            const float th = global.autoRecThreshold;
            segEnd = n;
            for (int i = done; i < n; ++i)
                if (std::abs (srcL[i]) >= th || std::abs (srcR[i]) >= th)
                {
                    segEnd = i;
                    break;
                }
        }
        else if (pa.type != Pending::None)
        {
            if (! running)
                segEnd = done;
            else
            {
                const double off = (pa.targetPpq - (chunkStartPpq + done * ppqPerSample)) * samplesPerPpq;
                segEnd = std::clamp (done + (int) std::ceil (off - 1.0e-6), done, n);
            }
        }

        if (segEnd > done)
            tr.process (srcL + done, srcR + done, tOutL.data() + done, tOutR.data() + done,
                        tFxL.data() + done, tFxR.data() + done, segEnd - done);

        if (segEnd >= n)
            break;
        if (pending[(size_t) t].type == Pending::None)
        {
            done = segEnd;
            continue;
        }
        if (pending[(size_t) t].type == Pending::AutoRecord && ! running)
            ensureClockRunning();
        if (! running && pending[(size_t) t].type != Pending::AutoRecord)
            break;     // waits for a clock that is not running (should not happen)
        blockPpq = chunkStartPpq + segEnd * ppqPerSample;
        executePending (t);
        blockPpq = chunkStartPpq;
        done = segEnd;
    }
    lock.exit();
}

//==============================================================================
void LooperEngine::process (const float* inL, const float* inR, float* outL, float* outR, const ExtraOutputs& extra,
                            int numSamples, const TimeInfo& time) noexcept
{
    int done = 0;
    TimeInfo t = time;
    while (done < numSamples)
    {
        const int n = std::min (maxBlock, numSamples - done);
        processChunk (inL + done, inR + done, outL + done, outR + done, extra, done, n, t);
        done += n;
        if (t.hostPlaying)
            t.ppq += n * (t.bpm / (60.0 * sampleRate));
    }
}

void LooperEngine::processChunk (const float* inL, const float* inR, float* outL, float* outR, const ExtraOutputs& extra,
                                 int offset, int n, const TimeInfo& time) noexcept
{
    float* cueL = extra.cueL != nullptr ? extra.cueL + offset : nullptr;
    float* cueR = extra.cueR != nullptr ? extra.cueR + offset : nullptr;
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
    {
        tr.setFadeSamples (fadeSamples);
        tr.setTempoRatio (tr.getSamplesPerPpq() > 0.0 ? tr.getSamplesPerPpq() / samplesPerPpq : 1.0);
    }

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

    // ---------------------------------------------------------------- retrospective capture buffer
    if (! ringL.empty())
    {
        const int size = (int) ringL.size();
        for (int i = 0; i < n; ++i)
        {
            ringL[(size_t) ringWrite] = inProcL[(size_t) i];
            ringR[(size_t) ringWrite] = inProcR[(size_t) i];
            if (++ringWrite >= size)
                ringWrite = 0;
        }
        ringFilled.store (std::min (size, ringFilled.load (std::memory_order_relaxed) + n), std::memory_order_relaxed);
    }

    // ---------------------------------------------------------------- rhythm (rendered first: it can duck the tracks)
    const double chunkStartPpq = blockPpq;
    std::fill (clickBuf.begin(), clickBuf.begin() + n, 0.0f);
    std::fill (kickBuf.begin(), kickBuf.begin() + n, 0.0f);
    rhythm.process (clickBuf.data(), clickBuf.data(), n, chunkStartPpq, ppqPerSample,
                    (int) std::lround (measurePpq), running && rhythmOn, kickBuf.data());
    // process() adds to both pointers: they are the same buffer, so halve it again
    for (int i = 0; i < n; ++i)
        clickBuf[(size_t) i] *= 0.5f;

    auto followEnvelope = [this, n] (const float* srcL, const float* srcR) {
        for (int i = 0; i < n; ++i)
        {
            const float x = std::max (std::abs (srcL[i]), srcR != nullptr ? std::abs (srcR[i]) : 0.0f);
            duckEnv = x > duckEnv ? x : duckEnv * duckRelease;
            duckEnvBuf[(size_t) i] = std::min (1.0f, duckEnv * 2.0f);
        }
    };
    if (global.duckSource == DuckSource::RhythmKick)
        followEnvelope (kickBuf.data(), nullptr);

    // ---------------------------------------------------------------- tracks
    // pass 0: tracks that record the input, pass 1: tracks that resample the mix of the others
    std::fill (resL.begin(), resL.begin() + n, 0.0f);
    std::fill (resR.begin(), resR.begin() + n, 0.0f);
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int t = 0; t < kNumTracks; ++t)
        {
            auto& tr = tracks[(size_t) t];
            const auto& ts = tr.getSettings();
            const bool resample = ts.input == InputRoute::Resample;
            if (resample != (pass == 1))
                continue;

            std::fill (tOutL.begin(), tOutL.begin() + n, 0.0f);
            std::fill (tOutR.begin(), tOutR.begin() + n, 0.0f);
            std::fill (tFxL.begin(), tFxL.begin() + n, 0.0f);
            std::fill (tFxR.begin(), tFxR.begin() + n, 0.0f);

            processTrack (t, resample ? resL.data() : inProcL.data(), resample ? resR.data() : inProcR.data(), n, chunkStartPpq);

            if (t == 0 && global.duckSource == DuckSource::Track1)
            {
                // the envelope of track 1 (before its own ducking, which never applies to the source)
                std::vector<float>& a = tOutL; std::vector<float>& b2 = tOutR;
                for (int i = 0; i < n; ++i)
                {
                    const float x = std::max (std::max (std::abs (a[(size_t) i]), std::abs (b2[(size_t) i])),
                                              std::max (std::abs (tFxL[(size_t) i]), std::abs (tFxR[(size_t) i])));
                    duckEnv = x > duckEnv ? x : duckEnv * duckRelease;
                    duckEnvBuf[(size_t) i] = std::min (1.0f, duckEnv * 2.0f);
                }
            }

            // sidechain ducking
            float minGain = 1.0f;
            const bool selfSource = t == 0 && global.duckSource == DuckSource::Track1;
            if (ts.duck > 0.0f && ! selfSource)
            {
                for (int i = 0; i < n; ++i)
                {
                    const float g = 1.0f - ts.duck * duckEnvBuf[(size_t) i];
                    minGain = std::min (minGain, g);
                    tOutL[(size_t) i] *= g; tOutR[(size_t) i] *= g;
                    tFxL[(size_t) i] *= g;  tFxR[(size_t) i] *= g;
                }
            }
            status[(size_t) t].duckGain.store (minGain, std::memory_order_relaxed);

            // routing: main output (FX-routed tracks via the track FX bus) and / or the direct output
            float* dL = extra.directL[(size_t) t] != nullptr ? extra.directL[(size_t) t] + offset : nullptr;
            float* dR = extra.directR[(size_t) t] != nullptr ? extra.directR[(size_t) t] + offset : nullptr;
            const bool hasDirect = dL != nullptr && dR != nullptr;
            const bool toMain = ! hasDirect || ts.output != TrackOutput::Direct;
            const bool toDirect = hasDirect && ts.output != TrackOutput::Main;
            for (int i = 0; i < n; ++i)
            {
                const auto k = (size_t) i;
                if (toMain)
                {
                    outL[i] += tOutL[k]; outR[i] += tOutR[k];
                    fxBusL[k] += tFxL[k]; fxBusR[k] += tFxR[k];
                }
                if (toDirect)
                {
                    dL[i] += (tOutL[k] + tFxL[k]) * global.masterLevel;
                    dR[i] += (tOutR[k] + tFxR[k]) * global.masterLevel;
                }
                if (pass == 0)
                {
                    resL[k] += tOutL[k] + tFxL[k];
                    resR[k] += tOutR[k] + tFxR[k];
                }
            }
        }
    }

    // ---------------------------------------------------------------- track FX, rhythm, monitor, master
    trackFx.process (fxBusL.data(), fxBusR.data(), n, chunkStartPpq, ppqPerSample, bpm);

    const bool hasCue = cueL != nullptr && cueR != nullptr;
    const bool clickToMain = ! hasCue || global.clickRoute != ClickRoute::Cue;
    const bool clickToCue = hasCue && global.clickRoute != ClickRoute::Main;

    const float mon = global.monitorLevel;
    const float master = global.masterLevel;
    float pl = 0.0f, pr = 0.0f, pc = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float click = clickBuf[(size_t) i];
        const float cm = clickToMain ? click : 0.0f;
        outL[i] = (outL[i] + cm + fxBusL[(size_t) i] + inProcL[(size_t) i] * mon) * master;
        outR[i] = (outR[i] + cm + fxBusR[(size_t) i] + inProcR[(size_t) i] * mon) * master;
        pl = std::max (pl, std::abs (outL[i]));
        pr = std::max (pr, std::abs (outR[i]));
    }
    if (hasCue)
    {
        for (int i = 0; i < n; ++i)
        {
            const float c = clickToCue ? clickBuf[(size_t) i] : 0.0f;
            cueL[i] = c;
            cueR[i] = c;
            pc = std::max (pc, std::abs (c));
        }
    }
    if (pl > masterPeakL.load (std::memory_order_relaxed)) masterPeakL.store (pl, std::memory_order_relaxed);
    if (pr > masterPeakR.load (std::memory_order_relaxed)) masterPeakR.store (pr, std::memory_order_relaxed);
    if (pc > cuePeak.load (std::memory_order_relaxed))     cuePeak.store (pc, std::memory_order_relaxed);

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
        {
            float ml, mr;
            bool in;
            tr.takeMeter (ml, mr, in);
            // hold the maximum until the UI reads it (exchange in the UI)
            if (ml > st.meterL.load (std::memory_order_relaxed)) st.meterL.store (ml, std::memory_order_relaxed);
            if (mr > st.meterR.load (std::memory_order_relaxed)) st.meterR.store (mr, std::memory_order_relaxed);
            st.meterIsInput.store (in, std::memory_order_relaxed);
        }
        st.lengthSamples.store (tr.getLength());
        st.lengthPpq.store (tr.getSamplesPerPpq() > 0.0 ? tr.getLength() / tr.getSamplesPerPpq() : 0.0);
        st.canUndo.store (tr.canUndo());
        st.canRedo.store (tr.canRedo());
        st.stopping.store (tr.isStopping());
        float ghost = -1.0f;
        if (tr.getState() == LoopTrack::State::Stopped && running && tr.hasAnchor() && tr.getLength() > 0)
            ghost = (float) tr.expectedPosition (blockPpq) / (float) tr.getLength();
        st.ghostProgress.store (ghost);
    }
    statusPpq.store (running ? blockPpq : 0.0);
    statusBpm.store (bpm);
    statusRunning.store (running);
    statusHost.store (hostDriven);
    statusMeasurePpq.store (measurePpq);
}
} // namespace livelooper
