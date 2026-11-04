// Remote control in the editor: SETUP > Remote (on/off, port, pairing code, paired devices).

#include "PluginEditor.h"
#include "remote/RemoteSettings.h"

namespace livelooper
{
using i18n::tr;

void LiveLooperEditor::addRemoteMenu (juce::PopupMenu& menu, juce::Array<int>& toggles)
{
    namespace rs = remote::settings;
    const bool on = rs::isEnabled();
    menu.addItem (2600, tr ("Remote control (livelooper control app)"), true, on);
    menu.addItem (2601, proc.getRemoteStatusText(), false);

    juce::PopupMenu portMenu;
    const int port = rs::getPort();
    for (int i = 0; i < (int) std::size (rs::kPortChoices); ++i)
        portMenu.addItem (2610 + i, juce::String (rs::kPortChoices[i]) + (i == 0 ? " (" + tr ("default") + ")" : juce::String()),
                          true, rs::kPortChoices[i] == port);
    menu.addSubMenu (tr ("Port"), portMenu);

    juce::PopupMenu pairing;
    const auto code = rs::getPairingCode();
    pairing.addItem (2620, tr ("Pairing code: %1", code.substring (0, 3) + " " + code.substring (3)), false);
    pairing.addItem (2621, tr ("New pairing code"));
    pairing.addItem (2622, tr ("Type this code in the app once; the device stays paired until you remove it"), false);
    menu.addSubMenu (tr ("Pairing"), pairing);

    juce::PopupMenu devicesMenu;
    remoteDeviceIds.clearQuick();
    for (auto& d : rs::getDevices())
    {
        devicesMenu.addItem (2630 + remoteDeviceIds.size(), tr ("Remove %1", d.name));
        remoteDeviceIds.add (d.id);
        if (remoteDeviceIds.size() >= 40)
            break;
    }
    if (remoteDeviceIds.isEmpty())
        devicesMenu.addItem (2629, tr ("No paired devices"), false);
    menu.addSubMenu (tr ("Paired devices"), devicesMenu);
    menu.addItem (2602, tr ("Only devices in your local network can connect. Off by default."), false);

    toggles.add (2600);
    for (int i = 0; i < (int) std::size (rs::kPortChoices); ++i)
        toggles.add (-(2610 + i));
}

bool LiveLooperEditor::handleRemoteMenu (int r)
{
    namespace rs = remote::settings;
    if (r < 2600 || r > 2699)
        return false;
    if (r == 2600)
    {
        rs::setEnabled (! rs::isEnabled());
        proc.applyRemoteSettings();
        showStatus (tr (rs::isEnabled() ? "REMOTE ON" : "REMOTE OFF"));
    }
    else if (r >= 2610 && r < 2610 + (int) std::size (rs::kPortChoices))
    {
        rs::setPort (rs::kPortChoices[r - 2610]);
        proc.applyRemoteSettings();
    }
    else if (r == 2621)
    {
        rs::regeneratePairingCode();
        proc.remotePairingCodeChanged();
    }
    else if (r >= 2630 && r < 2630 + remoteDeviceIds.size())
    {
        const auto id = remoteDeviceIds[r - 2630];
        rs::removeDevice (id);
        proc.remoteDeviceRevoked (id);
    }
    return true;
}
} // namespace livelooper
