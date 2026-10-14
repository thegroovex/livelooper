#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Localisation.h"
#include "push/PushDevice.h"
#include "controllers/GridControllers.h"   // v1.6
#include "AppSettings.h"
#include "DrumSampleLoader.h"
#include "SoundPacks.h"

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
        else if (isMomentary (ranged->getParameterID()))
            kinds.push_back (MidiMap::ParamKind::Momentary);
        else if (dynamic_cast<juce::AudioParameterBool*> (ranged) != nullptr)
            kinds.push_back (MidiMap::ParamKind::Toggle);
        else if (dynamic_cast<juce::AudioParameterChoice*> (ranged) != nullptr)
            kinds.push_back (MidiMap::ParamKind::Choice);
        else
            kinds.push_back (MidiMap::ParamKind::Continuous);
    }
    midiMap.setParamKinds (std::move (kinds));

    // ---- triggers
    auto addTrigger = [this] (const juce::String& id, LooperEngine::Command c, int track, bool isTap = false, int arg = 0, int octave = 0)
    {
        auto tr = std::make_unique<Trigger>();
        tr->raw = apvts.getRawParameterValue (id);
        tr->param = apvts.getParameter (id);
        tr->command = c;
        tr->track = track;
        tr->isTap = isTap;
        tr->arg = arg;
        tr->octave = octave;
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
        addTrigger (trackId (t, kCutSync), C::CutOff, t);
        addTrigger (trackId (t, kDouble),  C::Multiply, t);
        addTrigger (trackId (t, kHalve),   C::Halve, t);
        addTrigger (trackId (t, kCopy),    C::Copy, t, false, -1);
        addTrigger (trackId (t, kOctUp),   C::RecPlay, t, false, 0, 1);
        addTrigger (trackId (t, kOctDown), C::RecPlay, t, false, 0, -1);
        // v1.6 loop tricks (quantized, undoable)
        addTrigger (trackId (t, kHalfSpeed), C::LoopTrick, t, false, (int) LoopTrack::Trick::HalfSpeed);
        addTrigger (trackId (t, kDblSpeed),  C::LoopTrick, t, false, (int) LoopTrack::Trick::DoubleSpeed);
        addTrigger (trackId (t, kRevLoop),   C::LoopTrick, t, false, (int) LoopTrack::Trick::Reverse);

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
        for (int sl = 0; sl < kNumSlices; ++sl)
            tp.slices[(size_t) sl] = apvts.getRawParameterValue (trackId (t, kSlices[sl]));
        tp.mute      = apvts.getRawParameterValue (trackId (t, kMute));
        tp.solo      = apvts.getRawParameterValue (trackId (t, kSolo));
        tp.recLen    = apvts.getRawParameterValue (trackId (t, kRecLen));
        recLenFired[(size_t) t].store (false);
        pendingOctave[(size_t) t].store (0);
    }
    addTrigger (gAllStartStop, C::AllStartStop, -1);
    addTrigger (gAllStart,     C::AllStart, -1);
    addTrigger (gAllStop,      C::AllStop, -1);
    addTrigger (gClearAll,     C::ClearAll, -1);
    addTrigger (gTap,          C::AllStartStop, -1, true);
    addTrigger (gCapture,      C::Capture, -1);
    addTrigger (gClipExport,   C::Capture, -1);          // v1.6: handled on the message thread
    triggers.back()->special = 1;
    perfRecParam = apvts.getRawParameterValue (gPerfRec);
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
    rhythmKit     = apvts.getRawParameterValue (gRhythmKit);
    rhythmSwing   = apvts.getRawParameterValue (gRhythmSwing);
    rhythmHuman   = apvts.getRawParameterValue (gRhythmHuman);
    captureBars   = apvts.getRawParameterValue (gCaptureBars);
    autoRec       = apvts.getRawParameterValue (gAutoRec);
    autoRecThresh = apvts.getRawParameterValue (gAutoRecThresh);
    duckSource    = apvts.getRawParameterValue (gDuckSource);
    sceneStore    = apvts.getRawParameterValue (gSceneStore);
    tapeStopParam = apvts.getRawParameterValue (gTapeStop);
    perfFilterParam = apvts.getRawParameterValue (gPerfFilter);
    bfxOn        = apvts.getRawParameterValue (gBfxOn);
    bfxHold      = apvts.getRawParameterValue (gBfxHold);
    bfxType      = apvts.getRawParameterValue (gBfxType);
    bfxBeat      = apvts.getRawParameterValue (gBfxBeat);
    bfxDepth     = apvts.getRawParameterValue (gBfxDepth);
    bfxTarget    = apvts.getRawParameterValue (gBfxTarget);
    colorType    = apvts.getRawParameterValue (gColorType);
    for (int t = 0; t < kNumTracks; ++t)
        colorKnob[(size_t) t] = apvts.getRawParameterValue (trackId (t, kColor));
    repeatHold   = apvts.getRawParameterValue (gRepeatHold);
    repeatLatch  = apvts.getRawParameterValue (gRepeatLatch);
    repeatRate   = apvts.getRawParameterValue (gRepeatRate);
    stutterHold  = apvts.getRawParameterValue (gStutterHold);
    stutterLatch = apvts.getRawParameterValue (gStutterLatch);
    stutterRate  = apvts.getRawParameterValue (gStutterRate);
    metroOn      = apvts.getRawParameterValue (gMetroOn);
    metroLevel   = apvts.getRawParameterValue (gMetroLevel);
    metroAccent  = apvts.getRawParameterValue (gMetroAccent);
    linkOn       = apvts.getRawParameterValue (gLinkOn);
    linkStartStop = apvts.getRawParameterValue (gLinkStartStop);
    rhythmGroove = apvts.getRawParameterValue (gRhythmGroove);
    rhythmFills  = apvts.getRawParameterValue (gRhythmFills);
    engine.setDrumBank (getEmbeddedDrumBank());      // v1.5: recorded CC0 / public-domain kits
    // v1.6 rhythm: BEATS user patterns, sound packs, new parameters
    rhythmVariation = apvts.getRawParameterValue (gRhythmVariation);
    rhythmFill      = apvts.getRawParameterValue (gRhythmFill);
    beatSwitch      = apvts.getRawParameterValue (gBeatSwitch);
    drumsBfx        = apvts.getRawParameterValue (gDrumsBfx);
    drumsRepeat     = apvts.getRawParameterValue (gDrumsRepeat);
    for (int r = 0; r < kNumBeatRows; ++r)
    {
        beatMute[(size_t) r]  = apvts.getRawParameterValue (beatRowId (kBeatMute, r));
        beatSolo[(size_t) r]  = apvts.getRawParameterValue (beatRowId (kBeatSolo, r));
        beatVol[(size_t) r]   = apvts.getRawParameterValue (beatRowId (kBeatVol, r));
        beatPitch[(size_t) r] = apvts.getRawParameterValue (beatRowId (kBeatPitch, r));
    }
    engine.setUserPatterns (&userPatterns);
    updateDrumBank();

    keyboardAvailable = wrapperType == wrapperType_Standalone;
    if (wrapperType != wrapperType_Undefined)      // a real host or the standalone app (not the unit tests)
    {
        createPushDevice();
        createGridControllers();                  // v1.6
    }
    startTimerHz (30);
}

