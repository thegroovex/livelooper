#include "LoopTrack.h"

#if defined (__GNUC__)
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wshadow"
 #pragma GCC diagnostic ignored "-Wconversion"
 #pragma GCC diagnostic ignored "-Wsign-conversion"
 #pragma GCC diagnostic ignored "-Wfloat-conversion"
 #pragma GCC diagnostic ignored "-Wunused-parameter"
#elif defined (_MSC_VER)
 #pragma warning (push, 0)
#endif
#include <signalsmith-stretch/signalsmith-stretch.h>
#if defined (__GNUC__)
 #pragma GCC diagnostic pop
#elif defined (_MSC_VER)
 #pragma warning (pop)
#endif

#include <algorithm>
#include <cstring>

namespace livelooper
{
namespace
{
    constexpr float kDeclickMs = 5.0f;
}

//==============================================================================
struct LoopTrack::Stretch
{
    static constexpr int kMaxBlock = 512;      // output samples per stretcher call
    static constexpr double kMaxRatio = 4.0;
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    std::vector<float> inL, inR, outL, outR, seekL, seekR;
    bool configured = false;
    double configuredRate = 0.0;

    void configure (double sampleRate)
    {
        if (configured && configuredRate == sampleRate)
            return;
        stretch.presetDefault (2, (float) sampleRate);
        const auto inMax = (size_t) (kMaxBlock * kMaxRatio) + 8;
        inL.assign (inMax, 0.0f); inR.assign (inMax, 0.0f);
        outL.assign (8192, 0.0f); outR.assign (8192, 0.0f);
        const auto seekMax = (size_t) stretch.seekLength() + 8;
        seekL.assign (seekMax, 0.0f); seekR.assign (seekMax, 0.0f);
        configured = true;
        configuredRate = sampleRate;
    }
};

LoopTrack::LoopTrack() : stretchImpl (std::make_unique<Stretch>()) {}
LoopTrack::~LoopTrack() = default;

void LoopTrack::allocate (int maxLengthSamples)
{
    maxLength = std::max (kChunkSize, maxLengthSamples);
    const auto numChunks = (size_t) ((maxLength + kChunkSize - 1) >> kChunkShift);
    const auto padded = numChunks << kChunkShift;

    for (auto& layer : layers)
        for (auto& ch : layer)
            ch.assign (padded, 0.0f);

    chunkOwner.assign (numChunks, 0);
    chunkTouched.assign (numChunks, 0);
    this->numChunks = numChunks;
    for (auto& cp : chunkPeak)
    {
        cp.reset (new std::atomic<float>[numChunks]);
        for (size_t i = 0; i < numChunks; ++i)
            cp[i].store (0.0f);
    }
    clear();
}

void LoopTrack::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    stretchImpl->configure (sampleRate);
    stretchWasActive = false;
}

bool LoopTrack::wantsStretch() const noexcept
{
    if (state != State::Playing || length < 4096 || ! stretchImpl->configured)
        return false;
    const bool pitched = std::abs (settings.pitch) > 0.01f;
    const bool tempo = settings.stretch && std::abs (tempoRatio - 1.0) > 1.0e-4;
    return pitched || tempo;
}

