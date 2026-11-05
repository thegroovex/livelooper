#include "ClipExport.h"

#include "BinaryData.h"
#include "NativeVideo.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace livelooper::clip
{
namespace
{
    const juce::Colour kOrange { 0xffff7a18 }, kPink { 0xffff2d6f }, kViolet { 0xffb14bff };
    constexpr double kTargetRate = 48000.0;

    juce::ColourGradient brandGradient (float x1, float y, float x2)
    {
        juce::ColourGradient g (kOrange, x1, y, kViolet, x2, y, false);
        g.addColour (0.5, kPink);
        return g;
    }

    juce::String timeText (double s)
    {
        const int t = (int) std::floor (s + 1.0e-6);
        return juce::String (t / 60).paddedLeft ('0', 2) + ":" + juce::String (t % 60).paddedLeft ('0', 2);
    }
}

//============================================================================== audio
juce::AudioBuffer<float> prepareAudio (const juce::AudioBuffer<float>& audio, double sampleRate, double maxSeconds, bool fromStart)
{
    const int srcLen = audio.getNumSamples();
    const int wantSrc = std::min (srcLen, (int) std::llround (maxSeconds * sampleRate));
    const int start = fromStart ? 0 : srcLen - wantSrc;
    const double ratio = sampleRate / kTargetRate;
    const int dstLen = (int) std::floor ((double) wantSrc / ratio);
    juce::AudioBuffer<float> out (2, std::max (0, dstLen));
    out.clear();
    if (dstLen <= 0 || audio.getNumChannels() == 0)
        return out;
    for (int ch = 0; ch < 2; ++ch)
    {
        const float* src = audio.getReadPointer (std::min (ch, audio.getNumChannels() - 1)) + start;
        if (std::abs (ratio - 1.0) < 1.0e-9)
            out.copyFrom (ch, 0, src, dstLen);
        else
        {
            juce::LagrangeInterpolator interp;
            interp.process (ratio, src, out.getWritePointer (ch), dstLen, wantSrc, 0);
        }
    }
    return out;
}

juce::File defaultTarget (const juce::File& folder)
{
    auto f = folder.getChildFile ("livelooper-clip-" + juce::Time::getCurrentTime().formatted ("%Y-%m-%d_%H-%M-%S") + ".mp4");
    return f.exists() ? f.getNonexistentSibling() : f;
}

//============================================================================== renderer
Renderer::Renderer (const juce::AudioBuffer<float>& audio, double sampleRate, const Options& options) : opt (options)
{
    const int n = audio.getNumSamples();
    duration = sampleRate > 0.0 ? (double) n / sampleRate : 0.0;
    bold = juce::Typeface::createSystemTypefaceFor (BinaryData::Manrope800_ttf, (size_t) BinaryData::Manrope800_ttfSize);
    semi = juce::Typeface::createSystemTypefaceFor (BinaryData::Manrope600_ttf, (size_t) BinaryData::Manrope600_ttfSize);
    logo = juce::ImageCache::getFromMemory (BinaryData::beattacticslogotransparent_png, BinaryData::beattacticslogotransparent_pngSize);

    const int cols = std::max (16, (int) (opt.width * 0.84f / 7.0f));     // bars of 7 px (at 1080 wide)
    overview.assign ((size_t) cols, 0.0f);
    for (int c = 0; c < cols && n > 0; ++c)
    {
        const int a = (int) ((long long) c * n / cols), b = std::max (a + 1, (int) ((long long) (c + 1) * n / cols));
        float m = 0.0f;
        for (int ch = 0; ch < audio.getNumChannels(); ++ch)
            m = std::max (m, audio.getMagnitude (ch, a, std::min (b, n) - a));
        overview[(size_t) c] = m;
    }
    const float peak = std::max (1.0e-4f, *std::max_element (overview.begin(), overview.end()));
    for (auto& v : overview)
        v = std::sqrt (v / peak);       // a little compression: quiet parts stay visible

    const int hop = std::max (1, (int) (sampleRate / 60.0));
    for (int a = 0; a < n; a += hop)
    {
        float s = 0.0f;
        for (int ch = 0; ch < audio.getNumChannels(); ++ch)
            s = std::max (s, audio.getRMSLevel (ch, a, std::min (hop, n - a)));
        rms.push_back (s);
    }
    const float rmsPeak = rms.empty() ? 1.0f : std::max (1.0e-4f, *std::max_element (rms.begin(), rms.end()));
    for (auto& v : rms)
        v = std::min (1.0f, v / rmsPeak);

    drawBackground();
}

float Renderer::levelAt (double seconds) const
{
    if (rms.empty())
        return 0.0f;
    const int i = juce::jlimit (0, (int) rms.size() - 1, (int) (seconds * 60.0));
    return rms[(size_t) i];
}

void Renderer::drawBackground()
{
    const float W = (float) opt.width, H = (float) opt.height, u = W / 1080.0f;
    background = juce::Image (juce::Image::ARGB, opt.width, opt.height, true);
    juce::Graphics g (background);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0d0e12), 0, 0, juce::Colour (0xff1b1024), 0, H, false));
    g.fillAll();
    // soft brand glow behind the waveform
    juce::ColourGradient glow (kPink.withAlpha (0.22f), W * 0.5f, H * 0.47f, kViolet.withAlpha (0.0f), W * 0.5f + W * 0.62f, H * 0.47f, true);
    g.setGradientFill (glow);
    g.fillEllipse (W * 0.5f - W * 0.62f, H * 0.47f - W * 0.62f, W * 1.24f, W * 1.24f);

    // title
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (bold).withHeight (132.0f * u)));
    g.drawText ("livelooper", juce::Rectangle<float> (0, 150 * u, W, 160 * u), juce::Justification::centred);
    g.setGradientFill (brandGradient (W * 0.28f, 0, W * 0.72f));
    g.fillRoundedRectangle (W * 0.28f, 318 * u, W * 0.44f, 10 * u, 5 * u);
    if (opt.subtitle.isNotEmpty())
    {
        g.setColour (juce::Colours::white.withAlpha (0.72f));
        g.setFont (juce::Font (juce::FontOptions (semi).withHeight (44.0f * u)));
        g.drawText (opt.subtitle, juce::Rectangle<float> (40 * u, 352 * u, W - 80 * u, 60 * u), juce::Justification::centred);
    }

    // footer: brand
    if (logo.isValid())
    {
        const float lw = 380.0f * u, lh = lw * (float) logo.getHeight() / (float) std::max (1, logo.getWidth());
        g.drawImage (logo, juce::Rectangle<float> ((W - lw) * 0.5f, H - 300 * u - lh * 0.5f, lw, lh), juce::RectanglePlacement::centred);
    }
    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.setFont (juce::Font (juce::FontOptions (semi).withHeight (40.0f * u)));
    g.drawText ("designed by groovex", juce::Rectangle<float> (0, H - 190 * u, W, 60 * u), juce::Justification::centred);
}