LiveLooperProcessor::~LiveLooperProcessor()
{
    stopTimer();
    if (wrapperType != wrapperType_Undefined)
        saveLastSettings();
    linkPtr.store (nullptr);
    pushDevice.reset();
    gridControllers.reset();      // v1.6
}

void LiveLooperProcessor::createPushDevice()
{
    if (pushDevice == nullptr)
    {
        pushDevice = std::make_unique<push::PushDevice> (*this, wrapperType == wrapperType_Standalone);
        pushDevice->setEnabled (pushEnabledSetting);
    }
}

bool LiveLooperProcessor::isPushEnabled() const noexcept { return pushEnabledSetting; }

void LiveLooperProcessor::setPushEnabled (bool e)
{
    pushEnabledSetting = e;
    if (pushDevice != nullptr)
        pushDevice->setEnabled (e);
}

juce::String LiveLooperProcessor::getPushStatus() const
{
    return pushDevice != nullptr ? pushDevice->getStatusText() : i18n::tr ("Off");
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
    recorder.prepare (sampleRate);     // v1.6
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
    g.syncToHost    = sync->load() >= 0.5f || linkDriving.load();   // v1.5: Link drives the timeline like a host
    g.internalTempo = tempo->load();
    g.fadeMs        = fade->load();
    g.masterLevel   = master->load();
    g.monitorLevel  = monitor->load();
    g.clickRoute    = (ClickRoute) clickRoute.load();
    g.captureBars   = kCaptureBars[juce::jlimit (0, kNumCaptureChoices - 1, juce::roundToInt (captureBars->load()))];
    g.autoRecord    = autoRec->load() >= 0.5f;
    g.autoRecThreshold = juce::Decibels::decibelsToGain (autoRecThresh->load());
    g.duckSource    = (DuckSource) juce::jlimit (0, 1, juce::roundToInt (duckSource->load()));
    engine.setGlobalSettings (g);
    engine.setFollowHostTransport (follow->load() >= 0.5f);
    engine.setRhythm (rhythmOn->load() >= 0.5f, juce::roundToInt (rhythmPattern->load()), rhythmLevel->load());
    engine.setClickSound (juce::roundToInt (clickSound->load()));

    // v1.5 performance FX + metronome
    {
        BeatFxSettings b;
        b.on       = bfxOn->load() >= 0.5f || bfxHold->load() >= 0.5f;
        b.type     = (BeatFxType) juce::jlimit (0, (int) BeatFxType::NumTypes - 1, juce::roundToInt (bfxType->load()));
        b.fraction = juce::jlimit (0, (int) kBeatFractions.size() - 1, juce::roundToInt (bfxBeat->load()));
        b.depth    = bfxDepth->load();
        const bool stutter = stutterHold->load() >= 0.5f || stutterLatch->load() >= 0.5f;
        const bool repeat  = repeatHold->load() >= 0.5f || repeatLatch->load() >= 0.5f;
        const int rate = juce::roundToInt ((stutter ? stutterRate : repeatRate)->load());
        engine.setPerformanceFx (b, juce::roundToInt (bfxTarget->load()), stutter || repeat, rate, stutter);
        const auto ct = (ColorFxType) juce::jlimit (0, (int) ColorFxType::NumTypes - 1, juce::roundToInt (colorType->load()));
        for (int t = 0; t < kNumTracks; ++t)
            engine.setColorFx (ct, t, colorKnob[(size_t) t]->load());
        engine.setMetronome (metroOn->load() >= 0.5f, metroLevel->load(), metroAccent->load() >= 0.5f);
        engine.setRhythmGroove (juce::roundToInt (rhythmGroove->load()), juce::roundToInt (rhythmFills->load()));
    }
    // v1.6 rhythm: variation, pattern switch, fill on demand, drum FX routing, row mixer
    {
        engine.setRhythmExtras (rhythmVariation->load(), juce::roundToInt (beatSwitch->load()), rhythmFill->load() >= 0.5f);
        engine.setDrumFxRouting (drumsBfx->load() >= 0.5f, drumsRepeat->load() >= 0.5f);
        bool anyRowSolo = false;
        for (int r = 0; r < kNumBeatRows; ++r)
            anyRowSolo = anyRowSolo || beatSolo[(size_t) r]->load() >= 0.5f;
        for (int r = 0; r < kNumBeatRows; ++r)
            engine.setDrumVoiceMix (r, beatVol[(size_t) r]->load(), beatPitch[(size_t) r]->load(),
                                    beatMute[(size_t) r]->load() >= 0.5f || (anyRowSolo && beatSolo[(size_t) r]->load() < 0.5f));
    }
    engine.setRhythmFeel (juce::roundToInt (rhythmKit->load()), rhythmSwing->load() * 0.01f, rhythmHuman->load() * 0.01f);
    engine.setPerformance (tapeStopParam->load(), perfFilterParam->load());

    bool anySolo = false;
    for (int t = 0; t < visibleTracks.load(); ++t)
        anySolo = anySolo || trackParams[(size_t) t].solo->load() >= 0.5f;

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
        s.muted     = tp.mute->load() >= 0.5f || (anySolo && tp.solo->load() < 0.5f);
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
    else if (t.special == 1)
        clipExportRequested.store (true);
    else if (t.octave != 0)
        pendingOctave[(size_t) t.track].fetch_add (t.octave);      // the message thread changes the pitch
    else
        engine.queueCommand (t.command, t.track, t.arg);
}

