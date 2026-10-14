#pragma once

// v1.6 clip export: plain C++ interface to the operating system's video encoder (no JUCE
// headers, so the Objective-C++ / Windows files do not mix with the JUCE headers).

#include <cstdint>
#include <memory>
#include <string>

namespace livelooper::clip::native
{
class Encoder
{
public:
    virtual ~Encoder() = default;
    virtual bool open (const std::string& utf8Path, int width, int height, int fps, double sampleRate) = 0;
    /** One frame, top row first, 4 bytes per pixel in B, G, R, A order. */
    virtual bool addFrameBGRA (const std::uint8_t* pixels, int strideBytes) = 0;
    /** Interleaved stereo float samples. */
    virtual bool addAudio (const float* interleaved, int numFrames) = 0;
    virtual bool finish() = 0;
    virtual const char* name() const = 0;
};

/** The platform encoder, or null (Linux, or when the system has none). */
std::unique_ptr<Encoder> create();
} // namespace livelooper::clip::native
