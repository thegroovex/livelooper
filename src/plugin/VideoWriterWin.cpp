// v1.6 clip export: H.264 + AAC (.mp4) with Media Foundation (Windows).

#include "ClipExport.h"

#if JUCE_WINDOWS

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

#pragma comment (lib, "mfplat.lib")
#pragma comment (lib, "mfreadwrite.lib")
#pragma comment (lib, "mfuuid.lib")
#pragma comment (lib, "ole32.lib")

namespace livelooper::clip
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

class MediaFoundationWriter : public VideoWriter
{
public:
    ~MediaFoundationWriter() override
    {
        writer.reset();
        if (mfStarted) MFShutdown();
        if (comStarted) CoUninitialize();
    }

    juce::String getName() const override { return "Media Foundation"; }

    bool open (const juce::File& file, int w, int h, int frameRate, double rate) override
    {
        width = w; height = h; fps = frameRate; sampleRate = (UINT32) rate;
        file.deleteFile();
        const HRESULT co = CoInitializeEx (nullptr, COINIT_MULTITHREADED);
        comStarted = SUCCEEDED (co);
        if (FAILED (MFStartup (MF_VERSION, MFSTARTUP_FULL)))
            return false;
        mfStarted = true;

        ComPtr<IMFAttributes> attrs;
        if (FAILED (MFCreateAttributes (&attrs, 1)))
            return false;
        attrs->SetUINT32 (MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
        if (FAILED (MFCreateSinkWriterFromURL (file.getFullPathName().toWideCharPointer(), nullptr, attrs, &writer)))
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

    bool addFrame (const juce::Image& frame) override
    {
        // RGB32 is bottom-up: the last image row goes first
        pixels.resize ((size_t) width * (size_t) height);
        const juce::Image::BitmapData src (frame, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < height; ++y)
        {
            auto* row = pixels.data() + (size_t) (height - 1 - y) * (size_t) width;
            for (int x = 0; x < width; ++x)
            {
                const auto c = src.getPixelColour (x, y);
                row[x] = (uint32_t) 0xff000000u | ((uint32_t) c.getRed() << 16) | ((uint32_t) c.getGreen() << 8) | (uint32_t) c.getBlue();
            }
        }
        const LONGLONG dur = 10000000LL / fps;
        return writeBuffer (videoStream, pixels.data(), (DWORD) (pixels.size() * 4), frameIndex++ * dur, dur);
    }

    bool addAudio (const float* interleaved, int numFrames) override
    {
        pcm.resize ((size_t) numFrames * 2);
        for (size_t i = 0; i < pcm.size(); ++i)
            pcm[i] = (int16_t) juce::jlimit (-32768, 32767, (int) std::lrint (interleaved[i] * 32767.0f));
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

std::unique_ptr<VideoWriter> VideoWriter::createNative()
{
    return std::make_unique<MediaFoundationWriter>();
}
} // namespace livelooper::clip

#endif
