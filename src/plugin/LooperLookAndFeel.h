#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace livelooper
{
namespace colours
{
    const juce::Colour panel       { 0xff1b1d20 };
    const juce::Colour panelLight  { 0xff2a2d32 };
    const juce::Colour section     { 0xff23262a };
    const juce::Colour sectionEdge { 0xff3a3e44 };
    const juce::Colour text        { 0xffd4d8de };
    const juce::Colour textDim     { 0xff8b9199 };
    const juce::Colour red         { 0xffff3b30 };
    const juce::Colour green       { 0xff30d158 };
    const juce::Colour orange      { 0xffff9f0a };
    const juce::Colour yellow      { 0xffffd60a };
    const juce::Colour blue        { 0xff0a84ff };
    const juce::Colour cyan        { 0xff64d2ff };
    const juce::Colour lcdBack     { 0xff07141c };
    const juce::Colour lcdText     { 0xff7fe3ff };
    const juce::Colour learn       { 0xffbf5af2 };
}

/** Hardware-style look: dark brushed panel, rubber pads, LED rings. */
class LooperLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LooperLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getPopupMenuFont() override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    // button texts and tooltips are written in English in the code and translated here
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override;

    static juce::Font font (float height, bool bold = false);
};
} // namespace livelooper
