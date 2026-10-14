#include "PushSurface.h"
#include "../PluginProcessor.h"
#include "../Localisation.h"

namespace livelooper::push
{
using namespace params;

namespace
{
    constexpr int kProgressRows = 6;     // rows 3 .. 8 show the loop position

    juce::String percent (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
}

PushSurface::PushSurface (LiveLooperProcessor& p) : proc (p)
{
    invalidate();
}

void PushSurface::invalidate()
{
    padState.fill (-1);
    ccState.fill (-1);
    for (auto& l : displayState)
        l = "\x01";      // never equal to a real line
    sentOnce = false;
}

//============================================================================== parameters
void PushSurface::setParam (const juce::String& paramId, float normalised)
{
    if (auto* p = proc.getState().getParameter (paramId))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
        p->endChangeGesture();
    }
}

float PushSurface::getParam (const juce::String& paramId) const
{
    auto* p = proc.getState().getParameter (paramId);
    return p != nullptr ? p->getValue() : 0.0f;
}

void PushSurface::selectTrack (int t)
{
    if (t >= 0 && t < proc.getVisibleTracks())
        selectedTrack = t;
}

void PushSurface::showNotice (const juce::String& text)
{
    notice = text;
    noticeUntil = now() + kNoticeMs;
}

juce::String PushSurface::getNotice() const
{
    return (notice.isNotEmpty() && (int) (noticeUntil - now()) > 0) ? notice : juce::String();
}

bool PushSurface::isTrackMuted (int t) const
{
    if (getParam (trackId (t, kMute)) >= 0.5f)
        return true;
    bool anySolo = false;
    for (int i = 0; i < proc.getVisibleTracks(); ++i)
        anySolo = anySolo || getParam (trackId (i, kSolo)) >= 0.5f;
    return anySolo && getParam (trackId (t, kSolo)) < 0.5f;
}

void PushSurface::toggleParam (const juce::String& paramId)
{
    if (auto* p = proc.getState().getParameter (paramId))
        setParam (paramId, p->getValue() >= 0.5f ? 0.0f : 1.0f);
}

void PushSurface::stepChoice (const juce::String& paramId, int delta, bool wrap)
{
    auto* p = proc.getState().getParameter (paramId);
    if (p == nullptr)
        return;
    const int steps = std::max (2, p->getNumSteps());
    int idx = juce::roundToInt (p->getValue() * (float) (steps - 1)) + delta;
    idx = wrap ? ((idx % steps) + steps) % steps : juce::jlimit (0, steps - 1, idx);
    setParam (paramId, (float) idx / (float) (steps - 1));
}

void PushSurface::nudge (const juce::String& paramId, int steps, float stepSize)
{
    auto* p = proc.getState().getParameter (paramId);
    if (p == nullptr || steps == 0)
        return;
    const auto& range = p->getNormalisableRange();
    const int numSteps = range.interval >= 1.0f ? (int) std::round ((range.end - range.start) / range.interval) + 1 : p->getNumSteps();
    if (numSteps > 1 && numSteps < 100)
    {
        // stepped parameter (pitch): a few encoder ticks per step
        const int idx = juce::roundToInt (p->getValue() * (float) (numSteps - 1));
        const int slot = (int) (std::hash<std::string>() (paramId.toStdString()) % encoderRemainder.size());
        encoderRemainder[(size_t) slot] += (float) steps / 4.0f;
        const int move = (int) encoderRemainder[(size_t) slot];
        if (move == 0)
            return;
        encoderRemainder[(size_t) slot] -= (float) move;
        setParam (paramId, (float) juce::jlimit (0, numSteps - 1, idx + move) / (float) (numSteps - 1));
        return;
    }
    setParam (paramId, p->getValue() + (float) steps * stepSize);
}

juce::String PushSurface::fxParamId (const char* name) const
{
    return fxId (fxTarget < 4, fxTarget % 4, name);
}

//============================================================================== input
void PushSurface::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOnOrOff())
    {
        const int note = m.getNoteNumber();
        const int vel = m.isNoteOn() ? m.getVelocity() : 0;
        if (isPadNote (note))
            handlePad (padRow (note), padColumn (note), vel);
        else if (note == kTouchStripNote)
            handleStrip (vel > 0, -1);
        return;            // encoder touch notes are not used
    }
    if (m.isPitchWheel())
    {
        handleStrip (true, m.getPitchWheelValue());
        return;
    }
    if (m.isAftertouch())
    {
        if (isPadNote (m.getNoteNumber()))
            handlePressure (m.getAfterTouchValue());
        return;
    }
    if (m.isChannelPressure())
    {
        handlePressure (m.getChannelPressureValue());
        return;
    }
    if (! m.isController())
        return;

    const int number = m.getControllerNumber(), value = m.getControllerValue();
    if ((number >= kEncoderFirst && number <= kMasterEncoder) || number == kTempoEncoder || number == kSwingEncoder)
        handleEncoder (number, encoderDelta (value));
    else
        handleButton (number, value > 0);
}

