// v1.7 mouse diagnostics: what macOS knows about the mouse and the app's windows (see MouseLogNative.h).

#include "MouseLogNative.h"

#import <AppKit/AppKit.h>
#include <sstream>

namespace livelooper::mouselog::native
{
int pressedMouseButtons()
{
    return (int) [NSEvent pressedMouseButtons];
}

std::string describeWindows()
{
    std::ostringstream s;
    @autoreleasepool
    {
        const NSPoint mouse = [NSEvent mouseLocation];      // screen coordinates, origin bottom left
        const NSInteger under = [NSWindow windowNumberAtPoint: mouse belowWindowWithWindowNumber: 0];
        s << "  macOS: mouse " << (int) mouse.x << "," << (int) mouse.y << " (bottom-left origin), window under mouse #"
          << (long) under;
        bool ours = false;
        for (NSWindow* w in [NSApp orderedWindows])
            if ([w windowNumber] == under)
                ours = true;
        s << (ours ? " (this app)" : " (NOT a window of this app)") << "\n";
        s << "  macOS app active: " << ([NSApp isActive] ? "yes" : "no")
          << ", key window #" << (long) [[NSApp keyWindow] windowNumber]
          << ", main window #" << (long) [[NSApp mainWindow] windowNumber] << "\n";
        int i = 0;
        for (NSWindow* w in [NSApp orderedWindows])        // front to back
        {
            const NSRect f = [w frame];
            s << "  win[" << i++ << "] #" << (long) [w windowNumber]
              << " class " << [NSStringFromClass ([w class]) UTF8String]
              << " level " << (long) [w level]
              << " visible " << ([w isVisible] ? 1 : 0)
              << " onActiveSpace " << ([w isOnActiveSpace] ? 1 : 0)
              << " alpha " << [w alphaValue]
              << " ignoresMouse " << ([w ignoresMouseEvents] ? 1 : 0)
              << " key " << ([w isKeyWindow] ? 1 : 0)
              << " canBecomeKey " << ([w canBecomeKeyWindow] ? 1 : 0)
              << " frame " << (int) f.origin.x << "," << (int) f.origin.y << " " << (int) f.size.width << "x" << (int) f.size.height
              << " backing " << [w backingScaleFactor]
              << " parent #" << (long) [[w parentWindow] windowNumber]
              << " title \"" << ([w title] != nil ? [[w title] UTF8String] : "") << "\"\n";
        }
    }
    return s.str();
}
} // namespace livelooper::mouselog::native