juce::Image Renderer::renderFrame (double t) const
{
    const float W = (float) opt.width, H = (float) opt.height, u = W / 1080.0f;
    juce::Image img = background.createCopy();
    juce::Graphics g (img);
    const float progress = duration > 0.0 ? (float) juce::jlimit (0.0, 1.0, t / duration) : 0.0f;
    const float level = levelAt (t);

    // pulsing ring
    const float cx = W * 0.5f, cy = H * 0.47f;
    const float r = W * (0.30f + 0.06f * level);
    g.setColour (kPink.withAlpha (0.18f + 0.35f * level));
    g.drawEllipse (cx - r, cy - r, 2 * r, 2 * r, (4.0f + 10.0f * level) * u);

    // waveform overview (mirrored bars), played part in the brand gradient
    const int cols = (int) overview.size();
    const float left = W * 0.08f, width = W * 0.84f, colW = width / (float) cols;
    const float maxH = 330.0f * u;
    const auto played = brandGradient (left, 0, left + width);
    for (int c = 0; c < cols; ++c)
    {
        const float x = left + (float) c * colW;
        const float hgt = std::max (4.0f * u, overview[(size_t) c] * maxH);
        const bool done = ((float) c + 0.5f) / (float) cols <= progress;
        if (done)
            g.setGradientFill (played);
        else
            g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.fillRoundedRectangle (x + colW * 0.18f, cy - hgt * 0.5f, colW * 0.64f, hgt, colW * 0.3f);
    }
    // playhead
    g.setColour (juce::Colours::white);
    const float px = left + width * progress;
    g.fillRoundedRectangle (px - 3 * u, cy - maxH * 0.62f, 6 * u, maxH * 1.24f, 3 * u);

    // level meter: the last two seconds as moving bars (the "track view")
    const int bars = 32;
    const float mLeft = W * 0.12f, mWidth = W * 0.76f, bw = mWidth / (float) bars, mBase = H * 0.47f + 420 * u;
    for (int b = 0; b < bars; ++b)
    {
        const float v = levelAt (t - (bars - 1 - b) * (2.0 / bars));
        const float hgt = std::max (6.0f * u, v * 150.0f * u);
        g.setColour (juce::Colours::white.withAlpha (0.25f + 0.55f * (float) b / (float) bars));
        g.fillRoundedRectangle (mLeft + (float) b * bw + bw * 0.2f, mBase - hgt, bw * 0.6f, hgt, bw * 0.3f);
    }

    // time
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.setFont (juce::Font (juce::FontOptions (semi).withHeight (46.0f * u)));
    g.drawText (timeText (t) + "  /  " + timeText (duration), juce::Rectangle<float> (0, mBase + 30 * u, W, 64 * u), juce::Justification::centred);
    return img;
}

