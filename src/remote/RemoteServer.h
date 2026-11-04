#pragma once

#include "RemoteProtocol.h"

#include <array>
#include <atomic>
#include <memory>

namespace livelooper::remote
{
inline constexpr int kMaxRemoteTracks = 8;

struct ParamInfo
{
    juce::String id, name, type { "float" };      // type: float | bool | choice
    float minValue = 0.0f, maxValue = 1.0f, defaultValue = 0.0f;
    juce::StringArray choices;
};

struct TrackTelemetry
{
    juce::String state { "EMPTY" };               // EMPTY | REC | PLAY | DUB | STOP | ARMED
    float bars = 0.0f, phase = 0.0f, meter = 0.0f;
    float level = 0.0f, pan = 0.5f;               // normalized like their parameters
    bool muted = false, solo = false;
};

struct Telemetry
{
    float bpm = 120.0f;
    bool playing = false;
    int numTracks = 0;
    std::array<TrackTelemetry, kMaxRemoteTracks> tracks;
    float masterMeter = 0.0f;
};

/**
    What the server needs from livelooper. Every function is called on a server thread (never the
    audio thread). Reads must be lock-free or use locks the audio thread never takes; actions are
    handed on to the message thread by the implementation.
*/
struct RemoteHost
{
    virtual ~RemoteHost() = default;
    virtual int getNumParams() = 0;
    virtual ParamInfo getParamInfo (int index) = 0;
    virtual float getParamValue (int index) = 0;             // normalized 0..1
    virtual void setParam (int index, float normalized) = 0;
    virtual void setGesture (int index, bool begin) = 0;
    virtual void trigger (uint8_t act, uint8_t track, bool down) = 0;
    virtual void readTelemetry (Telemetry& t) = 0;
    /** Waveform overview of a track (0..255, count <= 512). version changes whenever the content
        changes; return false for an empty track. */
    virtual bool readWave (int track, std::vector<uint8_t>& peaks, uint32_t& version) = 0;
};

/** Paired devices (persisted by the owner). Called on server threads; must be thread-safe. */
struct DeviceStore
{
    virtual ~DeviceStore() = default;
    virtual bool findKey (const juce::String& deviceId, std::vector<uint8_t>& key) = 0;
    virtual void addDevice (const juce::String& deviceId, const juce::String& deviceName, const std::vector<uint8_t>& key) = 0;
};

/**
    livelooper remote server (protocol v1.0): TCP control port, UDP discovery, pairing, HMAC
    authentication, parameter table / sets / echoes, triggers, state, ticks and waveforms.

    Threads: one accept thread, one thread per client (reads + non-blocking writes, a stalled
    client only fills its own bounded queue and is dropped), one pump thread (telemetry 30 Hz,
    parameter echoes, discovery once a second). The audio thread is never involved.
*/
class RemoteServer
{
public:
    struct Config
    {
        int port = kDefaultPort;                  // 0 = any free port (tests)
        bool discovery = true;
        juce::String instanceName { "livelooper" };
        juce::String instanceId;
        juce::String version;
    };

    RemoteServer (RemoteHost& host, DeviceStore& devices, Config config);
    ~RemoteServer();

    /** Binds the port and starts the threads. False if the port is in use. */
    bool start();
    void stop();
    bool isRunning() const noexcept { return running.load(); }
    int  getBoundPort() const noexcept { return boundPort.load(); }

    void setPairingCode (const juce::String& code);
    juce::String getPairingCode() const;
    /** Drops the connections of a device (after it was revoked). */
    void disconnectDevice (const juce::String& deviceId);
    int  getNumClients (bool authenticatedOnly) const;

    const Config& getConfig() const noexcept { return config; }

private:
    class Client;
    class AcceptThread;
    class PumpThread;
    friend class Client;
    friend class AcceptThread;
    friend class PumpThread;

    RemoteHost& host;
    DeviceStore& devices;
    Config config;
    std::atomic<bool> running { false };
    std::atomic<int> boundPort { 0 };

    mutable juce::CriticalSection codeLock;
    juce::String pairingCode;

    mutable juce::CriticalSection clientLock;
    std::vector<std::shared_ptr<Client>> clients;

    juce::CriticalSection tableLock;
    juce::String paramTableJson;                 // built once per start (the table is fixed)

    std::unique_ptr<juce::StreamingSocket> listener;
    std::unique_ptr<AcceptThread> acceptThread;
    std::unique_ptr<PumpThread> pumpThread;

    void addClient (std::unique_ptr<juce::StreamingSocket> socket);
    void removeDeadClients();
    std::vector<std::shared_ptr<Client>> getClients() const;
    juce::String buildParamTable();
    juce::String getParamTable();
    juce::String buildState (const Telemetry& t) const;
    void sendInitialState (Client& c);

    JUCE_DECLARE_NON_COPYABLE (RemoteServer)
};
} // namespace livelooper::remote
