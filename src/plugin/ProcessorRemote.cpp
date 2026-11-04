// livelooper remote control: glue between the processor and the remote server (src/remote).
// Server threads only read atomics here; every action is handed to the message thread, where it
// takes the same path as the GUI / keyboard / MIDI mapping (trigger parameters, toggles).
#include "PluginProcessor.h"
#include "Localisation.h"
#include "remote/RemoteServer.h"
#include "remote/RemoteSettings.h"

namespace livelooper
{
using namespace params;
using i18n::tr;

namespace
{
    /** Alive flag shared with the callbacks that are still queued on the message thread. */
    struct Guard
    {
        std::atomic<LiveLooperProcessor*> proc { nullptr };
    };
}

struct RemoteRuntime : remote::RemoteHost
{
    explicit RemoteRuntime (LiveLooperProcessor& p) : proc (p), guard (std::make_shared<Guard>())
    {
        guard->proc.store (&p);
        for (auto& v : waveVersions)
            v = 1;
    }

    ~RemoteRuntime() override
    {
        server.reset();             // joins every server thread before the rest goes
        guard->proc.store (nullptr);
    }

    LiveLooperProcessor& proc;
    std::shared_ptr<Guard> guard;
    std::unique_ptr<remote::DeviceStore> ownStore;
    std::unique_ptr<remote::RemoteServer> server;
    bool bindFailed = false;
    int failedPort = 0;
    int retryTicks = 0, waveTicks = 0;

    // waveform cache: written on the message thread, read by the pump thread
    juce::CriticalSection waveLock;
    std::array<std::vector<uint8_t>, remote::kMaxRemoteTracks> waves;
    std::array<uint32_t, remote::kMaxRemoteTracks> waveVersions {};

    //========================================================================== RemoteHost (server threads)
    int getNumParams() override { return proc.paramList.size(); }