void LoopTrack::renderStretched (int numSamples) noexcept
{
    auto& s = *stretchImpl;
    const double ratio = settings.stretch ? std::clamp (tempoRatio, 1.0 / Stretch::kMaxRatio, Stretch::kMaxRatio) : 1.0;
    const bool reverse = settings.reverse;
    auto readAt = [this, reverse] (double pos, float& l, float& r) {
        int p = (int) pos % length;
        if (p < 0) p += length;
        const int index = reverse ? (length - 1 - p) : p;
        l = sampleAt (0, index);
        r = sampleAt (1, index);
    };

    if (std::abs (settings.pitch - lastPitch) > 0.001f || ! stretchWasActive)
    {
        s.stretch.setTransposeSemitones (settings.pitch);
        lastPitch = settings.pitch;
    }

    // (re)start: feed the stretcher the audio before the read position, the read position
    // runs ahead of the audible position by the stretcher's latency
    if (! stretchWasActive || playPos != expectedPlayPos)
    {
        s.stretch.reset();
        const double lead = s.stretch.inputLatency() + s.stretch.outputLatency() * ratio;
        readPos = std::fmod ((double) playPos + lead, (double) length);
        const int seekLen = std::min ((int) s.seekL.size(), s.stretch.seekLength());
        for (int i = 0; i < seekLen; ++i)
            readAt (readPos - seekLen + i, s.seekL[(size_t) i], s.seekR[(size_t) i]);
        float* seekCh[2] = { s.seekL.data(), s.seekR.data() };
        s.stretch.seek (seekCh, seekLen, ratio);
        stretchWasActive = true;
    }

    int done = 0;
    while (done < numSamples)
    {
        const int n = std::min (Stretch::kMaxBlock, numSamples - done);
        double want = readPos + ratio * n;
        int inCount = (int) std::floor (want) - (int) std::floor (readPos);
        inCount = std::clamp (inCount, 0, (int) s.inL.size());
        for (int i = 0; i < inCount; ++i)
            readAt (std::floor (readPos) + i, s.inL[(size_t) i], s.inR[(size_t) i]);
        readPos = std::fmod (want, (double) length);

        float* inCh[2] = { s.inL.data(), s.inR.data() };
        float* outCh[2] = { s.outL.data() + done, s.outR.data() + done };
        s.stretch.process (inCh, inCount, outCh, n);
        done += n;
    }
}

//==============================================================================
void LoopTrack::routeInput (float l, float r, float& outL, float& outR) const noexcept
{
    switch (settings.input)
    {
        case InputRoute::Left:   outL = outR = l; break;
        case InputRoute::Right:  outL = outR = r; break;
        case InputRoute::Stereo:
        default:                 outL = l; outR = r; break;
    }
}

void LoopTrack::setGainRamp (float target, int samples) noexcept
{
    gainTarget = target;
    if (samples <= 1)
    {
        gain = target;
        gainRampLeft = 0;
        gainStep = 0.0f;
        return;
    }
    gainRampLeft = samples;
    gainStep = (target - gain) / (float) samples;
}

//==============================================================================
void LoopTrack::startRecording (double ppqNow, double samplesPerPpq, int autoFinishSamples, RecAction afterAutoFinish)
{
    if (maxLength <= 0)
        return;

    std::fill (chunkOwner.begin(), chunkOwner.end(), (uint8_t) 0);
    std::fill (chunkTouched.begin(), chunkTouched.end(), (uint8_t) 0);
    for (size_t c = 0; c < numChunks; ++c)
        chunkPeak[0][c].store (0.0f, std::memory_order_relaxed);

    state = State::Recording;
    recPos = 0;
    playPos = 0;
    length = 0;
    autoFinish = std::min (autoFinishSamples, maxLength);
    pendingRecAction = afterAutoFinish;
    recSamplesPerPpq = samplesPerPpq;
    anchorPpq = ppqNow;
    anchorValid = true;
    undoKind = redoKind = UndoKind::None;
    stopping = stopAtWrap = false;
}