void PushSurface::handlePad (int row, int column, int velocity)
{
    if (mode == Mode::Step)
    {
        handleStepPad (row, column, velocity);
        return;
    }
    if (mode == Mode::FxPad)
    {
        if (velocity > 0)
            fxPadTouch (row, column);
        else if (--fxTouches <= 0)
            fxPadReleaseAll();
        return;
    }
    if (column >= proc.getVisibleTracks())
        return;

    const int t = column;
    const auto& st = proc.getEngine().getTrackStatus (t);
    const auto state = (LoopTrack::State) st.state.load();
    const auto pending = (LooperEngine::Pending) st.pending.load();

    if (velocity == 0)
    {
        // ---- releases: they end what the press started, whatever modifier is held now
        if (muteHold[(size_t) t].active)
        {
            auto& h = muteHold[(size_t) t];
            h.active = false;
            setParam (trackId (t, kMute), h.previous);
            if (now() - h.since < kHoldMs)
                proc.pressTrigger (trackId (t, kStop));       // a tap stops, as before
            return;
        }
        if (soloHold[(size_t) t].active)
        {
            soloHold[(size_t) t].active = false;
            setParam (trackId (t, kSolo), soloHold[(size_t) t].previous);
            return;
        }
        if (row == 0)
            recPadHeld[(size_t) t] = false;
        if (row >= 2)
        {
            auto& down = slicePadDown[(size_t) t][(size_t) (row - 2)];
            if (down)
            {
                down = false;
                setParam (trackId (t, kSlices[row - 2]), 0.0f);
            }
        }
        return;
    }

    // ---- presses with a modifier
    if (deleteHeld)
    {
        selectTrack (t);
        proc.pressTrigger (trackId (t, kClear));
        return;
    }
    if (duplicateHeld)
    {
        if (dupSource < 0)
        {
            if (state != LoopTrack::State::Empty)
            {
                dupSource = t;
                selectTrack (t);
            }
        }
        else if (t != dupSource)
        {
            proc.requestCopy (dupSource, t);
            showNotice (i18n::tr ("Copy T%1 to T%2", juce::String (dupSource + 1), juce::String (t + 1)));
            dupDone = true;
        }
        return;
    }
    if (muteHeld)
    {
        // mute at once (a drop on the beat); released within kHoldMs it becomes the old stop
        selectTrack (t);
        muteHold[(size_t) t] = { true, now(), getParam (trackId (t, kMute)) };
        setParam (trackId (t, kMute), 1.0f);
        return;
    }
    if (soloHeld)
    {
        soloHold[(size_t) t] = { true, now(), getParam (trackId (t, kSolo)) };
        setParam (trackId (t, kSolo), 1.0f);
        return;
    }
    if (selectHeld)
    {
        selectTrack (t);
        return;
    }

    if (row == 0)
    {
        selectTrack (t);
        recPadHeld[(size_t) t] = ! shift;
        proc.pressTrigger (trackId (t, shift ? kUndo : kRecPlay));
        return;
    }
    if (row == 1)
    {
        selectTrack (t);
        proc.pressTrigger (trackId (t, shift ? kRedo : kStop));
        return;
    }

    // ---- rows 3 - 8: loop position
    const int slice = row - 2;
    const bool recordingOrWaiting = state == LoopTrack::State::Empty || state == LoopTrack::State::Recording
                                    || pending == LooperEngine::Pending::Record || pending == LooperEngine::Pending::AutoRecord;
    if (recPadHeld[(size_t) t] && recordingOrWaiting)
    {
        // REC pad held + position pad: length of this recording (1, 2, 4, 8, 16, 32 bars)
        setParam (trackId (t, kRecLen), (float) (slice + 1) / 6.0f);
        const int bars = kRecLenBars[slice + 1];
        showNotice (bars == 1 ? i18n::tr ("Rec length: 1 bar") : i18n::tr ("Rec length: %1 bars", juce::String (bars)));
        return;
    }
    selectTrack (t);
    if (shift)
    {
        proc.pressTrigger (trackId (t, kCutSync));
        showNotice (i18n::tr ("T%1 back in sync", juce::String (t + 1)));
        return;
    }
    if (state == LoopTrack::State::Playing || state == LoopTrack::State::Overdubbing)
    {
        // loop cutting: the processor turns a tap into a jump, a hold into a stutter
        slicePadDown[(size_t) t][(size_t) slice] = true;
        setParam (trackId (t, kSlices[slice]), 1.0f);
    }
}

void PushSurface::handleStrip (bool touched, int value)
{
    if (! touched)
    {
        if (! stripTouched)
            return;
        stripTouched = false;
        // spring back: the tape runs again (in sync), the filter opens
        if (stripMode == StripMode::TapeStop)
            setParam (gTapeStop, 0.0f);
        else if (stripMode == StripMode::Filter)
            setParam (gPerfFilter, 0.5f);
        else if (stripMode == StripMode::RepeatRate && ! repeatHeld)
            setParam (gRepeatHold, 0.0f);
        else if (stripMode == StripMode::StutterRate && ! accentHeld)
            setParam (gStutterHold, 0.0f);
        return;
    }
    if (! stripTouched)
    {
        stripTouched = true;
        stripMode = repeatHeld ? StripMode::RepeatRate
                  : accentHeld ? StripMode::StutterRate
                  : shift      ? StripMode::Filter : StripMode::TapeStop;
        switch (stripMode)
        {
            case StripMode::Filter:      showNotice (i18n::tr ("Touch strip: filter")); break;
            case StripMode::TapeStop:    showNotice (i18n::tr ("Touch strip: tape stop")); break;
            case StripMode::RepeatRate:  setParam (gRepeatHold, 1.0f); break;
            case StripMode::StutterRate: setParam (gStutterHold, 1.0f); break;
        }
    }
    if (value < 0)
        return;          // touch without a position yet
    const float pos = juce::jlimit (0.0f, 1.0f, (float) value / 16383.0f);
    if (stripMode == StripMode::RepeatRate || stripMode == StripMode::StutterRate)
    {
        // five zones: bottom = 1/2 .. top = 1/32
        const auto& id = stripMode == StripMode::RepeatRate ? gRepeatRate : gStutterRate;
        const int idx = juce::jlimit (0, 4, (int) (pos * 5.0f));
        if (juce::roundToInt (getParam (id) * 4.0f) != idx)
        {
            setParam (id, (float) idx / 4.0f);
            showNotice (i18n::tr (stripMode == StripMode::RepeatRate ? "Beat repeat %1" : "Stutter %1",
                                  repeatRateChoices()[idx]));
        }
        return;
    }
    if (stripMode == StripMode::TapeStop)
        setParam (gTapeStop, 1.0f - pos);      // top = running, bottom = the tape stands still
    else
        setParam (gPerfFilter, pos);           // bottom = low-pass, middle = off, top = high-pass
}

void PushSurface::handlePressure (int value)
{
    if (mode != Mode::FxPad || fxTouches <= 0 || ! fxSnapshot.active)
        return;
    // press harder = more of the effect (from its own mix up to 100 %)
    const float pressure = juce::jlimit (0.0f, 1.0f, (float) value / 127.0f);
    setParam (fxParamId (kFxMix), fxSnapshot.mix + (1.0f - fxSnapshot.mix) * pressure);
}

void PushSurface::handleFootswitch (int number, bool pressed)
{
    const int t = selectedTrack;
    if (number == cc::footswitch1)
    {
        if (pressed)
        {
            // next track
            const int visible = std::max (1, proc.getVisibleTracks());
            selectTrack ((selectedTrack + 1) % visible);
            showNotice (i18n::tr ("T%1 selected", juce::String (selectedTrack + 1)));
        }
        return;
    }
    if (! pressed)
    {
        fs2Down = false;
        return;
    }
    const auto ms = now();
    fs2Down = true;
    fs2DownAt = ms;
    fs2HoldDone = false;
    if (fs2HasTap && ms - fs2LastTap <= kDoubleTapMs)
    {
        // double tap: STOP (the first tap already did REC / PLAY, as on a looper pedal)
        fs2HasTap = false;
        proc.pressTrigger (trackId (t, kStop));
        return;
    }
    fs2HasTap = true;
    fs2LastTap = ms;
    proc.pressTrigger (trackId (t, kRecPlay));
}

