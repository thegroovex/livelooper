#include "LatencyMeasure.h"

#include <algorithm>
#include <cmath>

namespace livelooper
{
void LatencyMeasure::makeBurst()
{
    // 3 ms burst: a few cycles of a 2 kHz tone with a little noise-like phase modulation in a
    // Hann window - short enough to locate precisely, broadband enough to survive speakers / mics
    const int n = std::max (32, (int) std::lround (sampleRate * 0.003));
    burst.assign ((size_t) n, 0.0f);
    unsigned seed = 0x1234567u;
    double phase = 0.0;
    for (int i = 0; i < n; ++i)
    {
        seed = seed * 1664525u + 1013904223u;
        const double jitter = ((double) ((seed >> 9) & 0xffff) / 65535.0 - 0.5) * 0.9;
        phase += 2.0 * 3.14159265358979 * (2000.0 / sampleRate) * (1.0 + jitter);
        const double w = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * (double) i / (double) (n - 1));
        burst[(size_t) i] = (float) (0.7 * w * std::sin (phase));
    }
}

void LatencyMeasure::start (double sr, int runs)
{
    status.store ((int) Status::Idle);       // the audio thread stops writing first
    sampleRate = sr > 0.0 ? sr : 48000.0;
    totalRuns = std::max (1, runs);
    makeBurst();
    gapSamples = (int) std::lround (sampleRate * kGapSeconds);
    windowSamples = (int) std::lround (sampleRate * kMaxLatencySeconds) + (int) burst.size();
    recordings.assign ((size_t) totalRuns, std::vector<float> ((size_t) (gapSamples + windowSamples), 0.0f));
    found.clear();
    analysed = 0;
    resultSamples = -1;
    detectedRuns = 0;
    spreadSamples = 0;
    pos = 0;
    run.store (0);
    runsRecorded.store (0);
    status.store ((int) Status::Running);
}

void LatencyMeasure::cancel() noexcept
{
    if (isRunning())
        status.store ((int) Status::Idle);
}

void LatencyMeasure::process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept
{
    if (status.load (std::memory_order_acquire) != (int) Status::Running)
        return;
    int r = run.load (std::memory_order_relaxed);
    const int runLength = gapSamples + windowSamples;
    for (int i = 0; i < numSamples; ++i)
    {
        if (r >= totalRuns)
        {
            outL[i] = 0.0f;
            if (outR != nullptr) outR[i] = 0.0f;
            continue;
        }
        // run layout: gap (silence, noise floor) | burst + window (listening)
        const int b = pos - gapSamples;
        const float click = (b >= 0 && b < (int) burst.size()) ? burst[(size_t) b] : 0.0f;
        outL[i] = click;
        if (outR != nullptr) outR[i] = click;
        const float in = inR != nullptr ? 0.5f * (inL[i] + inR[i]) : inL[i];
        recordings[(size_t) r][(size_t) pos] = in;
        if (++pos >= runLength)
        {
            pos = 0;
            ++r;
            runsRecorded.store (r, std::memory_order_release);
        }
    }
    run.store (r, std::memory_order_relaxed);
}

int LatencyMeasure::detect (const std::vector<float>& rec, const std::vector<float>& burst)
{
    // rec = gap (noise floor) followed by the listening window; the burst left the output at
    // index 'gap' of rec. Normalised cross-correlation; the result is relative to the start of rec.
    const int n = (int) rec.size(), m = (int) burst.size();
    if (n < m * 2 || m < 8)
        return -1;
    double be = 0.0;
    for (auto v : burst) be += (double) v * v;
    if (be <= 0.0)
        return -1;

    std::vector<double> prefix ((size_t) n + 1, 0.0);
    for (int i = 0; i < n; ++i)
        prefix[(size_t) i + 1] = prefix[(size_t) i] + (double) rec[(size_t) i] * rec[(size_t) i];

    int best = -1;
    double bestScore = 0.0, bestCorr = 0.0;
    for (int k = 0; k + m <= n; ++k)
    {
        double c = 0.0;
        for (int j = 0; j < m; ++j)
            c += (double) rec[(size_t) (k + j)] * burst[(size_t) j];
        const double e = prefix[(size_t) (k + m)] - prefix[(size_t) k];
        if (e <= 1.0e-12)
            continue;
        const double score = std::abs (c) / std::sqrt (e * be);    // 1 = perfect copy (any gain / polarity)
        if (score > bestScore + 1.0e-9)
        {
            bestScore = score;
            bestCorr = std::abs (c);
            best = k;
        }
    }
    if (best < 0 || bestScore < 0.5)
        return -1;
    // the match must clearly rise above the background (noise / hum): energy of the matched part
    // against the average energy of the recording
    const double matched = (prefix[(size_t) (best + m)] - prefix[(size_t) best]) / m;
    const double average = prefix[(size_t) n] / n;
    if (matched < average * 4.0 || bestCorr < 1.0e-4)
        return -1;
    return best;
}

int LatencyMeasure::median (std::vector<int> v)
{
    if (v.empty())
        return -1;
    std::sort (v.begin(), v.end());
    const size_t h = v.size() / 2;
    return v.size() % 2 == 1 ? v[h] : (v[h - 1] + v[h]) / 2;
}

bool LatencyMeasure::update()
{
    if (! isRunning())
        return false;
    const int ready = runsRecorded.load (std::memory_order_acquire);
    while (analysed < ready && analysed < totalRuns)
    {
        const int at = detect (recordings[(size_t) analysed], burst);
        if (at >= gapSamples)                   // found after the burst left (anything earlier is not our ping)
            found.push_back (at - gapSamples);
        ++analysed;
    }
    if (analysed < totalRuns)
        return false;

    detectedRuns = (int) found.size();
    const int needed = totalRuns / 2 + 1;
    if (detectedRuns < needed)
    {
        resultSamples = -1;
        status.store ((int) Status::NoSignal);
        return true;
    }
    resultSamples = median (found);
    // spread around the median: runs more than 1 ms away from it count as outliers
    const int tolerance = std::max (2, (int) std::lround (sampleRate * 0.001));
    int agree = 0, lo = resultSamples, hi = resultSamples;
    for (int f : found)
        if (std::abs (f - resultSamples) <= tolerance)
        {
            ++agree;
            lo = std::min (lo, f);
            hi = std::max (hi, f);
        }
    spreadSamples = 0;
    for (int f : found)
        spreadSamples = std::max (spreadSamples, std::abs (f - resultSamples));
    status.store ((int) (agree >= needed ? Status::Done : Status::Unstable));
    return true;
}
} // namespace livelooper
