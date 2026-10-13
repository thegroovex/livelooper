#include "PushSurface.h"
#include "../PluginProcessor.h"

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

void PushSurface::toggleParam (const juce::String& paramId)
{
    if (auto* p = proc.getState().getParameter (paramId))
        setParam (paramId, p->getValue() >= 0.5f ? 0.0f : 1.0f);
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
        return;            // encoder touch notes are not used
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
    if (mode == Mode::FxPad)
    {
        if (velocity > 0)
            fxPadTouch (row, column);
        else if (--fxTouches <= 0)
            fxPadReleaseAll();
        return;
    }
    if (velocity == 0 || column >= proc.getVisibleTracks())
        return;

    const int t = column;
    selectedTrack = t;
    if (deleteHeld)
    {
        proc.pressTrigger (trackId (t, kClear));
        return;
    }
    if (muteHeld)
    {
        proc.pressTrigger (trackId (t, kStop));
        return;
    }
    if (row == 0)
        proc.pressTrigger (trackId (t, shift ? kUndo : kRecPlay));
    else if (row == 1)
        proc.pressTrigger (trackId (t, shift ? kRedo : kStop));
}

void PushSurface::fxPadTouch (int row, int column)
{
    if (fxTouches <= 0)
    {
        fxTouches = 0;
        // remember the effect as it was, for the snap back
        auto value = [this] (const char* n) { auto* p = proc.getState().getParameter (fxParamId (n)); return p != nullptr ? p->getValue() : 0.0f; };
        fxSnapshot = { true, value (kFxOn), value (kFxP1), value (kFxP2) };
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
        default: break;
    }
    if (! pressed)
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
        case cc::volume:     encoderMode = EncoderMode::Volume; break;
        case cc::panSend:    encoderMode = EncoderMode::Pan; break;
        case cc::track:      encoderMode = EncoderMode::Pitch; break;
        case cc::clip:       encoderMode = EncoderMode::Feedback; break;
        case cc::device:     encoderMode = EncoderMode::Fx; break;
        case cc::play:       proc.pressTrigger (gAllStartStop); break;
        case cc::stop:       proc.pressTrigger (gAllStop); break;
        case cc::record:     proc.pressTrigger (gCapture); break;
        case cc::metronome:  toggleParam (gRhythmOn); break;
        case cc::tapTempo:   proc.pressTrigger (gTap); break;
        case cc::undo:       proc.pressTrigger (trackId (selectedTrack, shift ? kRedo : kUndo)); break;
        case cc::automation: toggleParam (gAutoRec); break;
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

int PushSurface::masterColumn() const
{
    if (encoderMode == EncoderMode::Fx || proc.getVisibleTracks() > 6)
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
    if (i >= proc.getVisibleTracks())
        return;
    selectedTrack = i;
    switch (encoderMode)
    {
        case EncoderMode::Volume:   nudge (trackId (i, kLevel), delta, 0.005f * fine); break;
        case EncoderMode::Pan:      nudge (trackId (i, kPan), delta, 0.01f * fine); break;
        case EncoderMode::Pitch:    nudge (trackId (i, kPitch), delta, 0.0f); break;
        case EncoderMode::Feedback: nudge (trackId (i, kFeedback), delta, 0.01f * fine); break;
        case EncoderMode::Fx:       break;
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
    if (empty)
        return pad::off;
    const int segment = row - 2;
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
        return dub ? pad::orange : pad::green;
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
                case EncoderMode::Fx:       break;
            }
        }
        else
        {
            names.add ({});
            values.add ({});
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

    juce::String status;
    status << "livelooper  " << juce::String (proc.getEngine().getCurrentBpm(), 1) << " BPM";
    if (masterColumn() < 0)
        status << "  Vol " << percent (value (gMaster));
    if (const int sc = proc.getLastScene(); sc >= 0)
        status << "  Scene " << (sc + 1);
    if (st.getRawParameterValue (gAutoRec)->load() >= 0.5f)
        status << "  AUTO REC";
    if (st.getRawParameterValue (gSceneStore)->load() >= 0.5f)
        status << "  STORE";
    if (mode == Mode::FxPad)
        status << "  XY: " << (fxTarget < 4 ? "In FX " : "Trk FX ") << juce::String::charToString ((juce::juce_wchar) ('A' + fxTarget % 4))
               << " " << text (fxParamId (kFxType));
    return { columns (names), columns (values), columns (states), status };
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
    for (int row = 0; row < kGridSize; ++row)
        for (int col = 0; col < kGridSize; ++col)
        {
            int c = pad::off;
            if (mode == Mode::FxPad)
                c = fxPadColour (row, col);
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
        else if (i < visible)
        {
            top = value (orange, isOn (trackId (i, kTrackFx)) ? full : dim);
            bottom = isOn (trackId (i, kReverse)) ? pad::cyan : pad::cyanDim;
        }
        setCC (kTopRowFirst + i, top);
        setCC (kBottomRowFirst + i, mode == Mode::FxPad ? pad::off : bottom);

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

    // ---- display
    const auto lines = buildDisplay();
    for (int l = 0; l < kDisplayLines; ++l)
        if (lines[(size_t) l] != displayState[(size_t) l])
        {
            out.push_back (displayLine (l, lines[(size_t) l]));
            displayState[(size_t) l] = lines[(size_t) l];
        }
}
} // namespace livelooper::push
