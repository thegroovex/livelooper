#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Localisation.h"

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

juce::AudioProcessor::BusesProperties LiveLooperProcessor::makeBuses (bool withCue, bool withTrackOutputs)
{
    auto b = BusesProperties()
                 .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                 .withOutput ("Output", juce::AudioChannelSet::stereo(), true);
    if (withCue)
        b = b.withOutput ("Cue", juce::AudioChannelSet::discreteChannels (kCueBusChannels), true);
    else if (withTrackOutputs)
        for (int t = 0; t < kNumTracks; ++t)
            b = b.withOutput ("Track " + juce::String (t + 1), juce::AudioChannelSet::stereo(), false);
    return b;
}

LiveLooperProcessor::LiveLooperProcessor (bool withCueOutput, bool withTrackOutputs)
    : AudioProcessor (makeBuses (withCueOutput, withTrackOutputs)),
      apvts (*this, nullptr, "LIVELOOPER", createLayout())
{
    cueBusAvailable = withCueOutput;
    trackBusesAvailable = ! withCueOutput && withTrackOutputs;
    for (auto& m : sceneMasks)
        m.store (0);
    for (auto& p : trackOutPair)
        p.store (0);
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
        addTrigger (trackId (t, kCapture), C::Capture, t);

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
        tp.feedback  = apvts.getRawParameterValue (trackId (t, kFeedback));
        tp.duck      = apvts.getRawParameterValue (trackId (t, kDuck));
        tp.output    = apvts.getRawParameterValue (trackId (t, kOutput));
        tp.pitch     = apvts.getRawParameterValue (trackId (t, kPitch));
        tp.stretch   = apvts.getRawParameterValue (trackId (t, kStretch));
    }
    addTrigger (gAllStartStop, C::AllStartStop, -1);
    addTrigger (gAllStart,     C::AllStart, -1);
    addTrigger (gAllStop,      C::AllStop, -1);
    addTrigger (gClearAll,     C::ClearAll, -1);
    addTrigger (gTap,          C::AllStartStop, -1, true);
    addTrigger (gCapture,      C::Capture, -1);
    for (int i = 0; i < kNumScenes; ++i)
    {
        addTrigger (sceneId (i), C::Start, -1);
        triggers.back()->scene = i;
    }

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
    clickSound    = apvts.getRawParameterValue (gClickSound);
    captureBars   = apvts.getRawParameterValue (gCaptureBars);
    autoRec       = apvts.getRawParameterValue (gAutoRec);
    autoRecThresh = apvts.getRawParameterValue (gAutoRecThresh);
    duckSource    = apvts.getRawParameterValue (gDuckSource);
    sceneStore    = apvts.getRawParameterValue (gSceneStore);

    keyboardAvailable = wrapperType == wrapperType_Standalone;
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
    for (int b = 1; b < layouts.outputBuses.size(); ++b)
    {
        const auto set = layouts.getChannelSet (false, b);
        if (set.isDisabled())
            continue;
        if (cueBusAvailable ? set.size() != kCueBusChannels : set != juce::AudioChannelSet::stereo())
            return false;
    }
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
    g.clickRoute    = (ClickRoute) clickRoute.load();
    g.captureBars   = kCaptureBars[juce::jlimit (0, 3, juce::roundToInt (captureBars->load()))];
    g.autoRecord    = autoRec->load() >= 0.5f;
    g.autoRecThreshold = juce::Decibels::decibelsToGain (autoRecThresh->load());
    g.duckSource    = (DuckSource) juce::jlimit (0, 1, juce::roundToInt (duckSource->load()));
    engine.setGlobalSettings (g);
    engine.setFollowHostTransport (follow->load() >= 0.5f);
    engine.setRhythm (rhythmOn->load() >= 0.5f, juce::roundToInt (rhythmPattern->load()), rhythmLevel->load());
    engine.setClickSound (juce::roundToInt (clickSound->load()));

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
        s.feedback  = tp.feedback->load();
        s.duck      = tp.duck->load();
        s.output    = (TrackOutput) juce::jlimit (0, 2, juce::roundToInt (tp.output->load()));
        s.pitch     = tp.pitch->load();
        s.stretch   = tp.stretch->load() >= 0.5f;
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
    else if (t.scene >= 0)
        handleScene (t.scene);
    else
        engine.queueCommand (t.command, t.track);
}

