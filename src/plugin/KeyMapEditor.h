#pragma once

#include "Widgets.h"

namespace livelooper
{
/**
    The keyboard mapping window (standalone app): every function with its key.
    Click a function and press the new key; Esc cancels, right-click removes the key.
*/
class KeyMapEditor : public juce::Component,
                     private juce::ListBoxModel
{
public:
    explicit KeyMapEditor (LiveLooperProcessor& p);
    ~KeyMapEditor() override;

    /** Opens the window centred on the editor. */
    static void show (LiveLooperProcessor& p, juce::Component& centreAround);

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

    // for tests
    void startCapture (int row);
    int getCaptureRow() const noexcept { return captureRow; }
    const juce::StringArray& getActions() const noexcept { return actions; }

private:
    int getNumRows() override { return actions.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent& e) override;
    void refresh();

    LooperLookAndFeel lookAndFeel;
    LiveLooperProcessor& proc;
    juce::StringArray actions;
    juce::ListBox list { {}, this };
    LLButton enableBtn { "KEYBOARD CONTROL", colours::green }, defaultsBtn { "DEFAULTS", colours::cyan },
             clearBtn { "REMOVE ALL", colours::red };
    int captureRow = -1;
    juce::String info;
};
} // namespace livelooper
