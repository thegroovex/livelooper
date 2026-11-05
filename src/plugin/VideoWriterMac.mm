// v1.6 clip export: H.264 + AAC (.mp4) with AVFoundation (macOS).

#include "NativeVideo.h"

#include <thread>
#include <chrono>

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#include <cstring>

namespace livelooper::clip::native
{
namespace
{
class AVFoundationWriter : public Encoder
{
public:
    ~AVFoundationWriter() override { release(); }

    const char* name() const override { return "AVFoundation"; }

    bool open (const std::string& path, int w, int h, int frameRate, double rate) override
    {
        width = w; height = h; fps = frameRate; sampleRate = rate;
        @autoreleasepool
        {
            NSError* err = nil;
            NSURL* url = [NSURL fileURLWithPath: [NSString stringWithUTF8String: path.c_str()]];
            writer = [[AVAssetWriter alloc] initWithURL: url fileType: AVFileTypeMPEG4 error: &err];
            if (writer == nil)
                return false;

            NSDictionary* videoSettings = @{
                AVVideoCodecKey: AVVideoCodecTypeH264,
                AVVideoWidthKey: @(w),
                AVVideoHeightKey: @(h),
                AVVideoCompressionPropertiesKey: @{ AVVideoAverageBitRateKey: @(8000000),
                                                    AVVideoProfileLevelKey: AVVideoProfileLevelH264HighAutoLevel }
            };
            videoIn = [[AVAssetWriterInput alloc] initWithMediaType: AVMediaTypeVideo outputSettings: videoSettings];
            videoIn.expectsMediaDataInRealTime = NO;
            NSDictionary* pbAttrs = @{
                (id) kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA),
                (id) kCVPixelBufferWidthKey: @(w),
                (id) kCVPixelBufferHeightKey: @(h)
            };
            adaptor = [[AVAssetWriterInputPixelBufferAdaptor alloc] initWithAssetWriterInput: videoIn sourcePixelBufferAttributes: pbAttrs];

            AudioChannelLayout layout {};
            layout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;
            NSDictionary* audioSettings = @{
                AVFormatIDKey: @(kAudioFormatMPEG4AAC),
                AVSampleRateKey: @(rate),
                AVNumberOfChannelsKey: @2,
                AVEncoderBitRateKey: @(192000),
                AVChannelLayoutKey: [NSData dataWithBytes: &layout length: sizeof (layout)]
            };
            audioIn = [[AVAssetWriterInput alloc] initWithMediaType: AVMediaTypeAudio outputSettings: audioSettings];
            audioIn.expectsMediaDataInRealTime = NO;

            if (! [writer canAddInput: videoIn] || ! [writer canAddInput: audioIn])
                return false;
            [writer addInput: videoIn];
            [writer addInput: audioIn];
            if (! [writer startWriting])
                return false;
            [writer startSessionAtSourceTime: kCMTimeZero];

            AudioStreamBasicDescription asbd {};
            asbd.mSampleRate = rate;
            asbd.mFormatID = kAudioFormatLinearPCM;
            asbd.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
            asbd.mBytesPerPacket = 8;
            asbd.mFramesPerPacket = 1;
            asbd.mBytesPerFrame = 8;
            asbd.mChannelsPerFrame = 2;
            asbd.mBitsPerChannel = 32;
            if (CMAudioFormatDescriptionCreate (kCFAllocatorDefault, &asbd, 0, nullptr, 0, nullptr, nullptr, &audioFormat) != noErr)
                return false;
        }
        return true;
    }

    bool waitReady (AVAssetWriterInput* input)
    {
        for (int i = 0; i < 20000 && ! input.readyForMoreMediaData; ++i)
        {
            if (writer.status == AVAssetWriterStatusFailed)
                return false;
            std::this_thread::sleep_for (std::chrono::milliseconds (1));
        }
        return input.readyForMoreMediaData;
    }