void PushSurface::update()
{
    const auto ms = now();
    if (fs2Down && ! fs2HoldDone && ms - fs2DownAt >= kFootHoldMs)
    {
        // footswitch held: clear the selected track
        fs2HoldDone = true;
        fs2HasTap = false;
        proc.pressTrigger (trackId (selectedTrack, kClear));
        showNotice (i18n::tr ("T%1 cleared", juce::String (selectedTrack + 1)));
    }

    // meters: peak since the last update, fast attack, falls about 24 dB per second
    const float dt = lastMeterUpdate == 0 ? 0.02f : juce::jlimit (0.0f, 0.5f, (float) (ms - lastMeterUpdate) * 0.001f);
    lastMeterUpdate = ms;
    auto& engine = proc.getEngine();
    for (int i = 0; i < 9; ++i)
    {
        const float peak = i < 8 ? engine.getTrackStatus (i).displayMeter.exchange (0.0f) : engine.takeDisplayMasterMeter();
        const float db = juce::Decibels::gainToDecibels (peak, -100.0f);
        const float norm = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
        auto& m = meterLevel[(size_t) i];
        m = std::max (norm, m - dt * 0.5f);
        if (peak >= 0.999f)
            clipUntil[(size_t) i] = ms + 1000;
    }
}

void PushSurface::fxPadTouch (int row, int column)
{
    if (fxTouches <= 0)
    {
        fxTouches = 0;
        // remember the effect as it was, for the snap back
        auto value = [this] (const char* n) { auto* p = proc.getState().getParameter (fxParamId (n)); return p != nullptr ? p->getValue() : 0.0f; };
        fxSnapshot = { true, value (kFxOn), value (kFxP1), value (kFxP2), value (kFxMix) };
        setParam (fxParamId (kFxOn), 1.0f);
    }
    ++fxTouches;
    fxTouchRow = row;
    fxTouchColumn = column;
    setParam (fxParamId (kFxP1), (float) column / (float) (kGridSize - 1));
    setParam (fxParamId (kFxP2), (float) row / (float) (kGridSize - 1));
}

void PushSurface::fxPadReleaseAll()
{
    fxTouches = 0;
    fxTouchRow = fxTouchColumn = -1;
    if (fxSnapshot.active && ! shift)
    {
        setParam (fxParamId (kFxP1), fxSnapshot.p1);
        setParam (fxParamId (kFxP2), fxSnapshot.p2);
        setParam (fxParamId (kFxMix), fxSnapshot.mix);
        setParam (fxParamId (kFxOn), fxSnapshot.on);
    }
    fxSnapshot.active = false;
}

void PushSurface::handleButton (int number, bool pressed)
{
    switch (number)
    {
        case cc::shift:     shift = pressed; return;
        case cc::deleteBtn: deleteHeld = pressed; return;
        case cc::mute:      muteHeld = pressed; return;
        case cc::select:    selectHeld = pressed; return;
        case cc::solo:      soloHeld = pressed; return;
        case cc::footswitch1:
        case cc::footswitch2: handleFootswitch (number, pressed); return;
        case cc::addTrack:
        case cc::repeat:
        case cc::accent:      handlePerformanceButton (number, pressed); return;
        case cc::inBtn:                                  // v1.6: FILL while held
            setParam (gRhythmFill, pressed ? 1.0f : 0.0f);
            if (pressed)
                showNotice (i18n::tr ("FILL"));
            return;
        case cc::duplicate:
            if (pressed)
            {
                duplicateHeld = true;
                dupSource = -1;
                dupDone = false;
                showNotice (i18n::tr ("DUPLICATE: press the track to copy, then an empty track"));
            }
            else if (duplicateHeld)
            {
                duplicateHeld = false;
                // no destination pressed: copy the chosen (or selected) track to the next empty track
                const int src = dupSource >= 0 ? dupSource : selectedTrack;
                if (! dupDone && proc.getEngine().getTrackStatus (src).state.load() != (int) LoopTrack::State::Empty)
                {
                    proc.pressTrigger (trackId (src, kCopy));
                    showNotice (i18n::tr ("Copy T%1 to the next empty track", juce::String (src + 1)));
                }
                dupSource = -1;
            }
            return;
        default: break;
    }
    if (! pressed)
        return;

    if (mode == Mode::Step && handleStepButton (number))      // v1.6
        return;

    if (const int scene = sceneForCC (number); scene >= 0)
    {
        if (shift)
            setParam (gSceneStore, 1.0f);        // the processor resets it after storing
        proc.pressTrigger (sceneId (scene));
        return;
    }
    if (number >= kTopRowFirst && number < kTopRowFirst + 8)
    {
        const int i = number - kTopRowFirst;
        if (mode == Mode::FxPad)
        {
            if (fxTouches > 0)
                fxPadReleaseAll();
            fxTarget = i;
        }
        else if (i < proc.getVisibleTracks())
            toggleParam (trackId (i, kTrackFx));
        return;
    }
    if (number >= kBottomRowFirst && number < kBottomRowFirst + 8)
    {
        const int i = number - kBottomRowFirst;
        if (i < proc.getVisibleTracks())
            toggleParam (trackId (i, kReverse));
        return;
    }

    switch (number)
    {
        case cc::session:    if (fxTouches > 0) fxPadReleaseAll(); mode = Mode::Loop; break;
        case cc::note:       mode = Mode::FxPad; break;
        case cc::scales:                                 // v1.6: STEP mode
            if (fxTouches > 0) fxPadReleaseAll();
            mode = mode == Mode::Step ? Mode::Loop : Mode::Step;
            if (mode == Mode::Step)
                showNotice (i18n::tr ("STEP mode: pads = steps, rows = voices"));
            break;
        case cc::outBtn:                                 // v1.6: drums through the performance FX
        {
            const auto& id = shift ? gDrumsRepeat : gDrumsBfx;
            toggleParam (id);
            const bool on = getParam (id) >= 0.5f;
            showNotice (i18n::tr (shift ? (on ? "Drums through repeat on" : "Drums through repeat off")
                                        : (on ? "Drums through beat FX on" : "Drums through beat FX off")));
            break;
        }
        case cc::volume:     encoderMode = EncoderMode::Volume; break;
        case cc::panSend:    encoderMode = EncoderMode::Pan; break;
        case cc::track:      encoderMode = EncoderMode::Pitch; break;
        case cc::clip:       encoderMode = EncoderMode::Feedback; break;
        case cc::device:     encoderMode = EncoderMode::Fx; break;
        case cc::play:       proc.pressTrigger (gAllStartStop); break;
        case cc::stop:       proc.pressTrigger (gAllStop); break;
        case cc::record:     proc.pressTrigger (gCapture); break;
        case cc::metronome:
            if (shift)
            {
                toggleParam (gLinkOn);
                showNotice (i18n::tr (getParam (gLinkOn) >= 0.5f ? "LINK ON" : "LINK OFF"));
            }
            else
                toggleParam (gRhythmOn);
            break;
        case cc::tapTempo:
            if (shift)
            {
                toggleParam (gMetroOn);
                showNotice (i18n::tr (getParam (gMetroOn) >= 0.5f ? "Metronome on" : "Metronome off"));
            }
            else
                proc.pressTrigger (gTap);
            break;
        case cc::browse:     encoderMode = EncoderMode::BeatFx; break;
        case cc::addEffect:
            toggleParam (gBfxOn);
            showNotice (i18n::tr (getParam (gBfxOn) >= 0.5f ? "Beat FX on" : "Beat FX off"));
            break;
        case cc::left:
        case cc::right:
            stepChoice (gBfxBeat, number == cc::right ? 1 : -1, false);
            showNotice (i18n::tr ("Beat FX beat %1", proc.getState().getParameter (gBfxBeat)->getCurrentValueAsText()));
            break;
        case cc::fixedLength:
        {
            stepChoice (gCaptureBars, shift ? -1 : 1, true);
            const int bars = kCaptureBars[juce::jlimit (0, kNumCaptureChoices - 1,
                                                        juce::roundToInt (getParam (gCaptureBars) * (float) (kNumCaptureChoices - 1)))];
            showNotice (bars == 1 ? i18n::tr ("Capture: 1 bar") : i18n::tr ("Capture: %1 bars", juce::String (bars)));
            break;
        }
        case cc::undo:       proc.pressTrigger (trackId (selectedTrack, shift ? kRedo : kUndo)); break;
        case cc::automation: toggleParam (gAutoRec); break;
        case cc::master:
            metersShown = ! metersShown;
            showNotice (metersShown ? i18n::tr ("Meters on") : i18n::tr ("Meters off"));
            break;
        case cc::doubleBtn:
            proc.pressTrigger (trackId (selectedTrack, shift ? kHalve : kDouble));
            showNotice (i18n::tr (shift ? "T%1 loop /2" : "T%1 loop x2", juce::String (selectedTrack + 1)));
            break;
        case cc::octaveUp:
        case cc::octaveDown:
            if (auto* p = proc.getState().getParameter (trackId (selectedTrack, kPitch)))
            {
                const bool up = number == cc::octaveUp;
                const float st = juce::jlimit (-12.0f, 12.0f, p->convertFrom0to1 (p->getValue()) + (up ? 12.0f : -12.0f));
                setParam (trackId (selectedTrack, kPitch), p->convertTo0to1 (st));
                showNotice (i18n::tr (up ? "T%1 octave up" : "T%1 octave down", juce::String (selectedTrack + 1)));
            }
            break;
        case cc::newBtn:     toggleParam (gSceneStore); break;
        case cc::quantize:
            if (auto* p = proc.getState().getParameter (gQuantize))
            {
                const int steps = std::max (2, p->getNumSteps());
                const int idx = (juce::roundToInt (p->getValue() * (float) (steps - 1)) + 1) % steps;
                setParam (gQuantize, (float) idx / (float) (steps - 1));
            }
            break;
        default: break;
    }
}

