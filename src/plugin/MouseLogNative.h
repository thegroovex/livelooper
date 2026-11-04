#pragma once

// v1.7 mouse diagnostics (SETUP > Display > Mouse diagnostics log): what the operating system knows about the
// mouse and the app's windows, independent of JUCE (plain C++ interface, no JUCE headers, so the Objective-C++
// file does not mix with them). Used to tell "the click never reached JUCE" from "JUCE sent it elsewhere".

#include <string>

namespace livelooper::mouselog::native
{
/** The mouse buttons the OS reports as pressed right now (bit 0 = left), or -1 when the OS gives no answer
    (Linux / Windows: JUCE's own state is used instead). */
int pressedMouseButtons();

/** macOS: the window under the mouse (windowNumberAtPoint - the check JUCE's peer uses before it accepts a
    click) and every window of the app front to back (number, level, visible, alpha, ignoresMouseEvents, key,
    frame, class). Empty on other systems. */
std::string describeWindows();
} // namespace livelooper::mouselog::native
