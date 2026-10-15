#pragma once

#include "LooperLookAndFeel.h"

namespace livelooper
{
class LiveLooperProcessor;

/**
    v1.6 Controller GUI (SETUP > Controllers > GUI: Controller): instead of the standard surface,
    a drawing of the connected controller (K.O. II or Push 1) with the live looper state on its
    pads, buttons and display. Pads and buttons can be clicked - the drawing is also playable.
    Offered only while a K.O. II or a Push 1 is connected; the editor falls back to the standard
    GUI when it is unplugged.
*/
class ControllerView : public juce::Component, private juce::Timer
{
public:
    enum class Device { Ko2, Push };

    explicit ControllerView (LiveLooperProcessor& p);
    ~ControllerView() override;

    void setDevice (Device d);
    Device getDevice() const noexcept { return device; }
    /** K.O. II: the page (group A - D) the drawing shows; -1 = follow the device. */
    void setKo2Page (int page) { viewPage = page; repaint(); }
    int  getShownKo2Page() const;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    /** "◀ Standard" pressed. */
    std::function<void()> onStandard;
    /** Both controllers connected: switch the drawing. */
    std::function<void (Device)> onSwitchDevice;
    bool bothConnected = false;

    /** Screen colour of a Push 1 pad palette value (tests, drawing). */
    static juce::Colour pushPadColour (int value);

private:
    void timerCallback() override;
    void paintKo2 (juce::Graphics&, juce::Rectangle<float> area);
    void paintPush (juce::Graphics&, juce::Rectangle<float> area);
    juce::Rectangle<float> ko2PadRect (int offset) const;
    juce::Rectangle<float> ko2TabRect (int group) const;
    juce::Rectangle<float> pushPadRect (int row, int col) const;
    juce::Rectangle<float> pushButtonRect (int cc) const;
    juce::Colour ko2PadColour (int group, int offset, bool& active) const;
    juce::String ko2PadText (int group, int offset) const;
    bool beatOn (double duty = 0.5) const;

    LiveLooperProcessor& proc;
    Device device = Device::Ko2;
    int viewPage = -1;
    juce::Rectangle<float> deviceArea, backButton, switchButton;
    juce::Rectangle<float> ko2Body, ko2Display, ko2Grid, ko2Tabs;
    juce::Rectangle<float> pushBody, pushGrid, pushDisplay;
    int pressedNote = -1, pressedCC = -1;
};
} // namespace livelooper
