#pragma once

#include "Widgets.h"

namespace livelooper
{
/**
    v1.6 welcome screen: the livelooper logo (brand gradient, Manrope, "designed by groovex") and the
    choice how to start - LOAD DEMO SET or START WITH EMPTY SET - with a short note that the demo set
    replaces the current set and that an empty set can be loaded any time (SETUP > Empty set).

    Shown once on the very first start (the guided tour follows the choice) and any time from
    SETUP > "Welcome / demo set ...". It is an overlay on the editor surface; the editor decides
    what the buttons do (it asks before the demo replaces a set that holds loops).
*/
class WelcomeScreen : public juce::Component
{
public:
    explicit WelcomeScreen (bool firstStart);

    std::function<void()> onDemo, onEmpty, onClose;

    bool isFirstStart() const noexcept { return firstStart; }
    /** The centred card (layout + tests). */
    juce::Rectangle<int> getCardBounds() const;
    juce::Button& getDemoButton() noexcept  { return demoBtn; }
    juce::Button& getEmptyButton() noexcept { return emptyBtn; }
    juce::Button& getCloseButton() noexcept { return closeBtn; }

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override {}      // the surface below stays untouched

    /** Manrope (embedded) - 800 for the logo, 600 for the rest. */
    static juce::Font brandFont (float height, bool extraBold);

private:
    bool firstStart;
    std::unique_ptr<juce::Drawable> icon;
    juce::Image heroImage;                   // resources/help/welcome-hero.png (marketing style)
    juce::Rectangle<int> heroBounds() const;
    LLButton demoBtn { "LOAD DEMO SET", colours::green }, emptyBtn { "START WITH EMPTY SET", colours::cyan },
             closeBtn { "CLOSE", colours::textDim };
};
} // namespace livelooper