void LiveLooperProcessor::handleScene (int scene) noexcept
{
    auto& mask = sceneMasks[(size_t) scene];
    const int visible = visibleTracks.load();
    if (mask.load() == 0 || sceneStore->load() >= 0.5f)
    {
        // store: the tracks that are playing (or recording / overdubbing) right now
        int m = 0;
        for (int t = 0; t < visible; ++t)
        {
            const auto& tr = engine.getTrack (t);
            const auto st = tr.getState();
            if ((st == LoopTrack::State::Playing && ! tr.isStopping()) || st == LoopTrack::State::Overdubbing
                || st == LoopTrack::State::Recording)
                m |= 1 << t;
        }
        mask.store (m == 0 ? (1 << 30) : m);       // bit 30: stored, but no track plays
        lastScene.store (scene);
        sceneStoreDone.store (true);
        return;
    }
    // recall: start the scene's tracks (quantized), stop the others
    const int m = mask.load();
    for (int t = 0; t < visible; ++t)
    {
        const auto& tr = engine.getTrack (t);
        if (tr.isEmpty())
            continue;
        if ((m & (1 << t)) != 0)
            engine.queueCommand (LooperEngine::Command::Start, t);
        else if (tr.isRunning() && ! tr.isStopping())
            engine.queueCommand (LooperEngine::Command::Stop, t);
    }
    lastScene.store (scene);
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

    // ---- cue output (standalone): the chosen pair of the cue bus, everything else silent
    float* cueL = nullptr;
    float* cueR = nullptr;
    if (cueBusAvailable && getBusCount (false) > 1)
    {
        if (auto* bus = getBus (false, 1); bus != nullptr && bus->isEnabled())
        {
            auto cueBuffer = getBusBuffer (buffer, false, 1);
            cueBuffer.clear();
            const int pair = cuePair.load();
            if (pair > 0 && pair * 2 <= cueBuffer.getNumChannels())
            {
                cueL = cueBuffer.getWritePointer ((pair - 1) * 2);
                cueR = cueBuffer.getWritePointer ((pair - 1) * 2 + 1);
            }
        }
    }

    LooperEngine::ExtraOutputs extra;
    extra.cueL = cueL;
    extra.cueR = cueR;
    if (cueBusAvailable && getBusCount (false) > 1 && getBus (false, 1)->isEnabled())
    {
        // standalone: direct track outputs on pairs of the cue bus (never on the click's pair)
        auto cueBuffer = getBusBuffer (buffer, false, 1);
        for (int t = 0; t < kNumTracks; ++t)
        {
            const int pair = trackOutPair[(size_t) t].load();
            if (pair > 0 && pair != cuePair.load() && pair * 2 <= cueBuffer.getNumChannels())
            {
                extra.directL[(size_t) t] = cueBuffer.getWritePointer ((pair - 1) * 2);
                extra.directR[(size_t) t] = cueBuffer.getWritePointer ((pair - 1) * 2 + 1);
            }
        }
    }
    else if (trackBusesAvailable)
    {
        // plug-in: one optional stereo bus per track
        for (int t = 0; t < kNumTracks && t + 1 < getBusCount (false); ++t)
            if (auto* bus = getBus (false, t + 1); bus != nullptr && bus->isEnabled() && bus->getNumberOfChannels() == 2)
            {
                auto tb = getBusBuffer (buffer, false, t + 1);
                tb.clear();
                extra.directL[(size_t) t] = tb.getWritePointer (0);
                extra.directR[(size_t) t] = tb.getWritePointer (1);
            }
    }

    engine.process (inL, inR, buffer.getWritePointer (0), buffer.getWritePointer (1), extra, numSamples, time);
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

    if (sceneStoreDone.exchange (false))
    {
        if (auto* p = apvts.getParameter (gSceneStore); p != nullptr && p->getValue() >= 0.5f)
            p->setValueNotifyingHost (0.0f);
        sendChangeMessage();
    }

    if (midiMap.popLearnedParam() >= 0)
        sendChangeMessage();
}

