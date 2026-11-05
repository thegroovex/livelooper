#pragma once

#include "PushSurface.h"

#include <juce_audio_devices/juce_audio_devices.h>

namespace livelooper::push
{
/**
    Talks directly to Push's User port (in the plug-in as well as in the standalone app):
    looks for the device every two seconds, opens the User port input and output, feeds
    incoming messages to PushSurface on the message thread and sends the LEDs / display.

    Windows: MIDI ports can only be opened by one program at a time, so Push's User port
    (MIDIIN2 / MIDIOUT2) must stay switched off in Live's MIDI preferences.
*/
class PushDevice : private juce::Timer,
                   private juce::AsyncUpdater,
                   private juce::MidiInputCallback
{
public:
    PushDevice (LiveLooperProcessor& p, bool standaloneApp);
    ~PushDevice() override;

    void setEnabled (bool e);
    bool isEnabled() const noexcept   { return enabled; }
    bool isConnected() const noexcept { return simulated || (input != nullptr && output != nullptr); }
    /** Tests / screenshots: behave as if Push were plugged in (the surface runs, nothing is sent). */
    void setSimulatedConnection (bool on) noexcept { simulated = on; }
    juce::String getStatusText() const;
    PushSurface& getSurface() noexcept { return surface; }

private:
    void timerCallback() override;
    /** Incoming messages are handled right away (not only on the next timer tick), so pads react instantly. */
    void handleAsyncUpdate() override;
    void processIncoming();
    void sendChanges (bool full);
    static constexpr int kTimerHz = 50;
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m) override;
    void connect();
    void disconnect();

    PushSurface surface;
    bool standalone;
    bool enabled = true;
    bool simulated = false;
    std::unique_ptr<juce::MidiInput> input;
    std::unique_ptr<juce::MidiOutput> output;
    juce::String inputId, outputId, deviceName;
    bool openFailed = false;
    bool usedElsewhere = false;
    /** Only one livelooper instance in a process drives Push at a time. */
    static std::atomic<PushDevice*> owner;
    int ticks = 0;

    // incoming short messages: MIDI thread -> message thread
    static constexpr int kQueueSize = 512;
    juce::AbstractFifo fifo { kQueueSize };
    std::array<std::array<juce::uint8, 3>, kQueueSize> queue {};
    std::atomic<juce::uint32> lastInputTime { 0 };
    juce::uint32 lastFullRefresh = 0;
    bool wasIdle = true;
};
} // namespace livelooper::push
