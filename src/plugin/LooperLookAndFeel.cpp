#include "LooperLookAndFeel.h"
#include "Localisation.h"
#include "BinaryData.h"

#include <array>
#include <atomic>

namespace livelooper
{
namespace
{
    std::array<std::atomic<juce::uint32>, 8> trackOverrides {};
}

void colours::setTrackOverride (int index, juce::uint32 argb)
{
    if (juce::isPositiveAndBelow (index, 8))
        trackOverrides[(size_t) index].store (argb);
}

juce::Colour colours::track (int index)
{
    if (const auto o = trackOverrides[(size_t) juce::jlimit (0, 7, index)].load(); o != 0)
        return juce::Colour (o);
    static const juce::Colour c[] = { juce::Colour (0xffff8a2a), juce::Colour (0xffff3d7a), juce::Colour (0xffb65cff), juce::Colour (0xff4fc8ff),
                                      juce::Colour (0xff3fdc7a), juce::Colour (0xffffcf3a), juce::Colour (0xff5a8cff), juce::Colour (0xffff5a4a) };
    return c[(size_t) juce::jlimit (0, 7, index)];
}

namespace
{
    juce::Colour ledOf (const juce::Component& c)
    {
        return c.getProperties().contains ("ledColour")
                   ? juce::Colour ((juce::uint32) (juce::int64) c.getProperties()["ledColour"])
                   : colours::orange;
    }
    int levelOf (const juce::Component& c)
    {
        return c.getProperties().contains ("level") ? (int) c.getProperties()["level"] : 1;
    }
}

LooperLookAndFeel::LooperLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::panel);
    setColour (juce::DocumentWindow::backgroundColourId, colours::panel);
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff23242b));
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::white.withAlpha (0.10f));
    setColour (juce::ComboBox::arrowColourId, colours::textDim);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xf41c1d23));
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::blue.withAlpha (0.85f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::PopupMenu::headerTextColourId, colours::textDim);
    setColour (juce::TextButton::buttonColourId, colours::panelLight);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::Slider::textBoxTextColourId, colours::lcdText);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xf01c1d23));
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf01c1d23));
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::TooltipWindow::outlineColourId, juce::Colours::white.withAlpha (0.12f));
    setColour (juce::CaretComponent::caretColourId, colours::cyan);
    setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff1c1d23));
    setColour (juce::TextEditor::textColourId, colours::text);
    setColour (juce::TextEditor::outlineColourId, juce::Colours::white.withAlpha (0.10f));
    setColour (juce::TextEditor::focusedOutlineColourId, colours::blue.withAlpha (0.8f));
    setColour (juce::TextEditor::highlightColourId, colours::blue.withAlpha (0.4f));
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ScrollBar::thumbColourId, juce::Colours::white.withAlpha (0.25f));
    setColour (juce::ToggleButton::textColourId, colours::text);
    setColour (juce::ToggleButton::tickColourId, colours::green);
    setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff1c1d23));
    setColour (juce::AlertWindow::textColourId, colours::text);
    setColour (juce::AlertWindow::outlineColourId, juce::Colours::white.withAlpha (0.12f));
    setDefaultSansSerifTypeface (font (14.0f).getTypefacePtr());
}

juce::Font LooperLookAndFeel::font (float height, bool bold)
{
    static const juce::Typeface::Ptr semi = juce::Typeface::createSystemTypefaceFor (BinaryData::Manrope600_ttf, (size_t) BinaryData::Manrope600_ttfSize);
    static const juce::Typeface::Ptr heavy = juce::Typeface::createSystemTypefaceFor (BinaryData::Manrope800_ttf, (size_t) BinaryData::Manrope800_ttfSize);
    const auto& tf = bold ? heavy : semi;
    if (tf == nullptr)
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    return juce::Font (juce::FontOptions (tf).withHeight (height));
}

juce::ColourGradient LooperLookAndFeel::brandGradient (juce::Rectangle<float> r, float alpha)
{
    juce::ColourGradient g (colours::brand0.withMultipliedAlpha (alpha), r.getX(), r.getY(),
                            colours::brand2.withMultipliedAlpha (alpha), r.getRight(), r.getY(), false);
    g.addColour (0.5, colours::brand1.withMultipliedAlpha (alpha));
    return g;
}

