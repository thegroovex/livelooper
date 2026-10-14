#pragma once

#include <atomic>
#include <memory>

namespace livelooper
{
/**
    v1.5 Ableton Link: tempo and beat phase shared with other apps on the network.

    Follows the Link test plan: joining a session never changes the session tempo (the
    session's tempo is adopted); the tempo is only sent to the session when the user changes
    livelooper's tempo. Start / stop sync is optional. The audio thread asks for the beat at
    the time its first sample is heard (host time + output latency, as the Link README
    describes), so livelooper sounds in phase with the other peers.

    The class compiles to a stub when livelooper is built without Link.
*/
class LinkSync
{
public:
    LinkSync();
    ~LinkSync();

    /** True when livelooper was built with Link. */
    static bool isAvailable() noexcept;

    // ---- message thread
    void setEnabled (bool on);
    bool isEnabled() const noexcept;
    void setStartStopSync (bool on);
    int getNumPeers() const noexcept { return numPeers.load(); }
    /** Session tempo as last seen by the audio thread (or the app state when idle). */
    double getSessionTempo() const noexcept { return sessionTempo.load(); }

    // ---- audio thread
    struct Block
    {
        bool active = false;      // Link is enabled: the values below are valid
        double bpm = 120.0;
        double beat = 0.0;        // beat (quarter note) at the first sample of the block, as heard
        bool playing = true;      // Link transport (only meaningful with start / stop sync)
    };
    /** Call once per block. requestTempo > 0: the user changed the tempo, send it to the session.
        wantPlay / wantStop: livelooper's transport started / stopped (start / stop sync).
        localBeat: livelooper's own beat, used when it starts the session transport. */
    Block beginBlock (double sampleRate, int numSamples, double outputLatencySeconds, double quantum,
                      double requestTempo, bool wantPlay, bool wantStop, double localBeat) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    std::atomic<int> numPeers { 0 };
    std::atomic<double> sessionTempo { 120.0 };
};
} // namespace livelooper
