#include "RemoteProtocol.h"

#include <juce_cryptography/juce_cryptography.h>

#include <cmath>
#include <cstring>
#include <mutex>
#include <random>

namespace livelooper::remote
{
juce::String Frame::payloadAsString() const
{
    return juce::String::fromUTF8 (reinterpret_cast<const char*> (payload.data()), (int) payload.size());
}

std::vector<uint8_t> encodeFrame (uint8_t type, const void* payload, size_t size)
{
    std::vector<uint8_t> out;
    out.reserve (5 + size);
    const auto len = (uint32_t) (size + 1);
    for (int i = 0; i < 4; ++i)
        out.push_back ((uint8_t) ((len >> (8 * i)) & 0xff));
    out.push_back (type);
    if (size > 0)
        out.insert (out.end(), static_cast<const uint8_t*> (payload), static_cast<const uint8_t*> (payload) + size);
    return out;
}

std::vector<uint8_t> encodeFrame (uint8_t type, const std::vector<uint8_t>& payload)
{
    return encodeFrame (type, payload.data(), payload.size());
}

std::vector<uint8_t> encodeJsonFrame (uint8_t type, const juce::String& json)
{
    const auto utf8 = json.toRawUTF8();
    return encodeFrame (type, utf8, std::strlen (utf8));
}

std::vector<uint8_t> encodeJsonFrame (uint8_t type, const juce::var& json)
{
    return encodeJsonFrame (type, juce::JSON::toString (json, true));
}

void FrameDecoder::feed (const void* data, size_t size)
{
    if (readPos > 0 && readPos == buffer.size())
    {
        buffer.clear();
        readPos = 0;
    }
    else if (readPos > 65536)
    {
        buffer.erase (buffer.begin(), buffer.begin() + (std::ptrdiff_t) readPos);
        readPos = 0;
    }
    buffer.insert (buffer.end(), static_cast<const uint8_t*> (data), static_cast<const uint8_t*> (data) + size);
}

FrameDecoder::Result FrameDecoder::next (Frame& out)
{
    if (getBuffered() < 4)
        return Result::NeedMore;
    const uint8_t* p = buffer.data() + readPos;
    const uint32_t len = (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24);
    if (len == 0 || len > kMaxFrameLength)
        return Result::Error;
    if (getBuffered() < 4 + (size_t) len)
        return Result::NeedMore;
    out.type = p[4];
    out.payload.assign (p + 5, p + 4 + len);
    readPos += 4 + (size_t) len;
    return Result::GotFrame;
}

//==============================================================================
Writer& Writer::u16 (uint16_t v)
{
    data.push_back ((uint8_t) (v & 0xff));
    data.push_back ((uint8_t) (v >> 8));
    return *this;
}

Writer& Writer::u32 (uint32_t v)
{
    for (int i = 0; i < 4; ++i)
        data.push_back ((uint8_t) ((v >> (8 * i)) & 0xff));
    return *this;
}

Writer& Writer::u64 (uint64_t v)
{
    for (int i = 0; i < 8; ++i)
        data.push_back ((uint8_t) ((v >> (8 * i)) & 0xff));
    return *this;
}

Writer& Writer::f32 (float v)
{
    uint32_t bits;
    std::memcpy (&bits, &v, 4);
    return u32 (bits);
}

bool Reader::u8 (uint8_t& v)
{
    if (remaining() < 1) return false;
    v = data[pos++];
    return true;
}

bool Reader::u16 (uint16_t& v)
{
    if (remaining() < 2) return false;
    v = (uint16_t) (data[pos] | (data[pos + 1] << 8));
    pos += 2;
    return true;
}

bool Reader::u32 (uint32_t& v)
{
    if (remaining() < 4) return false;
    v = 0;
    for (int i = 0; i < 4; ++i)
        v |= (uint32_t) data[pos + (size_t) i] << (8 * i);
    pos += 4;
    return true;
}

bool Reader::u64 (uint64_t& v)
{
    if (remaining() < 8) return false;
    v = 0;
    for (int i = 0; i < 8; ++i)
        v |= (uint64_t) data[pos + (size_t) i] << (8 * i);
    pos += 8;
    return true;
}

bool Reader::f32 (float& v)
{
    uint32_t bits;
    if (! u32 (bits)) return false;
    std::memcpy (&v, &bits, 4);
    return true;
}

std::vector<uint8_t> makeParamSet (uint16_t index, float normalized)
{
    Writer w;
    w.u16 (index).f32 (normalized);
    return w.data;
}

bool parseParamSet (const std::vector<uint8_t>& payload, uint16_t& index, float& normalized)
{
    Reader r (payload);
    return r.u16 (index) && r.f32 (normalized) && std::isfinite (normalized);
}

bool parseGesture (const std::vector<uint8_t>& payload, uint16_t& index, bool& begin)
{
    Reader r (payload);
    uint8_t b = 0;
    if (! (r.u16 (index) && r.u8 (b)))
        return false;
    begin = b != 0;
    return true;
}

bool parseTrigger (const std::vector<uint8_t>& payload, uint8_t& act, uint8_t& track, bool& down)
{
    Reader r (payload);
    uint8_t d = 0;
    if (! (r.u8 (act) && r.u8 (track) && r.u8 (d)))
        return false;
    down = d != 0;
    return true;
}

std::vector<uint8_t> makeWave (uint8_t track, const uint8_t* peaks, int count)
{
    count = juce::jlimit (0, kMaxWaveCount, count);
    Writer w;
    w.u8 (track).u16 ((uint16_t) count);
    w.data.insert (w.data.end(), peaks, peaks + count);
    return w.data;
}

//==============================================================================
juce::String toHex (const void* data, size_t size)
{
    static const char* digits = "0123456789abcdef";
    juce::String s;
    s.preallocateBytes (size * 2 + 1);
    auto* p = static_cast<const uint8_t*> (data);
    for (size_t i = 0; i < size; ++i)
    {
        s << digits[p[i] >> 4];
        s << digits[p[i] & 15];
    }
    return s;
}

juce::String toHex (const std::vector<uint8_t>& data) { return toHex (data.data(), data.size()); }

bool fromHex (const juce::String& hex, std::vector<uint8_t>& out)
{
    out.clear();
    const int n = hex.length();
    if (n % 2 != 0)
        return false;
    auto digit = [] (juce::juce_wchar c) -> int {
        if (c >= '0' && c <= '9') return (int) (c - '0');
        if (c >= 'a' && c <= 'f') return (int) (c - 'a' + 10);
        if (c >= 'A' && c <= 'F') return (int) (c - 'A' + 10);
        return -1;
    };
    out.reserve ((size_t) n / 2);
    for (int i = 0; i < n; i += 2)
    {
        const int hi = digit (hex[i]), lo = digit (hex[i + 1]);
        if (hi < 0 || lo < 0)
        {
            out.clear();
            return false;
        }
        out.push_back ((uint8_t) (hi * 16 + lo));
    }
    return true;
}

std::vector<uint8_t> sha256 (const void* data, size_t size)
{
    const juce::SHA256 h (data, size);
    const auto raw = h.getRawData();
    auto* p = static_cast<const uint8_t*> (raw.getData());
    return std::vector<uint8_t> (p, p + raw.getSize());
}

std::vector<uint8_t> hmacSha256 (const void* key, size_t keySize, const void* message, size_t messageSize)
{
    constexpr size_t block = 64;
    std::vector<uint8_t> k (block, 0);
    if (keySize > block)
    {
        const auto hk = sha256 (key, keySize);
        std::copy (hk.begin(), hk.end(), k.begin());
    }
    else if (keySize > 0)
    {
        std::memcpy (k.data(), key, keySize);
    }

    std::vector<uint8_t> inner (block + messageSize), outer (block + 32);
    for (size_t i = 0; i < block; ++i)
    {
        inner[i] = (uint8_t) (k[i] ^ 0x36);
        outer[i] = (uint8_t) (k[i] ^ 0x5c);
    }
    if (messageSize > 0)
        std::memcpy (inner.data() + block, message, messageSize);
    const auto ih = sha256 (inner.data(), inner.size());
    std::copy (ih.begin(), ih.end(), outer.begin() + (std::ptrdiff_t) block);
    return sha256 (outer.data(), outer.size());
}

bool constantTimeEquals (const std::vector<uint8_t>& a, const std::vector<uint8_t>& b)
{
    if (a.size() != b.size())
        return false;
    uint8_t diff = 0;
    for (size_t i = 0; i < a.size(); ++i)
        diff = (uint8_t) (diff | (a[i] ^ b[i]));
    return diff == 0;
}

std::vector<uint8_t> randomBytes (size_t count)
{
    static std::mutex m;
    static std::random_device rd;
    std::lock_guard<std::mutex> lock (m);
    std::vector<uint8_t> out (count);
    for (size_t i = 0; i < count; i += 4)
    {
        const auto v = (uint32_t) rd();
        for (size_t j = 0; j < 4 && i + j < count; ++j)
            out[i + j] = (uint8_t) ((v >> (8 * j)) & 0xff);
    }
    return out;
}

juce::String randomPairingCode()
{
    // rejection sampling: no bias towards low codes
    for (;;)
    {
        const auto b = randomBytes (4);
        const uint32_t v = (uint32_t) b[0] | ((uint32_t) b[1] << 8) | ((uint32_t) b[2] << 16) | ((uint32_t) b[3] << 24);
        if (v < 4294000000u)
            return juce::String (v % 1000000u).paddedLeft ('0', 6);
    }
}

bool isLocalAddress (const juce::String& addressIn)
{
    auto address = addressIn.trim().toLowerCase();
    if (address.startsWith ("::ffff:"))       // IPv4 mapped into IPv6
        address = address.substring (7);
    if (address.containsChar (':'))
        return address == "::1" || address.startsWith ("fe8") || address.startsWith ("fe9") || address.startsWith ("fea")
            || address.startsWith ("feb") || address.startsWith ("fc") || address.startsWith ("fd");

    juce::StringArray parts;
    parts.addTokens (address, ".", {});
    if (parts.size() != 4)
        return false;
    int b[4];
    for (int i = 0; i < 4; ++i)
    {
        if (! parts[i].containsOnly ("0123456789") || parts[i].isEmpty())
            return false;
        b[i] = parts[i].getIntValue();
        if (b[i] < 0 || b[i] > 255)
            return false;
    }
    return b[0] == 10 || b[0] == 127
        || (b[0] == 172 && b[1] >= 16 && b[1] <= 31)
        || (b[0] == 192 && b[1] == 168)
        || (b[0] == 169 && b[1] == 254);
}
} // namespace livelooper::remote
