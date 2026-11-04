#include "RemoteServer.h"

#include <cmath>
#include <deque>
#include <limits>

namespace livelooper::remote
{
namespace
{
    constexpr size_t kMaxQueuedBytes = 4u << 20;        // a client that stops reading is dropped beyond this
    constexpr juce::int64 kStallTimeoutMs = 10000;      // ... or when nothing could be written for this long
    constexpr int kMaxBadCodes = 5;                     // wrong pairing codes per minute and connection

    juce::var errorJson (const juce::String& code, const juce::String& message)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("code", code);
        o->setProperty ("message", message);
        return juce::var (o);
    }

    juce::var protoArray()
    {
        juce::Array<juce::var> a;
        a.add (kProtoMajor);
        a.add (kProtoMinor);
        return juce::var (a);
    }
}

//============================================================================== client connection
class RemoteServer::Client : public juce::Thread
{
public:
    Client (RemoteServer& o, std::unique_ptr<juce::StreamingSocket> s)
        : juce::Thread ("livelooper remote client"), owner (o), socket (std::move (s))
    {
        address = socket->getHostName();
        waveVersions.fill (0);
    }

    ~Client() override
    {
        stopThread (3000);
    }

    bool isAuthenticated() const noexcept { return authed.load(); }
    bool isDead() const noexcept          { return dead.load() || ! isThreadRunning(); }
    juce::String getDeviceId() const      { const juce::ScopedLock sl (idLock); return deviceId; }
    void kill() noexcept                  { dead.store (true); signalThreadShouldExit(); }

    /** Queues a frame (any thread). Beyond the queue limit the client is dropped. */
    void send (std::vector<uint8_t> frame)
    {
        const juce::ScopedLock sl (outLock);
        if (dead.load())
            return;
        if (queuedBytes + frame.size() > kMaxQueuedBytes)
        {
            dead.store (true);         // stalled: give up on it (only this client)
            return;
        }
        queuedBytes += frame.size();
        outQueue.push_back (std::move (frame));
    }

    void sendJson (uint8_t type, const juce::var& json) { send (encodeJsonFrame (type, json)); }
    void sendError (const juce::String& code, const juce::String& message) { sendJson (msg::Error, errorJson (code, message)); }

    /** Pump thread: queues PARAM_SET for every parameter whose value differs from what this client
        last saw (or set itself). */
    void echoParams (const std::vector<float>& values)
    {
        if (! authed.load())
            return;
        std::vector<uint16_t> changed;
        {
            const juce::ScopedLock sl (echoLock);
            if (lastSent.size() != values.size())
                lastSent.assign (values.size(), std::numeric_limits<float>::quiet_NaN());
            for (size_t i = 0; i < values.size(); ++i)
                if (! (std::abs (values[i] - lastSent[i]) < 1.0e-6f))
                {
                    lastSent[i] = values[i];
                    changed.push_back ((uint16_t) i);
                }
        }
        for (auto i : changed)
            send (encodeFrame (msg::ParamSet, makeParamSet (i, values[i])));
    }

    std::array<uint32_t, kMaxRemoteTracks> waveVersions;      // pump thread only (and before authed is set)

    void run() override
    {
        // non-blocking from here on (JUCE switches the socket mode in read()); writes never block
        char scratch[1];
        socket->read (scratch, 0, false);
        lastProgressMs = juce::Time::currentTimeMillis();

        std::vector<uint8_t> buf (16384);
        while (! threadShouldExit() && ! dead.load())
        {
            if (! flush())
                break;
            if (closing)
            {
                const juce::ScopedLock sl (outLock);
                if (outQueue.empty() || juce::Time::currentTimeMillis() - closingSince > 1000)
                    break;
            }
            const int r = socket->waitUntilReady (true, 5);
            if (r < 0)
                break;
            if (r == 0)
                continue;
            const int n = socket->read (buf.data(), (int) buf.size(), false);
            if (n <= 0)
                break;
            decoder.feed (buf.data(), (size_t) n);
            Frame f;
            for (;;)
            {
                const auto res = decoder.next (f);
                if (res == FrameDecoder::Result::NeedMore)
                    break;
                if (res == FrameDecoder::Result::Error)
                {
                    sendError ("bad_frame", "invalid frame length");
                    closeSoon();
                    break;
                }
                handle (f);
                if (closing)
                    break;
            }
        }
        flush();
        dead.store (true);
        authed.store (false);
        socket->close();
    }

private:
    RemoteServer& owner;
    std::unique_ptr<juce::StreamingSocket> socket;
    juce::String address;
    std::atomic<bool> authed { false }, dead { false };
    mutable juce::CriticalSection idLock;
    juce::String deviceId;
    std::vector<uint8_t> nonce;
    FrameDecoder decoder;
    juce::Array<juce::int64> badCodeTimes;
    bool closing = false;
    juce::int64 closingSince = 0;

