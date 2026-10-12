#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace livelooper
{
using namespace params;

namespace
{
    std::unique_ptr<juce::AudioFormatWriter> makeWriter (juce::AudioFormat& format, std::unique_ptr<juce::OutputStream> stream,
                                                         double sampleRate, int bits)
    {
        return format.createWriterFor (stream, juce::AudioFormatWriterOptions {}
                                                   .withSampleRate (sampleRate)
                                                   .withNumChannels (2)
                                                   .withBitsPerSample (bits));
    }
}

LiveLooperProcessor::LiveLooperProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LIVELOOPER", createLayout())
{
    // ---- parameter index table + MIDI kinds
    std::vector<MidiMap::ParamKind> kinds;
    for (auto* p : getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
        jassert (ranged != nullptr);
        paramIndexById.set (ranged->getParameterID(), paramList.size());
        paramList.add (ranged);

        if (isTrigger (ranged->getParameterID()))
            kinds.push_back (MidiMap::ParamKind::Trigger);
        else if (dynamic_cast<juce::AudioParameterBool*> (ranged) != nullptr)
            kinds.push_back (MidiMap::ParamKind::Toggle);
        else if (dynamic_cast<juce::AudioParameterChoice*> (ranged) != nullptr)
            kinds.push_back (MidiMap::ParamKind::Choice);
        else
            kinds.push_back (MidiMap::ParamKind::Continuous);
    }
    midiMap.setParamKinds (std::move (kinds));

    // ---- triggers
    auto addTrigger = [this] (const juce::String& id, LooperEngine::Command c, int track, bool isTap = false)
    {
        auto tr = std::make_unique<Trigger>();
        tr->raw = apvts.getRawParameterValue (id);
        tr->param = apvts.getParameter (id);
        tr->command = c;
        tr->track = track;
        tr->isTap = isTap;
        triggers.push_back (std::move (tr));
    };

    using C = LooperEngine::Command;
    for (int t = 0; t < kNumTracks; ++t)
    {
        addTrigger (trackId (t, kRecPlay), C::RecPlay, t);
        addTrigger (trackId (t, kStop),    C::Stop, t);
        addTrigger (trackId (t, kClear),   C::Clear, t);
        addTrigger (trackId (t, kUndo),    C::Undo, t);
        addTrigger (trackId (t, kRedo),    C::Redo, t);

        auto& tp = trackParams[(size_t) t];
        tp.level     = apvts.getRawParameterValue (trackId (t, kLevel));
        tp.pan       = apvts.getRawParameterValue (trackId (t, kPan));
        tp.reverse   = apvts.getRawParameterValue (trackId (t, kReverse));
        tp.oneShot   = apvts.getRawParameterValue (trackId (t, kOneShot));
        tp.loopSync  = apvts.getRawParameterValue (trackId (t, kLoopSync));
        tp.fx        = apvts.getRawParameterValue (trackId (t, kTrackFx));
        tp.input     = apvts.getRawParameterValue (trackId (t, kInput));
        tp.startMode = apvts.getRawParameterValue (trackId (t, kStartMode));
        tp.stopMode  = apvts.getRawParameterValue (trackId (t, kStopMode));
        tp.dubMode   = apvts.getRawParameterValue (trackId (t, kDubMode));
        tp.measure   = apvts.getRawParameterValue (trackId (t, kMeasure));
    }
    addTrigger (gAllStartStop, C::AllStartStop, -1);
    addTrigger (gAllStart,     C::AllStart, -1);
    addTrigger (gAllStop,      C::AllStop, -1);
    addTrigger (gClearAll,     C::ClearAll, -1);
    addTrigger (gTap,          C::AllStartStop, -1, true);

    for (int s = 0; s < kNumFxSlots; ++s)
        for (int pass = 0; pass < 2; ++pass)
        {
            const bool input = pass == 0;
            auto& fp = input ? inputFxParams[(size_t) s] : trackFxParams[(size_t) s];
            fp.on   = apvts.getRawParameterValue (fxId (input, s, kFxOn));
            fp.type = apvts.getRawParameterValue (fxId (input, s, kFxType));
            fp.p1   = apvts.getRawParameterValue (fxId (input, s, kFxP1));
            fp.p2   = apvts.getRawParameterValue (fxId (input, s, kFxP2));
            fp.mix  = apvts.getRawParameterValue (fxId (input, s, kFxMix));
        }

    master        = apvts.getRawParameterValue (gMaster);
    monitor       = apvts.getRawParameterValue (gMonitor);
    quantize      = apvts.getRawParameterValue (gQuantize);
    recAction     = apvts.getRawParameterValue (gRecAction);
    sync          = apvts.getRawParameterValue (gSync);
    follow        = apvts.getRawParameterValue (gFollow);
    tempo         = apvts.getRawParameterValue (gTempo);
    fade          = apvts.getRawParameterValue (gFade);
    rhythmOn      = apvts.getRawParameterValue (gRhythmOn);
    rhythmLevel   = apvts.getRawParameterValue (gRhythmLevel);
    rhythmPattern = apvts.getRawParameterValue (gRhythmPattern);

    startTimerHz (30);
}

LiveLooperProcessor::~LiveLooperProcessor()
{
    stopTimer();
}

//==============================================================================
bool LiveLooperProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo())
        return false;
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void LiveLooperProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // A new sample rate reallocates the tracks: keep the loops and resample them.
    if (prepared && std::abs (sampleRate - engine.getSampleRate()) > 0.5)
        captureLoops();

    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    engine.prepare (sampleRate, samplesPerBlock, maxLoopSeconds);
    prepared = true;
    applyPendingLoops();
}