void LiveLooperProcessor::handleSlicePads (int numSamples) noexcept
{
    using C = LooperEngine::Command;
    const auto holdSamples = (long long) (kSliceHoldSeconds * currentSampleRate);
    for (int t = 0; t < kNumTracks; ++t)
    {
        auto& ss = sliceState[(size_t) t];
        const auto& tp = trackParams[(size_t) t];
        for (int sl = 0; sl < kNumSlices; ++sl)
        {
            const bool on = tp.slices[(size_t) sl]->load() >= 0.5f;
            const bool was = ss.last[(size_t) sl];
            ss.last[(size_t) sl] = on;
            if (on && ! was)
            {
                // press: jump to the slice (quantized to the next 16th)
                engine.queueCommand (C::Cut, t, sl);
                ss.held = sl;
                ss.heldSamples = 0;
            }
            else if (! on && was && ss.held == sl)
            {
                // release: a hold returns to the running position, a tap stays where it jumped
                if (ss.stutter)
                    engine.queueCommand (C::CutOff, t);
                ss.held = -1;
                ss.stutter = false;
            }
        }
        if (ss.held >= 0)
        {
            ss.heldSamples += numSamples;
            if (! ss.stutter && ss.heldSamples >= holdSamples)
            {
                ss.stutter = true;
                engine.queueCommand (C::StutterOn, t);
            }
        }

        // loop length for the next / running recording (fires once, the timer resets the choice)
        const int recLen = juce::jlimit (0, 6, juce::roundToInt (tp.recLen->load()));
        if (recLen != ss.lastRecLen)
        {
            if (recLen > 0)
            {
                engine.queueCommand (C::SetRecLength, t, kRecLenBars[recLen]);
                recLenFired[(size_t) t].store (true);
            }
            ss.lastRecLen = recLen;
        }
    }

    const int copy = pendingCopy.exchange (0);
    if (copy > 0)
        engine.queueCommand (C::Copy, (copy - 1) / 16, (copy - 1) % 16);
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

    // ---- v1.5 Ableton Link: drives the timeline in the standalone app, and in a DAW when HOST SYNC is off
    {
        bool driving = false;
        if (auto* l = linkPtr.load())
        {
            const bool hostSync = sync->load() >= 0.5f && wrapperType != wrapperType_Standalone;
            const bool use = linkOn->load() >= 0.5f && ! hostSync;
            const float tp = tempo->load();
            const double request = (lastTempoParam > 0.0f && std::abs (tp - lastTempoParam) > 0.005f) ? (double) tp : 0.0;
            lastTempoParam = tp;
            const bool running = engine.isTimelineRunning();
            const auto blk = l->beginBlock (getSampleRate() > 0.0 ? getSampleRate() : 44100.0, numSamples,
                                            outputLatency.load() + numSamples / juce::jmax (1.0, getSampleRate()), 4.0,
                                            use ? request : 0.0, use && running && ! lastEngineRunning,
                                            use && ! running && lastEngineRunning, engine.getTimelinePpq());
            lastEngineRunning = running;
            if (use && blk.active)
            {
                driving = true;
                time.bpm = blk.bpm;
                time.ppq = blk.beat;
                time.numerator = 4;
                time.denominator = 4;
                time.hostPlaying = linkStartStop->load() >= 0.5f ? blk.playing : true;
            }
        }
        linkDriving.store (driving);
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

    handleSlicePads (numSamples);
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
    recorder.push (buffer.getReadPointer (0), buffer.getReadPointer (1), numSamples);    // v1.6: master mix
    sampleCounter += numSamples;
}

//==============================================================================
void LiveLooperProcessor::timerCallback()
{
    // v1.5: Link on / off, start settings (about 1 s after creation: a project would have loaded its
    // state by then), and the last used settings saved every 10 s when they changed
    updateLink();
    if (wrapperType != wrapperType_Undefined)
    {
        ++settingsTimer;
        if (! startupHandled && settingsTimer > 30)
            applyStartupSettingsIfFresh();
        if (settingsTimer % 300 == 0)
            saveLastSettings();
    }

    // v1.6: sound packs changed (import in another instance / rescan)
    if (SoundPacks::get().getVersion() != soundPackVersion)
        updateDrumBank();

    // momentary trigger parameters go back to "off" after they fired
    for (auto& t : triggers)
        if (t->needsReset.exchange (false) && t->param->getValue() >= 0.5f)
            t->param->setValueNotifyingHost (0.0f);

    // v1.4: one-shot rec length choices go back to "-", octave triggers move the pitch
    for (int t = 0; t < kNumTracks; ++t)
    {
        if (recLenFired[(size_t) t].exchange (false))
            if (auto* p = apvts.getParameter (trackId (t, kRecLen)); p != nullptr && p->getValue() > 0.0f)
                p->setValueNotifyingHost (0.0f);
        if (const int oct = pendingOctave[(size_t) t].exchange (0); oct != 0)
            if (auto* p = apvts.getParameter (trackId (t, kPitch)))
            {
                const float st = juce::jlimit (-12.0f, 12.0f, p->convertFrom0to1 (p->getValue()) + 12.0f * (float) oct);
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->convertTo0to1 (st));
                p->endChangeGesture();
            }
    }

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

    updatePerformanceRecorder();      // v1.6
}