void PushSurface::handlePerformanceButton (int number, bool pressed)
{
    if (number == cc::addTrack)
    {
        setParam (gBfxHold, pressed ? 1.0f : 0.0f);      // beat FX while held
        return;
    }
    const bool rep = number == cc::repeat;
    (rep ? repeatHeld : accentHeld) = pressed;
    const auto& hold = rep ? gRepeatHold : gStutterHold;
    const auto& latch = rep ? gRepeatLatch : gStutterLatch;
    if (pressed && shift)
    {
        toggleParam (latch);
        const bool on = getParam (latch) >= 0.5f;
        showNotice (rep ? i18n::tr (on ? "Beat repeat latch on" : "Beat repeat latch off")
                        : i18n::tr (on ? "Stutter latch on" : "Stutter latch off"));
        return;
    }
    if (pressed)
    {
        setParam (hold, 1.0f);
        const auto rate = proc.getState().getParameter (rep ? gRepeatRate : gStutterRate)->getCurrentValueAsText();
        showNotice (i18n::tr (rep ? "Beat repeat %1" : "Stutter %1", rate));
    }
    else if (! (stripTouched && stripMode == (rep ? StripMode::RepeatRate : StripMode::StutterRate)))
        setParam (hold, 0.0f);
}

int PushSurface::masterColumn() const
{
    if (mode == Mode::Step)
        return -1;
    if (encoderMode == EncoderMode::Fx || encoderMode == EncoderMode::BeatFx || proc.getVisibleTracks() > 6)
        return -1;
    return 6;           // encoder 7 = master level, encoder 8 = tempo
}

void PushSurface::handleEncoder (int number, int delta)
{
    const float fine = shift ? 0.1f : 1.0f;
    if (number == kMasterEncoder)
    {
        nudge (gMaster, delta, 0.005f * fine);
        return;
    }
    if (number == kSwingEncoder)
    {
        // swing encoder = rhythm swing, SHIFT: rhythm level
        if (shift)
            nudge (gRhythmLevel, delta, 0.01f);
        else
            nudge (gRhythmSwing, delta, 0.01f);
        return;
    }
    auto changeTempo = [this, delta]
    {
        auto* p = proc.getState().getParameter (gTempo);
        if (p != nullptr)
            proc.setInternalTempo (p->convertFrom0to1 (p->getValue()) + (float) delta * (shift ? 0.1f : 1.0f));
    };
    if (number == kTempoEncoder)
    {
        changeTempo();
        return;
    }

    const int i = number - kEncoderFirst;
    if (mode == Mode::Step && i >= 0 && i < kNumBeatRows)
    {
        // v1.6 STEP mode: row level, SHIFT: row pitch
        if (shift)
            nudge (beatRowId (kBeatPitch, i), delta, 0.0f);
        else
            nudge (beatRowId (kBeatVol, i), delta, 0.005f);
        return;
    }
    // master section on the two rightmost encoders while they are free (up to 6 tracks, not in FX mode)
    if (const int m = masterColumn(); m >= 0 && i >= m)
    {
        if (i == m)
            nudge (gMaster, delta, 0.005f * fine);
        else
            changeTempo();
        return;
    }
    if (encoderMode == EncoderMode::Fx)
    {
        // encoders 1 - 4: input FX A - D, 5 - 8: track FX A - D (parameter 1, SHIFT: mix)
        nudge (fxId (i < 4, i % 4, shift ? kFxMix : kFxP1), delta, 0.01f);
        return;
    }
    if (encoderMode == EncoderMode::BeatFx)
    {
        // beat FX type, beat, level, channel, colour FX, repeat rate, stutter rate, metronome level
        static const juce::String* const ids[] = { &gBfxType, &gBfxBeat, &gBfxDepth, &gBfxTarget,
                                                   &gColorType, &gRepeatRate, &gStutterRate, &gMetroLevel };
        if (i >= 0 && i < 8)
            nudge (*ids[i], delta, 0.01f * fine);
        return;
    }
    if (i >= proc.getVisibleTracks())
        return;
    selectedTrack = i;
    switch (encoderMode)
    {
        case EncoderMode::Volume:   nudge (trackId (i, kLevel), delta, 0.005f * fine); break;
        case EncoderMode::Pan:      nudge (trackId (i, kPan), delta, 0.01f * fine); break;
        case EncoderMode::Pitch:    nudge (trackId (i, kPitch), delta, 0.0f); break;
        case EncoderMode::Feedback: nudge (trackId (i, kFeedback), delta, 0.01f * fine); break;
        case EncoderMode::Fx:
        case EncoderMode::BeatFx:   break;
    }
}

