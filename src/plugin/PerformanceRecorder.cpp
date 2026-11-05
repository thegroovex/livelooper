#include "PerformanceRecorder.h"

#include "AppSettings.h"

namespace livelooper
{
PerformanceRecorder::PerformanceRecorder() = default;

PerformanceRecorder::~PerformanceRecorder()
{
    stop();
    writerThread.stopThread (2000);
}

void PerformanceRecorder::prepare (double sampleRate)
{
    const int size = (int) std::ceil (sampleRate * kHistorySeconds);
    if (sampleRate == rate && history.getNumSamples() == size)
        return;
    if (recording.load())
        stop();                      // a WAV file has one sample rate
    rate = sampleRate;
    history.setSize (2, size);
    history.clear();
    historyWrite.store (0);
    historyFilled.store (0);
}

juce::String PerformanceRecorder::makeFileName (juce::Time t)
{
    return "livelooper-" + t.formatted ("%Y-%m-%d_%H-%M-%S") + ".wav";
}

juce::File PerformanceRecorder::getDefaultFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("livelooper").getChildFile ("Recordings");
}

juce::File PerformanceRecorder::getFolder()
{
    const auto path = appsettings::getValue ("recordingFolder");
    if (path.isNotEmpty() && juce::File::isAbsolutePath (path))
        return juce::File (path);
    return getDefaultFolder();
}

void PerformanceRecorder::setFolder (const juce::File& f)
{
    appsettings::setValue ("recordingFolder", f == juce::File() ? juce::String() : f.getFullPathName());
}

juce::File PerformanceRecorder::start (const juce::File& folder)
{
    stop();
    if (! folder.createDirectory())
        return {};

    auto file = folder.getChildFile (makeFileName (juce::Time::getCurrentTime()));
    if (file.exists())
        file = file.getNonexistentSibling();

    auto stream = std::make_unique<juce::FileOutputStream> (file);
    if (! stream->openedOk())
        return {};

    juce::WavAudioFormat wav;
    juce::StringPairArray meta;
    meta.set (juce::WavAudioFormat::bwavDescription, "livelooper performance");
    meta.set (juce::WavAudioFormat::bwavOriginator, "livelooper");
    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), rate, 2, 24, meta, 0));
    if (writer == nullptr)
        return {};
    stream.release();         // owned by the writer now

    if (! writerThread.isThreadRunning())
        writerThread.startThread();

    // about 4 s of FIFO: the background thread empties it every few milliseconds
    auto tw = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (writer.release(), writerThread, (int) (rate * 4.0));
    {
        const juce::SpinLock::ScopedLockType sl (writerLock);
        threaded = std::move (tw);
        activeWriter.store (threaded.get());
    }
    recordedSamples.store (0);
    droppedBlocks.store (0);
    currentFile = file;
    recording.store (true);
    return file;
}

juce::File PerformanceRecorder::stop()
{
    if (! recording.exchange (false))
        return {};
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> old;
    {
        const juce::SpinLock::ScopedLockType sl (writerLock);
        activeWriter.store (nullptr);
        old = std::move (threaded);
    }
    old.reset();              // flushes the FIFO and closes the file
    lastFile = currentFile;
    currentFile = juce::File();
    return lastFile;
}

double PerformanceRecorder::getElapsedSeconds() const noexcept
{
    return rate > 0.0 ? (double) recordedSamples.load() / rate : 0.0;
}

void PerformanceRecorder::push (const float* l, const float* r, int numSamples) noexcept
{
    if (numSamples <= 0 || l == nullptr)
        return;
    if (r == nullptr)
        r = l;

    // history ring
    const int size = history.getNumSamples();
    if (size > 0)
    {
        int w = historyWrite.load (std::memory_order_relaxed);
        auto* hl = history.getWritePointer (0);
        auto* hr = history.getWritePointer (1);
        for (int i = 0; i < numSamples; ++i)
        {
            hl[w] = l[i];
            hr[w] = r[i];
            if (++w >= size)
                w = 0;
        }
        historyWrite.store (w, std::memory_order_relaxed);
        historyFilled.store (std::min ((long long) size, historyFilled.load (std::memory_order_relaxed) + numSamples),
                             std::memory_order_relaxed);
    }

    if (! recording.load (std::memory_order_relaxed))
        return;
    const juce::SpinLock::ScopedTryLockType sl (writerLock);
    if (! sl.isLocked())
        return;
    if (auto* w = activeWriter.load (std::memory_order_relaxed))
    {
        const float* chans[2] = { l, r };
        if (w->write (chans, numSamples))
            recordedSamples.fetch_add (numSamples, std::memory_order_relaxed);
        else
            droppedBlocks.fetch_add (1, std::memory_order_relaxed);
    }
}

juce::AudioBuffer<float> PerformanceRecorder::getHistory (double seconds) const
{
    const int size = history.getNumSamples();
    const int wanted = (int) std::min ((long long) std::llround (seconds * rate), historyFilled.load());
    juce::AudioBuffer<float> out (2, std::max (0, wanted));
    if (wanted <= 0 || size <= 0)
        return out;
    const int w = historyWrite.load();
    int start = ((w - wanted) % size + size) % size;
    for (int ch = 0; ch < 2; ++ch)
    {
        const int first = std::min (wanted, size - start);
        out.copyFrom (ch, 0, history, ch, start, first);
        if (first < wanted)
            out.copyFrom (ch, first, history, ch, 0, wanted - first);
    }
    return out;
}
} // namespace livelooper