    juce::CriticalSection outLock;
    std::deque<std::vector<uint8_t>> outQueue;
    size_t queuedBytes = 0, frontOffset = 0;
    juce::int64 lastProgressMs = 0;

    juce::CriticalSection echoLock;
    std::vector<float> lastSent;

    void closeSoon()
    {
        closing = true;
        closingSince = juce::Time::currentTimeMillis();
    }

    /** Writes as much as the socket takes without blocking. False = connection unusable. */
    bool flush()
    {
        for (;;)
        {
            std::vector<uint8_t>* front = nullptr;
            {
                const juce::ScopedLock sl (outLock);
                if (outQueue.empty())
                {
                    lastProgressMs = juce::Time::currentTimeMillis();
                    return true;
                }
                front = &outQueue.front();      // only this thread pops, so the element stays valid
            }
            if (socket->waitUntilReady (false, 0) != 1)
                return juce::Time::currentTimeMillis() - lastProgressMs < kStallTimeoutMs;
            const int toWrite = (int) std::min<size_t> (front->size() - frontOffset, 16384);
            const int w = socket->write (front->data() + frontOffset, toWrite);
            if (w <= 0)
                return juce::Time::currentTimeMillis() - lastProgressMs < kStallTimeoutMs;
            lastProgressMs = juce::Time::currentTimeMillis();
            frontOffset += (size_t) w;
            if (frontOffset >= front->size())
            {
                const juce::ScopedLock sl (outLock);
                queuedBytes -= front->size();
                outQueue.pop_front();
                frontOffset = 0;
            }
        }
    }

    void handle (const Frame& f)
    {
        switch (f.type)
        {
            case msg::Hello: handleHello (f); return;
            case msg::Pair:  handlePair (f);  return;
            case msg::Auth:  handleAuth (f);  return;
            case msg::Ping:  send (encodeFrame (msg::Pong, f.payload)); return;
            default: break;
        }
        if (! authed.load())
        {
            sendError ("unauthorized", "authenticate first (AUTH)");
            return;
        }
        auto& host = owner.host;
        switch (f.type)
        {
            case msg::ParamSet:
            {
                uint16_t i = 0;
                float v = 0.0f;
                if (! parseParamSet (f.payload, i, v) || (int) i >= host.getNumParams())
                {
                    sendError ("bad_frame", "PARAM_SET");
                    return;
                }
                v = juce::jlimit (0.0f, 1.0f, v);
                {
                    // the sender already shows this value: no echo back unless the host changes it
                    const juce::ScopedLock sl (echoLock);
                    if ((size_t) i < lastSent.size())
                        lastSent[(size_t) i] = v;
                }
                host.setParam (i, v);
                return;
            }
            case msg::Gesture:
            {
                uint16_t i = 0;
                bool begin = false;
                if (! parseGesture (f.payload, i, begin) || (int) i >= host.getNumParams())
                {
                    sendError ("bad_frame", "GESTURE");
                    return;
                }
                host.setGesture (i, begin);
                return;
            }
            case msg::Trigger:
            {
                uint8_t act = 0, track = 0;
                bool down = false;
                if (! parseTrigger (f.payload, act, track, down))
                {
                    sendError ("bad_frame", "TRIGGER");
                    return;
                }
                host.trigger (act, track, down);
                return;
            }
            default:
                return;     // unknown types are ignored (newer minor versions)
        }
    }