void LooperLookAndFeel::drawSoftShadow (juce::Graphics& g, juce::Rectangle<float> r, float radius, float strength, float spread, float dy)
{
    constexpr int steps = 6;
    for (int i = steps; i >= 1; --i)
    {
        const float k = (float) i / (float) steps;
        g.setColour (juce::Colours::black.withAlpha (strength * 0.16f * (1.0f - k * 0.7f)));
        g.fillRoundedRectangle (r.expanded (spread * k).translated (0.0f, dy * k), radius + spread * k);
    }
}

void LooperLookAndFeel::drawGlassPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius, int level, juce::Colour tint)
{
    const float shadow = level >= 2 ? 1.0f : (level == 1 ? 0.8f : 0.5f);
    drawSoftShadow (g, r, radius, shadow, level >= 2 ? 12.0f : 9.0f, level >= 2 ? 8.0f : 5.0f);

    // translucent body: a cool dark glass, a little lighter at the top
    const float topA = level >= 2 ? 0.96f : (level == 1 ? 0.90f : 0.80f);
    juce::ColourGradient body (juce::Colour (level >= 2 ? 0xff2a2b33 : (level == 1 ? 0xff24252c : 0xff1f2026)).withAlpha (topA), r.getX(), r.getY(),
                               juce::Colour (level >= 2 ? 0xff1a1b21 : 0xff17181d).withAlpha (topA), r.getX(), r.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (r, radius);
    if (! tint.isTransparent())
    {
        juce::ColourGradient t (tint.withAlpha (level >= 2 ? 0.16f : 0.10f), r.getX(), r.getY(), tint.withAlpha (0.0f), r.getX(), r.getY() + r.getHeight() * 0.55f, false);
        g.setGradientFill (t);
        g.fillRoundedRectangle (r, radius);
    }
    // specular sheen on the upper part (the "liquid" highlight)
    {
        juce::ColourGradient sheen (juce::Colours::white.withAlpha (level >= 2 ? 0.075f : 0.055f), r.getX(), r.getY(),
                                    juce::Colours::white.withAlpha (0.0f), r.getX(), r.getY() + juce::jmin (70.0f, r.getHeight() * 0.4f), false);
        g.setGradientFill (sheen);
        g.fillRoundedRectangle (r.reduced (1.0f), radius - 1.0f);
    }
    // hairline border: bright on top, dark at the bottom (lit from above)
    juce::ColourGradient edge (juce::Colours::white.withAlpha (level >= 2 ? 0.22f : 0.15f), r.getX(), r.getY(),
                               juce::Colours::white.withAlpha (0.04f), r.getX(), r.getBottom(), false);
    g.setGradientFill (edge);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    // specular top line
    juce::ColourGradient line (juce::Colours::white.withAlpha (0.0f), r.getX() + radius, r.getY(),
                               juce::Colours::white.withAlpha (0.0f), r.getRight() - radius, r.getY(), false);
    line.addColour (0.3, juce::Colours::white.withAlpha (level >= 2 ? 0.38f : 0.25f));
    line.addColour (0.7, juce::Colours::white.withAlpha (level >= 2 ? 0.38f : 0.25f));
    g.setGradientFill (line);
    g.fillRect (r.getX() + radius, r.getY() + 0.5f, r.getWidth() - radius * 2.0f, 1.0f);
}

//============================================================================== knobs
void LooperLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                          float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId, true);
    const bool enabled = slider.isEnabled();
    const float arcW = juce::jlimit (2.5f, 4.0f, radius * 0.12f);

    // value arc in a recessed groove, with a soft glow
    juce::Path track, value;
    track.addCentredArc (centre.x, centre.y, radius - arcW * 0.5f, radius - arcW * 0.5f, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.strokePath (track, juce::PathStrokeType (arcW + 1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    value.addCentredArc (centre.x, centre.y, radius - arcW * 0.5f, radius - arcW * 0.5f, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    if (enabled)
    {
        g.setColour (accent.withAlpha (0.22f));
        g.strokePath (value, juce::PathStrokeType (arcW * 2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (enabled ? accent : accent.withSaturation (0.1f).withAlpha (0.5f));
    g.strokePath (value, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // knob: drop shadow, bevelled skirt, satin cap with a specular highlight
    const float r = radius - arcW - 3.5f;
    if (r < 4.0f)
        return;
    const auto knobArea = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre);
    {
        juce::ColourGradient sh (juce::Colours::black.withAlpha (0.55f), centre.x, centre.y + r * 0.35f,
                                 juce::Colours::black.withAlpha (0.0f), centre.x, centre.y + r * 1.45f, true);
        g.setGradientFill (sh);
        g.fillEllipse (knobArea.expanded (r * 0.35f).translated (0.0f, r * 0.18f));
    }
    juce::ColourGradient skirt (juce::Colour (0xff6b6e78), centre.x, centre.y - r, juce::Colour (0xff141519), centre.x, centre.y + r, false);
    g.setGradientFill (skirt);
    g.fillEllipse (knobArea);
    const auto cap = knobArea.reduced (r * 0.12f);
    juce::ColourGradient capG (juce::Colour (0xff4a4d56), cap.getX() + cap.getWidth() * 0.3f, cap.getY() + cap.getHeight() * 0.2f,
                               juce::Colour (0xff1f2026), cap.getRight(), cap.getBottom(), true);
    g.setGradientFill (capG);
    g.fillEllipse (cap);
    juce::ColourGradient spec (juce::Colours::white.withAlpha (0.28f), centre.x, cap.getY(),
                               juce::Colours::white.withAlpha (0.0f), centre.x, cap.getY() + cap.getHeight() * 0.55f, false);
    g.setGradientFill (spec);
    g.fillEllipse (cap.reduced (cap.getWidth() * 0.12f, 0.0f).withHeight (cap.getHeight() * 0.55f).translated (0.0f, 1.0f));
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.drawEllipse (knobArea, 1.0f);

    // pointer: a light line with a lit tip
    const juce::Point<float> tip (centre.x + std::sin (angle) * (r * 0.78f), centre.y - std::cos (angle) * (r * 0.78f));
    const juce::Point<float> inner (centre.x + std::sin (angle) * (r * 0.25f), centre.y - std::cos (angle) * (r * 0.25f));
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawLine ({ inner.translated (0.0f, 1.0f), tip.translated (0.0f, 1.0f) }, juce::jmax (2.0f, r * 0.13f));
    g.setColour (enabled ? juce::Colour (0xfff4f5f8) : colours::textDim);
    g.drawLine ({ inner, tip }, juce::jmax (2.0f, r * 0.12f));
}

void LooperLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float,
                                          float, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, 0, 0, style, slider);
        return;
    }

    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat();
    const float cx = bounds.getCentreX();
    const auto accent = slider.findColour (juce::Slider::trackColourId, true);

    // recessed slot with an inner shadow
    const juce::Rectangle<float> slot (cx - 3.5f, bounds.getY(), 7.0f, bounds.getHeight());
    g.setColour (juce::Colours::black.withAlpha (0.75f));
    g.fillRoundedRectangle (slot, 3.5f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawRoundedRectangle (slot.translated (0.0f, 0.5f), 3.5f, 0.8f);

    // fill with a glow
    const juce::Rectangle<float> fill (cx - 2.0f, sliderPos, 4.0f, bounds.getBottom() - sliderPos);
    g.setColour (accent.withAlpha (0.25f));
    g.fillRoundedRectangle (fill.expanded (2.0f, 0.0f), 3.0f);
    juce::ColourGradient fg (accent.brighter (0.25f), cx, sliderPos, accent.darker (0.4f), cx, bounds.getBottom(), false);
    g.setGradientFill (fg);
    g.fillRoundedRectangle (fill, 2.0f);

    // tick marks (100 % = unity)
    g.setColour (colours::textDim.withAlpha (0.35f));
    for (int i = 0; i <= 8; ++i)
    {
        const float ty = bounds.getY() + bounds.getHeight() * (float) i / 8.0f;
        g.fillRect (cx - 12.0f, ty, 4.0f, 1.0f);
        g.fillRect (cx + 8.0f, ty, 4.0f, 1.0f);
    }

    // fader cap: shadow, bevel, grip line
    const juce::Rectangle<float> cap (cx - 13.0f, sliderPos - 8.0f, 26.0f, 16.0f);
    drawSoftShadow (g, cap, 4.0f, 0.9f, 4.0f, 3.0f);
    juce::ColourGradient grad (juce::Colour (0xff70737d), cap.getX(), cap.getY(), juce::Colour (0xff2a2c32), cap.getX(), cap.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (cap, 4.0f);
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.fillRoundedRectangle (cap.reduced (2.0f, 1.0f).withHeight (3.0f), 1.5f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (cap, 4.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.fillRect (cap.getX() + 5.0f, sliderPos - 0.5f, cap.getWidth() - 10.0f, 1.5f);
}

//============================================================================== buttons
void LooperLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                              bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.5f);
    const float corner = juce::jmin (9.0f, bounds.getHeight() * 0.32f);
    const bool on = button.getToggleState();
    const auto led = ledOf (button);
    const int level = levelOf (button);
    const bool enabled = button.isEnabled();

    // soft drop shadow (pressed buttons sit lower)
    if (! down)
        drawSoftShadow (g, bounds, corner, level == 0 ? 0.35f : 0.6f, 3.0f, 2.0f);

    // body: lit from above
    const juce::Colour top = down ? juce::Colour (0xff23242a) : juce::Colour (level == 0 ? 0xff2c2d34 : (level >= 2 ? 0xff40424b : 0xff36373f));
    const juce::Colour bottom = down ? juce::Colour (0xff2a2b31) : juce::Colour (level == 0 ? 0xff222329 : 0xff25262c);
    juce::ColourGradient grad (top, bounds.getX(), bounds.getY(), bottom, bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (bounds, corner);

    if (on)
    {
        // lit: the LED colour glows through the glass
        juce::ColourGradient lit (led.withAlpha (0.42f), bounds.getX(), bounds.getY(), led.withAlpha (0.20f), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (lit);
        g.fillRoundedRectangle (bounds, corner);
        g.setColour (led.withAlpha (0.28f));
        g.drawRoundedRectangle (bounds.expanded (1.0f), corner + 1.0f, 2.0f);
        g.setColour (led.brighter (0.2f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), corner, 1.2f);
    }
    else
    {
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (bounds, corner, 1.0f);
    }

    // specular top edge + sheen
    if (! down)
    {
        juce::ColourGradient sheen (juce::Colours::white.withAlpha (level == 0 ? 0.06f : 0.10f), bounds.getX(), bounds.getY(),
                                    juce::Colours::white.withAlpha (0.0f), bounds.getX(), bounds.getCentreY(), false);
        g.setGradientFill (sheen);
        g.fillRoundedRectangle (bounds.reduced (1.0f), corner - 1.0f);
        g.setColour (juce::Colours::white.withAlpha (level == 0 ? 0.10f : 0.17f));
        g.drawLine (bounds.getX() + corner, bounds.getY() + 1.2f, bounds.getRight() - corner, bounds.getY() + 1.2f, 1.0f);
    }

    if (highlighted && ! down && enabled)
    {
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRoundedRectangle (bounds, corner);
    }
}

void LooperLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    const bool on = button.getToggleState();
    const auto led = ledOf (button);
    const int level = levelOf (button);
    g.setFont (getTextButtonFont (button, button.getHeight()));
    juce::Colour c = on ? led.brighter (0.55f) : (level == 0 ? colours::textDim.brighter (0.25f) : colours::text);
    if (! button.isEnabled())
        c = colours::textDim.withAlpha (0.6f);
    g.setColour (c);
    // one line unless the text asks for two (a long translation shrinks a little instead of wrapping)
    const auto text = i18n::tr (button.getButtonText());
    g.drawFittedText (text, button.getLocalBounds().reduced (4, 1), juce::Justification::centred, text.containsChar ('\n') ? 2 : 1, 0.7f);
}

juce::Font LooperLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return font (juce::jlimit (9.5f, 15.0f, (float) buttonHeight * 0.40f), true);
}

void LooperLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    // an Apple-style switch on the right, the text on the left
    const auto bounds = b.getLocalBounds().toFloat();
    const float sh = juce::jmin (22.0f, bounds.getHeight() - 4.0f), sw = sh * 1.75f;
    const juce::Rectangle<float> sw_r (bounds.getRight() - sw - 2.0f, bounds.getCentreY() - sh * 0.5f, sw, sh);
    const bool on = b.getToggleState();
    g.setColour (on ? colours::green : juce::Colour (0xff3a3b43));
    g.fillRoundedRectangle (sw_r, sh * 0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (sw_r, sh * 0.5f, 1.0f);
    const float d = sh - 4.0f;
    const juce::Rectangle<float> knob (on ? sw_r.getRight() - d - 2.0f : sw_r.getX() + 2.0f, sw_r.getY() + 2.0f, d, d);
    drawSoftShadow (g, knob, d * 0.5f, 0.6f, 2.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (b.isEnabled() ? 1.0f : 0.5f));
    g.fillEllipse (knob);
    if (highlighted)
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRoundedRectangle (sw_r, sh * 0.5f);
    }
    g.setColour (b.isEnabled() ? colours::text : colours::textDim);
    g.setFont (font (14.0f));
    g.drawFittedText (i18n::tr (b.getButtonText()), bounds.withRight (sw_r.getX() - 8.0f).toNearestInt(), juce::Justification::centredLeft, 2, 0.8f);
}

//============================================================================== combo boxes, menus
void LooperLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool down, int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (1.0f);
    const float corner = juce::jmin (8.0f, bounds.getHeight() * 0.3f);
    drawSoftShadow (g, bounds, corner, 0.4f, 2.5f, 1.5f);
    juce::ColourGradient grad (box.findColour (juce::ComboBox::backgroundColourId).brighter (down ? 0.0f : 0.12f), 0.0f, bounds.getY(),
                               box.findColour (juce::ComboBox::backgroundColourId).darker (0.15f), 0.0f, bounds.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (bounds, corner);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds.reduced (0.5f), corner, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawLine (bounds.getX() + corner, bounds.getY() + 1.2f, bounds.getRight() - corner, bounds.getY() + 1.2f, 1.0f);

    // chevron (up / down) like a pop-up button
    const float ax = (float) width - 13.0f, ay = (float) height * 0.5f;
    juce::Path chev;
    chev.startNewSubPath (ax - 3.5f, ay - 1.5f);
    chev.lineTo (ax, ay - 5.0f);
    chev.lineTo (ax + 3.5f, ay - 1.5f);
    chev.startNewSubPath (ax - 3.5f, ay + 1.5f);
    chev.lineTo (ax, ay + 5.0f);
    chev.lineTo (ax + 3.5f, ay + 1.5f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.strokePath (chev, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void LooperLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font LooperLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return font (juce::jlimit (10.5f, 15.0f, (float) box.getHeight() * 0.48f), false);
}

juce::Font LooperLookAndFeel::getPopupMenuFont()
{
    return font (14.5f);
}

juce::Font LooperLookAndFeel::getLabelFont (juce::Label& l)
{
    return font (l.getFont().getHeight());
}

void LooperLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
    juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.05f), 0.0f, 0.0f, juce::Colours::white.withAlpha (0.0f), 0.0f, 60.0f, false);
    g.setGradientFill (sheen);
    g.fillRect (r);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawRect (r, 1.0f);
}

void LooperLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                                           bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                                           const juce::String& shortcutKeyText, const juce::Drawable* icon, const juce::Colour* textColour)
{
    if (isSeparator)
    {
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.fillRect (area.reduced (10, 0).withSizeKeepingCentre (area.getWidth() - 20, 1));
        return;
    }
    auto r = area.reduced (5, 1);
    if (isHighlighted && isActive)
    {
        g.setColour (findColour (juce::PopupMenu::highlightedBackgroundColourId));
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
    }
    auto c = textColour != nullptr ? *textColour : findColour (juce::PopupMenu::textColourId);
    if (isHighlighted && isActive)
        c = juce::Colours::white;
    if (! isActive)
        c = colours::textDim.withAlpha (0.8f);
    g.setColour (c);
    auto content = r.reduced (8, 0);
    auto tick = content.removeFromLeft (18);
    if (isTicked)
    {
        juce::Path p;
        const auto t = tick.toFloat().withSizeKeepingCentre (11.0f, 11.0f);
        p.startNewSubPath (t.getX(), t.getCentreY());
        p.lineTo (t.getX() + t.getWidth() * 0.38f, t.getBottom() - 1.0f);
        p.lineTo (t.getRight(), t.getY() + 1.0f);
        g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else if (icon != nullptr)
        icon->drawWithin (g, tick.toFloat().reduced (2.0f), juce::RectanglePlacement::centred, 1.0f);
    if (hasSubMenu)
    {
        auto arr = content.removeFromRight (12).toFloat();
        juce::Path p;
        p.startNewSubPath (arr.getX() + 3.0f, arr.getCentreY() - 4.0f);
        p.lineTo (arr.getX() + 7.0f, arr.getCentreY());
        p.lineTo (arr.getX() + 3.0f, arr.getCentreY() + 4.0f);
        g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setFont (getPopupMenuFont());
    if (shortcutKeyText.isNotEmpty())
    {
        g.setColour (c.withAlpha (0.6f));
        g.drawText (shortcutKeyText, content, juce::Justification::centredRight);
        g.setColour (c);
    }
    g.drawFittedText (text, content.withTrimmedLeft (4), juce::Justification::centredLeft, 1, 0.8f);
}

void LooperLookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& sectionName)
{
    g.setColour (colours::textDim);
    g.setFont (font (11.5f, true));
    g.drawFittedText (i18n::upper (sectionName), area.reduced (16, 0).withTrimmedTop (4), juce::Justification::centredLeft, 1);
}

void LooperLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& te)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    const auto bg = te.findColour (juce::TextEditor::backgroundColourId);
    if (bg.isTransparent())
        return;
    g.setColour (bg);
    g.fillRoundedRectangle (r, juce::jmin (9.0f, r.getHeight() * 0.3f));
}

void LooperLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& te)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    const auto c = te.hasKeyboardFocus (true) && ! te.isReadOnly() ? te.findColour (juce::TextEditor::focusedOutlineColourId)
                                                                    : te.findColour (juce::TextEditor::outlineColourId);
    if (c.isTransparent())
        return;
    g.setColour (c);
    g.drawRoundedRectangle (r, juce::jmin (9.0f, r.getHeight() * 0.3f), te.hasKeyboardFocus (true) && ! te.isReadOnly() ? 2.0f : 1.0f);
}

void LooperLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar& bar, int x, int y, int width, int height, bool vertical,
                                       int thumbStart, int thumbSize, bool over, bool down)
{
    juce::Rectangle<int> thumb = vertical ? juce::Rectangle<int> (x, thumbStart, width, thumbSize)
                                          : juce::Rectangle<int> (thumbStart, y, thumbSize, height);
    const auto t = thumb.toFloat().reduced (vertical ? 2.5f : 1.0f, vertical ? 1.0f : 2.5f);
    g.setColour (bar.findColour (juce::ScrollBar::thumbColourId).withMultipliedAlpha (over || down ? 1.6f : 1.0f));
    g.fillRoundedRectangle (t, juce::jmin (t.getWidth(), t.getHeight()) * 0.5f);
}

void LooperLookAndFeel::drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics& g, const juce::Path& path, juce::Image&)
{
    for (int i = 4; i >= 1; --i)
    {
        g.setColour (juce::Colours::black.withAlpha (0.07f));
        g.strokePath (path, juce::PathStrokeType ((float) i * 3.0f));
    }
    g.setColour (juce::Colour (0xf61d1e24));
    g.fillPath (path);
    g.setColour (juce::Colours::white.withAlpha (0.14f));
    g.strokePath (path, juce::PathStrokeType (1.0f));
}

