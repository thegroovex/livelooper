#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>

namespace livelooper
{
/**
    v1.6 performance recorder: writes the complete livelooper output (the master mix, exactly
    what is heard on the main output) as a 24-bit stereo WAV file, in the plug-in and in the
    standalone app. The audio thread only pushes samples into a lock-free FIFO; a background
    thread writes the file (juce::AudioFormatWriter::ThreadedWriter).

    It also keeps the last kHistorySeconds of the master output in memory (always, not only
    while recording), the source of the 9:16 clip export ("the last 20 seconds").

    start() / stop() run on the message thread, push() on the audio thread.
*/
class PerformanceRecorder
{
public:
    static constexpr double kHistorySeconds = 30.0;

    PerformanceRecorder();
    ~PerformanceRecorder();

    /** Message thread: allocates the history buffer (keeps it when the rate did not change). */
    void prepare (double sampleRate);

    /** Starts a new recording into 'folder' (created if needed). Returns the file or {} on error. */
    juce::File start (const juce::File& folder);
    /** Stops and closes the file. Returns the finished file ({} when nothing was recording). */
    juce::File stop();
    bool isRecording() const noexcept            { return recording.load(); }
    /** Seconds recorded in the running (or last) recording. */
    double getElapsedSeconds() const noexcept;
    juce::File getCurrentFile() const            { return currentFile; }
    juce::File getLastFile() const               { return lastFile; }

    /** Audio thread: the master output of one block. */
    void push (const float* l, const float* r, int numSamples) noexcept;

    /** Message thread: copies the last 'seconds' of the master output (fewer when less is there). */
    juce::AudioBuffer<float> getHistory (double seconds) const;
    double getSampleRate() const noexcept        { return rate; }

    /** "livelooper-2026-10-14_21-30-05.wav" for a time. */
    static juce::String makeFileName (juce::Time t);
    /** Default folder: Music/livelooper/Recordings (or the folder chosen in SETUP). */
    static juce::File getDefaultFolder();
    static juce::File getFolder();
    static void setFolder (const juce::File& f);

private:
    double rate = 44100.0;
    juce::TimeSliceThread writerThread { "livelooper recorder" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threaded;
    juce::SpinLock writerLock;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> activeWriter { nullptr };
    std::atomic<bool> recording { false };
    std::atomic<long long> recordedSamples { 0 };
    std::atomic<int> droppedBlocks { 0 };
    juce::File currentFile, lastFile;

    // history ring (audio thread writes, message thread reads; reads may race harmlessly)
    juce::AudioBuffer<float> history;
    std::atomic<int> historyWrite { 0 };
    std::atomic<long long> historyFilled { 0 };

    JUCE_DECLARE_NON_COPYABLE (PerformanceRecorder)
};
} // namespace livelooper