    bool addFrameBGRA (const std::uint8_t* pixels, int srcStride) override
    {
        @autoreleasepool
        {
            if (! waitReady (videoIn) || adaptor.pixelBufferPool == nil)
                return false;
            CVPixelBufferRef pb = nullptr;
            if (CVPixelBufferPoolCreatePixelBuffer (kCFAllocatorDefault, adaptor.pixelBufferPool, &pb) != kCVReturnSuccess || pb == nullptr)
                return false;
            CVPixelBufferLockBaseAddress (pb, 0);
            auto* dst = static_cast<uint8_t*> (CVPixelBufferGetBaseAddress (pb));
            const size_t dstStride = CVPixelBufferGetBytesPerRow (pb);
            for (int y = 0; y < height; ++y)
                std::memcpy (dst + (size_t) y * dstStride, pixels + (size_t) y * (size_t) srcStride, (size_t) width * 4);
            CVPixelBufferUnlockBaseAddress (pb, 0);
            const BOOL ok = [adaptor appendPixelBuffer: pb withPresentationTime: CMTimeMake (frameIndex++, fps)];
            CVPixelBufferRelease (pb);
            return ok == YES;
        }
    }

    bool addAudio (const float* interleaved, int numFrames) override
    {
        @autoreleasepool
        {
            if (! waitReady (audioIn))
                return false;
            const size_t bytes = (size_t) numFrames * 8;
            CMBlockBufferRef block = nullptr;
            if (CMBlockBufferCreateWithMemoryBlock (kCFAllocatorDefault, nullptr, bytes, kCFAllocatorDefault, nullptr, 0, bytes,
                                                    kCMBlockBufferAssureMemoryNowFlag, &block) != kCMBlockBufferNoErr)
                return false;
            CMBlockBufferReplaceDataBytes (interleaved, block, 0, bytes);
            CMSampleBufferRef sample = nullptr;
            const OSStatus st = CMAudioSampleBufferCreateReadyWithPacketDescriptions (kCFAllocatorDefault, block, audioFormat, numFrames,
                                                                                      CMTimeMake (audioFrames, (int32_t) sampleRate), nullptr, &sample);
            CFRelease (block);
            if (st != noErr || sample == nullptr)
                return false;
            const BOOL ok = [audioIn appendSampleBuffer: sample];
            CFRelease (sample);
            audioFrames += numFrames;
            return ok == YES;
        }
    }

    bool finish() override
    {
        if (writer == nil)
            return false;
        [videoIn markAsFinished];
        [audioIn markAsFinished];
        dispatch_semaphore_t done = dispatch_semaphore_create (0);
        [writer finishWritingWithCompletionHandler: ^{ dispatch_semaphore_signal (done); }];
        dispatch_semaphore_wait (done, dispatch_time (DISPATCH_TIME_NOW, (int64_t) 60 * NSEC_PER_SEC));
       #if ! __has_feature(objc_arc)
        dispatch_release (done);
       #endif
        const bool ok = writer.status == AVAssetWriterStatusCompleted;
        release();
        return ok;
    }

private:
    void release()
    {
        if (audioFormat != nullptr)
        {
            CFRelease (audioFormat);
            audioFormat = nullptr;
        }
       #if ! __has_feature(objc_arc)
        [adaptor release]; [videoIn release]; [audioIn release]; [writer release];
       #endif
        adaptor = nil; videoIn = nil; audioIn = nil; writer = nil;
    }

    AVAssetWriter* writer = nil;
    AVAssetWriterInput* videoIn = nil;
    AVAssetWriterInput* audioIn = nil;
    AVAssetWriterInputPixelBufferAdaptor* adaptor = nil;
    CMAudioFormatDescriptionRef audioFormat = nullptr;
    int width = 0, height = 0, fps = 30;
    double sampleRate = 48000.0;
    int64_t frameIndex = 0, audioFrames = 0;
};
} // namespace

std::unique_ptr<Encoder> create()
{
    return std::make_unique<AVFoundationWriter>();
}
} // namespace livelooper::clip::native