bool LiveLooperProcessor::snapshotTrack (int track, juce::AudioBuffer<float>& audio, double* samplesPerPpq)
{
    // Read without holding the track lock: the buffers are never reallocated while the
    // plug-in runs, so the audio thread keeps playing while the loop is copied.
    auto& tr = engine.getTrack (track);
    const int len = tr.getLength();
    if (tr.isEmpty() || len <= 0 || tr.getState() == LoopTrack::State::Recording)
        return false;
    audio.setSize (2, len);
    tr.readLoop (audio.getWritePointer (0), audio.getWritePointer (1));
    if (samplesPerPpq != nullptr)
        *samplesPerPpq = tr.getSamplesPerPpq();
    return true;
}

void LiveLooperProcessor::captureLoops()
{
    pendingLoops.clear();
    for (int t = 0; t < kNumTracks; ++t)
    {
        StoredLoop loop;
        if (snapshotTrack (t, loop.audio, &loop.samplesPerPpq))
        {
            loop.track = t;
            loop.sampleRate = engine.getSampleRate();
            pendingLoops.push_back (std::move (loop));
        }
    }
}

void LiveLooperProcessor::applyPendingLoops()
{
    const double targetRate = engine.getSampleRate();
    for (auto& loop : pendingLoops)
    {
        juce::AudioBuffer<float> audio;
        double spp = loop.samplesPerPpq;
        if (std::abs (loop.sampleRate - targetRate) > 0.5 && loop.sampleRate > 0.0)
        {
            const double ratio = loop.sampleRate / targetRate;
            const int dstLen = (int) (loop.audio.getNumSamples() / ratio);
            audio.setSize (2, dstLen);
            for (int ch = 0; ch < 2; ++ch)
            {
                juce::LagrangeInterpolator interp;
                interp.process (ratio, loop.audio.getReadPointer (ch), audio.getWritePointer (ch), dstLen);
            }
            spp /= ratio;
        }
        else
            audio = loop.audio;

        engine.lockTrack (loop.track);
        engine.getTrack (loop.track).loadLoop (audio.getReadPointer (0), audio.getReadPointer (1), audio.getNumSamples(), spp);
        engine.unlockTrack (loop.track);
    }
    pendingLoops.clear();
}

