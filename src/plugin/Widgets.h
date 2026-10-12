#pragma once

#include "LooperLookAndFeel.h"
#include "PluginProcessor.h"
#include "Localisation.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace livelooper
{
using APVTS = juce::AudioProcessorValueTreeState;

/** Implemented by the editor: shows the MIDI learn menu for a control. */
struct MidiMenuHost
{
    virtual ~MidiMenuHost() = default;
    virtual void showMidiMenuFor (juce::Component& c) = 0;
    virtual void exportTrack (int track) = 0;
    virtual void importTrack (int track) = 0;
    virtual void showTrackSettings (int track, juce::Component& anchor) = 0;
};

/** Marks a component as the UI of a parameter (used by MIDI learn). */
void setParamId (juce::Component& c, const juce::String& paramId);
juce::String getParamId (const juce::Component& c);
/** Walks up from c to the first component that carries a parameter id. */
juce::Component* findMappable (juce::Component* c);
/** Right-click handling shared by all mappable controls; returns true if consumed. */
bool handleMidiRightClick (juce::Component& c, const juce::MouseEvent& e);

//==============================================================================
class LLSlider : public juce::Slider
{
public:
    LLSlider (juce::Slider::SliderStyle style, juce::Colour accent);
    void mouseDown (const juce::MouseEvent& e) override;
    /** Without an explicit tooltip the (localised) name of the bound parameter is shown. */
    juce::String getTooltip() override;
};

class LLButton : public juce::TextButton
{
public:
    /** text is English; the look-and-feel draws it in the current UI language. */
    explicit LLButton (const juce::String& text, juce::Colour led = colours::orange);
    void mouseDown (const juce::MouseEvent& e) override;
    juce::String getTooltip() override { return tooltipFn ? tooltipFn() : juce::TextButton::getTooltip(); }

    /** Optional: computes the tooltip on demand (for texts with numbers / names in them). */
    std::function<juce::String()> tooltipFn;
};

/** Combo box whose items are given in English and shown in the current UI language
    (switches live when the language changes). Item ids are 1..n. */
class LLCombo : public juce::ComboBox, private i18n::Listener
{
public:
    LLCombo();
    ~LLCombo() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void setEnglishItems (const juce::StringArray& items);

private:
    void languageChanged() override;
    juce::StringArray englishItems;
};

/** A labelled knob: knob + caption underneath. */
class Knob : public juce::Component
{
public:
    Knob (APVTS& state, const juce::String& paramId, const juce::String& caption, juce::Colour accent = colours::cyan);
    void setCaption (const juce::String& c) { caption = c; repaint(); }
    void rebind (APVTS& state, const juce::String& paramId);
    void resized() override;
    void paint (juce::Graphics& g) override;

    LLSlider slider;

private:
    juce::String caption;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

//==============================================================================
/** The large round REC/PLAY pad with LED ring, colour by track state. */
class RecPlayButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    RecPlayButton (LiveLooperProcessor& p, int track);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void setBlinkPhase (bool on) { blink = on; }

private:
    LiveLooperProcessor& proc;
    int track;
    bool blink = false;
    bool pressed = false;
    void mouseUp (const juce::MouseEvent&) override { pressed = false; repaint(); }
};

/** STOP pad: press = stop, hold for 1.5 s = clear the track. */
class StopButton : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    StopButton (LiveLooperProcessor& p, int track);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    /** Progress (0..1) of a clear held on the computer keyboard; 0 = none. */
    void setKeyHoldProgress (float p) { if (p != keyHold) { keyHold = p; repaint(); } }
    float getKeyHoldProgress() const noexcept { return keyHold; }

private:
    void timerCallback() override;
    LiveLooperProcessor& proc;
    int track;
    float keyHold = 0.0f;
    bool down = false;
    bool cleared = false;
    juce::uint32 downTime = 0;
};

//==============================================================================
/** Display with several text lines in hardware LCD style. */
class Lcd : public juce::Component, public juce::SettableTooltipClient
{
public:
    void setContent (const juce::String& big, const juce::StringArray& small, juce::Colour accent, bool midiActivity);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    juce::String getTooltip() override;

    /** Area of the big tempo number; a click there calls onTempoClicked (type in the BPM). */
    juce::Rectangle<int> getTempoArea() const;
    std::function<void()> onTempoClicked;

private:
    juce::String bigText;
    juce::StringArray lines;
    juce::Colour accentColour = colours::lcdText;
    bool midi = false;
};

//==============================================================================
/** One FX slot (A-D): on/off pad, and the type name which selects the slot for editing. */
class FxSlot : public juce::Component
{
public:
    FxSlot (APVTS& state, bool inputFx, int slot, std::function<void (bool, int)> onSelect);
    void resized() override;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void setSelected (bool s) { selected = s; repaint(); }

    LLButton onButton;

private:
    APVTS& state;
    bool input;
    int slot;
    bool selected = false;
    std::function<void (bool, int)> onSelect;
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

//==============================================================================
/** One of the five track strips. */
class TrackStrip : public juce::Component
{
public:
    TrackStrip (LiveLooperProcessor& p, int track);
    void resized() override;
    void paint (juce::Graphics& g) override;
    void refresh (bool blinkPhase);
    int getTrackIndex() const noexcept { return track; }
    StopButton& getStopButton() noexcept { return stop; }

private:
    LiveLooperProcessor& proc;
    int track;
    juce::Rectangle<int> lcdArea;

    LLButton reverse { "REV", colours::cyan }, oneShot { "1SHOT", colours::cyan }, fx { "FX", colours::orange };
    LLButton undo { "UNDO", colours::yellow }, redo { "REDO", colours::yellow }, settings { "SETUP", colours::cyan };
    LLSlider level { juce::Slider::LinearVertical, colours::green };
    LLSlider pan { juce::Slider::RotaryHorizontalVerticalDrag, colours::cyan };
    RecPlayButton recPlay;
    StopButton stop;

    std::unique_ptr<APVTS::ButtonAttachment> reverseAtt, oneShotAtt, fxAtt;
    std::unique_ptr<APVTS::SliderAttachment> levelAtt, panAtt;
};

//==============================================================================
/** Per-track settings shown in a call-out box. */
class TrackSettingsPanel : public juce::Component
{
public:
    TrackSettingsPanel (LiveLooperProcessor& p, MidiMenuHost& host, int track);
    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    struct Row
    {
        juce::String caption;
        std::unique_ptr<LLCombo> combo;
        std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
    };

    LiveLooperProcessor& proc;
    int track;
    std::vector<Row> rows;
    LLButton loopSync { "LOOP SYNC", colours::green };
    LLButton clear { "CLEAR", colours::red }, exportBtn { "EXPORT WAV" }, importBtn { "IMPORT" };
    std::unique_ptr<APVTS::ButtonAttachment> loopSyncAtt;
};
} // namespace livelooper