//============================================================================== output
int PushSurface::trackPadColour (int t, int row, bool blinkOn) const
{
    const auto& st = proc.getEngine().getTrackStatus (t);
    const auto state = (LoopTrack::State) st.state.load();
    const auto pending = (LooperEngine::Pending) st.pending.load();
    const bool empty = state == LoopTrack::State::Empty;

    if (row == 0)
    {
        switch (pending)
        {
            case LooperEngine::Pending::Record:
            case LooperEngine::Pending::AutoRecord: return blinkOn ? pad::red : pad::off;
            case LooperEngine::Pending::Play:       return blinkOn ? pad::green : pad::greenDim;
            case LooperEngine::Pending::None:       break;
            default:                                return blinkOn ? pad::red : pad::redDim;    // recording ends soon
        }
        switch (state)
        {
            case LoopTrack::State::Recording:   return pad::red;
            case LoopTrack::State::Overdubbing: return pad::orange;
            case LoopTrack::State::Playing:     return st.stopping.load() ? (blinkOn ? pad::green : pad::greenDim) : pad::green;
            case LoopTrack::State::Stopped:     return pad::greenDim;
            case LoopTrack::State::Empty:       return t == selectedTrack ? pad::grey : pad::off;
        }
        return pad::off;
    }
    if (row == 1)
    {
        if (empty)
            return pad::off;
        return (state == LoopTrack::State::Stopped) ? pad::yellowDim : pad::yellow;
    }

    // rows 2 .. 7: loop position, bottom to top
    const int segment = row - 2;
    const bool lengthChoice = recPadHeld[(size_t) t] && (empty || state == LoopTrack::State::Recording);
    if (state == LoopTrack::State::Recording && st.recordingSpan.load())
    {
        // fixed length: progress bar
        const int filled = (int) (juce::jlimit (0.0f, 0.9999f, st.progress.load()) * kProgressRows);
        if (segment < filled)
            return pad::red;
        if (segment == filled)
            return blinkOn ? pad::red : pad::redDim;
        return lengthChoice ? pad::orangeDim : pad::off;
    }
    if (lengthChoice)
        return pad::orangeDim;      // REC pad held: the six lengths 1 .. 32 bars
    if (empty)
        return pad::off;
    if (state == LoopTrack::State::Recording)
        return pad::redDim;
    if (state == LoopTrack::State::Stopped)
    {
        const float ghost = st.ghostProgress.load();
        return (ghost >= 0.0f && (int) (ghost * kProgressRows) == segment) ? pad::grey : pad::off;
    }
    const float prog = juce::jlimit (0.0f, 0.9999f, st.progress.load());
    const int head = (int) (prog * kProgressRows);
    const bool dub = state == LoopTrack::State::Overdubbing;
    if (segment == head)
    {
        if (isTrackMuted (t))
            return pad::redDim;                 // muted (drop): the play head goes dark red
        if (st.cut.load())
            return pad::cyan;                   // cut / stutter: out of the normal position
        return dub ? pad::orange : pad::green;
    }
    return segment < head ? (dub ? pad::orangeDim : pad::greenDim) : pad::off;
}

int PushSurface::fxPadColour (int row, int column) const
{
    if (fxTouchRow >= 0)
    {
        if (row == fxTouchRow && column == fxTouchColumn)
            return pad::white;
        if (row == fxTouchRow || column == fxTouchColumn)
            return pad::cyan;
        return pad::blueDim;
    }
    auto value = [this] (const char* n) { auto* p = proc.getState().getParameter (fxParamId (n)); return p != nullptr ? p->getValue() : 0.0f; };
    const int x = juce::roundToInt (value (kFxP1) * (kGridSize - 1));
    const int y = juce::roundToInt (value (kFxP2) * (kGridSize - 1));
    const bool on = value (kFxOn) >= 0.5f;
    if (row == y && column == x)
        return on ? pad::magenta : pad::blue;
    return pad::blueDim;
}