    juce::var parseJson (const Frame& f)
    {
        auto v = juce::JSON::parse (f.payloadAsString());
        if (! v.isObject())
            sendError ("bad_frame", "JSON object expected");
        return v;
    }

    void handleHello (const Frame& f)
    {
        const auto j = parseJson (f);
        if (! j.isObject())
            return;
        const auto proto = j["proto"];
        const int major = proto.isArray() && proto.size() > 0 ? (int) proto[0] : 0;
        if (major != kProtoMajor)
        {
            sendError ("proto_major", major > kProtoMajor
                                          ? "livelooper speaks remote protocol " + juce::String (kProtoMajor) + "." + juce::String (kProtoMinor) + ": update livelooper"
                                          : "livelooper speaks remote protocol " + juce::String (kProtoMajor) + "." + juce::String (kProtoMinor) + ": update the app");
            closeSoon();
            return;
        }
        nonce = randomBytes (16);
        auto* o = new juce::DynamicObject();
        o->setProperty ("proto", protoArray());
        o->setProperty ("livelooper", owner.config.version);
        o->setProperty ("instance", owner.config.instanceName);
        o->setProperty ("nonce", toHex (nonce));
        o->setProperty ("caps", 0);
        sendJson (msg::Welcome, juce::var (o));
    }

    void handlePair (const Frame& f)
    {
        const auto j = parseJson (f);
        if (! j.isObject())
            return;
        const auto now = juce::Time::currentTimeMillis();
        badCodeTimes.removeIf ([now] (juce::int64 t) { return now - t > 60000; });
        if (badCodeTimes.size() >= kMaxBadCodes)
        {
            sendError ("bad_code", "too many wrong codes, try again in a minute");
            closeSoon();
            return;
        }
        const auto id = j["deviceId"].toString().trim();
        auto name = j["deviceName"].toString().trim();
        const auto code = j["code"].toString().trim();
        if (id.isEmpty() || id.length() > 128)
        {
            sendError ("bad_frame", "deviceId missing");
            return;
        }
        const auto expected = owner.getPairingCode();
        const std::vector<uint8_t> a (code.toRawUTF8(), code.toRawUTF8() + code.getNumBytesAsUTF8());
        const std::vector<uint8_t> b (expected.toRawUTF8(), expected.toRawUTF8() + expected.getNumBytesAsUTF8());
        if (expected.length() != 6 || ! constantTimeEquals (a, b))
        {
            badCodeTimes.add (now);
            sendError ("bad_code", "wrong pairing code");
            if (badCodeTimes.size() >= kMaxBadCodes)
                closeSoon();
            return;
        }
        if (name.isEmpty())
            name = "?";
        const auto key = randomBytes (32);
        owner.devices.addDevice (id, name.substring (0, 64), key);
        auto* o = new juce::DynamicObject();
        o->setProperty ("key", toHex (key));
        sendJson (msg::Paired, juce::var (o));
    }

    void handleAuth (const Frame& f)
    {
        const auto j = parseJson (f);
        if (! j.isObject())
            return;
        if (nonce.empty())
        {
            sendError ("unauthorized", "send HELLO first");
            return;
        }
        const auto id = j["deviceId"].toString().trim();
        std::vector<uint8_t> key, mac;
        if (id.isEmpty() || ! owner.devices.findKey (id, key))
        {
            sendError ("not_paired", "this device is not paired (or was removed): pair again");
            return;
        }
        const auto expected = hmacSha256 (key.data(), key.size(), nonce.data(), nonce.size());
        if (! fromHex (j["hmac"].toString(), mac) || ! constantTimeEquals (mac, expected))
        {
            // the device holds a key livelooper no longer has: let it pair again
            sendError ("not_paired", "authentication failed: pair again");
            return;
        }
        {
            const juce::ScopedLock sl (idLock);
            deviceId = id;
        }
        nonce.clear();          // one nonce, one authentication
        sendJson (msg::AuthOk, juce::var (new juce::DynamicObject()));
        owner.sendInitialState (*this);
        authed.store (true);
    }
};

