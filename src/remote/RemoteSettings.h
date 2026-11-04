#pragma once

#include "RemoteServer.h"

/**
    Remote settings of this computer (SETUP > Remote), stored in the app settings file
    (livelooper.settings): on/off (default off), TCP port, pairing code, paired devices.
*/
namespace livelooper::remote::settings
{
bool isEnabled();
void setEnabled (bool on);

/** Offered ports (SETUP > Remote > Port). */
inline constexpr int kPortChoices[] = { 47800, 47810, 47820, 47830, 47840 };
int  getPort();
void setPort (int port);

/** The 6-digit pairing code (made on first use). */
juce::String getPairingCode();
juce::String regeneratePairingCode();

struct Device
{
    juce::String id, name;
    std::vector<uint8_t> key;
    juce::int64 pairedMs = 0;
};
std::vector<Device> getDevices();
void addDevice (const juce::String& id, const juce::String& name, const std::vector<uint8_t>& key);
bool removeDevice (const juce::String& id);

/** DeviceStore on top of the settings file (thread-safe: every call reads / writes the file under its lock). */
struct SettingsDeviceStore : DeviceStore
{
    bool findKey (const juce::String& deviceId, std::vector<uint8_t>& key) override;
    void addDevice (const juce::String& deviceId, const juce::String& deviceName, const std::vector<uint8_t>& key) override;
};
} // namespace livelooper::remote::settings