std::array<juce::String, kDisplayLines> PushSurface::buildDisplay() const
{
    if (mode == Mode::Step)
        return buildStepDisplay();
    auto& st = proc.getState();
    auto text = [&st] (const juce::String& id) {
        auto* p = st.getParameter (id);
        return p != nullptr ? p->getCurrentValueAsText() : juce::String();
    };
    auto value = [&st] (const juce::String& id) {
        auto* p = st.getParameter (id);
        return p != nullptr ? p->convertFrom0to1 (p->getValue()) : 0.0f;
    };
    const int visible = proc.getVisibleTracks();

    juce::StringArray names, values, states;
    for (int i = 0; i < 8; ++i)
    {
        if (encoderMode == EncoderMode::Fx)
        {
            const bool in = i < 4;
            const auto type = text (fxId (in, i % 4, kFxType));
            names.add ((in ? "In" : "Tr") + juce::String::charToString ((juce::juce_wchar) ('A' + i % 4)) + " " + type.substring (0, 4));
            values.add (percent (value (fxId (in, i % 4, shift ? kFxMix : kFxP1))));
        }
        else if (encoderMode == EncoderMode::BeatFx)
        {
            static const char* const labels[] = { "BeatFX", "Beat", "Level", "Channel", "ColorFX", "Repeat", "Stutter", "Click" };
            static const juce::String* const ids[] = { &gBfxType, &gBfxBeat, &gBfxDepth, &gBfxTarget,
                                                       &gColorType, &gRepeatRate, &gStutterRate, &gMetroLevel };
            names.add (labels[i]);
            values.add (i == 2 || i == 7 ? percent (value (*ids[i])) : text (*ids[i]).substring (0, 8));
        }
        else if (const int m = masterColumn(); m >= 0 && i >= m)
        {
            if (i == m)
            {
                names.add ("Master");
                values.add (percent (value (gMaster)));
            }
            else
            {
                names.add ("Tempo");
                values.add (juce::String (proc.getEngine().getCurrentBpm(), 1));
            }
        }
        else if (i < visible)
        {
            switch (encoderMode)
            {
                case EncoderMode::Volume:   names.add ("Lvl " + juce::String (i + 1)); values.add (percent (value (trackId (i, kLevel)))); break;
                case EncoderMode::Pan:      names.add ("Pan " + juce::String (i + 1)); values.add (text (trackId (i, kPan))); break;
                case EncoderMode::Pitch:    names.add ("Pit " + juce::String (i + 1)); values.add (text (trackId (i, kPitch)) + "st"); break;
                case EncoderMode::Feedback: names.add ("Fbk " + juce::String (i + 1)); values.add (percent (value (trackId (i, kFeedback)))); break;
                case EncoderMode::Fx:
                case EncoderMode::BeatFx:   break;
            }
        }
        else
        {
            names.add ({});
            values.add ({});
        }

        if (encoderMode == EncoderMode::BeatFx)
        {
            auto on = [&st] (const juce::String& id) { return st.getRawParameterValue (id)->load() >= 0.5f; };
            juce::String s;
            switch (i)
            {
                case 0: s = on (gBfxOn) || on (gBfxHold) ? "ON" : "off"; break;
                case 5: s = on (gRepeatHold) || on (gRepeatLatch) ? "ON" : "off"; break;
                case 6: s = on (gStutterHold) || on (gStutterLatch) ? "ON" : "off"; break;
                case 7: s = on (gMetroOn) ? "ON" : "off"; break;
                default: break;
            }
            states.add (s);
            continue;
        }
        if (const int m = masterColumn(); m >= 0 && i >= m)
        {
            const bool hostTempo = i > m && st.getRawParameterValue (gSync)->load() >= 0.5f && proc.getEngine().isHostDriven();
            states.add (i == m ? "MASTER" : (hostTempo ? "Host" : "BPM"));
            continue;
        }
        if (i >= visible)
        {
            states.add ({});
            continue;
        }
        const auto& ts = proc.getEngine().getTrackStatus (i);
        const auto state = (LoopTrack::State) ts.state.load();
        const auto pending = (LooperEngine::Pending) ts.pending.load();
        juce::String s;
        switch (state)
        {
            case LoopTrack::State::Empty:       s = "empty"; break;
            case LoopTrack::State::Recording:   s = "REC"; break;
            case LoopTrack::State::Overdubbing: s = "DUB"; break;
            case LoopTrack::State::Playing:     s = ts.stopping.load() ? "stop.." : "PLAY"; break;
            case LoopTrack::State::Stopped:     s = "stop"; break;
        }
        if (pending == LooperEngine::Pending::AutoRecord) s = "auto..";
        else if (pending == LooperEngine::Pending::Record) s = "rec..";
        else if (pending == LooperEngine::Pending::Play)   s = "play..";
        states.add ((i == selectedTrack ? ">" : "") + s);
    }

    juce::String line3 = columns (states);
    if (metersShown)
    {
        // level meters: selection mark, state letter and a 6-cell bar under every track
        line3.clear();
        const int m = masterColumn();
        for (int i = 0; i < 8; ++i)
        {
            const int width = (i % 2 == 0) ? 8 : 9;
            juce::String col;
            if (m >= 0 && i == m)
                col = " M" + levelBar (meterLevel[8], 6);
            else if (m >= 0 && i > m)
            {
                const auto t = states[i];
                const int pad = width - t.length();
                col = juce::String::repeatedString (" ", pad / 2) + t + juce::String::repeatedString (" ", pad - pad / 2);
            }
            else if (i < visible)
                col = meterColumn (i, 8);
            col = (col + juce::String::repeatedString (" ", width)).substring (0, width);
            line3 << col;
        }
    }

    juce::String status;
    if (metersShown && masterColumn() < 0)
        status << "Out " << levelBar (meterLevel[8], 6) << "  " << juce::String (proc.getEngine().getCurrentBpm(), 1) << " BPM";
    else
        status << "livelooper  " << juce::String (proc.getEngine().getCurrentBpm(), 1) << " BPM";
    if (masterColumn() < 0)
        status << "  Vol " << percent (value (gMaster));
    if (const int sc = proc.getLastScene(); sc >= 0)
        status << "  Scene " << (sc + 1);
    if (st.getRawParameterValue (gAutoRec)->load() >= 0.5f)
        status << "  AUTO REC";
    if (st.getRawParameterValue (gLinkOn)->load() >= 0.5f)
        status << (proc.isLinkDriving() ? "  Link " + juce::String (proc.getLinkPeers()) : juce::String ("  Link"));
    if (st.getRawParameterValue (gSceneStore)->load() >= 0.5f)
        status << "  STORE";
    if (mode == Mode::FxPad)
        status << "  XY: " << (fxTarget < 4 ? "In FX " : "Trk FX ") << juce::String::charToString ((juce::juce_wchar) ('A' + fxTarget % 4))
               << " " << text (fxParamId (kFxType));
    if (const auto n = getNotice(); n.isNotEmpty())
        status = n;
    return { columns (names), columns (values), line3, status };
}

juce::String PushSurface::meterColumn (int t, int width) const
{
    const auto& ts = proc.getEngine().getTrackStatus (t);
    const auto state = (LoopTrack::State) ts.state.load();
    const auto pending = (LooperEngine::Pending) ts.pending.load();
    // R rec, D dub, P play, S stopped, - empty; lower case = waiting for the bar; M muted
    juce::juce_wchar letter = '-';
    switch (state)
    {
        case LoopTrack::State::Empty:       letter = '-'; break;
        case LoopTrack::State::Recording:   letter = 'R'; break;
        case LoopTrack::State::Overdubbing: letter = 'D'; break;
        case LoopTrack::State::Playing:     letter = ts.stopping.load() ? 's' : 'P'; break;
        case LoopTrack::State::Stopped:     letter = 'S'; break;
    }
    if (pending == LooperEngine::Pending::Record || pending == LooperEngine::Pending::AutoRecord) letter = 'r';
    else if (pending == LooperEngine::Pending::Play) letter = 'p';
    if ((state == LoopTrack::State::Playing || state == LoopTrack::State::Overdubbing) && isTrackMuted (t))
        letter = 'M';
    auto bar = levelBar (meterLevel[(size_t) t], width - 2);
    if ((int) (clipUntil[(size_t) t] - now()) > 0)
        bar = bar.dropLastCharacters (1) + juce::String::charToString (glyph::fullBlock);
    return juce::String::charToString (t == selectedTrack ? '>' : ' ') + juce::String::charToString (letter) + bar;
}

