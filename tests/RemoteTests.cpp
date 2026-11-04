// Remote control (protocol v1.0, docs/remote/PROTOCOL.md): frames, HMAC, pairing / authentication over
// loopback, and a stalled client next to the audio thread.

#include "plugin/PluginProcessor.h"
#include "remote/RemoteProtocol.h"
#include "remote/RemoteServer.h"

#include <map>

namespace livelooper
{
using namespace remote;

namespace
{
    juce::String hexOf (const std::vector<uint8_t>& v) { return toHex (v); }

    struct FakeHost : RemoteHost
    {
        std::array<std::atomic<float>, 3> values {};
        std::atomic<int> lastSetIndex { -1 }, triggers { 0 }, lastAction { 0 }, lastTrack { 0 }, gestures { 0 };
        std::atomic<float> lastSetValue { 0.0f };

        int getNumParams() override { return 3; }
        ParamInfo getParamInfo (int i) override
        {
            ParamInfo p;
            p.id = "p" + juce::String (i);
            p.name = "Param " + juce::String (i);
            if (i == 1) p.type = "bool";
            if (i == 2) { p.type = "choice"; p.choices = { "A", "B" }; }
            return p;
        }
        float getParamValue (int i) override { return values[(size_t) i].load(); }
        void setParam (int i, float v) override { lastSetValue = v; values[(size_t) i] = v; lastSetIndex = i; }
        void setGesture (int, bool) override { ++gestures; }
        void trigger (uint8_t a, uint8_t t, bool down) override
        {
            if (! down) return;
            lastAction = a;
            lastTrack = t;
            ++triggers;
        }
        void readTelemetry (Telemetry& t) override
        {
            t.bpm = 120.0f;
            t.playing = true;
            t.numTracks = 2;
            t.tracks[0].state = "PLAY";
            t.tracks[0].bars = 2.0f;
        }
        bool readWave (int track, std::vector<uint8_t>& peaks, uint32_t& version) override
        {
            version = 1;
            if (track == 0)
            {
                peaks.assign (512, 100);
                return true;
            }
            peaks.clear();
            return false;
        }
    };

    struct MemStore : DeviceStore
    {
        juce::CriticalSection lock;
        std::map<juce::String, std::vector<uint8_t>> keys;
        bool findKey (const juce::String& id, std::vector<uint8_t>& key) override
        {
            const juce::ScopedLock sl (lock);
            auto it = keys.find (id);
            if (it == keys.end()) return false;
            key = it->second;
            return true;
        }
        void addDevice (const juce::String& id, const juce::String&, const std::vector<uint8_t>& key) override
        {
            const juce::ScopedLock sl (lock);
            keys[id] = key;
        }
        void remove (const juce::String& id)
        {
            const juce::ScopedLock sl (lock);
            keys.erase (id);
        }
    };

    struct TestClient
    {
        juce::StreamingSocket socket;
        FrameDecoder decoder;
        bool closed = false;

        bool connect (int port) { return socket.connect ("127.0.0.1", port, 3000); }
        void send (const std::vector<uint8_t>& frame) { socket.write (frame.data(), (int) frame.size()); }
        void sendJson (uint8_t type, const juce::String& json) { send (encodeJsonFrame (type, json)); }

        bool receive (Frame& f, int timeoutMs = 3000)
        {
            const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
            for (;;)
            {
                if (decoder.next (f) == FrameDecoder::Result::GotFrame)
                    return true;
                if (closed || juce::Time::getMillisecondCounter() > end)
                    return false;
                const int r = socket.waitUntilReady (true, 20);
                if (r < 0) { closed = true; return false; }
                if (r == 0) continue;
                char buf[65536];
                const int n = socket.read (buf, (int) sizeof (buf), false);
                if (n <= 0) { closed = true; return false; }
                decoder.feed (buf, (size_t) n);
            }
        }

        /** Next frame of this type (others are skipped: ticks, echoes). */
        bool receiveType (uint8_t type, Frame& f, int timeoutMs = 3000)
        {
            const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
            while (juce::Time::getMillisecondCounter() <= end)
                if (receive (f, 100) && f.type == type)
                    return true;
            return false;
        }

        /** True when the server closed the connection within the time. */
        bool waitClosed (int timeoutMs = 3000)
        {
            Frame f;
            const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
            while (! closed && juce::Time::getMillisecondCounter() <= end)
                receive (f, 50);
            return closed;
        }

        juce::String hello (const juce::String& deviceId, int major = 1)
        {
            sendJson (msg::Hello, "{\"proto\":[" + juce::String (major) + ",0],\"app\":\"test\",\"deviceId\":\"" + deviceId
                                      + "\",\"deviceName\":\"Test device\",\"caps\":0}");
            Frame f;
            if (! receiveType (msg::Welcome, f))
                return {};
            return juce::JSON::parse (f.payloadAsString())["nonce"].toString();
        }