void LiveLooperProcessor::setVisibleTracks (int n)
{
    n = n > kDefaultVisibleTracks ? kNumTracks : kDefaultVisibleTracks;
    if (n == visibleTracks.load() && engine.getActiveTracks() == n)
        return;
    visibleTracks.store (n);
    suspendProcessing (true);
    engine.setActiveTracks (n);
    suspendProcessing (false);
    sendChangeMessage();
}

int LiveLooperProcessor::exportStems (const juce::File& folder)
{
    folder.createDirectory();
    const double bpm = std::max (20.0, engine.getCurrentBpm());
    int written = 0;
    for (int t = 0; t < visibleTracks.load(); ++t)
    {
        juce::AudioBuffer<float> audio;
        double spp = 0.0;
        if (! snapshotTrack (t, audio, &spp) || audio.getNumSamples() == 0)
            continue;
        const double loopBpm = spp > 0.0 ? engine.getSampleRate() * 60.0 / spp : bpm;
        const double beats = spp > 0.0 ? audio.getNumSamples() / spp : 0.0;
        const auto file = folder.getChildFile ("livelooper-track" + juce::String (t + 1) + "-"
                                               + juce::String (juce::roundToInt (loopBpm)) + "bpm.wav");
        file.deleteFile();
        auto stream = std::unique_ptr<juce::OutputStream> (file.createOutputStream());
        if (stream == nullptr)
            continue;
        std::unordered_map<juce::String, juce::String> meta {
            { juce::WavAudioFormat::acidOneShot, "0" },   { juce::WavAudioFormat::acidRootSet, "0" },
            { juce::WavAudioFormat::acidStretch, "1" },   { juce::WavAudioFormat::acidDiskBased, "0" },
            { juce::WavAudioFormat::acidizerFlag, "1" },  { juce::WavAudioFormat::acidRootNote, "60" },
            { juce::WavAudioFormat::acidBeats, juce::String (juce::roundToInt (beats)) },
            { juce::WavAudioFormat::acidDenominator, "4" }, { juce::WavAudioFormat::acidNumerator, "4" },
            { juce::WavAudioFormat::acidTempo, juce::String (loopBpm, 3) }
        };
        juce::WavAudioFormat wav;
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions {}
                                                       .withSampleRate (engine.getSampleRate())
                                                       .withNumChannels (2)
                                                       .withBitsPerSample (24)
                                                       .withMetadataValues (meta));
        if (writer != nullptr && writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples()))
            ++written;
    }
    return written;
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