//============================================================================== accept thread
class RemoteServer::AcceptThread : public juce::Thread
{
public:
    explicit AcceptThread (RemoteServer& o) : juce::Thread ("livelooper remote accept"), owner (o) {}
    ~AcceptThread() override { stopThread (3000); }

    void run() override
    {
        while (! threadShouldExit())
        {
            const int r = owner.listener->waitUntilReady (true, 100);
            if (r < 0)
                break;
            if (r == 0 || threadShouldExit())
                continue;
            std::unique_ptr<juce::StreamingSocket> s (owner.listener->waitForNextConnection());
            if (s == nullptr || threadShouldExit())
                continue;
            if (! isLocalAddress (s->getHostName()))
            {
                s->close();         // only the local network (private / link-local / loopback)
                continue;
            }
            owner.addClient (std::move (s));
        }
    }

private:
    RemoteServer& owner;
};

//============================================================================== pump thread
class RemoteServer::PumpThread : public juce::Thread
{
public:
    explicit PumpThread (RemoteServer& o) : juce::Thread ("livelooper remote pump"), owner (o) {}
    ~PumpThread() override { stopThread (3000); }

    void run() override
    {
        std::unique_ptr<juce::DatagramSocket> udp;
        juce::StringArray targets;
        juce::int64 lastDiscovery = 0, lastTargets = 0, lastStateCheck = 0, lastStateSent = 0;
        juce::String lastState;
        std::vector<float> values;
        int tick = 0;

        while (! threadShouldExit())
        {
            const auto now = juce::Time::currentTimeMillis();
            owner.removeDeadClients();

            // ---- discovery, once a second
            if (owner.config.discovery && now - lastDiscovery >= 1000)
            {
                lastDiscovery = now;
                if (udp == nullptr)
                    udp = std::make_unique<juce::DatagramSocket> (true);
                if (now - lastTargets > 10000 || targets.isEmpty())
                {
                    lastTargets = now;
                    targets.clearQuick();
                    targets.add ("255.255.255.255");
                    for (auto& a : juce::IPAddress::getAllAddresses (false))
                    {
                        const auto b = juce::IPAddress::getInterfaceBroadcastAddress (a);
                        if (! b.isNull() && ! a.isNull() && a != juce::IPAddress::local())
                            targets.addIfNotAlreadyThere (b.toString());
                    }
                }
                auto* o = new juce::DynamicObject();
                o->setProperty ("name", owner.config.instanceName);
                o->setProperty ("port", owner.getBoundPort());
                o->setProperty ("version", owner.config.version);
                o->setProperty ("id", owner.config.instanceId);
                const auto text = "LLRC1 " + juce::JSON::toString (juce::var (o), true);
                for (auto& t : targets)
                    udp->write (t, kDiscoveryPort, text.toRawUTF8(), (int) text.getNumBytesAsUTF8());
            }

            const auto clients = owner.getClients();
            bool anyAuthed = false;
            for (auto& c : clients)
                anyAuthed = anyAuthed || c->isAuthenticated();

            if (anyAuthed)
            {
                Telemetry t;
                owner.host.readTelemetry (t);
                const int n = juce::jlimit (0, kMaxRemoteTracks, t.numTracks);

                // ---- TICK ~30 Hz
                Writer w;
                w.f32 (t.bpm).u8 ((uint8_t) n);
                for (int i = 0; i < n; ++i)
                    w.f32 (t.tracks[(size_t) i].phase).f32 (t.tracks[(size_t) i].meter);
                w.f32 (t.masterMeter);
                const auto tickFrame = encodeFrame (msg::Tick, w.data);

                // ---- STATE on change (max 20 Hz) and every second
                std::vector<uint8_t> stateFrame;
                if (now - lastStateCheck >= 50)
                {
                    lastStateCheck = now;
                    const auto s = owner.buildState (t);
                    if (s != lastState || now - lastStateSent >= 1000)
                    {
                        lastState = s;
                        lastStateSent = now;
                        stateFrame = encodeJsonFrame (msg::State, s);
                    }
                }

                // ---- parameter echoes (30 Hz polling: host automation, MIDI, GUI)
                const int np = owner.host.getNumParams();
                values.resize ((size_t) np);
                for (int i = 0; i < np; ++i)
                    values[(size_t) i] = owner.host.getParamValue (i);

                // ---- waveforms when their content changed (about 5 times a second)
                std::array<std::vector<uint8_t>, kMaxRemoteTracks> waves;
                std::array<uint32_t, kMaxRemoteTracks> versions {};
                const bool waveTick = (tick % 6) == 0;
                if (waveTick)
                    for (int i = 0; i < n; ++i)
                        if (! owner.host.readWave (i, waves[(size_t) i], versions[(size_t) i]))
                            waves[(size_t) i].clear();

                for (auto& c : clients)
                {
                    if (! c->isAuthenticated())
                        continue;
                    c->send (tickFrame);
                    if (! stateFrame.empty())
                        c->send (stateFrame);
                    c->echoParams (values);
                    if (waveTick)
                        for (int i = 0; i < n; ++i)
                            if (versions[(size_t) i] != 0 && c->waveVersions[(size_t) i] != versions[(size_t) i])
                            {
                                c->waveVersions[(size_t) i] = versions[(size_t) i];
                                const auto& wv = waves[(size_t) i];
                                c->send (encodeFrame (msg::Wave, makeWave ((uint8_t) i, wv.data(), (int) wv.size())));
                            }
                }
            }
            ++tick;
            wait (33);
        }
    }

private:
    RemoteServer& owner;
};

