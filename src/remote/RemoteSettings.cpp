#include "RemoteSettings.h"

#include "plugin/AppSettings.h"

#include <algorithm>

namespace livelooper::remote::settings
{
namespace
{
    juce::CriticalSection& deviceLock()
    {
        static juce::CriticalSection c;
        return c;
    }

    void writeDevices (const std::vector<Device>& list)
    {
        juce::Array<juce::var> arr;
        for (auto& d : list)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("id", d.id);
            o->setProperty ("name", d.name);
            o->setProperty ("key", toHex (d.key));
            o->setProperty ("paired", juce::String (d.pairedMs));
            arr.add (juce::var (o));
        }
        appsettings::setValue ("remote.devices", juce::JSON::toString (juce::var (arr), true));
    }
}

bool isEnabled()           { return appsettings::getValue ("remote.enabled", "0") == "1"; }
void setEnabled (bool on)  { appsettings::setValue ("remote.enabled", on ? "1" : "0"); }

int getPort()
{
    const int p = appsettings::getValue ("remote.port", juce::String (kDefaultPort)).getIntValue();
    return (p >= 1024 && p <= 65535) ? p : kDefaultPort;
}

void setPort (int port) { appsettings::setValue ("remote.port", juce::String (port)); }

juce::String getPairingCode()
{
    const juce::ScopedLock sl (deviceLock());
    auto code = appsettings::getValue ("remote.code");
    if (code.length() != 6 || ! code.containsOnly ("0123456789"))
    {
        code = randomPairingCode();
        appsettings::setValue ("remote.code", code);
    }
    return code;
}

juce::String regeneratePairingCode()
{
    const juce::ScopedLock sl (deviceLock());
    auto code = randomPairingCode();
    appsettings::setValue ("remote.code", code);
    return code;
}

std::vector<Device> getDevices()
{
    const juce::ScopedLock sl (deviceLock());
    std::vector<Device> list;
    const auto v = juce::JSON::parse (appsettings::getValue ("remote.devices", "[]"));
    if (auto* arr = v.getArray())
        for (auto& e : *arr)
        {
            Device d;
            d.id = e["id"].toString();
            d.name = e["name"].toString();
            d.pairedMs = e["paired"].toString().getLargeIntValue();
            if (d.id.isNotEmpty() && fromHex (e["key"].toString(), d.key) && d.key.size() == 32)
                list.push_back (std::move (d));
        }
    return list;
}

void addDevice (const juce::String& id, const juce::String& name, const std::vector<uint8_t>& key)
{
    const juce::ScopedLock sl (deviceLock());
    auto list = getDevices();
    list.erase (std::remove_if (list.begin(), list.end(), [&id] (const Device& d) { return d.id == id; }), list.end());
    list.push_back ({ id, name, key, juce::Time::currentTimeMillis() });
    writeDevices (list);
}

bool removeDevice (const juce::String& id)
{
    const juce::ScopedLock sl (deviceLock());
    auto list = getDevices();
    const auto before = list.size();
    list.erase (std::remove_if (list.begin(), list.end(), [&id] (const Device& d) { return d.id == id; }), list.end());
    if (list.size() == before)
        return false;
    writeDevices (list);
    return true;
}

bool SettingsDeviceStore::findKey (const juce::String& deviceId, std::vector<uint8_t>& key)
{
    for (auto& d : getDevices())
        if (d.id == deviceId)
        {
            key = d.key;
            return true;
        }
    return false;
}

void SettingsDeviceStore::addDevice (const juce::String& deviceId, const juce::String& deviceName, const std::vector<uint8_t>& key)
{
    settings::addDevice (deviceId, deviceName, key);
}
} // namespace livelooper::remote::settings