void LiveLooperProcessor::setInternalTempo (double bpm)
{
    if (auto* p = apvts.getParameter (gTempo))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) juce::jlimit (40.0, 300.0, bpm)));
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

    for (int t = 0; t < kDefaultVisibleTracks; ++t)
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

    file.getParentDirectory().createDirectory();
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
    root.setAttribute ("cuePair", cuePair.load());
    root.setAttribute ("clickRoute", clickRoute.load());
    root.setAttribute ("visibleTracks", visibleTracks.load());
    {
        juce::StringArray outs, scenes;
        for (auto& p : trackOutPair) outs.add (juce::String (p.load()));
        for (auto& m : sceneMasks)   scenes.add (juce::String (m.load()));
        root.setAttribute ("trackOutputs", outs.joinIntoString (","));
        root.setAttribute ("scenes", scenes.joinIntoString (","));
    }

    if (auto xml = state.createXml())
        root.addChildElement (xml.release());

    root.addChildElement (midiMap.toXml ([this] (int i) { return getParamIdForIndex (i); }).release());
    root.addChildElement (keyMap.toXml().release());

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
    if (root->hasAttribute ("cuePair"))
        setCuePair (root->getIntAttribute ("cuePair", 0));
    if (root->hasAttribute ("clickRoute"))
        setClickRoute ((ClickRoute) juce::jlimit (0, 2, root->getIntAttribute ("clickRoute", 0)));
    setMaxLoopSeconds (root->getDoubleAttribute ("maxLoopSeconds", 60.0));
    setVisibleTracks (root->getIntAttribute ("visibleTracks", kDefaultVisibleTracks));
    if (root->hasAttribute ("trackOutputs"))
    {
        const auto outs = juce::StringArray::fromTokens (root->getStringAttribute ("trackOutputs"), ",", "");
        for (int t = 0; t < kNumTracks && t < outs.size(); ++t)
            setTrackOutputPair (t, outs[t].getIntValue());
    }
    {
        const auto scenes = juce::StringArray::fromTokens (root->getStringAttribute ("scenes"), ",", "");
        for (int i = 0; i < kNumScenes; ++i)
            sceneMasks[(size_t) i].store (i < scenes.size() ? scenes[i].getIntValue() : 0);
        lastScene.store (-1);
    }

    if (auto* params = root->getChildByName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*params));

    pendingLoops.clear();
    if (auto* mm = root->getChildByName ("MIDIMAP"))
        midiMap.fromXml (*mm, [this] (const juce::String& id) { return getParamIndex (id); });
    if (auto* km = root->getChildByName ("KEYMAP"))
        keyMap.fromXml (*km);

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

//============================================================================== cue + sets
juce::String LiveLooperProcessor::getCuePairName (int pair)
{
    if (pair <= 0)
        return i18n::tr ("Off");
    return i18n::tr ("Outputs %1 / %2", juce::String (pair * 2 + 1), juce::String (pair * 2 + 2));
}

bool LiveLooperProcessor::saveSet (const juce::File& file)
{
    const bool keepSaveLoops = saveLoops;
    saveLoops = true;                       // a set always contains its loops
    juce::MemoryBlock block;
    getStateInformation (block);
    saveLoops = keepSaveLoops;

    auto xml = getXmlFromBinary (block.getData(), (int) block.getSize());
    if (xml == nullptr)
        return false;
    xml->setTagName ("LIVELOOPER_SET");
    xml->setAttribute ("saveLoops", keepSaveLoops);
    xml->removeAttribute ("cuePair");
    xml->removeAttribute ("clickRoute");
    xml->removeAttribute ("trackOutputs");
    if (auto* mm = xml->getChildByName ("MIDIMAP")) xml->removeChildElement (mm, true);
    if (auto* km = xml->getChildByName ("KEYMAP"))  xml->removeChildElement (km, true);

    auto target = file.hasFileExtension (kSetExtension) ? file : file.withFileExtension (kSetExtension);
    return xml->writeTo (target);
}

bool LiveLooperProcessor::loadSet (const juce::File& file, juce::String& error)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("LIVELOOPER_SET"))
    {
        error = i18n::tr ("This is not a livelooper set.");
        return false;
    }
    // the app keeps its own MIDI / keyboard mapping and cue routing
    xml->setTagName ("LIVELOOPER_STATE");
    xml->setAttribute ("cuePair", cuePair.load());
    xml->setAttribute ("clickRoute", clickRoute.load());
    {
        juce::StringArray outs;
        for (auto& p : trackOutPair) outs.add (juce::String (p.load()));
        xml->setAttribute ("trackOutputs", outs.joinIntoString (","));
    }
    xml->setAttribute ("saveLoops", saveLoops);
    if (auto* mm = xml->getChildByName ("MIDIMAP")) xml->removeChildElement (mm, true);
    xml->addChildElement (midiMap.toXml ([this] (int i) { return getParamIdForIndex (i); }).release());
    if (auto* km = xml->getChildByName ("KEYMAP")) xml->removeChildElement (km, true);
    xml->addChildElement (keyMap.toXml().release());

    juce::MemoryBlock block;
    copyXmlToBinary (*xml, block);
    setStateInformation (block.getData(), (int) block.getSize());
    return true;
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
