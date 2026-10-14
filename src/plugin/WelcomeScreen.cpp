#include "WelcomeScreen.h"

#include <BinaryData.h>

namespace livelooper
{
using i18n::tr;

namespace
{
    // brand gradient #FF7A18 -> #FF2D6F -> #B14BFF (docs/brand)
    juce::ColourGradient brandGradient (juce::Rectangle<float> r)
    {
        juce::ColourGradient grad (juce::Colour (0xffff7a18), r.getX(), r.getY(), juce::Colour (0xffb14bff), r.getRight(), r.getBottom(), false);
        grad.addColour (0.55, juce::Colour (0xffff2d6f));
        return grad;
    }
}

juce::Font WelcomeScreen::brandFont (float height, bool extraBold)
{
    static juce::Typeface::Ptr bold = juce::Typeface::createSystemTypefaceFor (BinaryData::Manrope800_ttf, (size_t) BinaryData::Manrope800_ttfSize);
    static juce::Typeface::Ptr semi = juce::Typeface::createSystemTypefaceFor (BinaryData::Manrope600_ttf, (size_t) BinaryData::Manrope600_ttfSize);
    auto tf = extraBold ? bold : semi;
    if (tf == nullptr)
        return LooperLookAndFeel::font (height, extraBold);
    return juce::Font (juce::FontOptions (tf).withHeight (height));
}

WelcomeScreen::WelcomeScreen (bool first) : firstStart (first)
{
    setInterceptsMouseClicks (true, true);
    icon = juce::Drawable::createFromImageData (BinaryData::beattacticsicon_svg, (size_t) BinaryData::beattacticsicon_svgSize);
    for (auto* b : { &demoBtn, &emptyBtn, &closeBtn })
        addAndMakeVisible (b);
    demoBtn.onClick  = [this] { if (onDemo) onDemo(); };
    emptyBtn.onClick = [this] { if (onEmpty) onEmpty(); };
    closeBtn.onClick = [this] { if (onClose) onClose(); };
    demoBtn.setTooltip ("A groove and three loops, ready to play - replaces the current set");
    emptyBtn.setTooltip ("A blank looper: every loop cleared, every setting back to its default");
}

juce::Rectangle<int> WelcomeScreen::getCardBounds() const
{
    const int w = juce::jmin (640, getWidth() - 24), h = juce::jmin (430, getHeight() - 24);
    return getLocalBounds().withSizeKeepingCentre (w, h);
}

void WelcomeScreen::resized()
{
    auto card = getCardBounds().reduced (28);
    closeBtn.setBounds (card.getRight() - 84, card.getY() - 12, 84, 26);
    auto buttons = card.withTrimmedTop (238).withHeight (46);
    const int gap = 16, bw = (buttons.getWidth() - gap) / 2;
    demoBtn.setBounds (buttons.removeFromLeft (bw));
    buttons.removeFromLeft (gap);
    emptyBtn.setBounds (buttons);
}

void WelcomeScreen::paint (juce::Graphics& g)
{
    g.setColour (juce::Colours::black.withAlpha (0.70f));
    g.fillAll();

    const auto cardI = getCardBounds();
    const auto card = cardI.toFloat();
    g.setColour (juce::Colour (0xf5181a1e));
    g.fillRoundedRectangle (card, 16.0f);
    g.setGradientFill (brandGradient (card));
    g.drawRoundedRectangle (card.reduced (0.75f), 16.0f, 1.5f);

    auto inner = cardI.reduced (28);
    // logo: icon + "livelooper" in the brand gradient, "designed by groovex"
    auto logoRow = inner.removeFromTop (96);
    const int iconSize = 88;
    const auto textW = juce::GlyphArrangement::getStringWidth (brandFont (52.0f, true), "livelooper");
    const int totalW = iconSize + 18 + (int) std::ceil (textW);
    auto logo = logoRow.withSizeKeepingCentre (totalW, 96);
    const auto iconArea = logo.removeFromLeft (iconSize).withSizeKeepingCentre (iconSize, iconSize).toFloat();
    if (icon != nullptr)
        icon->drawWithin (g, iconArea, juce::RectanglePlacement::centred, 1.0f);
    logo.removeFromLeft (18);
    const auto wordArea = logo.withTrimmedBottom (26).toFloat();
    g.setGradientFill (brandGradient (wordArea));
    g.setFont (brandFont (52.0f, true));
    g.drawText ("livelooper", wordArea, juce::Justification::bottomLeft, false);
    g.setColour (colours::textDim);
    g.setFont (brandFont (14.0f, false));
    g.drawText ("designed by groovex", logo.withTrimmedTop (70).toFloat().translated (3.0f, 0.0f), juce::Justification::topLeft, false);

    inner.removeFromTop (22);
    g.setColour (colours::text);
    g.setFont (brandFont (22.0f, true));
    g.drawText (firstStart ? tr ("Welcome! How do you want to start?") : tr ("Demo set or empty set?"),
                inner.removeFromTop (32), juce::Justification::centred);
    g.setColour (colours::text.withAlpha (0.82f));
    g.setFont (brandFont (14.5f, false));
    g.drawFittedText (tr ("The demo set has a groove and three loops - press ALL START / STOP and you hear music. "
                          "The empty set is a blank looper for your own first loop."),
                      inner.removeFromTop (60).reduced (12, 0), juce::Justification::centred, 3, 1.0f);

    // under the buttons: the note about replacing the set
    auto note = getCardBounds().reduced (28).withTrimmedTop (238 + 46 + 16);
    auto warn = note.removeFromTop (54).toFloat();
    g.setColour (colours::orange.withAlpha (0.10f));
    g.fillRoundedRectangle (warn, 8.0f);
    g.setColour (colours::orange.withAlpha (0.55f));
    g.drawRoundedRectangle (warn.reduced (0.5f), 8.0f, 1.0f);
    g.setColour (colours::orange);
    g.setFont (brandFont (13.5f, true));
    g.drawText ("!", warn.removeFromLeft (30.0f), juce::Justification::centred, false);
    g.setColour (colours::text);
    g.setFont (brandFont (13.0f, false));
    g.drawFittedText (tr ("The demo set replaces the current set - loops and settings. "
                          "An empty set can be loaded any time: SETUP > Empty set (clear everything)."),
                      warn.reduced (4.0f, 4.0f).toNearestInt(), juce::Justification::centredLeft, 3, 1.0f);
    if (firstStart)
    {
        g.setColour (colours::textDim);
        g.setFont (brandFont (12.0f, false));
        g.drawText (tr ("After your choice a short tour shows the five things to try first."),
                    note.withTrimmedTop (6).withHeight (18), juce::Justification::centred, false);
    }
}
} // namespace livelooper