void LooperLookAndFeel::drawBubble (juce::Graphics& g, juce::BubbleComponent& comp, const juce::Point<float>& tip, const juce::Rectangle<float>& body)
{
    LookAndFeel_V4::drawBubble (g, comp, tip, body);
}

juce::Label* LooperLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (slider);
    l->setFont (font (11.0f, true));
    l->setJustificationType (juce::Justification::centred);
    return l;
}

void LooperLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    g.setColour (findColour (juce::TooltipWindow::backgroundColourId));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (findColour (juce::TooltipWindow::outlineColourId));
    g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
    juce::AttributedString s;
    s.setJustification (juce::Justification::centredLeft);
    s.append (i18n::tr (text), font (13.5f), findColour (juce::TooltipWindow::textColourId));
    juce::TextLayout tl;
    tl.createLayoutWithBalancedLineLengths (s, (float) width - 20.0f);
    tl.draw (g, r.reduced (10.0f, 6.0f));
}

juce::Rectangle<int> LooperLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                                          juce::Rectangle<int> parentArea)
{
    juce::AttributedString s;
    s.append (i18n::tr (tipText), font (13.5f), juce::Colours::white);
    juce::TextLayout tl;
    tl.createLayoutWithBalancedLineLengths (s, 360.0f);
    const int w = (int) (tl.getWidth() + 24.0f), h = (int) (tl.getHeight() + 14.0f);
    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
        .constrainedWithin (parentArea);
}
} // namespace livelooper