        juce::String errorCode()
        {
            Frame f;
            if (! receiveType (msg::Error, f))
                return {};
            return juce::JSON::parse (f.payloadAsString())["code"].toString();
        }
    };

    juce::String authJson (const juce::String& deviceId, const std::vector<uint8_t>& key, const juce::String& nonceHex)
    {
        std::vector<uint8_t> nonce;
        fromHex (nonceHex, nonce);
        const auto mac = hmacSha256 (key.data(), key.size(), nonce.data(), nonce.size());
        return "{\"deviceId\":\"" + deviceId + "\",\"hmac\":\"" + toHex (mac) + "\"}";
    }

    /** HELLO + PAIR + AUTH; returns the key (empty on failure). */
    std::vector<uint8_t> pairAndAuth (TestClient& c, const juce::String& deviceId, const juce::String& code)
    {
        const auto nonce = c.hello (deviceId);
        if (nonce.length() != 32)
            return {};
        c.sendJson (msg::Pair, "{\"deviceId\":\"" + deviceId + "\",\"deviceName\":\"Test device\",\"code\":\"" + code + "\"}");
        Frame f;
        if (! c.receiveType (msg::Paired, f))
            return {};
        std::vector<uint8_t> key;
        fromHex (juce::JSON::parse (f.payloadAsString())["key"].toString(), key);
        c.sendJson (msg::Auth, authJson (deviceId, key, nonce));
        if (! c.receiveType (msg::AuthOk, f))
            return {};
        return key;
    }
}

class RemoteTests : public juce::UnitTest
{
public:
    RemoteTests() : juce::UnitTest ("Remote control (protocol v1.0)", "livelooper") {}

