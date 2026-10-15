#include "WelcomeScreen.h"

#include <BinaryData.h>
#include "InfoWindows.h"

namespace livelooper
{
using i18n::tr;

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
    const int w = juce::jmin (680, getWidth() - 24), h = juce::jmin (540, getHeight() - 24);
    return getLocalBounds().withSizeKeepingCentre (w, h);
}

juce::Rectangle<int> WelcomeScreen::heroBounds() const
{
    const auto card = getCardBounds();
    return card.withHeight (juce::roundToInt ((float) card.getWidth() * 0.3f));
}

void WelcomeScreen::resized()
{
    const auto card = getCardBounds();
    closeBtn.setBounds (card.getRight() - 16 - 92, card.getY() + 14, 92, 28);
    auto inner = card.reduced (32, 0).withTrimmedTop (heroBounds().getHeight() + 20 + 34 + 58 + 12);
    auto buttons = inner.removeFromTop (50);
    const int gap = 16, bw = (buttons.getWidth() - gap) / 2;
    demoBtn.setBounds (buttons.removeFromLeft (bw));
    buttons.removeFromLeft (gap);
    emptyBtn.setBounds (buttons);
    LooperLookAndFeel::setEmphasis (demoBtn, 2);
    LooperLookAndFeel::setEmphasis (closeBtn, 0);
    demoBtn.setToggleState (true, juce::dontSendNotification);      // the inviting choice is lit
}

void WelcomeScreen::paint (juce::Graphics& g)
{
    g.setColour (juce::Colours::black.withAlpha (0.66f));
    g.fillAll();

    const auto cardI = getCardBounds();
    const auto card = cardI.toFloat();
    LooperLookAndFeel::drawSoftShadow (g, card, 22.0f, 1.0f, 18.0f, 12.0f);
    g.setColour (juce::Colour (0xfa17171c));
    g.fillRoundedRectangle (card, 22.0f);

    // hero picture (same style as the marketing material), clipped to the card's top corners
    const auto hero = heroBounds().toFloat();
    if (heroImage.isNull())
        heroImage = getEmbeddedImage ("welcome-hero.png");
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip;
        clip.addRoundedRectangle (hero.getX(), hero.getY(), hero.getWidth(), hero.getHeight(), 22.0f, 22.0f, true, true, false, false);
        g.reduceClipRegion (clip);
        g.setOpacity (1.0f);
        if (heroImage.isValid())
            g.drawImage (heroImage, hero, juce::RectanglePlacement::fillDestination);
        juce::ColourGradient fade (juce::Colour (0x0017171c), 0.0f, hero.getY() + hero.getHeight() * 0.35f, juce::Colour (0xff17171c), 0.0f, hero.getBottom(), false);
        g.setGradientFill (fade);
        g.fillRect (hero);
    }
    g.setGradientFill (LooperLookAndFeel::brandGradient (card, 0.9f));
    g.drawRoundedRectangle (card.reduced (0.75f), 22.0f, 1.5f);

    // logo on the picture: icon + "livelooper" in the brand gradient, "designed by groovex"
    {
        const int iconSize = 58;
        const auto iconArea = juce::Rectangle<float> (hero.getX() + 32.0f, hero.getBottom() - iconSize - 6.0f, (float) iconSize, (float) iconSize);
        if (icon != nullptr)
            icon->drawWithin (g, iconArea, juce::RectanglePlacement::centred, 1.0f);
        const auto f = brandFont (42.0f, true);
        const auto word = juce::Rectangle<float> (iconArea.getRight() + 14.0f, iconArea.getY() - 6.0f, juce::GlyphArrangement::getStringWidth (f, "livelooper") + 4.0f, 46.0f);
        g.setGradientFill (LooperLookAndFeel::brandGradient (word));
        g.setFont (f);
        g.drawText ("livelooper", word, juce::Justification::centredLeft, false);
        g.setColour (colours::text.withAlpha (0.7f));
        g.setFont (brandFont (13.0f, false));
        g.drawText ("designed by groovex", word.translated (2.0f, 40.0f).withHeight (18.0f).withWidth (240.0f), juce::Justification::centredLeft, false);
    }

    auto inner = cardI.reduced (32, 0).withTrimmedTop (heroBounds().getHeight() + 20);
    g.setColour (colours::text);
    g.setFont (brandFont (23.0f, true));
    g.drawText (firstStart ? tr ("Welcome! How do you want to start?") : tr ("Demo set or empty set?"),
                inner.removeFromTop (34), juce::Justification::centredLeft);
    g.setColour (colours::text.withAlpha (0.78f));
    g.setFont (brandFont (14.5f, false));
    g.drawFittedText (tr ("The demo set has a groove and three loops - press ALL START / STOP and you hear music. "
                          "The empty set is a blank looper for your own first loop."),
                      inner.removeFromTop (58), juce::Justification::centredLeft, 3, 1.0f);

    // under the buttons: the note about replacing the set (and the muted input of the demo)
    auto note = inner.withTrimmedTop (12 + 50 + 16);
    auto warn = note.removeFromTop (72).toFloat();
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.fillRoundedRectangle (warn, 12.0f);
    g.setColour (juce::Colours::white.withAlpha (0.09f));
    g.drawRoundedRectangle (warn.reduced (0.5f), 12.0f, 1.0f);
    auto iconCol = warn.removeFromLeft (40.0f);
    g.setColour (colours::orange);
    g.fillEllipse (iconCol.withSizeKeepingCentre (20.0f, 20.0f));
    g.setColour (juce::Colours::black);
    g.setFont (brandFont (14.0f, true));
    g.drawText ("!", iconCol, juce::Justification::centred, false);
    g.setColour (colours::text.withAlpha (0.85f));
    g.setFont (brandFont (12.5f, false));
    g.drawFittedText (tr ("The demo set replaces the current set - loops and settings. "
                          "An empty set can be loaded any time: SETUP > Empty set (clear everything).")
                          + "\n" + tr ("The demo set starts with the input muted (no feedback) - turn up INPUT in MASTER to hear your microphone."),
                      warn.reduced (4.0f, 6.0f).toNearestInt(), juce::Justification::centredLeft, 4, 0.9f);
    if (firstStart)
    {
        g.setColour (colours::textDim);
        g.setFont (brandFont (12.0f, false));
        g.drawText (tr ("After your choice a short tour shows the five things to try first."),
                    note.withTrimmedTop (8).withHeight (18), juce::Justification::centred, false);
    }
}
} // namespace livelooper
