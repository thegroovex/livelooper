// v1.7 (branch v1.7-wip): latency measurement + compensation, velocity curves, generic MIDI feedback.
#include "PluginProcessor.h"
#include "AppSettings.h"
#include "Localisation.h"
#include "push/PushDevice.h"

namespace livelooper
{
using namespace params;

void LiveLooperProcessor::initV17()
{
    latComp      = apvts.getRawParameterValue (gLatComp);
    latOffset    = apvts.getRawParameterValue (gLatOffset);
    velPush      = apvts.getRawParameterValue (gVelPush);
    velKo2       = apvts.getRawParameterValue (gVelKo2);
    velMidi      = apvts.getRawParameterValue (gVelMidi);
    midiFb       = apvts.getRawParameterValue (gMidiFeedback);
    pushProgress = apvts.getRawParameterValue (gPushProgress);
    if (wrapperType != wrapperType_Standalone)
    {
        const juce::ScopedLock sl (deviceKeyLock);
        audioDeviceKey = "plugin: " + juce::String (juce::PluginHostType().getHostDescription());
    }
}

//============================================================================== latency
juce::String LiveLooperProcessor::latencySettingsKey (const juce::String& deviceKey)
{
    juce::String k;
    for (auto c : deviceKey.toLowerCase())
        k << (juce::CharacterFunctions::isLetterOrDigit (c) ? juce::String::charToString (c) : juce::String ("_"));
    return "latency." + k.substring (0, 120);
}

juce::String LiveLooperProcessor::getAudioDeviceKey() const
{
    const juce::ScopedLock sl (deviceKeyLock);
    return audioDeviceKey.isNotEmpty() ? audioDeviceKey : juce::String ("default");
}

void LiveLooperProcessor::setAudioDeviceInfo (const juce::String& deviceKey, int reportedRoundTripSamples)
{
    reportedLatency.store (juce::jmax (0, reportedRoundTripSamples));
    const juce::ScopedLock sl (deviceKeyLock);
    if (deviceKey != audioDeviceKey)
    {
        audioDeviceKey = deviceKey;
        cachedMeasuredMs = -2.0;
    }
}

double LiveLooperProcessor::getMeasuredLatencyMs() const
{
    const auto v = appsettings::getValue (latencySettingsKey (getAudioDeviceKey()));
    return v.isEmpty() ? -1.0 : juce::jlimit (0.0, 1000.0, v.getDoubleValue());
}

void LiveLooperProcessor::setMeasuredLatencyMs (double ms)
{
    appsettings::setValue (latencySettingsKey (getAudioDeviceKey()), juce::String (juce::jlimit (0.0, 1000.0, ms), 3));
    cachedMeasuredMs = -2.0;
}

void LiveLooperProcessor::clearMeasuredLatency()
{
    appsettings::setValue (latencySettingsKey (getAudioDeviceKey()), {});
    cachedMeasuredMs = -2.0;
}

bool LiveLooperProcessor::isLatencyCompensationOn() const noexcept
{
    return latComp == nullptr || latComp->load() >= 0.5f;
}

float LiveLooperProcessor::getLatencyOffsetMs() const noexcept
{
    return latOffset != nullptr ? latOffset->load() : 0.0f;
}

void LiveLooperProcessor::setLatencyOffsetMs (float ms)
{
    if (auto* p = apvts.getParameter (gLatOffset))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (-50.0f, 50.0f, ms)));
        p->endChangeGesture();
    }
}

int LiveLooperProcessor::getLatencySource() const
{
    // record K.O. II: its own offset (SETUP > Controllers > K.O. II > Latency offset) stays in charge
    if (isKo2Connected() && getControllerAudioMode() == 1)
        return 2;
    if (getMeasuredLatencyMs() >= 0.0)
        return 0;
    if (reportedLatency.load() > 0)
        return 1;
    return 3;
}

int LiveLooperProcessor::getLatencyCompensationSamples() const
{
    if (! isLatencyCompensationOn())
        return 0;
    const double sr = currentSampleRate > 0.0 ? currentSampleRate : 44100.0;
    double base = 0.0;
    switch (getLatencySource())
    {
        case 0: base = getMeasuredLatencyMs() * 0.001 * sr; break;
        case 1: base = (double) reportedLatency.load(); break;
        case 2: base = (double) getKo2EffectiveOffsetSamples(); break;
        default: break;
    }
    const double total = base + getLatencyOffsetMs() * 0.001 * sr;
    return juce::jmax (0, (int) std::lround (total));
}