//============================================================================== v1.6 performance recorder
void LiveLooperProcessor::updatePerformanceRecorder()
{
    const bool want = perfRecParam != nullptr && perfRecParam->load() >= 0.5f;
    if (want == recorder.isRecording())
        return;
    if (want)
    {
        if (recorder.start (PerformanceRecorder::getFolder()) == juce::File())
            if (auto* p = apvts.getParameter (gPerfRec))
                p->setValueNotifyingHost (0.0f);          // folder not writable: back off
    }
    else
    {
        const auto f = recorder.stop();
        if (f != juce::File() && onRecordingFinished)
            onRecordingFinished (f);
    }
    sendChangeMessage();
}

bool LiveLooperProcessor::startPerformanceRecording()
{
    if (auto* p = apvts.getParameter (gPerfRec))
        p->setValueNotifyingHost (1.0f);
    updatePerformanceRecorder();
    return recorder.isRecording();
}

juce::File LiveLooperProcessor::stopPerformanceRecording()
{
    if (auto* p = apvts.getParameter (gPerfRec))
        p->setValueNotifyingHost (0.0f);
    const bool was = recorder.isRecording();
    updatePerformanceRecorder();
    return was ? recorder.getLastFile() : juce::File();
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

bool LiveLooperProcessor::writeLoopWav (int t, const juce::File& file)
{
    if (! juce::isPositiveAndBelow (t, kNumTracks))
        return false;
    const double bpm = std::max (20.0, engine.getCurrentBpm());
    juce::AudioBuffer<float> audio;
    double spp = 0.0;
    if (! snapshotTrack (t, audio, &spp) || audio.getNumSamples() == 0)
        return false;
    const double loopBpm = spp > 0.0 ? engine.getSampleRate() * 60.0 / spp : bpm;
    const double beats = spp > 0.0 ? audio.getNumSamples() / spp : 0.0;
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    auto stream = std::unique_ptr<juce::OutputStream> (file.createOutputStream());
    if (stream == nullptr)
        return false;
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
    return writer != nullptr && writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
}

static juce::String loopFileStem (int t, double bpm)
{
    return "livelooper-track" + juce::String (t + 1) + "-" + juce::String (juce::roundToInt (bpm)) + "bpm";
}

int LiveLooperProcessor::exportStems (const juce::File& folder)
{
    folder.createDirectory();
    int written = 0;
    for (int t = 0; t < visibleTracks.load(); ++t)
    {
        if (engine.getTrack (t).getLength() <= 0)
            continue;
        const double spp = engine.getTrack (t).getSamplesPerPpq();
        const double bpm = spp > 0.0 ? engine.getSampleRate() * 60.0 / spp : std::max (20.0, engine.getCurrentBpm());
        if (writeLoopWav (t, folder.getChildFile (loopFileStem (t, bpm) + ".wav")))
            ++written;
    }
    return written;
}

juce::File LiveLooperProcessor::getDragExportFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("livelooper").getChildFile ("Dragged Loops");
}