void LiveLooperProcessor::setMaxLoopSeconds (double seconds)
{
    seconds = juce::jlimit (10.0, 600.0, seconds);
    if (std::abs (seconds - maxLoopSeconds) < 0.5)
        return;
    maxLoopSeconds = seconds;
    if (prepared)
    {
        captureLoops();
        suspendProcessing (true);
        engine.prepare (currentSampleRate, currentBlockSize, maxLoopSeconds);
        applyPendingLoops();
        suspendProcessing (false);
    }
}

//==============================================================================
void LiveLooperProcessor::pushSettingsToEngine() noexcept
{
    GlobalSettings g;
    g.quantize      = (Quantize) juce::roundToInt (quantize->load());
    g.recAction     = (RecAction) juce::roundToInt (recAction->load());
    g.syncToHost    = sync->load() >= 0.5f;
    g.internalTempo = tempo->load();
    g.fadeMs        = fade->load();
    g.masterLevel   = master->load();
    g.monitorLevel  = monitor->load();
    engine.setGlobalSettings (g);
    engine.setFollowHostTransport (follow->load() >= 0.5f);
    engine.setRhythm (rhythmOn->load() >= 0.5f, juce::roundToInt (rhythmPattern->load()), rhythmLevel->load());

    for (int t = 0; t < kNumTracks; ++t)
    {
        const auto& tp = trackParams[(size_t) t];
        TrackSettings s;
        s.level     = tp.level->load();
        s.pan       = tp.pan->load();
        s.reverse   = tp.reverse->load() >= 0.5f;
        s.oneShot   = tp.oneShot->load() >= 0.5f;
        s.loopSync  = tp.loopSync->load() >= 0.5f;
        s.trackFx   = tp.fx->load() >= 0.5f;
        s.input     = (InputRoute) juce::roundToInt (tp.input->load());
        s.startMode = (StartMode) juce::roundToInt (tp.startMode->load());
        s.stopMode  = (StopMode) juce::roundToInt (tp.stopMode->load());
        s.dubMode   = (DubMode) juce::roundToInt (tp.dubMode->load());
        s.measures  = kMeasureChoices[(size_t) juce::jlimit (0, (int) kMeasureChoices.size() - 1, juce::roundToInt (tp.measure->load()))];
        engine.setTrackSettings (t, s);
    }

    for (int s = 0; s < kNumFxSlots; ++s)
        for (int pass = 0; pass < 2; ++pass)
        {
            const auto& fp = pass == 0 ? inputFxParams[(size_t) s] : trackFxParams[(size_t) s];
            FxSettings fx;
            fx.enabled = fp.on->load() >= 0.5f;
            fx.type = (FxType) juce::jlimit (0, (int) FxType::NumTypes - 1, juce::roundToInt (fp.type->load()));
            fx.p1 = fp.p1->load();
            fx.p2 = fp.p2->load();
            fx.mix = fp.mix->load();
            if (pass == 0) engine.setInputFx (s, fx);
            else           engine.setTrackFx (s, fx);
        }
}

void LiveLooperProcessor::fireTrigger (Trigger& t) noexcept
{
    if (t.isTap)
        handleTap();
    else
        engine.queueCommand (t.command, t.track);
}

void LiveLooperProcessor::handleTap() noexcept
{
    if (lastTapSample >= 0)
    {
        const double interval = (double) (sampleCounter - lastTapSample) / currentSampleRate;
        if (interval > 2.0)
            numTapIntervals = 0;
        else if (interval > 0.15)
        {
            for (size_t i = tapIntervals.size() - 1; i > 0; --i)
                tapIntervals[i] = tapIntervals[i - 1];
            tapIntervals[0] = interval;
            numTapIntervals = std::min (numTapIntervals + 1, (int) tapIntervals.size());
            double sum = 0.0;
            for (int i = 0; i < numTapIntervals; ++i)
                sum += tapIntervals[(size_t) i];
            pendingTapTempo.store ((float) juce::jlimit (40.0, 300.0, 60.0 / (sum / numTapIntervals)));
        }
    }
    lastTapSample = sampleCounter;
}