void LiveLooperProcessor::startLatencyMeasurement()
{
    latencyMeasure.start (currentSampleRate > 0.0 ? currentSampleRate : 44100.0);
}

bool LiveLooperProcessor::updateLatency()
{
    bool finished = false;
    if (latencyMeasureRequested.exchange (false))
        startLatencyMeasurement();
    if (latencyMeasure.update())
    {
        finished = true;
        const auto status = latencyMeasure.getStatus();
        if (status == LatencyMeasure::Status::Done)
        {
            setMeasuredLatencyMs (latencyMeasure.getResultMs());
            postControllerNotice (i18n::tr ("Latency: %1 ms (%2 samples)", juce::String (latencyMeasure.getResultMs(), 1),
                                            juce::String (latencyMeasure.getResultSamples())));
        }
        else if (status == LatencyMeasure::Status::Unstable)
            postControllerNotice (i18n::tr ("Latency measurement unstable - not saved"));
        else
            postControllerNotice (i18n::tr ("No ping detected - connect an output to an input (cable) or hold a microphone to the speakers"));
        sendChangeMessage();
    }
    compensationSamples.store (getLatencyCompensationSamples());
    return finished;
}

void LiveLooperProcessor::processV17 (const float* inL, const float* inR, juce::AudioBuffer<float>& buffer, int numSamples) noexcept
{
    // the ping replaces everything on the main output; other outputs stay silent while measuring
    latencyMeasure.process (inL, inR, buffer.getWritePointer (0), buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr, numSamples);
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);
}

//============================================================================== velocity curves
velocity::Curve LiveLooperProcessor::getVelocityCurve (VelocitySource s) const noexcept
{
    const std::atomic<float>* p = s == VelocitySource::Push ? velPush : (s == VelocitySource::Ko2 ? velKo2 : velMidi);
    return p != nullptr ? velocity::fromIndex (juce::roundToInt (p->load())) : velocity::Curve::Normal;
}

void LiveLooperProcessor::setVelocityCurve (VelocitySource s, velocity::Curve c)
{
    const auto& id = s == VelocitySource::Push ? gVelPush : (s == VelocitySource::Ko2 ? gVelKo2 : gVelMidi);
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) (int) c));
        p->endChangeGesture();
    }
}

//============================================================================== generic MIDI feedback
bool LiveLooperProcessor::isMidiFeedbackOn() const noexcept { return midiFb != nullptr && midiFb->load() >= 0.5f; }
bool LiveLooperProcessor::isPushProgressShown() const noexcept { return pushProgress == nullptr || pushProgress->load() >= 0.5f; }

juce::String LiveLooperProcessor::getMidiFeedbackOutput() const { return appsettings::getValue ("midifb.output"); }
void LiveLooperProcessor::setMidiFeedbackOutput (const juce::String& identifier) { appsettings::setValue ("midifb.output", identifier); }

MidiFeedback::Profile LiveLooperProcessor::getMidiFeedbackProfile() const
{
    MidiFeedback::Profile p;
    p.onValue = juce::jlimit (0, 127, appsettings::getValue ("midifb.on", "127").getIntValue());
    p.offValue = juce::jlimit (0, 127, appsettings::getValue ("midifb.off", "0").getIntValue());
    const int blink = appsettings::getValue ("midifb.blink", "-1").getIntValue();
    p.softBlink = blink < 0;
    p.blinkValue = blink < 0 ? 2 : juce::jlimit (0, 127, blink);
    return p;
}

void LiveLooperProcessor::setMidiFeedbackProfile (const MidiFeedback::Profile& p)
{
    appsettings::setValue ("midifb.on", juce::String (p.onValue));
    appsettings::setValue ("midifb.off", juce::String (p.offValue));
    appsettings::setValue ("midifb.blink", juce::String (p.softBlink ? -1 : p.blinkValue));
}