    void runTest() override
    {
        beginTest ("Frames: the hex examples of the spec, decoding in pieces, bad lengths");
        {
            Writer ping;
            ping.u32 (1).u64 (0);
            const auto pingFrame = encodeFrame (msg::Ping, ping.data);
            expectEquals (hexOf (pingFrame), juce::String ("0d000000" "30" "01000000" "0000000000000000"));
            const auto setFrame = encodeFrame (msg::ParamSet, makeParamSet (3, 0.5f));
            expectEquals (hexOf (setFrame), juce::String ("07000000" "11" "0300" "0000003f"));
            Writer trig;
            trig.u8 (action::RecPlayDub).u8 (0).u8 (1);
            const auto trigFrame = encodeFrame (msg::Trigger, trig.data);
            expectEquals (hexOf (trigFrame), juce::String ("04000000" "13" "01" "00" "01"));

            std::vector<uint8_t> stream;
            for (auto* f : { &pingFrame, &setFrame, &trigFrame })
                stream.insert (stream.end(), f->begin(), f->end());
            FrameDecoder dec;
            std::vector<Frame> frames;
            for (auto b : stream)
            {
                dec.feed (&b, 1);
                Frame f;
                while (dec.next (f) == FrameDecoder::Result::GotFrame)
                    frames.push_back (f);
            }
            expectEquals ((int) frames.size(), 3);
            if (frames.size() == 3)
            {
                expectEquals ((int) frames[0].type, (int) msg::Ping);
                uint16_t i = 0;
                float v = 0.0f;
                expect (parseParamSet (frames[1].payload, i, v));
                expectEquals ((int) i, 3);
                expectEquals (v, 0.5f);
                uint8_t a = 0, t = 9;
                bool down = false;
                expect (parseTrigger (frames[2].payload, a, t, down));
                expect (a == action::RecPlayDub && t == 0 && down);
            }
            expectEquals ((int) dec.getBuffered(), 0);

            FrameDecoder big;
            const uint8_t huge[] = { 0x01, 0x00, 0x10, 0x00, 0x11 };     // 1 MiB + 1
            big.feed (huge, sizeof (huge));
            Frame f;
            expect (big.next (f) == FrameDecoder::Result::Error);
            FrameDecoder zero;
            const uint8_t empty[] = { 0, 0, 0, 0 };
            zero.feed (empty, sizeof (empty));
            expect (zero.next (f) == FrameDecoder::Result::Error);
            uint16_t i = 0;
            float v = 0.0f;
            expect (! parseParamSet ({ 1, 2, 3 }, i, v), "short PARAM_SET");
        }

        beginTest ("SHA-256 and HMAC-SHA256 (RFC 4231 test cases 1, 2, 6), hex");
        {
            const char* abc = "abc";
            expectEquals (toHex (sha256 (abc, 3)), juce::String ("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
            const std::vector<uint8_t> k1 (20, 0x0b);
            const juce::String m1 ("Hi There");
            expectEquals (toHex (hmacSha256 (k1.data(), k1.size(), m1.toRawUTF8(), 8)),
                          juce::String ("b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"));
            const juce::String k2 ("Jefe"), m2 ("what do ya want for nothing?");
            expectEquals (toHex (hmacSha256 (k2.toRawUTF8(), 4, m2.toRawUTF8(), (size_t) m2.length())),
                          juce::String ("5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"));
            const std::vector<uint8_t> k6 (131, 0xaa);
            const juce::String m6 ("Test Using Larger Than Block-Size Key - Hash Key First");
            expectEquals (toHex (hmacSha256 (k6.data(), k6.size(), m6.toRawUTF8(), (size_t) m6.length())),
                          juce::String ("60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54"));
            std::vector<uint8_t> back;
            expect (fromHex ("00ff10Ab", back) && back == std::vector<uint8_t> { 0x00, 0xff, 0x10, 0xab });
            expect (! fromHex ("abc", back) && ! fromHex ("zz", back));
            expectEquals ((int) randomBytes (16).size(), 16);
            const auto code = randomPairingCode();
            expect (code.length() == 6 && code.containsOnly ("0123456789"));
        }

        beginTest ("Only private, link-local and loopback addresses");
        {
            for (auto a : { "192.168.1.5", "10.0.0.1", "172.16.0.1", "172.31.255.255", "169.254.3.4", "127.0.0.1",
                            "::1", "fe80::1", "fd12::3", "::ffff:192.168.0.2" })
                expect (isLocalAddress (a), a);
            for (auto a : { "8.8.8.8", "172.32.0.1", "192.169.0.1", "2001:db8::1", "", "1.2.3", "abc.def.1.2" })
                expect (! isLocalAddress (a), a);
        }

        beginTest ("Loopback: HELLO / WELCOME, pairing (rate limited), HMAC auth, table, state, wave, set, trigger, ping, echo");
        {
            FakeHost host;
            MemStore store;
            RemoteServer::Config cfg;
            cfg.port = 0;
            cfg.discovery = false;
            cfg.version = "1.7.0";
            RemoteServer server (host, store, cfg);
            expect (server.start());
            const int port = server.getBoundPort();
            expect (port > 0);
            server.setPairingCode ("123456");

            TestClient a;
            expect (a.connect (port));
            a.send (encodeFrame (msg::ParamSet, makeParamSet (0, 1.0f)));
            expectEquals (a.errorCode(), juce::String ("unauthorized"));
            const auto nonce = a.hello ("dev-1");
            expectEquals (nonce.length(), 32);
            a.sendJson (msg::Pair, "{\"deviceId\":\"dev-1\",\"deviceName\":\"Test\",\"code\":\"000000\"}");
            expectEquals (a.errorCode(), juce::String ("bad_code"));
            a.sendJson (msg::Pair, "{\"deviceId\":\"dev-1\",\"deviceName\":\"Test\",\"code\":\"123456\"}");
            Frame f;
            expect (a.receiveType (msg::Paired, f));
            std::vector<uint8_t> key;
            expect (fromHex (juce::JSON::parse (f.payloadAsString())["key"].toString(), key) && key.size() == 32);
            std::vector<uint8_t> stored;
            expect (store.findKey ("dev-1", stored) && stored == key);

            a.sendJson (msg::Auth, "{\"deviceId\":\"dev-1\",\"hmac\":\"" + juce::String::repeatedString ("00", 32) + "\"}");
            expectEquals (a.errorCode(), juce::String ("not_paired"));
            a.sendJson (msg::Auth, authJson ("dev-1", key, nonce));
            expect (a.receive (f) && f.type == msg::AuthOk, "AUTH_OK");
            expect (a.receive (f) && f.type == msg::ParamTable, "PARAM_TABLE right after AUTH_OK");
            const auto table = juce::JSON::parse (f.payloadAsString());
            expectEquals (table["params"].size(), 3);
            expectEquals (table["params"][2]["type"].toString(), juce::String ("choice"));
            expectEquals (table["params"][2]["choices"].size(), 2);
            expectEquals (table["params"][1]["id"].toString(), juce::String ("p1"));
            expect (table["hash"].toString().isNotEmpty());
            expect (a.receive (f) && f.type == msg::State, "STATE");
            const auto state = juce::JSON::parse (f.payloadAsString());
            expectEquals (state["tracks"].size(), 2);
            expectEquals (state["tracks"][0]["state"].toString(), juce::String ("PLAY"));
            expect (a.receive (f) && f.type == msg::Wave, "WAVE of the non-empty track");
            expect (f.payload.size() == 3 + 512 && f.payload[0] == 0 && f.payload[1] == 0 && f.payload[2] == 2);

            a.send (encodeFrame (msg::ParamSet, makeParamSet (1, 1.0f)));
            Writer trig;
            trig.u8 (action::Clear).u8 (2).u8 (1);
            a.send (encodeFrame (msg::Trigger, trig.data));
            Writer ping;
            ping.u32 (7).u64 (123456789);
            a.send (encodeFrame (msg::Ping, ping.data));
            expect (a.receiveType (msg::Pong, f) && f.payload == ping.data, "PONG echoes PING");
            expectEquals (host.lastSetIndex.load(), 1);
            expectEquals (host.lastSetValue.load(), 1.0f);
            expectEquals (host.triggers.load(), 1);
            expect (host.lastAction == action::Clear && host.lastTrack == 2);
            expect (a.receiveType (msg::Tick, f, 1000), "TICK");
            {
                Reader r (f.payload);
                float bpm = 0.0f;
                uint8_t n = 0;
                expect (r.f32 (bpm) && r.u8 (n) && std::abs (bpm - 120.0f) < 1.0e-6f && n == 2);
            }

            host.values[2] = 0.5f;       // changed by "the host" (automation, MIDI, GUI)
            bool echoed = false;
            for (int k = 0; k < 100 && ! echoed; ++k)
                if (a.receiveType (msg::ParamSet, f, 200))
                {
                    uint16_t i = 0;
                    float v = 0.0f;
                    echoed = parseParamSet (f.payload, i, v) && i == 2 && std::abs (v - 0.5f) < 1.0e-6f;
                }
            expect (echoed, "PARAM_SET echo of a host change");
            expectEquals (server.getNumClients (true), 1);

            TestClient b;
            expect (b.connect (port));
            b.sendJson (msg::Hello, "{\"proto\":[2,0],\"deviceId\":\"x\"}");
            expectEquals (b.errorCode(), juce::String ("proto_major"));
            expect (b.waitClosed(), "closed after proto_major");

            TestClient c;
            expect (c.connect (port));
            const auto nc = c.hello ("unknown");
            c.sendJson (msg::Auth, authJson ("unknown", key, nc));
            expectEquals (c.errorCode(), juce::String ("not_paired"));

            TestClient d;
            expect (d.connect (port));
            d.hello ("dev-2");
            for (int k = 0; k < 5; ++k)
            {
                d.sendJson (msg::Pair, "{\"deviceId\":\"dev-2\",\"deviceName\":\"x\",\"code\":\"99999" + juce::String (k) + "\"}");
                expectEquals (d.errorCode(), juce::String ("bad_code"));
            }
            expect (d.waitClosed(), "closed after 5 wrong codes");

            // revoked: the connection goes, the next AUTH says not_paired
            store.remove ("dev-1");
            server.disconnectDevice ("dev-1");
            expect (a.waitClosed(), "revoked device disconnected");
            TestClient e;
            expect (e.connect (port));
            const auto ne = e.hello ("dev-1");
            e.sendJson (msg::Auth, authJson ("dev-1", key, ne));
            expectEquals (e.errorCode(), juce::String ("not_paired"));

            const auto t0 = juce::Time::getMillisecondCounter();
            server.stop();
            expect (juce::Time::getMillisecondCounter() - t0 < 5000, "stops promptly");
            expect (! server.isRunning());
        }

        beginTest ("A stalled client cannot block processBlock");
        {
            LiveLooperProcessor proc (false, false);
            proc.prepareToPlay (48000.0, 512);
            MemStore store;
            const int port = proc.startRemoteForTests (0, "654321", store);
            expect (port > 0);
            {
                TestClient stalled;
                expect (stalled.connect (port));
                expect (! pairAndAuth (stalled, "stalled", "654321").empty(), "pairing with the processor");
                // from here on the client reads nothing: the server keeps sending ticks, state and echoes
                juce::AudioBuffer<float> buffer (2, 512);
                juce::MidiBuffer midi;
                double worstMs = 0.0;
                const auto until = juce::Time::getMillisecondCounter() + 1500;
                int blocks = 0;
                while (juce::Time::getMillisecondCounter() < until)
                {
                    buffer.clear();
                    const auto t0 = juce::Time::getHighResolutionTicks();
                    proc.processBlock (buffer, midi);
                    worstMs = juce::jmax (worstMs, juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) * 1000.0);
                    ++blocks;
                    juce::Thread::sleep (2);
                }
                expect (blocks > 100);
                expect (worstMs < 200.0, "worst processBlock " + juce::String (worstMs, 2) + " ms");

                // the server still serves others
                TestClient other;
                expect (other.connect (port));
                expectEquals (other.hello ("other").length(), 32);
            }
            const auto t0 = juce::Time::getMillisecondCounter();
            proc.stopRemote();
            expect (juce::Time::getMillisecondCounter() - t0 < 5000, "remote stops promptly");
            expect (proc.getRemoteServer() == nullptr);
        }
    }
};

static RemoteTests remoteTests;
} // namespace livelooper
