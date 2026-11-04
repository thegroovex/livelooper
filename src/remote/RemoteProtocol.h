#pragma once

#include <juce_core/juce_core.h>

#include <cstdint>
#include <vector>

/**
    livelooper remote protocol v1.0 (docs/remote/PROTOCOL.md): frame encoding / decoding, the binary
    payloads, hex, HMAC-SHA256 and the address filter. Pure functions, no sockets, no threads -
    shared by the RemoteServer and the unit tests.
*/
namespace livelooper::remote
{
inline constexpr int kProtoMajor = 1;
inline constexpr int kProtoMinor = 0;
inline constexpr int kDefaultPort = 47800;
inline constexpr int kDiscoveryPort = 47801;
inline constexpr uint32_t kMaxFrameLength = 1u << 20;     // bytes of type + payload
inline constexpr int kMaxWaveCount = 512;

namespace msg
{
    enum : uint8_t
    {
        Hello = 0x01, Welcome = 0x02, Auth = 0x03, Pair = 0x04, Paired = 0x05, AuthOk = 0x06, Error = 0x07,
        ParamTable = 0x10, ParamSet = 0x11, Gesture = 0x12, Trigger = 0x13,
        State = 0x20, Tick = 0x21, Wave = 0x22,
        Ping = 0x30, Pong = 0x31
    };
}

/** TRIGGER actions (u8). */
namespace action
{
    enum : uint8_t
    {
        RecPlayDub = 1, Stop = 2, Clear = 3, Undo = 4, MuteToggle = 5, SoloToggle = 6,
        AllStartStop = 7, TapTempo = 8, Capture = 9
    };
}
inline constexpr uint8_t kGlobalTrack = 255;

struct Frame
{
    uint8_t type = 0;
    std::vector<uint8_t> payload;

    juce::String payloadAsString() const;
};

/** u32 length (LE, type + payload) . u8 type . payload */
std::vector<uint8_t> encodeFrame (uint8_t type, const void* payload, size_t size);
std::vector<uint8_t> encodeFrame (uint8_t type, const std::vector<uint8_t>& payload);
std::vector<uint8_t> encodeJsonFrame (uint8_t type, const juce::var& json);
std::vector<uint8_t> encodeJsonFrame (uint8_t type, const juce::String& json);

/** Splits a byte stream into frames. */
class FrameDecoder
{
public:
    enum class Result { NeedMore, GotFrame, Error };

    void feed (const void* data, size_t size);
    /** Takes the next complete frame. Error = length 0 or above 1 MiB (the stream is unusable). */
    Result next (Frame& out);
    size_t getBuffered() const noexcept { return buffer.size() - readPos; }

private:
    std::vector<uint8_t> buffer;
    size_t readPos = 0;
};

//============================================================================== little-endian payloads
class Writer
{
public:
    Writer& u8 (uint8_t v)   { data.push_back (v); return *this; }
    Writer& u16 (uint16_t v);
    Writer& u32 (uint32_t v);
    Writer& u64 (uint64_t v);
    Writer& f32 (float v);
    std::vector<uint8_t> data;
};

class Reader
{
public:
    Reader (const std::vector<uint8_t>& d) : data (d) {}
    bool u8 (uint8_t& v);
    bool u16 (uint16_t& v);
    bool u32 (uint32_t& v);
    bool u64 (uint64_t& v);
    bool f32 (float& v);
    size_t remaining() const noexcept { return data.size() - pos; }

private:
    const std::vector<uint8_t>& data;
    size_t pos = 0;
};

std::vector<uint8_t> makeParamSet (uint16_t index, float normalized);
bool parseParamSet (const std::vector<uint8_t>& payload, uint16_t& index, float& normalized);
bool parseGesture (const std::vector<uint8_t>& payload, uint16_t& index, bool& begin);
bool parseTrigger (const std::vector<uint8_t>& payload, uint8_t& act, uint8_t& track, bool& down);
std::vector<uint8_t> makeWave (uint8_t track, const uint8_t* peaks, int count);

//============================================================================== crypto + helpers
juce::String toHex (const void* data, size_t size);
juce::String toHex (const std::vector<uint8_t>& data);
/** False if the text is not an even number of hex digits. */
bool fromHex (const juce::String& hex, std::vector<uint8_t>& out);

std::vector<uint8_t> sha256 (const void* data, size_t size);
std::vector<uint8_t> hmacSha256 (const void* key, size_t keySize, const void* message, size_t messageSize);
/** Compares without an early exit (no timing hint about the matching prefix). */
bool constantTimeEquals (const std::vector<uint8_t>& a, const std::vector<uint8_t>& b);

/** Cryptographically strong random bytes (std::random_device). */
std::vector<uint8_t> randomBytes (size_t count);
/** Six random digits ("042917"). */
juce::String randomPairingCode();

/** IPv4 dotted / IPv6 text: true for private, link-local and loopback addresses (the only ones the
    server talks to). */
bool isLocalAddress (const juce::String& address);
} // namespace livelooper::remote