std::vector<MidiFeedback::Item> LiveLooperProcessor::buildMidiFeedbackItems() const
{
    std::vector<MidiFeedback::Item> items;
    using Led = MidiFeedback::Led;
    for (const auto& [key, index] : midiMap.getAllMappings())
    {
        if (index < 0 || index >= paramList.size())
            continue;
        auto* p = paramList.getUnchecked (index);
        const auto id = p->getParameterID();
        MidiFeedback::Item item;
        item.key = key;
        const float v = p->getValue();

        if (id.length() > 3 && id[0] == 't' && id[2] == '_' && isTrigger (id))
        {
            const int t = id[1] - '1';
            const auto name = id.substring (3);
            if (! juce::isPositiveAndBelow (t, kNumTracks))
                continue;
            const auto& st = engine.getTrackStatus (t);
            const auto state = (LoopTrack::State) st.state.load();
            const auto pending = (LooperEngine::Pending) st.pending.load();
            const bool waiting = pending == LooperEngine::Pending::Record || pending == LooperEngine::Pending::Play
                              || pending == LooperEngine::Pending::AutoRecord;
            if (name == kRecPlay)
                item.led = (state == LoopTrack::State::Recording || state == LoopTrack::State::Overdubbing || waiting) ? Led::Blink
                         : (state == LoopTrack::State::Playing ? Led::On : Led::Off);
            else if (name == kStop)
                item.led = state == LoopTrack::State::Stopped ? Led::On : Led::Off;
            else if (name == kUndo)
                item.led = st.canUndo.load() ? Led::On : Led::Off;
            else if (name == kRedo)
                item.led = st.canRedo.load() ? Led::On : Led::Off;
            else
                item.led = state != LoopTrack::State::Empty ? Led::On : Led::Off;
        }
        else if (id.startsWith ("scene") && isTrigger (id))
            item.led = getLastScene() == id.substring (5).getIntValue() - 1 ? Led::On : Led::Off;
        else if (id == gAllStartStop || id == gAllStart)
        {
            bool any = false;
            for (int t = 0; t < kNumTracks; ++t)
                any = any || engine.getTrackStatus (t).state.load() == (int) LoopTrack::State::Playing
                          || engine.getTrackStatus (t).state.load() == (int) LoopTrack::State::Overdubbing;
            item.led = any ? Led::On : Led::Off;
        }
        else if (id == gLatMeasure)
            item.led = latencyMeasure.isRunning() ? Led::Blink : Led::Off;
        else if (isTrigger (id))
            item.led = Led::Off;
        else if (dynamic_cast<juce::AudioParameterBool*> (p) != nullptr || isMomentary (id))
            item.led = v >= 0.5f ? Led::On : Led::Off;
        else if (key.isCC)
            item.value = juce::jlimit (0, 127, juce::roundToInt (v * 127.0f));    // continuous / choice: the value
        else
            item.led = v > 0.0f ? Led::On : Led::Off;
        items.push_back (item);
    }
    return items;
}

void LiveLooperProcessor::renderMidiFeedback (std::vector<juce::MidiMessage>& out, bool full)
{
    midiFeedback.setProfile (getMidiFeedbackProfile());
    const double ppq = engine.getTimelinePpq();
    const bool blink = engine.isTimelineRunning() ? std::fmod (ppq, 1.0) < 0.5
                                                  : (juce::Time::getMillisecondCounter() / 250) % 2 == 0;
    midiFeedback.render (buildMidiFeedbackItems(), blink, out, full);
}

void LiveLooperProcessor::timerV17()
{
    updateLatency();

    if (! isMidiFeedbackOn())
    {
        if (feedbackOut != nullptr)
        {
            feedbackOut.reset();
            feedbackOutId.clear();
        }
        return;
    }
    const auto wanted = getMidiFeedbackOutput();
    if (wanted != feedbackOutId)
    {
        feedbackOut.reset();
        feedbackOutId = wanted;
        midiFeedback.invalidate();
        if (wanted.isNotEmpty())
            for (const auto& d : juce::MidiOutput::getAvailableDevices())
                if (d.identifier == wanted || d.name == wanted)
                {
                    feedbackOut = juce::MidiOutput::openDevice (d.identifier);
                    break;
                }
    }
    if (feedbackOut == nullptr)
        return;
    std::vector<juce::MidiMessage> out;
    renderMidiFeedback (out, ++feedbackFullCounter % 150 == 0);     // everything again every few seconds
    for (const auto& m : out)
        feedbackOut->sendMessageNow (m);
}
} // namespace livelooper