juce::File LiveLooperProcessor::exportLoopForDrag (int t)
{
    if (! juce::isPositiveAndBelow (t, kNumTracks) || engine.getTrack (t).getLength() <= 0)
        return {};
    const double spp = engine.getTrack (t).getSamplesPerPpq();
    const double bpm = spp > 0.0 ? engine.getSampleRate() * 60.0 / spp : std::max (20.0, engine.getCurrentBpm());
    // a new file each time: Live keeps referencing the files it imported
    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S");
    auto file = getDragExportFolder().getChildFile (loopFileStem (t, bpm) + "-" + stamp + ".wav").getNonexistentSibling (false);
    return writeLoopWav (t, file) ? file : juce::File();
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
    root.setAttribute ("push", pushEnabledSetting);
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
    writeBeatsState (root);     // v1.6

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
    stateLoaded = true;

    saveLoops = root->getBoolAttribute ("saveLoops", true);
    if (root->hasAttribute ("cuePair"))
        setCuePair (root->getIntAttribute ("cuePair", 0));
    if (root->hasAttribute ("clickRoute"))
        setClickRoute ((ClickRoute) juce::jlimit (0, 2, root->getIntAttribute ("clickRoute", 0)));
    setMaxLoopSeconds (root->getDoubleAttribute ("maxLoopSeconds", 60.0));
    setVisibleTracks (root->getIntAttribute ("visibleTracks", kDefaultVisibleTracks));
    setPushEnabled (root->getBoolAttribute ("push", true));
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
    readBeatsState (*root);     // v1.6

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

//============================================================================== v1.5 Link + global settings
juce::String LiveLooperProcessor::getLinkStatus() const
{
    if (! LinkSync::isAvailable())
        return i18n::tr ("Link is not available in this build");
    auto* l = linkPtr.load();
    if (l == nullptr || ! l->isEnabled())
        return i18n::tr ("Link off");
    const int peers = l->getNumPeers();
    return i18n::tr (peers == 1 ? "Link: 1 peer, %2 BPM" : "Link: %1 peers, %2 BPM", juce::String (peers),
                     juce::String (l->getSessionTempo(), 1));
}

int LiveLooperProcessor::getLinkPeers() const noexcept
{
    auto* l = linkPtr.load();
    return l != nullptr ? l->getNumPeers() : 0;
}

void LiveLooperProcessor::updateLink()
{
    const bool want = linkOn->load() >= 0.5f;
    if (want && link == nullptr && LinkSync::isAvailable())
    {
        link = std::make_unique<LinkSync>();       // created on first use, kept until the processor goes
        linkPtr.store (link.get());
    }
    if (link != nullptr)
    {
        link->setStartStopSync (linkStartStop->load() >= 0.5f);
        link->setEnabled (want);
        // show the session tempo on the tempo control while Link drives
        if (linkDriving.load() && link->getNumPeers() > 0)
        {
            const double bpm = link->getSessionTempo();
            if (std::abs (bpm - tempo->load()) > 0.05)
            {
                lastTempoParam = (float) juce::jlimit (40.0, 300.0, bpm);
                setInternalTempo (bpm);
            }
        }
    }
}

std::unique_ptr<juce::XmlElement> LiveLooperProcessor::createSettingsSnapshot()
{
    auto root = std::make_unique<juce::XmlElement> ("LIVELOOPER_SETTINGS");
    root->setAttribute ("version", LIVELOOPER_VERSION);
    root->setAttribute ("maxLoopSeconds", maxLoopSeconds);
    root->setAttribute ("saveLoops", saveLoops);
    root->setAttribute ("cuePair", cuePair.load());
    root->setAttribute ("clickRoute", clickRoute.load());
    root->setAttribute ("visibleTracks", visibleTracks.load());
    root->setAttribute ("push", pushEnabledSetting);
    juce::StringArray outs;
    for (auto& p : trackOutPair) outs.add (juce::String (p.load()));
    root->setAttribute ("trackOutputs", outs.joinIntoString (","));
    auto* params = root->createNewChildElement ("PARAMS");
    for (auto* p : paramList)
    {
        const auto id = p->getParameterID();
        if (isTrigger (id) || isMomentary (id) || id == gTapeStop)
            continue;
        auto* e = params->createNewChildElement ("P");
        e->setAttribute ("id", id);
        e->setAttribute ("v", p->convertFrom0to1 (p->getValue()));
    }
    root->addChildElement (midiMap.toXml ([this] (int i) { return getParamIdForIndex (i); }).release());
    root->addChildElement (keyMap.toXml().release());
    return root;
}

void LiveLooperProcessor::applySettingsSnapshot (const juce::XmlElement& s)
{
    if (! s.hasTagName ("LIVELOOPER_SETTINGS"))
        return;
    saveLoops = s.getBoolAttribute ("saveLoops", saveLoops);
    setCuePair (s.getIntAttribute ("cuePair", cuePair.load()));
    setClickRoute ((ClickRoute) juce::jlimit (0, 2, s.getIntAttribute ("clickRoute", clickRoute.load())));
    setMaxLoopSeconds (s.getDoubleAttribute ("maxLoopSeconds", maxLoopSeconds));
    setVisibleTracks (s.getIntAttribute ("visibleTracks", visibleTracks.load()));
    setPushEnabled (s.getBoolAttribute ("push", pushEnabledSetting));
    const auto outs = juce::StringArray::fromTokens (s.getStringAttribute ("trackOutputs"), ",", "");
    for (int t = 0; t < kNumTracks && t < outs.size(); ++t)
        setTrackOutputPair (t, outs[t].getIntValue());
    if (auto* params = s.getChildByName ("PARAMS"))
        for (auto* e : params->getChildWithTagNameIterator ("P"))
            if (auto* p = apvts.getParameter (e->getStringAttribute ("id")))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->convertTo0to1 ((float) e->getDoubleAttribute ("v")));
                p->endChangeGesture();
            }
    if (auto* mm = s.getChildByName ("MIDIMAP"))
        midiMap.fromXml (*mm, [this] (const juce::String& id) { return getParamIndex (id); });
    if (auto* km = s.getChildByName ("KEYMAP"))
        keyMap.fromXml (*km);
    sendChangeMessage();
}

