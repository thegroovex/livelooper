#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace livelooper
{
/**
    v1.7 display settings: colour theme (dark = default, light) and stage mode (high contrast, larger
    type). theme::apply() rewrites the colours:: values below (text, panels ...); accent colours, the
    displays (LCDs) and the VU meters stay the same in every theme. Message thread only.
*/
namespace theme
{
    void apply (bool light, bool stage);
    bool isLight() noexcept;
    bool isStage() noexcept;
    /** Changes with every apply() (cache keys of pre-rendered surfaces). */
    int version() noexcept;
    /** The dark or the light variant of a surface colour. */
    inline juce::Colour pick (juce::uint32 dark, juce::uint32 light) { return juce::Colour (isLight() ? light : dark); }
    /** Strength of black shadows / recesses (light theme: softer). */
    inline float shade() noexcept { return isLight() ? 0.35f : 1.0f; }
    /** Stage mode: small captions grow most, titles a little (the fixed layout stays intact). */
    float fontScale (float height) noexcept;
}

namespace colours
{
    inline juce::Colour panel       { 0xff141418 };
    inline juce::Colour panelLight  { 0xff2a2d32 };
    inline juce::Colour section     { 0xff1e1f25 };
    inline juce::Colour sectionEdge { 0xff3a3e44 };
    inline juce::Colour text        { 0xffe6e8ee };
    inline juce::Colour textDim     { 0xff8e939c };
    inline juce::Colour red         { 0xffff3b30 };
    inline juce::Colour green       { 0xff30d158 };
    inline juce::Colour orange      { 0xffff9f0a };
    inline juce::Colour yellow      { 0xffffd60a };
    inline juce::Colour blue        { 0xff0a84ff };
    inline juce::Colour cyan        { 0xff64d2ff };
    inline juce::Colour lcdBack     { 0xff06121a };
    inline juce::Colour lcdText     { 0xff7fe3ff };
    inline juce::Colour learn       { 0xffbf5af2 };
    // brand gradient
    inline juce::Colour brand0      { 0xffff7a18 };
    inline juce::Colour brand1      { 0xffff2d6f };
    inline juce::Colour brand2      { 0xffb14bff };

    /** v1.6: one colour per track (accent of the strip, its title bar and the idle pad). */
    juce::Colour track (int index);
    /** v1.6: a colour the user picked for a track (Push encoder page "Track colour"); 0 = the default. */
    void setTrackOverride (int index, juce::uint32 argb);
}

/**
    v1.6 look: dark "liquid glass" - translucent panels with a specular top edge, continuous
    rounded corners, soft shadows; knobs, buttons and the display lit like real objects.
    Everything is drawn with gradients only (no per-frame blur), the static surface is cached.
*/
class LooperLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LooperLookAndFeel();
    /** v1.7: re-reads the colours:: values after theme::apply(). */
    void applyTheme();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                            bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText, const juce::Drawable* icon, const juce::Colour* textColour) override;
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>& area, const juce::String& sectionName) override;
    int getPopupMenuBorderSize() override { return 6; }
    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height, bool isScrollbarVertical,
                        int thumbStartPosition, int thumbSize, bool isMouseOver, bool isMouseDown) override;
    int getDefaultScrollbarWidth() override { return 10; }
    void drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics&, const juce::Path&, juce::Image&) override;
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>& tip, const juce::Rectangle<float>& body) override;

    // button texts and tooltips are written in English in the code and translated here
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override;

    /** Manrope (embedded): bold = 800, otherwise 600. */
    static juce::Font font (float height, bool bold = false);

    /** A glass panel: translucent fill, specular top edge, hairline border, soft shadow.
        level: 2 = loop tracks (brightest), 1 = FX, 0 = everything else (subdued). */
    static void drawGlassPanel (juce::Graphics&, juce::Rectangle<float> r, float radius, int level,
                                juce::Colour tint = juce::Colours::transparentBlack);
    /** Soft shadow of a rounded rectangle from a few stacked translucent shapes (no blur). */
    static void drawSoftShadow (juce::Graphics&, juce::Rectangle<float> r, float radius, float strength, float spread = 8.0f, float dy = 5.0f);
    /** The brand gradient horizontally across r. */
    static juce::ColourGradient brandGradient (juce::Rectangle<float> r, float alpha = 1.0f);

    /** Button property "level": 0 = subdued (header, settings), 1 = normal (default), 2 = prominent. */
    static void setEmphasis (juce::Component& c, int level) { c.getProperties().set ("level", level); }
};
} // namespace livelooper
