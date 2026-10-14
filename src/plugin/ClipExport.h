#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

/**
    v1.6 clip export 9:16: a vertical video (1080 x 1920, 30 fps) of the last 20 seconds of the
    livelooper output (or of a performance recording) with an animated waveform / level view and
    the livelooper branding - ready for Reels, TikTok and Shorts.

    Video encoding uses what the operating system brings, nothing is bundled:
      - macOS:   AVFoundation (H.264 + AAC in .mp4)
      - Windows: Media Foundation (H.264 + AAC in .mp4)
      - Linux:   an ffmpeg program installed on the system (called as a separate program), if any.
    Without an encoder the export writes a folder with the audio (WAV), the rendered PNG frame
    sequence and a cover image instead, plus a text file with the ffmpeg command that turns them
    into a video.
*/
namespace livelooper::clip
{
struct Options
{
    int width = 1080, height = 1920, fps = 30;
    double maxSeconds = 20.0;
    juce::String subtitle;                 // e.g. "live loop performance - 100 BPM"
    bool allowNative = true;               // tests can force the fallbacks
    bool allowFfmpeg = true;
};

struct Result
{
    enum class Kind { None, Video, Frames };
    Kind kind = Kind::None;
    juce::File file;                       // the .mp4, or the folder with the frames
    juce::File cover;                      // PNG cover image (always written next to the result)
    juce::String encoder;                  // "AVFoundation", "Media Foundation", "ffmpeg", "PNG frames"
    juce::String error;
    bool ok() const noexcept { return kind != Kind::None; }
};

/** Draws the frames: static background once, then waveform, progress, levels and time per frame. */
class Renderer
{
public:
    Renderer (const juce::AudioBuffer<float>& audio, double sampleRate, const Options& options);
    juce::Image renderFrame (double seconds) const;
    double getDuration() const noexcept { return duration; }

private:
    void drawBackground();
    float levelAt (double seconds) const;
    Options opt;
    double duration = 0.0;
    std::vector<float> overview;           // waveform peaks, one per column
    std::vector<float> rms;                // RMS per 1/60 s
    juce::Image background;
    juce::Typeface::Ptr bold, semi;
    juce::Image logo;
};

/** Platform video encoder (null when there is none). */
class VideoWriter
{
public:
    virtual ~VideoWriter() = default;
    virtual bool open (const juce::File& file, int width, int height, int fps, double sampleRate) = 0;
    /** ARGB image of the configured size. */
    virtual bool addFrame (const juce::Image& frame) = 0;
    /** Interleaved stereo float samples. */
    virtual bool addAudio (const float* interleaved, int numFrames) = 0;
    virtual bool finish() = 0;
    virtual juce::String getName() const = 0;

    static std::unique_ptr<VideoWriter> createNative();
};

/** The last 'maxSeconds' of the audio, resampled to 48 kHz stereo (what the encoders want). */
juce::AudioBuffer<float> prepareAudio (const juce::AudioBuffer<float>& audio, double sampleRate, double maxSeconds, bool fromStart);

/** Exports synchronously (call it from a background thread). target = the .mp4 file to write;
    the fallback writes "<name> frames" next to it. progress (0..1) returns false to cancel. */
Result exportClip (const juce::AudioBuffer<float>& audio48k, const juce::File& target, const Options& options,
                   std::function<bool (float)> progress = {});

/** Finds an installed ffmpeg program (PATH, /usr/bin, /usr/local/bin, /opt/homebrew/bin), or {}. */
juce::File findFfmpeg();

/** Default file name: "livelooper-clip-2026-10-14_21-30-05.mp4" in the recordings folder. */
juce::File defaultTarget (const juce::File& folder);
} // namespace livelooper::clip
