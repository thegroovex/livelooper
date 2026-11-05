// v1.6 clip export: H.264 + AAC (.mp4) with Media Foundation (Windows).

#include "NativeVideo.h"

#ifdef _WIN32

#ifndef NOMINMAX
 #define NOMINMAX
#endif
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

#pragma comment (lib, "mfplat.lib")
#pragma comment (lib, "mfreadwrite.lib")
#pragma comment (lib, "mfuuid.lib")
#pragma comment (lib, "ole32.lib")

namespace livelooper::clip::native
{
namespace
{
template <typename T>
struct ComPtr
{
    T* p = nullptr;
    ~ComPtr() { reset(); }
    void reset() { if (p != nullptr) { p->Release(); p = nullptr; } }
    T** operator&() { reset(); return &p; }
    T* operator->() const { return p; }
    operator T*() const { return p; }
};

class MediaFoundationWriter : public Encoder
{
public:
    ~MediaFoundationWriter() override
    {
        writer.reset();
        if (mfStarted) MFShutdown();
        if (comStarted) CoUninitialize();
    }

    const char* name() const override { return "Media Foundation"; }

    bool open (const std::string& path, int w, int h, int frameRate, double rate) override
    {
        width = w; height = h; fps = frameRate; sampleRate = (UINT32) rate;
        const int wlen = MultiByteToWideChar (CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
        std::wstring wpath ((size_t) std::max (0, wlen), L'\0');
        if (wlen > 0)
            MultiByteToWideChar (CP_UTF8, 0, path.c_str(), -1, wpath.data(), wlen);
        DeleteFileW (wpath.c_str());
        const HRESULT co = CoInitializeEx (nullptr, COINIT_MULTITHREADED);
        comStarted = SUCCEEDED (co);
        if (FAILED (MFStartup (MF_VERSION, MFSTARTUP_FULL)))
            return false;
        mfStarted = true;

        ComPtr<IMFAttributes> attrs;
        if (FAILED (MFCreateAttributes (&attrs, 1)))
            return false;
        attrs->SetUINT32 (MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
        if (FAILED (MFCreateSinkWriterFromURL (wpath.c_str(), nullptr, attrs, &writer)))
            return false;

        // ---- video: RGB32 in, H.264 out
        ComPtr<IMFMediaType> vOut, vIn;
        if (FAILED (MFCreateMediaType (&vOut)) || FAILED (MFCreateMediaType (&vIn)))
            return false;
        vOut->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Video);
        vOut->SetGUID (MF_MT_SUBTYPE, MFVideoFormat_H264);
        vOut->SetUINT32 (MF_MT_AVG_BITRATE, 8000000);
        vOut->SetUINT32 (MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        MFSetAttributeSize (vOut, MF_MT_FRAME_SIZE, (UINT32) w, (UINT32) h);
        MFSetAttributeRatio (vOut, MF_MT_FRAME_RATE, (UINT32) fps, 1);
        MFSetAttributeRatio (vOut, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        if (FAILED (writer->AddStream (vOut, &videoStream)))
            return false;
        vIn->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Video);
        vIn->SetGUID (MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        vIn->SetUINT32 (MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        MFSetAttributeSize (vIn, MF_MT_FRAME_SIZE, (UINT32) w, (UINT32) h);
        MFSetAttributeRatio (vIn, MF_MT_FRAME_RATE, (UINT32) fps, 1);
        MFSetAttributeRatio (vIn, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        if (FAILED (writer->SetInputMediaType (videoStream, vIn, nullptr)))
            return false;

        // ---- audio: 16-bit PCM in, AAC out (192 kbit/s)
        ComPtr<IMFMediaType> aOut, aIn;
        if (FAILED (MFCreateMediaType (&aOut)) || FAILED (MFCreateMediaType (&aIn)))
            return false;
        aOut->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        aOut->SetGUID (MF_MT_SUBTYPE, MFAudioFormat_AAC);
        aOut->SetUINT32 (MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
        aOut->SetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate);
        aOut->SetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, 2);
        aOut->SetUINT32 (MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 24000);
        if (FAILED (writer->AddStream (aOut, &audioStream)))
            return false;
        aIn->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        aIn->SetGUID (MF_MT_SUBTYPE, MFAudioFormat_PCM);
        aIn->SetUINT32 (MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
        aIn->SetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate);
        aIn->SetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, 2);
        aIn->SetUINT32 (MF_MT_AUDIO_BLOCK_ALIGNMENT, 4);
        aIn->SetUINT32 (MF_MT_AUDIO_AVG_BYTES_PER_SECOND, sampleRate * 4);
        if (FAILED (writer->SetInputMediaType (audioStream, aIn, nullptr)))
            return false;

        return SUCCEEDED (writer->BeginWriting());
    }

    bool writeBuffer (DWORD stream, const void* data, DWORD bytes, LONGLONG time, LONGLONG duration)
    {
        ComPtr<IMFMediaBuffer> buffer;
        ComPtr<IMFSample> sample;
        if (FAILED (MFCreateMemoryBuffer (bytes, &buffer)))
            return false;
        BYTE* dst = nullptr;
        if (FAILED (buffer->Lock (&dst, nullptr, nullptr)))
            return false;
        memcpy (dst, data, bytes);
        buffer->Unlock();
        buffer->SetCurrentLength (bytes);
        if (FAILED (MFCreateSample (&sample)) || FAILED (sample->AddBuffer (buffer)))
            return false;
        sample->SetSampleTime (time);
        sample->SetSampleDuration (duration);
        return SUCCEEDED (writer->WriteSample (stream, sample));
    }

    bool addFrameBGRA (const std::uint8_t* src, int srcStride) override
    {
        // RGB32 (B, G, R, X bytes) is bottom-up: the last image row goes first
        pixels.resize ((size_t) width * (size_t) height);
        for (int y = 0; y < height; ++y)
            std::memcpy (pixels.data() + (size_t) (height - 1 - y) * (size_t) width, src + (size_t) y * (size_t) srcStride, (size_t) width * 4);
        const LONGLONG dur = 10000000LL / fps;
        return writeBuffer (videoStream, pixels.data(), (DWORD) (pixels.size() * 4), frameIndex++ * dur, dur);
    }

    bool addAudio (const float* interleaved, int numFrames) override
    {
        pcm.resize ((size_t) numFrames * 2);
        for (size_t i = 0; i < pcm.size(); ++i)
            pcm[i] = (int16_t) std::clamp ((int) std::lrint (interleaved[i] * 32767.0f), -32768, 32767);
        const LONGLONG t = audioFrames * 10000000LL / sampleRate;
        const LONGLONG d = (LONGLONG) numFrames * 10000000LL / sampleRate;
        audioFrames += numFrames;
        return writeBuffer (audioStream, pcm.data(), (DWORD) (pcm.size() * 2), t, d);
    }

    bool finish() override
    {
        if (writer == nullptr)
            return false;
        const bool ok = SUCCEEDED (writer->Finalize());
        writer.reset();
        return ok;
    }

private:
    ComPtr<IMFSinkWriter> writer;
    DWORD videoStream = 0, audioStream = 0;
    int width = 0, height = 0, fps = 30;
    UINT32 sampleRate = 48000;
    LONGLONG frameIndex = 0, audioFrames = 0;
    std::vector<uint32_t> pixels;
    std::vector<int16_t> pcm;
    bool comStarted = false, mfStarted = false;
};
} // namespace

std::unique_ptr<Encoder> create()
{
    return std::make_unique<MediaFoundationWriter>();
}
} // namespace livelooper::clip::native

#endif