void LoopTrack::finishRecording (RecAction next, int trimSamples)
{
    if (state != State::Recording)
        return;

    trimSamples = std::clamp (trimSamples, 0, std::max (0, recPos - 1));
    length = recPos - trimSamples;

    if (length < 64)          // too short to be a loop - treat as cancelled
    {
        clear();
        return;
    }

    // Short declick fades at the loop seam (about 2 ms).
    const int seam = std::min (length / 4, (int) (sampleRate * 0.002));
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = layers[0][ch].data();
        for (int i = 0; i < seam; ++i)
        {
            const float g = (float) i / (float) seam;
            d[i] *= g;
            d[length - 1 - i] *= g;
        }
    }

    // Retroactive finish: the audio played after the loop end belongs to the next pass.
    if (trimSamples > 0 && next == RecAction::RecDub)
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = layers[0][ch].data();
            for (int i = 0; i < trimSamples && i < length; ++i)
            {
                d[i] += d[length + i];
                d[length + i] = 0.0f;
            }
        }

    // the seam fades and the moved tail changed the first and last chunks
    {
        const size_t last = (size_t) ((length - 1) >> kChunkShift);
        const size_t firstEnd = std::min (last, (size_t) ((std::max (seam, trimSamples) + kChunkSize - 1) >> kChunkShift));
        for (size_t c = 0; c <= firstEnd; ++c)
            recomputeChunkPeak (0, c);
        recomputeChunkPeak (0, last);
        for (size_t c = last + 1; c < numChunks; ++c)
            chunkPeak[0][c].store (0.0f, std::memory_order_relaxed);
    }

    playPos = trimSamples % length;
    undoKind = UndoKind::Recording;
    redoKind = UndoKind::None;
    gain = gainTarget = 1.0f;
    gainRampLeft = 0;
    state = State::Playing;

    if (next == RecAction::RecDub)
        startOverdub();
    else
        state = State::Playing;
}

void LoopTrack::startOverdub()
{
    if (state == State::Recording)
    {
        finishRecording (RecAction::RecDub);
        return;
    }

    if (length <= 0)
        return;

    if (state == State::Stopped)
    {
        playPos = 0;
        setGainRamp (1.0f, (int) (sampleRate * kDeclickMs / 1000.0f));
    }

    // A new overdub session: the previous one becomes permanent, redo is lost.
    std::fill (chunkTouched.begin(), chunkTouched.end(), (uint8_t) 0);
    redoKind = UndoKind::None;
    undoKind = UndoKind::Overdub;
    state = State::Overdubbing;
    stopping = false;
}

void LoopTrack::endOverdub()
{
    if (state == State::Overdubbing)
        state = State::Playing;
}

void LoopTrack::play (double ppqNow, bool syncToTimeline)
{
    if (length <= 0)
        return;

    if (state == State::Overdubbing)
    {
        state = State::Playing;
        return;
    }
    if (state == State::Playing && ! stopping)
        return;

    if (state != State::Playing)
    {
        if (syncToTimeline && settings.loopSync && anchorValid && ! settings.oneShot)
            playPos = expectedPosition (ppqNow);
        else
        {
            playPos = 0;
            anchorPpq = ppqNow;
            anchorValid = true;
        }
        gain = 0.0f;
    }

    state = State::Playing;
    stopping = false;
    stopAtWrap = false;
    const int ramp = settings.startMode == StartMode::FadeIn ? fadeSamples
                                                              : (int) (sampleRate * kDeclickMs / 1000.0f);
    setGainRamp (1.0f, ramp);
}

void LoopTrack::stop (bool allowFade)
{
    if (state == State::Recording)
    {
        finishRecording (RecAction::RecPlay);
        if (state == State::Empty)
            return;
    }

    if (state == State::Overdubbing)
        state = State::Playing;

    if (state != State::Playing)
        return;

    stopping = true;
    const bool fade = allowFade && settings.stopMode == StopMode::FadeOut;
    setGainRamp (0.0f, fade ? fadeSamples : (int) (sampleRate * kDeclickMs / 1000.0f));
}

void LoopTrack::finishStop() noexcept
{
    state = State::Stopped;
    stopping = false;
    stopAtWrap = false;
    playPos = 0;
    gain = gainTarget = 0.0f;
    gainRampLeft = 0;
}

void LoopTrack::clear()
{
    state = State::Empty;
    length = recPos = playPos = autoFinish = savedLength = 0;
    undoKind = redoKind = UndoKind::None;
    stopping = stopAtWrap = false;
    anchorValid = false;
    gain = gainTarget = 0.0f;
    gainRampLeft = 0;
}