void LiveLooperProcessor::applyDefaultSettings()
{
    for (auto* p : paramList)
        if (! isTrigger (p->getParameterID()) && ! isMomentary (p->getParameterID()))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getDefaultValue());
            p->endChangeGesture();
        }
    setCuePair (0);
    setClickRoute (ClickRoute::Main);
    for (int t = 0; t < kNumTracks; ++t)
        setTrackOutputPair (t, 0);
    setPushEnabled (true);
    sendChangeMessage();
}

void LiveLooperProcessor::applyStartupSettingsIfFresh()
{
    startupHandled = true;
    if (stateLoaded || appsettings::getStartupMode() != appsettings::StartupMode::LastSettings)
        return;
    if (auto s = appsettings::loadLastSettings())
        applySettingsSnapshot (*s);
}

void LiveLooperProcessor::saveLastSettings()
{
    auto s = createSettingsSnapshot();
    const auto text = s->toString();
    if (text == lastSnapshot)
        return;
    lastSnapshot = text;
    appsettings::saveLastSettings (*s);
}

std::unique_ptr<juce::XmlElement> LiveLooperProcessor::createSetXml()
{
    const bool keepSaveLoops = saveLoops;
    saveLoops = true;                       // a set always contains its loops
    juce::MemoryBlock block;
    getStateInformation (block);
    saveLoops = keepSaveLoops;

    auto xml = getXmlFromBinary (block.getData(), (int) block.getSize());
    if (xml == nullptr)
        return nullptr;
    xml->setTagName ("LIVELOOPER_SET");
    xml->setAttribute ("saveLoops", keepSaveLoops);
    xml->removeAttribute ("cuePair");
    xml->removeAttribute ("clickRoute");
    xml->removeAttribute ("trackOutputs");
    xml->removeAttribute ("push");
    if (auto* mm = xml->getChildByName ("MIDIMAP")) xml->removeChildElement (mm, true);
    if (auto* km = xml->getChildByName ("KEYMAP"))  xml->removeChildElement (km, true);
    return xml;
}

