#include "PushDevice.h"
#include "../Localisation.h"

namespace livelooper::push
{
namespace
{
    template <typename DeviceList>
    juce::MidiDeviceInfo findUserPort (const DeviceList& devices)
    {
        int index = 0;
        for (const auto& d : devices)
        {
            if (! isPushDevice (d.name))
                continue;
            if (isPushUserPort (d.name, index))
                return d;
            ++index;
        }
        return {};
    }
}

std::atomic<PushDevice*> PushDevice::owner { nullptr };

PushDevice::PushDevice (LiveLooperProcessor& p, bool standaloneApp) : surface (p), standalone (standaloneApp)
{
    startTimerHz (kTimerHz);
}

PushDevice::~PushDevice()
{
    stopTimer();
    cancelPendingUpdate();
    disconnect();
}

void PushDevice::setEnabled (bool e)
{
    if (e == enabled)
        return;
    enabled = e;
    if (! enabled)
        disconnect();
    openFailed = false;
}

juce::String PushDevice::getStatusText() const
{
    if (! enabled)
        return i18n::tr ("Off");
    if (isConnected())
        return deviceName.isEmpty() ? i18n::tr ("Connected") : i18n::tr ("Connected: %1", deviceName);
    if (usedElsewhere)
        return i18n::tr ("Push is controlled by another livelooper instance");
    if (openFailed)
        return i18n::tr ("Push found, but its User port is in use (Windows: switch it off in your DAW's MIDI settings)");
    return i18n::tr ("No Push found");
}

void PushDevice::connect()
{
    PushDevice* expected = nullptr;
    if (! owner.compare_exchange_strong (expected, this) && expected != this)
    {
        usedElsewhere = true;
        return;
    }
    usedElsewhere = false;
    struct Release { PushDevice& d; ~Release() { if (! d.isConnected()) { auto* self = &d; owner.compare_exchange_strong (self, nullptr); } } } release { *this };

    const auto inInfo = findUserPort (juce::MidiInput::getAvailableDevices());
    const auto outInfo = findUserPort (juce::MidiOutput::getAvailableDevices());
    if (inInfo.identifier.isEmpty() || outInfo.identifier.isEmpty())
    {
        openFailed = false;
        return;
    }

    input = juce::MidiInput::openDevice (inInfo.identifier, this);
    output = juce::MidiOutput::openDevice (outInfo.identifier);
    if (input == nullptr || output == nullptr)
    {
        input.reset();
        output.reset();
        openFailed = true;
        return;
    }
    openFailed = false;
    inputId = inInfo.identifier;
    outputId = outInfo.identifier;
    deviceName = inInfo.name;
    input->start();

    // Without Live (standalone app) nobody switches Push to User mode, so we do it.
    // Inside Live, the User button does it and Live's own Push stays in charge otherwise.
    if (standalone)
        output->sendMessageNow (userModeSysex (true));
    // v1.6: polyphonic aftertouch, so every pad reports its own pressure (chop speed per track, FX amount)
    output->sendMessageNow (aftertouchModeSysex (true));
    surface.invalidate();
    lastFullRefresh = 0;
}

void PushDevice::disconnect()
{
    if (input != nullptr)
        input->stop();
    if (output != nullptr && standalone)
        output->sendMessageNow (userModeSysex (false));
    input.reset();
    output.reset();
    inputId.clear();
    outputId.clear();
    auto* self = this;
    owner.compare_exchange_strong (self, nullptr);
}

void PushDevice::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    if (m.getRawDataSize() > 3 || m.isSysEx())
        return;
    lastInputTime.store (juce::Time::getMillisecondCounter());
    const auto scope = fifo.write (1);
    auto store = [&] (int index) {
        auto& slot = queue[(size_t) index];
        slot.fill (0);
        std::memcpy (slot.data(), m.getRawData(), (size_t) m.getRawDataSize());
    };
    if (scope.blockSize1 > 0) store (scope.startIndex1);
    else if (scope.blockSize2 > 0) store (scope.startIndex2);
    triggerAsyncUpdate();
}

void PushDevice::processIncoming()
{
    const auto scope = fifo.read (fifo.getNumReady());
    auto handle = [this] (int start, int size) {
        for (int i = 0; i < size; ++i)
        {
            const auto& d = queue[(size_t) (start + i)];
            const int len = juce::MidiMessage::getMessageLengthFromFirstByte (d[0]);
            surface.handleMidi (juce::MidiMessage (d.data(), juce::jlimit (1, 3, len)));
        }
    };
    handle (scope.startIndex1, scope.blockSize1);
    handle (scope.startIndex2, scope.blockSize2);
}

void PushDevice::sendChanges (bool full)
{
    // v1.6 fix: this called itself (endless recursion on the first incoming message while connected)
    if (output == nullptr)
        return;
    std::vector<juce::MidiMessage> out;
    surface.render (out, full);
    for (const auto& m : out)
        output->sendMessageNow (m);
}

void PushDevice::handleAsyncUpdate()
{
    if (! enabled || ! isConnected())
        return;
    processIncoming();
    sendChanges (false);       // immediate LED feedback for the press
}

void PushDevice::timerCallback()
{
    ++ticks;
    if (! enabled)
        return;
    if (simulated)
    {
        // no hardware: keep the surface state current for the on-screen Controller GUI
        surface.update();
        std::vector<juce::MidiMessage> discard;
        surface.render (discard, false);
        return;
    }

    if (! isConnected())
    {
        if (ticks % (2 * kTimerHz) == 1)          // every two seconds
            connect();
        if (! isConnected())
            return;
    }
    else if (ticks % (2 * kTimerHz) == 0)
    {
        // the device went away (unplugged)?
        bool present = false;
        for (const auto& d : juce::MidiInput::getAvailableDevices())
            present = present || d.identifier == inputId;
        if (! present)
        {
            disconnect();
            return;
        }
    }

    // incoming (normally already handled by handleAsyncUpdate), held gestures, meters
    processIncoming();
    surface.update();

    // outgoing: changes every frame, everything again once a second and whenever Push
    // starts talking after a pause (it was just switched back to User mode)
    const auto now = juce::Time::getMillisecondCounter();
    const bool idle = now - lastInputTime.load() > 3000;
    const bool full = now - lastFullRefresh > 1000 || (wasIdle && ! idle);
    if (wasIdle && ! idle)
        output->sendMessageNow (aftertouchModeSysex (true));     // Push came back (User mode again): poly aftertouch
    wasIdle = idle;
    if (full)
        lastFullRefresh = now;

    std::vector<juce::MidiMessage> out;
    surface.render (out, full);
    for (const auto& m : out)
        output->sendMessageNow (m);
}
} // namespace livelooper::push