bool LoopTrack::undo()
{
    if (undoKind == UndoKind::None)
        return false;

    if (state == State::Overdubbing)
        state = State::Playing;

    if (undoKind == UndoKind::Recording)
    {
        savedLength = length;
        state = State::Empty;
        length = 0;
        stopping = stopAtWrap = false;
        redoKind = UndoKind::Recording;
    }
    else
    {
        for (size_t c = 0; c < chunkTouched.size(); ++c)
            if (chunkTouched[c] != 0)
                chunkOwner[c] ^= 1;
        redoKind = UndoKind::Overdub;
    }

    undoKind = UndoKind::None;
    return true;
}

bool LoopTrack::redo()
{
    if (redoKind == UndoKind::None)
        return false;

    if (redoKind == UndoKind::Recording)
    {
        if (savedLength <= 0)
            return false;
        length = savedLength;
        state = State::Stopped;
        playPos = 0;
        undoKind = UndoKind::Recording;
    }
    else
    {
        if (state == State::Overdubbing)
            state = State::Playing;
        for (size_t c = 0; c < chunkTouched.size(); ++c)
            if (chunkTouched[c] != 0)
                chunkOwner[c] ^= 1;
        undoKind = UndoKind::Overdub;
    }

    redoKind = UndoKind::None;
    return true;
}

int LoopTrack::expectedPosition (double ppq) const noexcept
{
    if (length <= 0 || recSamplesPerPpq <= 0.0)
        return 0;
    const double samples = (ppq - anchorPpq) * recSamplesPerPpq;
    return (int) positiveModulo (std::round (samples), (double) length) % length;
}

void LoopTrack::resyncToTimeline (double ppqNow)
{
    if (! settings.loopSync || settings.oneShot || ! anchorValid || length <= 0)
        return;
    if (state == State::Playing || state == State::Overdubbing)
        playPos = expectedPosition (ppqNow);
}

float LoopTrack::getProgress() const noexcept
{
    if (state == State::Recording)
        return autoFinish > 0 ? (float) recPos / (float) autoFinish : 0.0f;
    if (length <= 0)
        return 0.0f;
    return (float) playPos / (float) length;
}

//==============================================================================
void LoopTrack::touchChunkForWrite (int index) noexcept
{
    const auto c = (size_t) (index >> kChunkShift);
    if (chunkTouched[c] != 0)
        return;

    const auto from = chunkOwner[c];
    const auto to = (uint8_t) (from ^ 1);
    const auto offset = c << kChunkShift;
    for (int ch = 0; ch < 2; ++ch)
        std::memcpy (layers[to][ch].data() + offset, layers[from][ch].data() + offset, sizeof (float) * kChunkSize);

    chunkPeak[to][c].store (settings.dubMode == DubMode::Replace ? 0.0f : chunkPeak[from][c].load (std::memory_order_relaxed),
                            std::memory_order_relaxed);
    chunkOwner[c] = to;
    chunkTouched[c] = 1;
}