//============================================================================== ffmpeg (Linux, optional)
juce::File findFfmpeg()
{
   #if JUCE_WINDOWS
    const juce::String exe ("ffmpeg.exe");
    const char sep = ';';
   #else
    const juce::String exe ("ffmpeg");
    const char sep = ':';
   #endif
    const auto path = juce::SystemStats::getEnvironmentVariable ("PATH", {});
    juce::StringArray dirs;
    dirs.addTokens (path, juce::String::charToString (sep), {});
    dirs.addArray ({ "/usr/bin", "/usr/local/bin", "/opt/homebrew/bin" });
    for (const auto& d : dirs)
        if (d.isNotEmpty() && juce::File::isAbsolutePath (d))
            if (auto f = juce::File (d).getChildFile (exe); f.existsAsFile())
                return f;
    return {};
}

namespace
{
    bool writeWav (const juce::AudioBuffer<float>& audio, const juce::File& file)
    {
        file.deleteFile();
        auto stream = std::make_unique<juce::FileOutputStream> (file);
        if (! stream->openedOk())
            return false;
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (stream.get(), kTargetRate, 2, 24, {}, 0));
        if (w == nullptr)
            return false;
        stream.release();
        return w->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
    }

    bool writePng (const juce::Image& img, const juce::File& file)
    {
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat png;
        return out.openedOk() && png.writeImageToStream (img, out);
    }

    juce::StringArray ffmpegArgList (const juce::String& frames, const juce::String& wav, const juce::String& mp4, int fps)
    {
        return { "-y", "-loglevel", "error", "-framerate", juce::String (fps), "-i", frames, "-i", wav,
                 "-c:v", "libx264", "-pix_fmt", "yuv420p", "-preset", "veryfast", "-crf", "20",
                 "-c:a", "aac", "-b:a", "192k", "-shortest", "-movflags", "+faststart", mp4 };
    }

    juce::String ffmpegArgs (const juce::String& frames, const juce::String& wav, const juce::String& mp4, int fps)
    {
        juce::StringArray a;
        for (auto s : ffmpegArgList (frames, wav, mp4, fps))
            a.add (s.containsChar (' ') ? s.quoted() : s);
        return a.joinIntoString (" ");
    }
}