void LiveLooperProcessor::applyMidiAction (const MidiMap::Action& a) noexcept
{
    if (a.op == MidiMap::Action::Op::None || a.paramIndex < 0 || a.paramIndex >= paramList.size())
        return;

    auto* p = paramList.getUnchecked (a.paramIndex);
    switch (a.op)
    {
        case MidiMap::Action::Op::Trigger:
        {
            const auto id = p->getParameterID();
            for (auto& t : triggers)
                if (t->param == p)
                {
                    fireTrigger (*t);
                    break;
                }
            juce::ignoreUnused (id);
            break;
        }
        case MidiMap::Action::Op::Toggle:
            p->setValueNotifyingHost (p->getValue() >= 0.5f ? 0.0f : 1.0f);
            break;
        case MidiMap::Action::Op::StepChoice:
        {
            const int steps = std::max (2, p->getNumSteps());
            int idx = juce::roundToInt (p->getValue() * (float) (steps - 1)) + 1;
            if (idx >= steps) idx = 0;
            p->setValueNotifyingHost ((float) idx / (float) (steps - 1));
            break;
        }
        case MidiMap::Action::Op::SetNormalized:
            p->setValueNotifyingHost (a.value);
            break;
        case MidiMap::Action::Op::None:
            break;
    }
}

void LiveLooperProcessor::handleMidiMessage (const juce::MidiMessage& m)
{
    if (m.isNoteOn() || m.isController())
        midiActivity.fetch_add (1);
    applyMidiAction (midiMap.handle (m));
}

void LiveLooperProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    // ---- time
    TimeInfo time;
    time.bpm = tempo->load();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm())
                time.bpm = *b;
            if (auto sig = pos->getTimeSignature())
            {
                time.numerator = sig->numerator;
                time.denominator = sig->denominator;
            }
            if (auto ppq = pos->getPpqPosition())
            {
                time.ppq = *ppq;
                time.hostPlaying = pos->getIsPlaying();
            }
        }

    // ---- MIDI (learn + mapping)
    for (const auto meta : midi)
        handleMidiMessage (meta.getMessage());
    midi.clear();

    // ---- trigger parameters (rising edge)
    for (auto& t : triggers)
    {
        const bool on = t->raw->load() >= 0.5f;
        if (on && ! t->last)
            fireTrigger (*t);
        if (on)
            t->needsReset.store (true);
        t->last = on;
    }

    pushSettingsToEngine();

    // ---- audio
    const int numIn = getTotalNumInputChannels();
    const float* inL = numIn > 0 ? buffer.getReadPointer (0) : nullptr;
    const float* inR = numIn > 1 ? buffer.getReadPointer (1) : inL;
    juce::AudioBuffer<float> silence;
    if (inL == nullptr)
    {
        silence.setSize (1, numSamples);
        silence.clear();
        inL = inR = silence.getReadPointer (0);
    }

    engine.process (inL, inR, buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples, time);
    sampleCounter += numSamples;
}

//==============================================================================
void LiveLooperProcessor::timerCallback()
{
    // momentary trigger parameters go back to "off" after they fired
    for (auto& t : triggers)
        if (t->needsReset.exchange (false) && t->param->getValue() >= 0.5f)
            t->param->setValueNotifyingHost (0.0f);

    const float tap = pendingTapTempo.exchange (0.0f);
    if (tap > 0.0f)
        if (auto* p = apvts.getParameter (gTempo))
            p->setValueNotifyingHost (p->convertTo0to1 (tap));

    if (midiMap.popLearnedParam() >= 0)
        sendChangeMessage();
}

void LiveLooperProcessor::pressTrigger (const juce::String& paramId)
{
    if (auto* p = apvts.getParameter (paramId))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (1.0f);
        p->endChangeGesture();
    }
}

int LiveLooperProcessor::getParamIndex (const juce::String& paramId) const
{
    return paramIndexById.contains (paramId) ? paramIndexById[paramId] : -1;
}

juce::String LiveLooperProcessor::getParamIdForIndex (int index) const
{
    if (auto* p = getParamForIndex (index))
        return p->getParameterID();
    return {};
}