void PushSurface::render (std::vector<juce::MidiMessage>& out, bool full)
{
    if (full || ! sentOnce)
    {
        padState.fill (-1);
        ccState.fill (-1);
        for (auto& l : displayState)
            l = "\x01";
        sentOnce = true;
    }
    const bool blinkOn = ((juce::Time::getMillisecondCounter() - startTime) / 250) % 2 == 0;
    const int visible = proc.getVisibleTracks();
    auto& st = proc.getState();

    // ---- pads
    if (mode == Mode::Step)
    {
        stepCache = stepView();
        stepCachePlaying = stepPlayingCell (stepCache);
    }
    for (int row = 0; row < kGridSize; ++row)
        for (int col = 0; col < kGridSize; ++col)
        {
            int c = pad::off;
            if (mode == Mode::FxPad)
                c = fxPadColour (row, col);
            else if (mode == Mode::Step)
                c = stepPadColour (row, col, blinkOn);
            else if (col < visible)
                c = trackPadColour (col, row, blinkOn);
            auto& last = padState[(size_t) (row * kGridSize + col)];
            if (c != last)
            {
                out.push_back (juce::MidiMessage::noteOn (1, padNote (row, col), (juce::uint8) c));
                last = c;
            }
        }

    // ---- buttons
    auto setCC = [this, &out] (int number, int value) {
        auto& last = ccState[(size_t) number];
        if (value != last)
        {
            out.push_back (juce::MidiMessage::controllerEvent (1, number, value));
            last = value;
        }
    };
    auto isOn = [&st] (const juce::String& id) { return st.getRawParameterValue (id)->load() >= 0.5f; };

    for (int i = 0; i < 8; ++i)
    {
        using namespace twoColour;
        int top = 0, bottom = pad::off;
        if (mode == Mode::FxPad)
        {
            const bool on = isOn (fxId (i < 4, i % 4, kFxOn));
            top = i == fxTarget ? value (yellow, full) : value (i < 4 ? green : orange, on ? full : dim);
        }
        else if (mode == Mode::Step)
        {
            top = value (yellow, isOn (beatRowId (kBeatMute, i)) ? full : dim);
            bottom = isOn (beatRowId (kBeatSolo, i)) ? pad::cyan : pad::cyanDim;
        }
        else if (i < visible)
        {
            top = value (orange, isOn (trackId (i, kTrackFx)) ? full : dim);
            bottom = isOn (trackId (i, kReverse)) ? pad::cyan : pad::cyanDim;
        }
        setCC (kTopRowFirst + i, top);
        setCC (kBottomRowFirst + i, mode == Mode::FxPad ? pad::off : bottom);   // STEP mode: row solo

        const int sceneColour = ! proc.sceneIsStored (i) ? 0
                              : value (green, proc.getLastScene() == i ? full : dim);
        setCC (ccForScene (i), isOn (gSceneStore) && ! proc.sceneIsStored (i) ? value (red, dimBlink) : sceneColour);
    }

    bool anyRunning = false;
    for (int t = 0; t < visible; ++t)
        anyRunning = anyRunning || proc.getEngine().getTrack (t).isRunning();

    setCC (cc::play, anyRunning ? button::on : button::dim);
    setCC (cc::record, button::dim);
    setCC (cc::stop, button::dim);
    setCC (cc::metronome, isOn (gRhythmOn) ? button::on : button::dim);
    setCC (cc::tapTempo, button::dim);
    setCC (cc::undo, button::dim);
    setCC (cc::deleteBtn, deleteHeld ? button::on : button::dim);
    setCC (cc::mute, muteHeld ? button::on : button::dim);
    setCC (cc::shift, shift ? button::on : button::dim);
    setCC (cc::automation, isOn (gAutoRec) ? button::on : button::dim);
    setCC (cc::newBtn, isOn (gSceneStore) ? button::onBlink : button::dim);
    setCC (cc::quantize, button::dim);
    setCC (cc::session, mode == Mode::Loop ? button::on : button::dim);
    setCC (cc::note, mode == Mode::FxPad ? button::on : button::dim);
    setCC (cc::volume, encoderMode == EncoderMode::Volume ? button::on : button::dim);
    setCC (cc::panSend, encoderMode == EncoderMode::Pan ? button::on : button::dim);
    setCC (cc::track, encoderMode == EncoderMode::Pitch ? button::on : button::dim);
    setCC (cc::clip, encoderMode == EncoderMode::Feedback ? button::on : button::dim);
    setCC (cc::device, encoderMode == EncoderMode::Fx ? button::on : button::dim);
    setCC (cc::select, selectHeld ? button::on : button::dim);
    setCC (cc::solo, soloHeld ? button::on : button::dim);
    setCC (cc::duplicate, duplicateHeld ? button::on : button::dim);
    setCC (cc::master, metersShown ? button::on : button::dim);
    setCC (cc::doubleBtn, button::dim);
    setCC (cc::octaveUp, button::dim);
    setCC (cc::octaveDown, button::dim);
    setCC (cc::browse, encoderMode == EncoderMode::BeatFx ? button::on : button::dim);
    setCC (cc::addEffect, isOn (gBfxOn) ? button::on : button::dim);
    setCC (cc::addTrack, isOn (gBfxHold) ? button::on : button::dim);
    setCC (cc::repeat, isOn (gRepeatHold) || isOn (gRepeatLatch) ? button::on : button::dim);
    setCC (cc::accent, isOn (gStutterHold) || isOn (gStutterLatch) ? button::on : button::dim);
    setCC (cc::fixedLength, button::dim);
    setCC (cc::left, button::dim);
    setCC (cc::right, button::dim);
    // v1.6
    setCC (cc::scales, mode == Mode::Step ? button::on : button::dim);
    setCC (cc::inBtn, isOn (gRhythmFill) || proc.getEngine().getRhythm().isFillPlaying() ? button::on : button::dim);
    setCC (cc::outBtn, isOn (gDrumsBfx) || isOn (gDrumsRepeat) ? button::on : button::dim);
    setCC (cc::up, mode == Mode::Step ? button::dim : button::off);
    setCC (cc::down, mode == Mode::Step ? button::dim : button::off);

    // ---- display
    const auto lines = buildDisplay();
    const auto ms = now();
    for (int l = 0; l < kDisplayLines; ++l)
        if (lines[(size_t) l] != displayState[(size_t) l])
        {
            // lines with meters change all the time: at most every kMeterIntervalMs
            const bool meterLine = metersShown && (l == 2 || (l == 3 && masterColumn() < 0));
            if (meterLine && ! full && displayState[(size_t) l] != "\x01" && ms - lastLineSent[(size_t) l] < kMeterIntervalMs)
                continue;
            out.push_back (displayLine (l, lines[(size_t) l]));
            displayState[(size_t) l] = lines[(size_t) l];
            lastLineSent[(size_t) l] = ms;
        }
}
//============================================================================== v1.6 STEP mode (branch v1.6-rhythm)
BeatPattern PushSurface::stepView() const
{
    const int current = proc.getCurrentPattern();
    if (RhythmGenerator::isUserPattern (current))
        return proc.getUserPatterns().load (RhythmGenerator::userSlotOf (current));
    return RhythmGenerator::presetToPattern (current);
}

