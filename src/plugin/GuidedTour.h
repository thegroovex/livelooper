#pragma once

#include "Widgets.h"

namespace livelooper
{
/**
    v1.6 guided tour: a 60-second overlay with five steps (Record, Overdub, Groove, Beat FX,
    Scene). Each step dims the surface, highlights one area and explains it in two or three
    sentences. NEXT / BACK / SKIP; shown once after the first start, always available from
    the help button (?) and SETUP.
*/
class GuidedTour : public juce::Component
{
public:
    struct Step
    {
        juce::String title;     // English, translated when drawn
        juce::String text;      // English, translated when drawn
        std::function<juce::Rectangle<int>()> area;   // highlighted area in the tour's coordinates
        std::function<void()> enter {};                // optional: prepares the surface for this step (e.g. opens an FX tab)
    };

    GuidedTour (std::vector<Step> steps, std::function<void()> onClose);

    int  getStep() const noexcept       { return step; }
    int  getNumSteps() const noexcept   { return (int) steps.size(); }
    void next();
    void back();
    void close();

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override {}      // the surface below stays untouched

    /** The card with text and buttons (for the layout and tests). */
    juce::Rectangle<int> getCardBounds() const;

private:
    void update();
    std::vector<Step> steps;
    std::function<void()> onClose;
    int step = 0;
    LLButton nextBtn { "NEXT", colours::green }, backBtn { "BACK", colours::cyan }, skipBtn { "SKIP TOUR", colours::red };
};
} // namespace livelooper