bool LiveLooperProcessor::saveSet (const juce::File& file)
{
    auto xml = createSetXml();
    if (xml == nullptr)
        return false;
    auto target = file.hasFileExtension (kSetExtension) ? file : file.withFileExtension (kSetExtension);
    return xml->writeTo (target);
}

bool LiveLooperProcessor::loadSet (const juce::File& file, juce::String& error)
{
    if (file.hasFileExtension (kContainerExtension))      // v1.6: shared set with all audio in one file
        return loadSetContainer (file, error);
    return loadSetXml (juce::XmlDocument::parse (file), error);
}

bool LiveLooperProcessor::loadSetXml (std::unique_ptr<juce::XmlElement> xml, juce::String& error)
{
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
    xml->setAttribute ("push", pushEnabledSetting);
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
//============================================================================== v1.6 rhythm (branch v1.6-rhythm)
void LiveLooperProcessor::updateDrumBank()
{
    auto& packs = SoundPacks::get();
    currentBank = packs.getBank();
    soundPackVersion = packs.getVersion();
    engine.setDrumBankPointer (currentBank.get());
}

void LiveLooperProcessor::refreshSoundPacks()
{
    SoundPacks::get().rescan();
    updateDrumBank();
}

bool LiveLooperProcessor::importSoundPack (const juce::File& f, juce::String& error)
{
    const bool ok = SoundPacks::get().importPack (f, error);
    updateDrumBank();
    return ok;
}

juce::String LiveLooperProcessor::getUserPatternName (int slot) const
{
    if (! juce::isPositiveAndBelow (slot, BeatPatternBank::kNumSlots))
        return {};
    return userPatternNames[(size_t) slot];
}

void LiveLooperProcessor::setUserPatternName (int slot, const juce::String& name)
{
    if (juce::isPositiveAndBelow (slot, BeatPatternBank::kNumSlots))
    {
        userPatternNames[(size_t) slot] = name.substring (0, 40);
        sendChangeMessage();
    }
}

void LiveLooperProcessor::storeUserPattern (int slot, const BeatPattern& p, const juce::String& name)
{
    if (! juce::isPositiveAndBelow (slot, BeatPatternBank::kNumSlots))
        return;
    userPatterns.store (slot, p);
    userPatternNames[(size_t) slot] = name.substring (0, 40);
    sendChangeMessage();
}

void LiveLooperProcessor::selectUserPattern (int slot)
{
    if (auto* p = apvts.getParameter (gRhythmPattern))
    {
        const int idx = RhythmGenerator::getNumPatterns() + juce::jlimit (0, BeatPatternBank::kNumSlots - 1, slot);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) idx));
        p->endChangeGesture();
    }
}

int LiveLooperProcessor::getCurrentKit() const noexcept    { return juce::roundToInt (rhythmKit->load()); }
int LiveLooperProcessor::getCurrentPattern() const noexcept { return juce::roundToInt (rhythmPattern->load()); }

int LiveLooperProcessor::prepareEditSlot()
{
    const int current = getCurrentPattern();
    if (RhythmGenerator::isUserPattern (current))
    {
        beatsEditSlot = RhythmGenerator::userSlotOf (current);
        return beatsEditSlot;
    }
    // a preset plays: editing starts from a copy of it in the edit slot (an empty slot when the edit slot is used)
    int slot = beatsEditSlot;
    if (! userPatterns.isEmpty (slot))
        for (int s = 0; s < BeatPatternBank::kNumSlots; ++s)
            if (userPatterns.isEmpty (s)) { slot = s; break; }
    if (current > 0)
        storeUserPattern (slot, RhythmGenerator::presetToPattern (current), i18n::tr (RhythmGenerator::getPatternName (current)));
    beatsEditSlot = slot;
    selectUserPattern (slot);
    return slot;
}