//============================================================================== server
RemoteServer::RemoteServer (RemoteHost& h, DeviceStore& d, Config c)
    : host (h), devices (d), config (std::move (c))
{
    if (config.instanceId.isEmpty())
        config.instanceId = juce::Uuid().toDashedString();
}

RemoteServer::~RemoteServer()
{
    stop();
}

bool RemoteServer::start()
{
    if (running.load())
        return true;
    listener = std::make_unique<juce::StreamingSocket>();
    if (! listener->createListener (config.port))
    {
        listener.reset();
        return false;
    }
    boundPort.store (config.port != 0 ? config.port : listener->getBoundPort());
    {
        const juce::ScopedLock sl (tableLock);
        paramTableJson.clear();
    }
    running.store (true);
    acceptThread = std::make_unique<AcceptThread> (*this);
    pumpThread = std::make_unique<PumpThread> (*this);
    acceptThread->startThread();
    pumpThread->startThread();
    return true;
}

void RemoteServer::stop()
{
    if (! running.exchange (false))
        return;
    if (acceptThread != nullptr)
        acceptThread->signalThreadShouldExit();
    if (pumpThread != nullptr)
        pumpThread->signalThreadShouldExit();
    if (listener != nullptr)
        listener->close();
    acceptThread.reset();
    pumpThread.reset();
    std::vector<std::shared_ptr<Client>> old;
    {
        const juce::ScopedLock sl (clientLock);
        old.swap (clients);
    }
    for (auto& c : old)
        c->kill();
    old.clear();           // joins the client threads
    listener.reset();
    boundPort.store (0);
}

void RemoteServer::setPairingCode (const juce::String& code)
{
    const juce::ScopedLock sl (codeLock);
    pairingCode = code;
}

juce::String RemoteServer::getPairingCode() const
{
    const juce::ScopedLock sl (codeLock);
    return pairingCode;
}

void RemoteServer::disconnectDevice (const juce::String& deviceId)
{
    for (auto& c : getClients())
        if (c->getDeviceId() == deviceId)
            c->kill();
}

int RemoteServer::getNumClients (bool authenticatedOnly) const
{
    int n = 0;
    for (auto& c : getClients())
        if (! c->isDead() && (! authenticatedOnly || c->isAuthenticated()))
            ++n;
    return n;
}

void RemoteServer::addClient (std::unique_ptr<juce::StreamingSocket> socket)
{
    auto c = std::make_shared<Client> (*this, std::move (socket));
    c->startThread();           // running before it is listed (a listed client that is not running is dead)
    const juce::ScopedLock sl (clientLock);
    if (clients.size() >= 16)
    {
        c->kill();              // enough is enough
        return;                 // (joined when c goes, outside the list)
    }
    clients.push_back (c);
}

