// Ableton Link must be included before any JUCE / Windows header (winsock2 before windows.h).
#if LIVELOOPER_WITH_LINK
 #include <ableton/Link.hpp>
 #include <ableton/link/HostTimeFilter.hpp>
#endif

#include "LinkSync.h"

#include <chrono>
#include <cmath>

namespace livelooper
{
#if LIVELOOPER_WITH_LINK
struct LinkSync::Impl
{
    ableton::Link link { 120.0 };
    ableton::link::HostTimeFilter<ableton::link::platform::Clock> filter;
    double sampleTime = 0.0;
    double lastRate = 0.0;
    bool wasEnabled = false;
};

LinkSync::LinkSync() : impl (std::make_unique<Impl>())
{
    impl->link.setNumPeersCallback ([this] (std::size_t n) { numPeers.store ((int) n); });
    impl->link.setTempoCallback ([this] (double bpm) { sessionTempo.store (bpm); });
}

LinkSync::~LinkSync()
{
    impl->link.enable (false);
    impl->link.setNumPeersCallback ([] (std::size_t) {});
    impl->link.setTempoCallback ([] (double) {});
}

bool LinkSync::isAvailable() noexcept { return true; }

void LinkSync::setEnabled (bool on)
{
    if (on == impl->link.isEnabled())
        return;
    // joining: the session keeps its tempo - livelooper adopts it (never the other way round)
    impl->link.enable (on);
    if (! on)
        numPeers.store (0);
    else
        sessionTempo.store (impl->link.captureAppSessionState().tempo());
}

bool LinkSync::isEnabled() const noexcept { return impl->link.isEnabled(); }

void LinkSync::setStartStopSync (bool on)
{
    if (impl->link.isStartStopSyncEnabled() != on)
        impl->link.enableStartStopSync (on);
}

LinkSync::Block LinkSync::beginBlock (double sampleRate, int numSamples, double outputLatencySeconds, double quantum,
                                      double requestTempo, bool wantPlay, bool wantStop, double localBeat) noexcept
{
    Block b;
    auto& im = *impl;
    if (sampleRate != im.lastRate)
    {
        im.filter.reset();
        im.lastRate = sampleRate;
    }
    // host time of this buffer from the sample counter (filtered), plus the time until it is heard
    const auto hostTime = im.filter.sampleTimeToHostTime (im.sampleTime)
                          + std::chrono::microseconds ((long long) (outputLatencySeconds * 1.0e6));
    im.sampleTime += numSamples;

    const bool enabled = im.link.isEnabled();
    if (! enabled)
    {
        im.wasEnabled = false;
        return b;
    }

    auto state = im.link.captureAudioSessionState();
    bool commit = false;
    if (requestTempo > 0.0 && im.wasEnabled && std::abs (requestTempo - state.tempo()) > 1.0e-3)
    {
        state.setTempo (requestTempo, hostTime);
        commit = true;
    }
    if (im.link.isStartStopSyncEnabled())
    {
        if (wantPlay && ! state.isPlaying())
        {
            state.setIsPlayingAndRequestBeatAtTime (true, hostTime, localBeat, quantum);
            commit = true;
        }
        else if (wantStop && state.isPlaying())
        {
            state.setIsPlaying (false, hostTime);
            commit = true;
        }
    }
    if (commit)
        im.link.commitAudioSessionState (state);
    im.wasEnabled = true;

    b.active = true;
    b.bpm = state.tempo();
    b.beat = state.beatAtTime (hostTime, quantum);
    b.playing = state.isPlaying();
    sessionTempo.store (b.bpm);
    return b;
}
#else
struct LinkSync::Impl {};
LinkSync::LinkSync() = default;
LinkSync::~LinkSync() = default;
bool LinkSync::isAvailable() noexcept { return false; }
void LinkSync::setEnabled (bool) {}
bool LinkSync::isEnabled() const noexcept { return false; }
void LinkSync::setStartStopSync (bool) {}
LinkSync::Block LinkSync::beginBlock (double, int, double, double, double, bool, bool, double) noexcept { return {}; }
#endif
} // namespace livelooper