int PushSurface::stepPlayingCell (const BeatPattern& p) const
{
    const auto& rhythm = proc.getEngine().getRhythm();
    if (rhythm.getActivePattern() != proc.getCurrentPattern())
        return -1;
    const auto step = rhythm.getCurrentStep();
    if (step < 0 || ! proc.getEngine().isTimelineRunning())
        return -1;
    const long long len16 = 16LL * p.bars;
    return (int) (((step % len16) + len16) % len16) * (p.stepsPerBar / 16);
}

void PushSurface::handleStepPad (int row, int column, int velocity)
{
    if (velocity == 0)
        return;
    if (muteHeld || soloHeld)
    {
        toggleParam (beatRowId (muteHeld ? kBeatMute : kBeatSolo, row));
        const bool on = getParam (beatRowId (muteHeld ? kBeatMute : kBeatSolo, row)) >= 0.5f;
        showNotice (muteHeld ? i18n::tr (on ? "Row %1 muted" : "Row %1 on", juce::String (row + 1))
                             : (on ? i18n::tr ("Row %1 solo", juce::String (row + 1)) : i18n::tr ("Solo off")));
        return;
    }
    const int slot = proc.prepareEditSlot();      // a built-in groove becomes a user pattern first
    auto& bank = proc.getUserPatterns();
    const int steps = bank.getStepsPerBar (slot) * bank.getBars (slot);
    if (deleteHeld)
    {
        for (int s = 0; s < steps; ++s)
            bank.setCell (slot, row, s, 0);
        showNotice (i18n::tr ("Row %1 cleared", juce::String (row + 1)));
        return;
    }
    const int step = stepPage + column;
    if (step >= steps)
        return;
    const int v = bank.getCell (slot, row, step);
    if (shift)
        bank.setCell (slot, row, step, v == 127 ? 0 : 127);
    else
        bank.setCell (slot, row, step, v > 0 ? 0 : juce::jlimit (40, 127, velocity));
}

bool PushSurface::handleStepButton (int number)
{
    const auto p = stepView();
    const int steps = p.numSteps();
    if (number == cc::left || number == cc::right)
    {
        stepPage = juce::jlimit (0, juce::jmax (0, steps - 8), stepPage + (number == cc::right ? 8 : -8));
        showNotice (i18n::tr ("Steps %1 - %2", juce::String (stepPage + 1), juce::String (juce::jmin (steps, stepPage + 8))));
        return true;
    }
    if (number == cc::up || number == cc::down)
    {
        stepChoice (gRhythmPattern, number == cc::up ? 1 : -1, false);
        stepPage = 0;
        showNotice (i18n::tr ("Pattern: %1", proc.getPatternDisplayName (proc.getCurrentPattern())));
        return true;
    }
    if (number >= kTopRowFirst && number < kTopRowFirst + 8)
    {
        const int r = number - kTopRowFirst;
        toggleParam (beatRowId (kBeatMute, r));
        showNotice (i18n::tr (getParam (beatRowId (kBeatMute, r)) >= 0.5f ? "Row %1 muted" : "Row %1 on", juce::String (r + 1)));
        return true;
    }
    if (number >= kBottomRowFirst && number < kBottomRowFirst + 8)
    {
        const int r = number - kBottomRowFirst;
        toggleParam (beatRowId (kBeatSolo, r));
        showNotice (getParam (beatRowId (kBeatSolo, r)) >= 0.5f ? i18n::tr ("Row %1 solo", juce::String (r + 1)) : i18n::tr ("Solo off"));
        return true;
    }
    return false;
}

int PushSurface::stepPadColour (int row, int column, bool blinkOn) const
{
    juce::ignoreUnused (blinkOn);
    const auto& p = stepCache;
    const int step = stepPage + column;
    if (step >= p.numSteps())
        return pad::off;
    const bool muted = getParam (beatRowId (kBeatMute, row)) >= 0.5f;
    const int v = p.get (row, step);
    const int perBeat = p.stepsPerBar / 4;
    if (step == stepCachePlaying)
        return v > 0 ? pad::white : pad::green;
    if (v == 0)
        return (step / perBeat) % 2 == 0 ? pad::grey : pad::off;
    if (muted)
        return pad::yellowDim;
    return v >= 120 ? pad::red : v >= 70 ? pad::orange : pad::yellow;
}

std::array<juce::String, kDisplayLines> PushSurface::buildStepDisplay() const
{
    auto& st = proc.getState();
    juce::StringArray names, values;
    for (int r = 0; r < kNumBeatRows; ++r)
    {
        names.add (proc.getVoiceDisplayName (r).substring (0, 8));
        auto* prm = st.getParameter (beatRowId (shift ? kBeatPitch : kBeatVol, r));
        juce::String v = prm != nullptr ? prm->getCurrentValueAsText() + (shift ? "st" : "%") : juce::String();
        if (getParam (beatRowId (kBeatMute, r)) >= 0.5f)
            v = "MUTE";
        else if (getParam (beatRowId (kBeatSolo, r)) >= 0.5f)
            v = "SOLO " + v;
        values.add (v);
    }
    const auto p = stepView();
    juce::String line3 = "STEP  " + proc.getPatternDisplayName (proc.getCurrentPattern()).substring (0, 24)
                         + "  " + juce::String (stepPage + 1) + "-" + juce::String (juce::jmin (p.numSteps(), stepPage + 8))
                         + "/" + juce::String (p.numSteps());
    juce::String status = "Kit " + proc.getKitDisplayName (proc.getCurrentKit()).substring (0, 20);
    if (proc.getEngine().getRhythm().isFillPlaying())
        status << "  FILL";
    if (st.getRawParameterValue (gDrumsBfx)->load() >= 0.5f)
        status << "  DRUMS>FX";
    if (st.getRawParameterValue (gDrumsRepeat)->load() >= 0.5f)
        status << "  DRUMS>REP";
    if (const auto n = getNotice(); n.isNotEmpty())
        status = n;
    return { columns (names), columns (values), line3, status };
}
} // namespace livelooper::push