juce::RangedAudioParameter* LiveLooperProcessor::getParamForIndex (int index) const
{
    return juce::isPositiveAndBelow (index, paramList.size()) ? paramList.getUnchecked (index) : nullptr;
}

void LiveLooperProcessor::startMidiLearn (const juce::String& paramId)
{
    midiMap.startLearn (getParamIndex (paramId));
}

void LiveLooperProcessor::cancelMidiLearn()
{
    midiMap.startLearn (-1);
}

juce::String LiveLooperProcessor::getMidiLearnParamId() const
{
    return getParamIdForIndex (midiMap.getLearnTarget());
}

void LiveLooperProcessor::removeMidiMapping (const juce::String& paramId)
{
    const int idx = getParamIndex (paramId);
    if (idx >= 0)
        midiMap.removeParam (idx);
    sendChangeMessage();
}

juce::String LiveLooperProcessor::describeMidiMapping (const juce::String& paramId) const
{
    MidiMap::Key key;
    const int idx = getParamIndex (paramId);
    if (idx >= 0 && midiMap.getKeyForParam (idx, key))
        return MidiMap::describe (key);
    return {};
}

void LiveLooperProcessor::loadDefaultMidiMap()
{
    midiMap.clear();
    auto map = [this] (bool cc, int number, const juce::String& id)
    {
        const int idx = getParamIndex (id);
        if (idx >= 0)
            midiMap.assign ({ 0, cc, number }, idx);
    };

    for (int t = 0; t < kNumTracks; ++t)
    {
        map (false, 36 + t, trackId (t, kRecPlay));   // C1 ... E1
        map (false, 41 + t, trackId (t, kStop));      // F1 ... A1
        map (false, 48 + t, trackId (t, kUndo));      // C2 ... E2
        map (true,  20 + t, trackId (t, kLevel));     // CC 20 ... 24
    }
    map (false, 46, gAllStartStop);                   // A#1
    map (false, 47, gTap);                            // B1
    map (true, 25, gMaster);
    for (int s = 0; s < kNumFxSlots; ++s)
    {
        map (false, 60 + s, fxId (true, s, kFxOn));   // C3 ... D#3
        map (false, 64 + s, fxId (false, s, kFxOn));  // E3 ... G3
    }
    sendChangeMessage();
}

//==============================================================================
bool LiveLooperProcessor::exportTrack (int track, const juce::File& file)
{
    if (! juce::isPositiveAndBelow (track, kNumTracks))
        return false;

    juce::AudioBuffer<float> audio;
    if (! snapshotTrack (track, audio, nullptr))
        return false;

    if (audio.getNumSamples() == 0)
        return false;

    file.deleteFile();
    auto stream = std::unique_ptr<juce::OutputStream> (file.createOutputStream());
    if (stream == nullptr)
        return false;

    juce::WavAudioFormat wav;
    auto writer = makeWriter (wav, std::move (stream), engine.getSampleRate(), 24);
    return writer != nullptr && writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
}

bool LiveLooperProcessor::importTrack (int track, const juce::File& file, juce::String& error)
{
    if (! juce::isPositiveAndBelow (track, kNumTracks))
        return false;

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
    {
        error = "Unsupported audio file";
        return false;
    }

    const double targetRate = engine.getSampleRate();
    const auto srcLen = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * maxLoopSeconds));
    juce::AudioBuffer<float> src (2, srcLen);
    reader->read (&src, 0, srcLen, 0, true, true);
    if (reader->numChannels == 1)
        src.copyFrom (1, 0, src, 0, 0, srcLen);

    juce::AudioBuffer<float> dst = src;
    if (std::abs (reader->sampleRate - targetRate) > 0.5)
    {
        const double ratio = reader->sampleRate / targetRate;
        const int dstLen = (int) (srcLen / ratio);
        dst.setSize (2, dstLen);
        for (int ch = 0; ch < 2; ++ch)
        {
            juce::LagrangeInterpolator interp;
            interp.process (ratio, src.getReadPointer (ch), dst.getWritePointer (ch), dstLen);
        }
    }

    const double samplesPerPpq = targetRate * 60.0 / std::max (20.0, engine.getCurrentBpm());
    engine.lockTrack (track);
    engine.getTrack (track).loadLoop (dst.getReadPointer (0), dst.getReadPointer (1), dst.getNumSamples(), samplesPerPpq);
    engine.unlockTrack (track);
    return true;
}