void LoopTrack::process (const float* inL, const float* inR,
                         float* outL, float* outR, float* fxL, float* fxR, int numSamples) noexcept
{
    if (state == State::Empty || state == State::Stopped)
        return;

    if (state == State::Recording)
    {
        auto* dL = layers[0][0].data();
        auto* dR = layers[0][1].data();
        for (int i = 0; i < numSamples; ++i)
        {
            float l, r;
            routeInput (inL[i], inR[i], l, r);
            dL[recPos] = l;
            dR[recPos] = r;
            notePeak (0, recPos, l, r);
            inPeakL = std::max (inPeakL, std::abs (l));
            inPeakR = std::max (inPeakR, std::abs (r));
            ++recPos;

            if ((autoFinish > 0 && recPos >= autoFinish) || recPos >= maxLength)
            {
                finishRecording (pendingRecAction);
                if (state != State::Empty && i + 1 < numSamples)
                    process (inL + i + 1, inR + i + 1, outL + i + 1, outR + i + 1, fxL + i + 1, fxR + i + 1, numSamples - i - 1);
                return;
            }
        }
        return;
    }

    // Playing / overdubbing
    const float level = settings.level;
    const float panL = settings.pan <= 0.0f ? 1.0f : 1.0f - settings.pan;
    const float panR = settings.pan >= 0.0f ? 1.0f : 1.0f + settings.pan;
    float* busL = settings.trackFx ? fxL : outL;
    float* busR = settings.trackFx ? fxR : outR;
    const bool reverse = settings.reverse;

    // time-stretch / pitch shift (playback only; overdubs always run at the original speed)
    const bool stretched = wantsStretch() && numSamples <= (int) stretchImpl->outL.size();
    double advance = 1.0;
    if (stretched)
    {
        renderStretched (numSamples);
        advance = settings.stretch ? std::clamp (tempoRatio, 0.25, 4.0) : 1.0;
    }
    else
    {
        stretchWasActive = false;
        playFrac = 0.0;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const int index = reverse ? (length - 1 - playPos) : playPos;
        float l = stretched ? stretchImpl->outL[(size_t) i] : sampleAt (0, index);
        float r = stretched ? stretchImpl->outR[(size_t) i] : sampleAt (1, index);

        if (state == State::Overdubbing)
        {
            float il, ir;
            routeInput (inL[i], inR[i], il, ir);
            touchChunkForWrite (index);
            const auto owner = chunkOwner[(size_t) (index >> kChunkShift)];
            const float fb = settings.feedback;
            const float nl = settings.dubMode == DubMode::Replace ? il : l * fb + il;
            const float nr = settings.dubMode == DubMode::Replace ? ir : r * fb + ir;
            layers[owner][0][(size_t) index] = nl;
            layers[owner][1][(size_t) index] = nr;
            notePeak (owner, index, nl, nr);
            inPeakL = std::max (inPeakL, std::abs (il));
            inPeakR = std::max (inPeakR, std::abs (ir));
        }

        if (gainRampLeft > 0)
        {
            gain += gainStep;
            if (--gainRampLeft == 0)
                gain = gainTarget;
        }

        const float g = gain * level;
        const float oL = l * g * panL;
        const float oR = r * g * panR;
        busL[i] += oL;
        busR[i] += oR;
        peak = std::max (peak, std::max (std::abs (oL), std::abs (oR)));
        outPeakL = std::max (outPeakL, std::abs (oL));
        outPeakR = std::max (outPeakR, std::abs (oR));

        if (stopping && gainRampLeft == 0 && gain <= 0.0f)
        {
            finishStop();
            return;
        }

        int step = 1;
        if (stretched)
        {
            playFrac += advance;
            step = (int) playFrac;
            playFrac -= step;
        }
        playPos += step;
        if (playPos >= length)
        {
            playPos -= length;
            if (playPos >= length) playPos = 0;
            if (settings.oneShot || stopAtWrap)
            {
                if (state == State::Overdubbing)
                    state = State::Playing;
                finishStop();
                expectedPlayPos = -1;
                return;
            }
        }
    }
    expectedPlayPos = playPos;
}

//==============================================================================
void LoopTrack::recomputeChunkPeak (int layer, size_t chunk) noexcept
{
    if (chunk >= numChunks)
        return;
    const size_t start = chunk << kChunkShift;
    float m = 0.0f;
    for (size_t i = start; i < start + (size_t) kChunkSize; ++i)
        m = std::max (m, std::max (std::abs (layers[layer][0][i]), std::abs (layers[layer][1][i])));
    chunkPeak[layer][chunk].store (m, std::memory_order_relaxed);
}

bool LoopTrack::getWaveform (float* bins, int numBins, int maxSpanSamples) const noexcept
{
    std::fill (bins, bins + numBins, 0.0f);
    const auto st = state;
    const int len = st == State::Recording ? recPos : length;
    if (st == State::Empty || len <= 0 || numBins <= 0 || numChunks == 0)
        return false;

    const int span = (st == State::Recording && maxSpanSamples > len) ? maxSpanSamples : len;
    const int usedChunks = std::min ((int) numChunks, (len + kChunkSize - 1) >> kChunkShift);
    const int binsUsed = std::clamp ((int) ((long long) len * numBins / span), 1, numBins);
    for (int b = 0; b < binsUsed; ++b)
    {
        int c0 = (int) ((long long) b * usedChunks / binsUsed);
        int c1 = (int) ((long long) (b + 1) * usedChunks / binsUsed);
        c1 = std::max (c1, c0 + 1);
        float m = 0.0f;
        for (int c = c0; c < c1 && c < usedChunks; ++c)
            m = std::max (m, chunkPeak[chunkOwner[(size_t) c] & 1][(size_t) c].load (std::memory_order_relaxed));
        const int dst = (settings.reverse && st != State::Recording) ? binsUsed - 1 - b : b;
        bins[dst] = std::min (1.0f, m);
    }
    return true;
}

