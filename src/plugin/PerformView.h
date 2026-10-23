#pragma once

#include "LiveView.h"

namespace livelooper
{
/**
    v1.7 Performance view: the FX-focused stage view (4th choice of the host parameter View).

    A compact track row on top (state word, loop length, mini waveform only when the global waveforms
    setting is on, REC / PLAY / DUB pad, level, STOP) and a large PERFORMANCE FX area: punch FX target,
    the XY field of the punch FX with the punch amount and LATCH, the performance filter (bipolar,
    CENTER), beat FX (on / off, type, beat, level, hold), REPEAT / STUTTER / FILL with the repeat rate,
    the 12 punch FX pads and MASTER (level, meter, ALL START / STOP, CAPTURE). Header: logo, view
    switcher, transport display, TAP and EXIT (Esc).

    Every control is an existing function (host parameter) and MIDI-learnable with a right-click; no
    DSP of its own. Shown over the full surface or in its own fullscreen window. Lays itself out for
    any size; designed at 1180 x 856.
*/
class PerformView : public juce::Component
{
public:
    PerformView (LiveLooperProcessor& p, std::function<TrackStrip*(int)> strips);
    ~PerformView() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    /** Called by the editor's timer (30 Hz). */
    void refresh (bool blinkPhase);
    bool keyPressed (const juce::KeyPress& key) override;

    /** EXIT / Esc. */
    std::function<void()> onExit;
    /** The editor's selected punch FX (XY field, amount bar) and selecting one (a pad press). */
    std::function<int()> selectedPunch;
    std::function<void (int)> onSelectPunch;
    /** Live punch amount while a pad is held (hold ramp / pressure), -1 = none. */
    std::function<float()> livePunchAmount;
    /** Main output meter (0 .. 1 meter positions, left / right). */
    std::function<void (float&, float&)> masterMeter;

    // for tests / screenshots
    int getNumCards() const noexcept { return numCards; }
    juce::Rectangle<int> getCardArea (int track) const { return juce::isPositiveAndBelow (track, kNumTracks) ? cards[(size_t) track] : juce::Rectangle<int>(); }
    juce::Rectangle<int> getFxArea() const noexcept { return fxArea; }
    ChoiceChips& getViewSwitcher() noexcept { return *viewChips; }
    ChoiceChips& getTargetChips() noexcept { return *targetChips; }
    ChoiceChips& getRepeatRateChips() noexcept { return *rateChips; }
    ChoiceChips& getBeatTypeChips() noexcept { return *bfxTypeChips; }
    ChoiceChips& getBeatChips() noexcept { return *bfxBeatChips; }
    StagePad& getPunchPad (int i) noexcept { return *punchPads[(size_t) juce::jlimit (0, kNumPunchFx - 1, i)]; }
    StagePad& getRepeatPad() noexcept { return *repeatPad; }
    StagePad& getBeatFxPad() noexcept { return *bfxPad; }
    StagePad& getExitButton() noexcept { return *exitPad; }
    juce::Component& getXYPad() noexcept;
    juce::Button& getCenterButton() noexcept { return centerBtn; }
    /** True when the compact cards draw mini waveforms (only with the global waveforms setting on). */
    bool areWaveformsShown() const noexcept;

    /** Short pad labels of the 12 punch FX (English). */
    static const char* punchLabel (int index) noexcept;

private:
    class XYPad;
    class AxisSlider;
    class AmountBar;

    struct Card
    {
        std::unique_ptr<RecPlayButton> pad;
        std::unique_ptr<StopButton> stop;
        std::unique_ptr<LiveView::LevelBar> level;
        std::unique_ptr<APVTS::SliderAttachment> levelAtt;
        std::unique_ptr<LoopLengthPill> length;
        juce::Rectangle<int> display, levelCaption, title;
    };

    void paintCard (juce::Graphics& g, int t);
    void paintSection (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, juce::Colour mark, const juce::String& right = {});
    juce::String beatFxText() const;
    int shownPunch() const;

    LiveLooperProcessor& proc;
    std::function<TrackStrip*(int)> stripFn;
    std::array<Card, kNumTracks> cardParts;
    std::array<juce::Rectangle<int>, kNumTracks> cards;
    int numCards = 0;

    juce::Rectangle<int> headArea, transportArea, hintArea, fxArea, xyArea, amountCaption, hintBottom,
                         filterArea, filterCaption, beatArea, rateCaption, punchCaption, masterArea, meterArea, targetCaption;

    std::unique_ptr<ChoiceChips> viewChips, targetChips, rateChips, bfxTypeChips, bfxBeatChips;
    std::unique_ptr<StagePad> exitPad, tapPad, repeatPad, stutterPad, fillPad, bfxPad, bfxHoldPad, allPad, capturePad;
    std::array<std::unique_ptr<StagePad>, kNumPunchFx> punchPads;
    std::unique_ptr<XYPad> xyPad;
    std::unique_ptr<AxisSlider> xAxis, yAxis;
    std::unique_ptr<AmountBar> amountBar;
    std::unique_ptr<APVTS::SliderAttachment> xAtt, yAtt, amountAtt, filterAtt;
    LLButton latchBtn { "LATCH", colours::yellow }, centerBtn { "CENTER", colours::cyan };
    std::unique_ptr<APVTS::ButtonAttachment> latchAtt;
    LLSlider filterKnob { juce::Slider::RotaryHorizontalVerticalDrag, colours::cyan };
    std::unique_ptr<Knob> bfxLevelKnob, masterKnob;
    float meterL = 0.0f, meterR = 0.0f;
    int shownTheme = -1;
    bool shownWaves = false;
    juce::String shownMaster, shownDepth;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PerformView)
};
} // namespace livelooper