//==============================================================================
void LiveLooperProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::XmlElement root ("LIVELOOPER_STATE");
    root.setAttribute ("version", 1);
    root.setAttribute ("maxLoopSeconds", maxLoopSeconds);
    root.setAttribute ("saveLoops", saveLoops);

    if (auto xml = state.createXml())
        root.addChildElement (xml.release());

    root.addChildElement (midiMap.toXml ([this] (int i) { return getParamIdForIndex (i); }).release());

    if (saveLoops)
    {
        auto* loops = root.createNewChildElement ("LOOPS");
        for (int t = 0; t < kNumTracks; ++t)
        {
            juce::AudioBuffer<float> audio;
            double spp = 0.0;
            if (! snapshotTrack (t, audio, &spp))
                continue;

            juce::MemoryBlock flac;
            {
                juce::FlacAudioFormat format;
                auto writer = makeWriter (format, std::make_unique<juce::MemoryOutputStream> (flac, false), engine.getSampleRate(), 24);
                if (writer == nullptr)
                    continue;
                writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
            }
            auto* e = loops->createNewChildElement ("LOOP");
            e->setAttribute ("track", t);
            e->setAttribute ("samplesPerPpq", spp);
            e->setAttribute ("sampleRate", engine.getSampleRate());
            e->setAttribute ("data", flac.toBase64Encoding());
        }
    }

    copyXmlToBinary (root, destData);
}

void LiveLooperProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto root = getXmlFromBinary (data, sizeInBytes);
    if (root == nullptr || ! root->hasTagName ("LIVELOOPER_STATE"))
        return;

    saveLoops = root->getBoolAttribute ("saveLoops", true);
    setMaxLoopSeconds (root->getDoubleAttribute ("maxLoopSeconds", 60.0));

    if (auto* params = root->getChildByName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*params));

    pendingLoops.clear();
    if (auto* mm = root->getChildByName ("MIDIMAP"))
        midiMap.fromXml (*mm, [this] (const juce::String& id) { return getParamIndex (id); });

    if (auto* loops = root->getChildByName ("LOOPS"))
    {
        for (auto* e : loops->getChildWithTagNameIterator ("LOOP"))
        {
            const int t = e->getIntAttribute ("track", -1);
            if (! juce::isPositiveAndBelow (t, kNumTracks))
                continue;

            juce::MemoryBlock flac;
            if (! flac.fromBase64Encoding (e->getStringAttribute ("data")))
                continue;

            juce::FlacAudioFormat format;
            std::unique_ptr<juce::AudioFormatReader> reader (
                format.createReaderFor (new juce::MemoryInputStream (flac, false), true));
            if (reader == nullptr)
                continue;

            StoredLoop loop;
            const int len = (int) reader->lengthInSamples;
            loop.track = t;
            loop.sampleRate = reader->sampleRate;
            loop.samplesPerPpq = e->getDoubleAttribute ("samplesPerPpq", 0.0);
            loop.audio.setSize (2, len);
            reader->read (&loop.audio, 0, len, 0, true, true);
            pendingLoops.push_back (std::move (loop));
        }
    }

    if (prepared)
    {
        for (int t = 0; t < kNumTracks; ++t)
        {
            engine.lockTrack (t);
            engine.getTrack (t).clear();
            engine.unlockTrack (t);
        }
        applyPendingLoops();
    }
    sendChangeMessage();
}

//==============================================================================
juce::AudioProcessorEditor* LiveLooperProcessor::createEditor()
{
    return new LiveLooperEditor (*this);
}
} // namespace livelooper

#if ! LIVELOOPER_NO_PLUGIN_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new livelooper::LiveLooperProcessor();
}
#endif