juce::String LiveLooperProcessor::getKitDisplayName (int k) const
{
    if (k < RhythmGenerator::getNumKits())
        return i18n::tr (RhythmGenerator::getKitName (k));
    const int slot = k - RhythmGenerator::getNumKits();
    const auto name = SoundPacks::get().getSlotName (slot);
    return name.isNotEmpty() ? name : i18n::tr ("Pack Kit %1 (empty)", juce::String (slot + 1));
}

juce::String LiveLooperProcessor::getPatternDisplayName (int p) const
{
    if (! RhythmGenerator::isUserPattern (p))
        return i18n::tr (RhythmGenerator::getPatternName (p));
    const int slot = RhythmGenerator::userSlotOf (p);
    const auto name = getUserPatternName (slot);
    return i18n::tr ("User %1", juce::String (slot + 1)) + (name.isNotEmpty() ? ": " + name
                                                                             : (userPatterns.isEmpty (slot) ? i18n::tr (" (empty)") : juce::String()));
}

juce::String LiveLooperProcessor::getVoiceDisplayName (int voice) const
{
    const int k = getCurrentKit();
    const int v = juce::jlimit (0, 7, voice);
    if (currentBank != nullptr && k >= RhythmGenerator::getNumKits())
        if (const auto* kit = currentBank->getKit (k); kit != nullptr && ! kit->voiceNames[(size_t) v].empty())
            return juce::String (kit->voiceNames[(size_t) v]);
    return i18n::tr (RhythmGenerator::getKitVoiceName (k, v));
}

void LiveLooperProcessor::writeBeatsState (juce::XmlElement& root) const
{
    auto* beats = root.createNewChildElement ("BEATS");
    beats->setAttribute ("editSlot", beatsEditSlot);
    for (int s = 0; s < BeatPatternBank::kNumSlots; ++s)
    {
        if (userPatterns.isEmpty (s) && userPatternNames[(size_t) s].isEmpty())
            continue;
        const auto p = userPatterns.load (s);
        auto* e = beats->createNewChildElement ("PATTERN");
        e->setAttribute ("slot", s);
        e->setAttribute ("name", userPatternNames[(size_t) s]);
        e->setAttribute ("stepsPerBar", p.stepsPerBar);
        e->setAttribute ("bars", p.bars);
        for (int r = 0; r < BeatPattern::kRows; ++r)
        {
            juce::String hex;
            for (int st = 0; st < p.numSteps(); ++st)
                hex << juce::String::toHexString ((int) p.get (r, st)).paddedLeft ('0', 2);
            e->setAttribute ("row" + juce::String (r), hex);
        }
    }
}

void LiveLooperProcessor::readBeatsState (const juce::XmlElement& root)
{
    for (int s = 0; s < BeatPatternBank::kNumSlots; ++s)
    {
        userPatterns.store (s, BeatPattern());
        userPatternNames[(size_t) s] = {};
    }
    const auto* beats = root.getChildByName ("BEATS");
    if (beats == nullptr)
        return;
    beatsEditSlot = juce::jlimit (0, BeatPatternBank::kNumSlots - 1, beats->getIntAttribute ("editSlot", 0));
    for (auto* e : beats->getChildWithTagNameIterator ("PATTERN"))
    {
        const int s = e->getIntAttribute ("slot", -1);
        if (! juce::isPositiveAndBelow (s, BeatPatternBank::kNumSlots))
            continue;
        BeatPattern p;
        p.stepsPerBar = e->getIntAttribute ("stepsPerBar", 16) >= 32 ? 32 : 16;
        p.bars = juce::jlimit (1, BeatPattern::kMaxBars, e->getIntAttribute ("bars", 1));
        for (int r = 0; r < BeatPattern::kRows; ++r)
        {
            const auto hex = e->getStringAttribute ("row" + juce::String (r));
            for (int st = 0; st < p.numSteps() && st * 2 + 1 < hex.length(); ++st)
                p.set (r, st, hex.substring (st * 2, st * 2 + 2).getHexValue32());
        }
        userPatterns.store (s, p);
        userPatternNames[(size_t) s] = e->getStringAttribute ("name");
    }
}
} // namespace livelooper

#if ! LIVELOOPER_NO_PLUGIN_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new livelooper::LiveLooperProcessor();
}
#endif