//============================================================================== export
Result exportClip (const juce::AudioBuffer<float>& audio, const juce::File& target, const Options& opt, std::function<bool (float)> progress)
{
    Result res;
    if (audio.getNumSamples() < (int) (0.5 * kTargetRate))
    {
        res.error = "Not enough audio for a clip yet.";
        return res;
    }
    target.getParentDirectory().createDirectory();
    Renderer renderer (audio, kTargetRate, opt);
    const int numFrames = std::max (1, (int) std::floor (renderer.getDuration() * opt.fps));
    const auto mp4 = target.withFileExtension ("mp4");
    res.cover = mp4.getSiblingFile (mp4.getFileNameWithoutExtension() + "-cover.png");
    writePng (renderer.renderFrame (renderer.getDuration() * 0.5), res.cover);

    auto report = [&progress] (float p) { return ! progress || progress (p); };

    // ---- native encoder (macOS / Windows)
    if (opt.allowNative)
        if (auto writer = VideoWriter::createNative())
        {
            if (writer->open (mp4, opt.width, opt.height, opt.fps, kTargetRate))
            {
                std::vector<float> inter;
                int audioDone = 0;
                bool ok = true;
                for (int f = 0; f < numFrames && ok; ++f)
                {
                    ok = writer->addFrame (renderer.renderFrame ((double) f / opt.fps));
                    const int audioEnd = f == numFrames - 1 ? audio.getNumSamples()
                                                            : std::min (audio.getNumSamples(), (int) std::llround ((f + 1) * kTargetRate / opt.fps));
                    const int n = audioEnd - audioDone;
                    if (ok && n > 0)
                    {
                        inter.resize ((size_t) n * 2);
                        for (int i = 0; i < n; ++i)
                        {
                            inter[(size_t) i * 2] = audio.getSample (0, audioDone + i);
                            inter[(size_t) i * 2 + 1] = audio.getSample (1, audioDone + i);
                        }
                        ok = writer->addAudio (inter.data(), n);
                        audioDone = audioEnd;
                    }
                    if (! report ((float) (f + 1) / (float) numFrames))
                        ok = false;
                }
                if (writer->finish() && ok && mp4.getSize() > 0)
                {
                    res.kind = Result::Kind::Video;
                    res.file = mp4;
                    res.encoder = writer->getName();
                    return res;
                }
                mp4.deleteFile();
            }
            res.error = "The video encoder of the system failed - writing PNG frames instead.";
        }

    // ---- frames + WAV (+ ffmpeg when installed)
    const auto folder = mp4.getSiblingFile (mp4.getFileNameWithoutExtension() + " frames");
    folder.deleteRecursively();
    folder.createDirectory();
    const auto wav = folder.getChildFile ("audio.wav");
    if (! writeWav (audio, wav))
    {
        res.error = "Could not write " + wav.getFullPathName();
        return res;
    }
    for (int f = 0; f < numFrames; ++f)
    {
        if (! writePng (renderer.renderFrame ((double) f / opt.fps), folder.getChildFile ("frame_" + juce::String (f + 1).paddedLeft ('0', 5) + ".png")))
        {
            res.error = "Could not write the frames to " + folder.getFullPathName();
            return res;
        }
        if (! report (0.9f * (float) (f + 1) / (float) numFrames))
        {
            res.error = "Cancelled";
            return res;
        }
    }
    writePng (renderer.renderFrame (renderer.getDuration() * 0.5), folder.getChildFile ("cover.png"));
    const auto pattern = folder.getChildFile ("frame_%05d.png").getFullPathName();
    folder.getChildFile ("make-video.txt").replaceWithText (
        "livelooper clip: " + juce::String (numFrames) + " frames at " + juce::String (opt.fps) + " fps + audio.wav\n"
        "To make an .mp4 with ffmpeg (https://ffmpeg.org):\n\n  ffmpeg "
        + ffmpegArgs ("frame_%05d.png", "audio.wav", "clip.mp4", opt.fps) + "\n\n"
        "livelooper-Clip: Einzelbilder + audio.wav. Mit ffmpeg wird daraus ein Video (Befehl oben, im Ordner ausfuehren).\n");

    if (opt.allowFfmpeg)
        if (auto ff = findFfmpeg(); ff.existsAsFile())
        {
            juce::ChildProcess p;
            auto cmd = ffmpegArgList (pattern, wav.getFullPathName(), mp4.getFullPathName(), opt.fps);
            cmd.insert (0, ff.getFullPathName());
            if (p.start (cmd, juce::ChildProcess::wantStdErr) && p.waitForProcessToFinish (600000)
                && p.getExitCode() == 0 && mp4.getSize() > 0)
            {
                folder.deleteRecursively();
                res.kind = Result::Kind::Video;
                res.file = mp4;
                res.encoder = "ffmpeg";
                report (1.0f);
                return res;
            }
        }

    res.kind = Result::Kind::Frames;
    res.file = folder;
    res.encoder = "PNG frames";
    report (1.0f);
    return res;
}

//============================================================================== native encoders
namespace
{
    class NativeWriter : public VideoWriter
    {
    public:
        explicit NativeWriter (std::unique_ptr<native::Encoder> e) : enc (std::move (e)) {}
        bool open (const juce::File& file, int w, int h, int fps, double rate) override
        {
            width = w; height = h;
            file.deleteFile();
            return enc->open (file.getFullPathName().toStdString(), w, h, fps, rate);
        }
        bool addFrame (const juce::Image& frame) override
        {
            const auto img = frame.convertedToFormat (juce::Image::ARGB);
            const juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
            if (bd.width != width || bd.height != height)
                return false;
            // JUCE keeps ARGB pixels as B, G, R, A bytes in memory (premultiplied; the frames are opaque)
            buffer.resize ((size_t) width * (size_t) height * 4);
            for (int y = 0; y < height; ++y)
            {
                const auto* src = bd.getLinePointer (y);
                auto* dst = buffer.data() + (size_t) y * (size_t) width * 4;
                for (int x = 0; x < width; ++x)
                {
                    const auto c = reinterpret_cast<const juce::PixelARGB*> (src + x * bd.pixelStride);
                    dst[x * 4 + 0] = c->getBlue();
                    dst[x * 4 + 1] = c->getGreen();
                    dst[x * 4 + 2] = c->getRed();
                    dst[x * 4 + 3] = 255;
                }
            }
            return enc->addFrameBGRA (buffer.data(), width * 4);
        }
        bool addAudio (const float* interleaved, int numFrames) override { return enc->addAudio (interleaved, numFrames); }
        bool finish() override { return enc->finish(); }
        juce::String getName() const override { return enc->name(); }

    private:
        std::unique_ptr<native::Encoder> enc;
        std::vector<std::uint8_t> buffer;
        int width = 0, height = 0;
    };
}

std::unique_ptr<VideoWriter> VideoWriter::createNative()
{
    if (auto e = native::create())
        return std::make_unique<NativeWriter> (std::move (e));
    return nullptr;
}

#if ! JUCE_MAC && ! JUCE_WINDOWS
namespace native { std::unique_ptr<Encoder> create() { return nullptr; } }
#endif
} // namespace livelooper::clip