    remote::ParamInfo getParamInfo (int index) override
    {
        remote::ParamInfo info;
        auto* p = proc.paramList[index];
        if (p == nullptr)
            return info;
        info.id = p->getParameterID();
        info.name = p->getName (64);
        const auto& r = p->getNormalisableRange();
        info.minValue = r.start;
        info.maxValue = r.end;
        info.defaultValue = p->convertFrom0to1 (p->getDefaultValue());
        if (dynamic_cast<juce::AudioParameterBool*> (p) != nullptr)
            info.type = "bool";
        else if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p))
        {
            info.type = "choice";
            info.choices = c->choices;
        }
        return info;
    }

    float getParamValue (int index) override
    {
        auto* p = proc.paramList[index];
        return p != nullptr ? p->getValue() : 0.0f;
    }

    template <typename Fn>
    void onMessageThread (Fn&& fn)
    {
        std::weak_ptr<Guard> weak = guard;
        juce::MessageManager::callAsync ([weak, fn = std::forward<Fn> (fn)]() mutable {
            if (auto g = weak.lock())
                if (auto* p = g->proc.load())
                    fn (*p);
        });
    }

    void setParam (int index, float normalized) override
    {
        onMessageThread ([index, normalized] (LiveLooperProcessor& p) {
            if (auto* param = p.getParamForIndex (index))
                param->setValueNotifyingHost (normalized);
        });
    }

    void setGesture (int index, bool begin) override
    {
        onMessageThread ([index, begin] (LiveLooperProcessor& p) {
            if (auto* param = p.getParamForIndex (index))
            {
                if (begin) param->beginChangeGesture();
                else       param->endChangeGesture();
            }
        });
    }

    /** TRIGGER: the trigger parameters MIDI learn and the keyboard use (rising edge on the audio
        thread -> the same command), toggles like a learned MIDI toggle. */
    void trigger (uint8_t act, uint8_t track, bool down) override
    {
        if (! down)
            return;                 // every mapped action fires on press
        const bool global = track == remote::kGlobalTrack;
        if (! global && track >= kNumTracks)
            return;
        const int t = (int) track;
        juce::String id;
        bool toggle = false;
        switch (act)
        {
            case remote::action::RecPlayDub:   if (! global) id = trackId (t, kRecPlay); break;
            case remote::action::Stop:         id = global ? gAllStop : trackId (t, kStop); break;
            case remote::action::Clear:        id = global ? gClearAll : trackId (t, kClear); break;
            case remote::action::Undo:         if (! global) id = trackId (t, kUndo); break;
            case remote::action::MuteToggle:   if (! global) { id = trackId (t, kMute); toggle = true; } break;
            case remote::action::SoloToggle:   if (! global) { id = trackId (t, kSolo); toggle = true; } break;
            case remote::action::AllStartStop: id = gAllStartStop; break;
            case remote::action::TapTempo:     id = gTap; break;
            case remote::action::Capture:      id = global ? gCapture : trackId (t, kCapture); break;
            default: break;
        }
        if (id.isEmpty())
            return;
        onMessageThread ([id, toggle] (LiveLooperProcessor& p) {
            if (! toggle)
            {
                p.pressTrigger (id);
                return;
            }
            if (auto* param = p.getState().getParameter (id))
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost (param->getValue() >= 0.5f ? 0.0f : 1.0f);
                param->endChangeGesture();
            }
        });
    }

    static float raw (std::atomic<float>* a) { return a != nullptr ? a->load() : 0.0f; }

    void readTelemetry (remote::Telemetry& t) override
    {
        auto& eng = proc.engine;
        t.bpm = (float) eng.getCurrentBpm();
        t.playing = eng.isTimelineRunning();
        t.numTracks = juce::jlimit (0, juce::jmin (remote::kMaxRemoteTracks, (int) kNumTracks), proc.getVisibleTracks());
        const double bar = juce::jmax (0.25, eng.getMeasurePpq());
        for (int i = 0; i < t.numTracks; ++i)
        {
            const auto& st = eng.getTrackStatus (i);
            auto& tr = t.tracks[(size_t) i];
            const auto state = (LoopTrack::State) st.state.load();
            const auto pending = (LooperEngine::Pending) st.pending.load();
            switch (state)
            {
                case LoopTrack::State::Recording:   tr.state = "REC"; break;
                case LoopTrack::State::Playing:     tr.state = "PLAY"; break;
                case LoopTrack::State::Overdubbing: tr.state = "DUB"; break;
                case LoopTrack::State::Stopped:     tr.state = "STOP"; break;
                case LoopTrack::State::Empty:
                default:
                    tr.state = (pending == LooperEngine::Pending::Record || pending == LooperEngine::Pending::AutoRecord) ? "ARMED" : "EMPTY";
                    break;
            }
            tr.bars = (float) (st.lengthPpq.load() / bar);
            tr.phase = juce::jlimit (0.0f, 1.0f, st.progress.load());
            tr.meter = juce::jlimit (0.0f, 1.0f, st.remoteMeter.exchange (0.0f));
            const auto& tp = proc.trackParams[(size_t) i];
            if (auto* p = proc.getState().getParameter (trackId (i, kLevel))) tr.level = p->getValue();
            if (auto* p = proc.getState().getParameter (trackId (i, kPan)))   tr.pan = p->getValue();
            tr.muted = raw (tp.mute) >= 0.5f;
            tr.solo = raw (tp.solo) >= 0.5f;
        }
        t.masterMeter = juce::jlimit (0.0f, 1.0f, eng.takeRemoteMasterMeter());
    }

    bool readWave (int track, std::vector<uint8_t>& peaks, uint32_t& version) override
    {
        if (! juce::isPositiveAndBelow (track, remote::kMaxRemoteTracks))
            return false;
        const juce::ScopedLock sl (waveLock);
        peaks = waves[(size_t) track];
        version = waveVersions[(size_t) track];
        return ! peaks.empty();
    }

    //========================================================================== message thread
    /** Refreshes the waveform cache (about 5 times a second while a device is connected). */
    void updateWaves()
    {
        if (server == nullptr || server->getNumClients (true) == 0)
            return;
        if (++waveTicks < 6)
            return;
        waveTicks = 0;
        std::vector<float> bins ((size_t) remote::kMaxWaveCount);
        const int n = juce::jmin (remote::kMaxRemoteTracks, (int) kNumTracks);
        for (int t = 0; t < n; ++t)
        {
            std::vector<uint8_t> peaks;
            auto& eng = proc.engine;
            const auto state = (LoopTrack::State) eng.getTrackStatus (t).state.load();
            if (t < proc.getVisibleTracks() && state != LoopTrack::State::Empty)
            {
                auto& tr = eng.getTrack (t);
                if (tr.getWaveform (bins.data(), remote::kMaxWaveCount, tr.getRecordingSpan()))
                {
                    peaks.resize (bins.size());
                    for (size_t i = 0; i < bins.size(); ++i)
                        peaks[i] = (uint8_t) juce::jlimit (0, 255, juce::roundToInt (juce::jlimit (0.0f, 1.0f, bins[i]) * 255.0f));
                }
            }
            const juce::ScopedLock sl (waveLock);
            if (peaks != waves[(size_t) t])
            {
                waves[(size_t) t] = std::move (peaks);
                ++waveVersions[(size_t) t];
            }
        }
    }

    remote::RemoteServer::Config makeConfig (int port) const
    {
        remote::RemoteServer::Config c;
        c.port = port;
        const bool app = proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
        c.instanceName = juce::String (app ? "livelooper" : "livelooper plug-in") + " (" + juce::SystemStats::getComputerName() + ")";
        c.instanceId = juce::Uuid().toDashedString();
        c.version = LIVELOOPER_VERSION;
        return c;
    }

    bool startServer (int port, remote::DeviceStore& store, bool discovery, const juce::String& code)
    {
        server.reset();
        auto config = makeConfig (port);
        config.discovery = discovery;
        server = std::make_unique<remote::RemoteServer> (*this, store, config);
        server->setPairingCode (code);
        if (server->start())
        {
            bindFailed = false;
            return true;
        }
        server.reset();
        bindFailed = true;
        failedPort = port;
        return false;
    }
};

