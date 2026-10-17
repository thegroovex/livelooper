#pragma once

#include "Widgets.h"

namespace livelooper
{
/**
    v1.7 Live view: the stage surface. One large card per visible loop track (state word, bars, loop
    position, waveform with play head, the big REC / PLAY / DUB pad, level, STOP), a transport display
    (tempo, bar . beat, beat dots) with TAP and EXIT LIVE on top, and an optional PERFORMANCE FX strip
    at the bottom (filter, beat FX, punch FX, master, echo / repeat / stutter / fill, punch FX amount,
    ALL START / STOP, CAPTURE).

    Shown over the full surface (host parameter "Live View", the LIVE button in the header, Esc to
    leave) or in its own fullscreen window. Lays itself out for any size; designed at 1180 x 856.
*/
class LiveView : public juce::Component
{
public:
    LiveView (LiveLooperProcessor& p, std::function<TrackStrip*(int)> strips);
    ~LiveView() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    /** Called by the editor's timer (30 Hz). */
    void refresh (bool blinkPhase);

    /** Waveforms in the track displays of the Live view (SETUP > Display > Live view, default on). */
    void setShowWaveforms (bool on);
    bool getShowWaveforms() const noexcept { return waveforms; }
    /** The PERFORMANCE FX strip at the bottom (default on). */
    void setShowFxStrip (bool on);
    bool getShowFxStrip() const noexcept { return fxStrip; }

    /** EXIT LIVE / Esc. */
    std::function<void()> onExit;
    /** The punch FX the amount bar names when none is held (the editor's selected punch pad). */
    std::function<int()> selectedPunch;
    /** Live punch amount while a pad is held (hold ramp / pressure), -1 = none. */
    std::function<float()> livePunchAmount;

    bool keyPressed (const juce::KeyPress& key) override;

    // for tests / screenshots
    juce::Rectangle<int> getCardArea (int track) const { return juce::isPositiveAndBelow (track, (int) cards.size()) ? cards[(size_t) track] : juce::Rectangle<int>(); }
    juce::Rectangle<int> getFxStripArea() const noexcept { return footArea; }
    int getNumCards() const noexcept { return numCards; }
    LLButton& getExitButton() noexcept { return exitBtn; }
    /** v1.7 the loop length pill of a card (shown while the track is empty or recording its first loop). */
    LoopLengthPill* getLengthPill (int track) const { return juce::isPositiveAndBelow (track, kNumTracks) ? cardParts[(size_t) track].length.get() : nullptr; }
    juce::String getBeatFxButtonText() const { return bfxBtn.getButtonText(); }

private:
    /** Horizontal level fader drawn as a meter with a level mark (MIDI learn on right-click). */
    class LevelBar : public LLSlider
    {
    public:
        LevelBar() : LLSlider (juce::Slider::LinearHorizontal, colours::green) {}
        void paint (juce::Graphics& g) override;
        float meter = 0.0f;
    };

    struct Card
    {
        std::unique_ptr<RecPlayButton> pad;
        std::unique_ptr<StopButton> stop;
        std::unique_ptr<LevelBar> level;
        std::unique_ptr<APVTS::SliderAttachment> levelAtt;
        std::unique_ptr<LoopLengthPill> length;     // v1.7: shown while the track is empty / recording
        juce::Rectangle<int> display, levelCaption;
    };

    void paintCard (juce::Graphics& g, int t);
    void paintTransport (juce::Graphics& g);
    void paintFxStrip (juce::Graphics& g);
    juce::String beatFxText() const;

    LiveLooperProcessor& proc;
    std::function<TrackStrip*(int)> stripFn;
    std::array<Card, kNumTracks> cardParts;
    std::array<juce::Rectangle<int>, kNumTracks> cards;
    int numCards = 0;
    bool waveforms = true, fxStrip = true;
    juce::Rectangle<int> headArea, transportArea, footArea, punchBar;

    LLButton exitBtn { "EXIT LIVE", colours::cyan }, tapBtn { "TAP", colours::yellow };
    LLButton allBtn { "ALL START / STOP", colours::green }, captureBtn { "CAPTURE", colours::red };
    LLButton bfxBtn { "ECHO", colours::orange };
    std::unique_ptr<APVTS::ButtonAttachment> bfxAtt;
    std::unique_ptr<MomentaryButton> repeatBtn, stutterBtn, fillBtn;
    std::unique_ptr<Knob> filterKnob, beatKnob, punchKnob, masterKnob;
    juce::String shownBfx;
    int shownTheme = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveView)
};

/** The small LIVE button in the header (next to the logo): a pill in the brand gradient. */
class LiveButton : public juce::Button
{
public:
    LiveButton() : juce::Button ("LIVE") {}
    void paintButton (juce::Graphics& g, bool over, bool down) override;
    void mouseDown (const juce::MouseEvent& e) override;
};
} // namespace livelooper
