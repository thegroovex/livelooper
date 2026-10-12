#include "LoopTrack.h"

#include <algorithm>
#include <cstring>

namespace livelooper
{
namespace
{
    constexpr float kDeclickMs = 5.0f;
}

//==============================================================================
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
    clear();
}

void LoopTrack::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
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

    for (int i = 0; i < numSamples; ++i)
    {
        const int index = reverse ? (length - 1 - playPos) : playPos;
        float l = sampleAt (0, index);
        float r = sampleAt (1, index);

        if (state == State::Overdubbing)
        {
            float il, ir;
            routeInput (inL[i], inR[i], il, ir);
            touchChunkForWrite (index);
            const auto owner = chunkOwner[(size_t) (index >> kChunkShift)];
            if (settings.dubMode == DubMode::Replace)
            {
                layers[owner][0][(size_t) index] = il;
                layers[owner][1][(size_t) index] = ir;
            }
            else
            {
                layers[owner][0][(size_t) index] = l + il;
                layers[owner][1][(size_t) index] = r + ir;
            }
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

        if (stopping && gainRampLeft == 0 && gain <= 0.0f)
        {
            finishStop();
            return;
        }

        if (++playPos >= length)
        {
            playPos = 0;
            if (settings.oneShot || stopAtWrap)
            {
                if (state == State::Overdubbing)
                    state = State::Playing;
                finishStop();
                return;
            }
        }
    }
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
    recSamplesPerPpq = samplesPerPpq;
    anchorValid = false;
    state = State::Stopped;
}
} // namespace livelooper