//==============================================================================
void LoopTrack::readLoop (float* dstL, float* dstR) const noexcept
{
    for (int i = 0; i < length; ++i)
    {
        dstL[i] = sampleAt (0, i);
        dstR[i] = sampleAt (1, i);
    }
}

void LoopTrack::loadLoop (const float* srcL, const float* srcR, int numSamples, double samplesPerPpq)
{
    clear();
    numSamples = std::min (numSamples, maxLength);
    if (numSamples < 64)
        return;

    std::fill (chunkOwner.begin(), chunkOwner.end(), (uint8_t) 0);
    std::fill (chunkTouched.begin(), chunkTouched.end(), (uint8_t) 0);
    std::memcpy (layers[0][0].data(), srcL, sizeof (float) * (size_t) numSamples);
    std::memcpy (layers[0][1].data(), srcR, sizeof (float) * (size_t) numSamples);
    length = recPos = numSamples;
    for (size_t c = 0; c < numChunks; ++c)
    {
        chunkPeak[0][c].store (0.0f);
        chunkPeak[1][c].store (0.0f);
    }
    for (size_t c = 0; c <= (size_t) ((numSamples - 1) >> kChunkShift) && c < numChunks; ++c)
        recomputeChunkPeak (0, c);
    recSamplesPerPpq = samplesPerPpq;
    anchorValid = false;
    state = State::Stopped;
}

void LoopTrack::loadLoopFromRing (const float* ringL, const float* ringR, int ringSize, int start, int numSamples,
                                  int xfade, double samplesPerPpq, double newAnchorPpq)
{
    clear();
    numSamples = std::min (numSamples, maxLength);
    if (numSamples < 64 || ringSize <= 0)
        return;

    std::fill (chunkOwner.begin(), chunkOwner.end(), (uint8_t) 0);
    std::fill (chunkTouched.begin(), chunkTouched.end(), (uint8_t) 0);
    auto* dL = layers[0][0].data();
    auto* dR = layers[0][1].data();
    for (int i = 0; i < numSamples; ++i)
    {
        const int k = (start + i) % ringSize;
        dL[i] = ringL[k];
        dR[i] = ringR[k];
    }
    // seam: the loop end fades into what was played just before the loop start,
    // which continues seamlessly into the first sample of the loop
    xfade = std::min (xfade, numSamples / 4);
    for (int i = 0; i < xfade; ++i)
    {
        const float w = (float) (i + 1) / (float) (xfade + 1);
        const int k = ((start - xfade + i) % ringSize + ringSize) % ringSize;
        const int d = numSamples - xfade + i;
        dL[d] = dL[d] * (1.0f - w) + ringL[k] * w;
        dR[d] = dR[d] * (1.0f - w) + ringR[k] * w;
    }

    length = recPos = numSamples;
    for (size_t c = 0; c < numChunks; ++c)
    {
        chunkPeak[0][c].store (0.0f);
        chunkPeak[1][c].store (0.0f);
    }
    for (size_t c = 0; c <= (size_t) ((numSamples - 1) >> kChunkShift) && c < numChunks; ++c)
        recomputeChunkPeak (0, c);
    recSamplesPerPpq = samplesPerPpq;
    anchorPpq = newAnchorPpq;
    anchorValid = true;
    undoKind = UndoKind::Recording;      // UNDO removes the captured loop again
    state = State::Stopped;
}
} // namespace livelooper
