#pragma once

#include <atomic>
#include <vector>

namespace livelooper
{
/**
    v1.7 round-trip latency measurement ("loopback ping").

    The audio thread plays a short click burst on the output (everything else is muted while it
    measures) and records the input after it. The analysis (cross-correlation with the burst) runs
    on the message thread in update(). Several pings are sent; the result is the median of the
    detected runs. Without a detected signal (no cable / microphone, input muted) or with runs
    that disagree, the result says so instead of returning a number.

    Plain data, no JUCE dependency, so it is tested on synthetic signals.
*/
class LatencyMeasure
{
public:
    enum class Status : int { Idle = 0, Running, Done, NoSignal, Unstable };

    static constexpr int kDefaultRuns = 7;
    static constexpr double kMaxLatencySeconds = 0.5;   // longest round trip we look for
    static constexpr double kGapSeconds = 0.12;         // silence between pings (noise floor)

    /** Message thread: starts a new measurement (also while one runs). */
    void start (double sampleRate, int runs = kDefaultRuns);
    void cancel() noexcept;

    /** Audio thread. While running, replaces out (adds nothing else) with the ping and records in. */
    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept;

    /** Message thread: analyses finished runs; returns true when the status changed. */
    bool update();

    Status getStatus() const noexcept         { return (Status) status.load(); }
    bool isRunning() const noexcept           { return getStatus() == Status::Running; }
    /** Median round trip in samples (Done / Unstable), -1 otherwise. */
    int getResultSamples() const noexcept     { return resultSamples; }
    double getResultMs() const noexcept       { return resultSamples < 0 ? -1.0 : resultSamples * 1000.0 / sampleRate; }
    int getDetectedRuns() const noexcept      { return detectedRuns; }
    int getTotalRuns() const noexcept         { return totalRuns; }
    /** Largest difference between two detected runs (samples). */
    int getSpreadSamples() const noexcept     { return spreadSamples; }
    double getSampleRate() const noexcept     { return sampleRate; }

    /** The burst the output plays (tests). */
    const std::vector<float>& getBurst() const noexcept { return burst; }

    /** Finds the burst in a recording: returns the offset of its start, or -1 when it is not there.
        Public for tests (synthetic signals). */
    static int detect (const std::vector<float>& recording, const std::vector<float>& burst);
    /** Median of the values (empty: -1). */
    static int median (std::vector<int> values);

private:
    void makeBurst();

    double sampleRate = 48000.0;
    int totalRuns = kDefaultRuns;
    std::vector<float> burst;
    std::vector<float> record;          // input of the current run (mono)
    std::atomic<int> status { (int) Status::Idle };
    std::atomic<int> run { 0 };         // run being recorded (audio thread)
    std::atomic<int> runsRecorded { 0 }; // runs complete in 'recordings' (audio -> message thread)
    std::vector<std::vector<float>> recordings;
    int pos = 0;                        // sample position inside the current run (audio thread)
    int gapSamples = 0, windowSamples = 0;
    int analysed = 0;
    std::vector<int> found;
    int resultSamples = -1, detectedRuns = 0, spreadSamples = 0;
};
} // namespace livelooper
