#include "LooperLookAndFeel.h"

namespace livelooper
{
LooperLookAndFeel::LooperLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::panel);
    setColour (juce::ComboBox::backgroundColourId, colours::lcdBack);
    setColour (juce::ComboBox::textColourId, colours::lcdText);
    setColour (juce::ComboBox::outlineColourId, colours::sectionEdge);
    setColour (juce::ComboBox::arrowColourId, colours::lcdText);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff15171a));
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::blue.withAlpha (0.6f));
    setColour (juce::PopupMenu::headerTextColourId, colours::cyan);
    setColour (juce::TextButton::buttonColourId, colours::panelLight);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::Slider::textBoxTextColourId, colours::lcdText);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xff15171a));
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff15171a));
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::CaretComponent::caretColourId, colours::cyan);
    setColour (juce::TextEditor::backgroundColourId, colours::lcdBack);
    setColour (juce::TextEditor::textColourId, colours::lcdText);
}

juce::Font LooperLookAndFeel::font (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

void LooperLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                          float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId, true);

    // value arc
    juce::Path track, value;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (accent);
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // knob body
    const float r = radius - 5.0f;
    juce::ColourGradient body (juce::Colour (0xff4a4f57), centre.x, centre.y - r,
                               juce::Colour (0xff1e2024), centre.x, centre.y + r, false);
    g.setGradientFill (body);
    g.fillEllipse (centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (centre.x - r, centre.y - r, r * 2.0f, r * 2.0f, 1.0f);

    // pointer
    juce::Path p;
    p.addRoundedRectangle (-1.5f, -r + 2.0f, 3.0f, r * 0.55f, 1.5f);
    g.setColour (colours::text);
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (centre));
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

    // slot
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (cx - 3.0f, bounds.getY(), 6.0f, bounds.getHeight(), 3.0f);

    // fill
    g.setColour (slider.findColour (juce::Slider::trackColourId, true).withAlpha (0.8f));
    g.fillRoundedRectangle (cx - 2.0f, sliderPos, 4.0f, bounds.getBottom() - sliderPos, 2.0f);

    // tick marks (100 % = unity)
    g.setColour (colours::textDim.withAlpha (0.5f));
    for (int i = 0; i <= 8; ++i)
    {
        const float ty = bounds.getY() + bounds.getHeight() * (float) i / 8.0f;
        g.drawHorizontalLine ((int) ty, cx - 12.0f, cx - 7.0f);
        g.drawHorizontalLine ((int) ty, cx + 7.0f, cx + 12.0f);
    }

    // cap
    const juce::Rectangle<float> cap (cx - 14.0f, sliderPos - 7.0f, 28.0f, 14.0f);
    juce::ColourGradient grad (juce::Colour (0xff5c626b), cap.getX(), cap.getY(),
                               juce::Colour (0xff2b2e33), cap.getX(), cap.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (cap, 3.0f, 1.0f);
    g.setColour (colours::text);
    g.drawHorizontalLine ((int) sliderPos, cap.getX() + 4.0f, cap.getRight() - 4.0f);
}

void LooperLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                              bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const float corner = juce::jmin (6.0f, bounds.getHeight() * 0.3f);
    const bool on = button.getToggleState();
    const auto led = button.getProperties().contains ("ledColour")
                         ? juce::Colour ((juce::uint32) (juce::int64) button.getProperties()["ledColour"])
                         : colours::orange;

    juce::ColourGradient grad (juce::Colour (down ? 0xff26292d : 0xff3c4046), bounds.getX(), bounds.getY(),
                               juce::Colour (down ? 0xff303338 : 0xff26292d), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (bounds, corner);

    if (on)
    {
        g.setColour (led.withAlpha (0.22f));
        g.fillRoundedRectangle (bounds, corner);
        g.setColour (led);
        g.drawRoundedRectangle (bounds.reduced (0.5f), corner, 1.6f);
    }
    else
    {
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawRoundedRectangle (bounds, corner, 1.0f);
    }

    if (highlighted && ! down)
    {
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRoundedRectangle (bounds, corner);
    }
}

void LooperLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    const bool on = button.getToggleState();
    const auto led = button.getProperties().contains ("ledColour")
                         ? juce::Colour ((juce::uint32) (juce::int64) button.getProperties()["ledColour"])
                         : colours::orange;
    g.setFont (getTextButtonFont (button, button.getHeight()));
    g.setColour (on ? led.brighter (0.4f) : (button.isEnabled() ? colours::text : colours::textDim));
    g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (3, 1), juce::Justification::centred, 2, 0.8f);
}

juce::Font LooperLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return font (juce::jlimit (9.0f, 15.0f, (float) buttonHeight * 0.42f), true);
}

void LooperLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    juce::Path arrow;
    const float ax = (float) width - 12.0f, ay = (float) height * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

juce::Font LooperLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return font (juce::jlimit (10.0f, 15.0f, (float) box.getHeight() * 0.5f), true);
}

juce::Font LooperLookAndFeel::getPopupMenuFont()
{
    return font (14.0f);
}

juce::Label* LooperLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (slider);
    l->setFont (font (11.0f, true));
    l->setJustificationType (juce::Justification::centred);
    return l;
}
} // namespace livelooper