void RemoteServer::removeDeadClients()
{
    std::vector<std::shared_ptr<Client>> gone;
    {
        const juce::ScopedLock sl (clientLock);
        for (auto it = clients.begin(); it != clients.end();)
        {
            if ((*it)->isDead())
            {
                gone.push_back (*it);
                it = clients.erase (it);
            }
            else
                ++it;
        }
    }
    for (auto& c : gone)
        c->kill();
    // joined here, outside the lock
}

std::vector<std::shared_ptr<RemoteServer::Client>> RemoteServer::getClients() const
{
    const juce::ScopedLock sl (clientLock);
    return clients;
}

juce::String RemoteServer::buildParamTable()
{
    juce::Array<juce::var> list;
    juce::String ids;
    const int n = host.getNumParams();
    for (int i = 0; i < n; ++i)
    {
        const auto p = host.getParamInfo (i);
        auto* o = new juce::DynamicObject();
        o->setProperty ("i", i);
        o->setProperty ("id", p.id);
        o->setProperty ("name", p.name);
        o->setProperty ("min", p.minValue);
        o->setProperty ("max", p.maxValue);
        o->setProperty ("def", p.defaultValue);
        o->setProperty ("type", p.type);
        if (p.type == "choice")
        {
            juce::Array<juce::var> ch;
            for (auto& s : p.choices)
                ch.add (s);
            o->setProperty ("choices", ch);
        }
        list.add (juce::var (o));
        ids << p.id << "\n";
    }
    const auto h = sha256 (ids.toRawUTF8(), ids.getNumBytesAsUTF8());
    auto* root = new juce::DynamicObject();
    root->setProperty ("hash", toHex (h.data(), 8));
    root->setProperty ("params", list);
    return juce::JSON::toString (juce::var (root), true);
}

juce::String RemoteServer::getParamTable()
{
    const juce::ScopedLock sl (tableLock);
    if (paramTableJson.isEmpty())
        paramTableJson = buildParamTable();
    return paramTableJson;
}

juce::String RemoteServer::buildState (const Telemetry& t) const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("bpm", std::round (t.bpm * 100.0f) / 100.0f);
    root->setProperty ("playing", t.playing);
    juce::Array<juce::var> tracks;
    const int n = juce::jlimit (0, kMaxRemoteTracks, t.numTracks);
    auto round3 = [] (float v) { return std::round (v * 1000.0f) / 1000.0f; };
    for (int i = 0; i < n; ++i)
    {
        const auto& tr = t.tracks[(size_t) i];
        auto* o = new juce::DynamicObject();
        o->setProperty ("state", tr.state);
        o->setProperty ("bars", round3 (tr.bars));
        o->setProperty ("level", round3 (tr.level));
        o->setProperty ("muted", tr.muted);
        o->setProperty ("solo", tr.solo);
        o->setProperty ("pan", round3 (tr.pan));
        tracks.add (juce::var (o));
    }
    root->setProperty ("tracks", tracks);
    return juce::JSON::toString (juce::var (root), true);
}

void RemoteServer::sendInitialState (Client& c)
{
    c.send (encodeJsonFrame (msg::ParamTable, getParamTable()));
    Telemetry t;
    host.readTelemetry (t);
    c.send (encodeJsonFrame (msg::State, buildState (t)));
    const int n = juce::jlimit (0, kMaxRemoteTracks, t.numTracks);
    for (int i = 0; i < n; ++i)
    {
        std::vector<uint8_t> peaks;
        uint32_t version = 0;
        if (host.readWave (i, peaks, version))
        {
            c.waveVersions[(size_t) i] = version;
            c.send (encodeFrame (msg::Wave, makeWave ((uint8_t) i, peaks.data(), (int) peaks.size())));
        }
        else
            c.waveVersions[(size_t) i] = version;       // empty: nothing to send until it changes
    }
}
} // namespace livelooper::remote
