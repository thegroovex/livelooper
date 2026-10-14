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

/** v1.5: a button that holds a (momentary) parameter on while it is pressed - beat FX hold,
    beat repeat / stutter hold. The button lights while the parameter is on (also via MIDI). */
class MomentaryButton : public LLButton, private juce::Timer
{
public:
    MomentaryButton (APVTS& state, const juce::String& paramId, const juce::String& text, juce::Colour led);
    ~MomentaryButton() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    void timerCallback() override;
    void set (bool on);
    juce::RangedAudioParameter* param = nullptr;
    bool down = false;
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
    // for tests
    void simulateLongPressForTests() { down = true; cleared = true; }
    bool isShowingCleared() const noexcept { return cleared; }

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
/** Meter ballistics for the UI: fast attack, about 26 dB/s release, 1.2 s peak hold. */
struct MeterBallistics
{
    float level = 0.0f, hold = 0.0f;
    int holdFrames = 0;
    void push (float peak, float frameRateHz = 30.0f) noexcept;
    /** 0..1 position of a linear gain on the -60..+6 dB scale. */
    static float toPosition (float gain) noexcept;
};

/** Draws a vertical or horizontal stereo meter (green, yellow from -12 dB, orange from -3 dB, red from 0 dBFS). */
void drawMeterBar (juce::Graphics& g, juce::Rectangle<float> area, const MeterBallistics& m, bool vertical, juce::Colour base);

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

    /** Master output meter in the top row of the display (peaks since the last frame). */
    void pushMasterMeter (float peakL, float peakR);
    const MeterBallistics& getMasterMeter (int ch) const noexcept { return master[(size_t) (ch & 1)]; }

private:
    std::array<MeterBallistics, 2> master;
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
class TrackStrip : public juce::Component,
                   public juce::SettableTooltipClient
{
public:
    TrackStrip (LiveLooperProcessor& p, int track);
    void resized() override;
    void paint (juce::Graphics& g) override;
    // drag the waveform out of the plug-in (e.g. onto an audio track in Live)
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    juce::String getTooltip() override;
    bool canDragOut() const noexcept { return waveValid && proc.getEngine().getTrack (track).getLength() > 0; }
    void refresh (bool blinkPhase);
    int getTrackIndex() const noexcept { return track; }
    StopButton& getStopButton() noexcept { return stop; }
    static constexpr int kWaveBins = 96;
    const std::array<float, kWaveBins>& getWaveform() const noexcept { return wave; }
    bool hasWaveform() const noexcept { return waveValid; }
    const MeterBallistics& getMeter (int ch) const noexcept { return meter[(size_t) (ch & 1)]; }
    bool isMeteringInput() const noexcept { return meterInput; }

private:
    LiveLooperProcessor& proc;
    int track;
    juce::Rectangle<int> lcdArea;

    LLButton reverse { "REV", colours::cyan }, oneShot { "1SHOT", colours::cyan }, fx { "FX", colours::orange };
    LLButton undo { "UNDO", colours::yellow }, redo { "REDO", colours::yellow }, settings { "SETUP", colours::cyan };
    LLSlider level { juce::Slider::LinearVertical, colours::green };
    LLSlider pan { juce::Slider::RotaryHorizontalVerticalDrag, colours::cyan };
    LLSlider color { juce::Slider::RotaryHorizontalVerticalDrag, colours::blue };
    RecPlayButton recPlay;
    StopButton stop;

    std::unique_ptr<APVTS::ButtonAttachment> reverseAtt, oneShotAtt, fxAtt;
    std::unique_ptr<APVTS::SliderAttachment> levelAtt, panAtt, colorAtt;

    std::array<float, kWaveBins> wave {};
    bool waveValid = false;
    bool dragOutStarted = false;
    int waveCounter = 0;
    std::array<MeterBallistics, 2> meter;
    bool meterInput = false;
    bool blink = false;
    juce::Rectangle<int> meterArea, ledArea;
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
    // v1.3
    std::unique_ptr<Knob> feedback, duck, pitch;
    LLButton stretch { "FOLLOW TEMPO", colours::cyan }, capture { "CAPTURE", colours::red };
    std::unique_ptr<APVTS::ButtonAttachment> stretchAtt;
};
} // namespace livelooper