//==============================================================================
void LiveLooperProcessor::applyRemoteSettings()
{
    if (wrapperType == wrapperType_Undefined)
        return;                    // unit tests: only startRemoteForTests
    const bool on = remote::settings::isEnabled();
    const int port = remote::settings::getPort();
    if (! on)
    {
        stopRemote();
        return;
    }
    if (remoteRuntime != nullptr && remoteRuntime->server != nullptr && remoteRuntime->server->getConfig().port == port)
        return;                    // already listening there
    if (remoteRuntime == nullptr)
        remoteRuntime = std::make_shared<RemoteRuntime> (*this);
    if (remoteRuntime->ownStore == nullptr)
        remoteRuntime->ownStore = std::make_unique<remote::settings::SettingsDeviceStore>();
    remoteRuntime->startServer (port, *remoteRuntime->ownStore, true, remote::settings::getPairingCode());
}

int LiveLooperProcessor::startRemoteForTests (int port, const juce::String& pairingCode, remote::DeviceStore& store)
{
    stopRemote();
    remoteRuntime = std::make_shared<RemoteRuntime> (*this);
    if (! remoteRuntime->startServer (port, store, false, pairingCode))
        return 0;
    return remoteRuntime->server->getBoundPort();
}

void LiveLooperProcessor::stopRemote()
{
    remoteRuntime.reset();
}

remote::RemoteServer* LiveLooperProcessor::getRemoteServer() const noexcept
{
    return remoteRuntime != nullptr ? remoteRuntime->server.get() : nullptr;
}

void LiveLooperProcessor::remotePairingCodeChanged()
{
    if (auto* s = getRemoteServer())
        s->setPairingCode (remote::settings::getPairingCode());
}

void LiveLooperProcessor::remoteDeviceRevoked (const juce::String& deviceId)
{
    if (auto* s = getRemoteServer())
        s->disconnectDevice (deviceId);
}

juce::String LiveLooperProcessor::getRemoteStatusText() const
{
    if (! remote::settings::isEnabled())
        return tr ("Remote control is off");
    if (remoteRuntime != nullptr && remoteRuntime->bindFailed)
        return tr ("Port %1 is in use (another livelooper?) - trying again", juce::String (remoteRuntime->failedPort));
    auto* s = getRemoteServer();
    if (s == nullptr)
        return tr ("Remote control is off");
    juce::StringArray ips;
    for (auto& a : juce::IPAddress::getAllAddresses (false))
        if (a != juce::IPAddress::local() && remote::isLocalAddress (a.toString()))
            ips.add (a.toString());
    auto text = tr ("Ready: %1, port %2", ips.isEmpty() ? juce::String ("-") : ips.joinIntoString (", "), juce::String (s->getBoundPort()));
    const int n = s->getNumClients (true);
    if (n > 0)
        text << " - " << tr ("%1 connected", juce::String (n));
    return text;
}

void LiveLooperProcessor::timerRemote()
{
    if (remoteRuntime == nullptr)
        return;
    remoteRuntime->updateWaves();
    // the port was taken (another instance): try again every 5 s
    if (remoteRuntime->bindFailed && remoteRuntime->ownStore != nullptr && ++remoteRuntime->retryTicks >= 150)
    {
        remoteRuntime->retryTicks = 0;
        if (remote::settings::isEnabled())
            remoteRuntime->startServer (remote::settings::getPort(), *remoteRuntime->ownStore, true, remote::settings::getPairingCode());
    }
}
} // namespace livelooper
