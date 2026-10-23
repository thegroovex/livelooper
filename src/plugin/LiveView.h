#pragma once

#include "Widgets.h"

namespace livelooper
{
/** v1.7 views: a row of chips bound to a choice parameter (view switcher, punch FX target, beat FX type /
    beat, repeat rate). Each chip selects one choice; right-click = MIDI learn of the whole parameter
    (one frame in MIDI learn). Labels are English and drawn in the UI language. */
class ChoiceChips : public juce::Component, public juce::SettableTooltipClient
{
public:
    /** choices: the choice index of each chip (empty = 0 .. labels.size() - 1). */
    ChoiceChips (APVTS& state, const juce::String& paramId, const juce::StringArray& englishLabels,
                 std::vector<int> choices = {}, juce::Colour accent = colours::cyan);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    /** Called instead of setting the parameter (e.g. the view switcher sets view + live_view together). */
    std::function<void (int choice)> onSelect;
    /** The selected chip in the brand gradient (view switcher). */
    void setBrandStyle (bool b) { brand = b; repaint(); }
    /** One pill around all chips (view switcher) instead of separate chips. */
    void setGrouped (bool g) { grouped = g; repaint(); }
    /** The chip of the current choice, -1 if the choice has no chip. */
    int getSelectedChip() const;
    int getNumChips() const noexcept { return labels.size(); }
    juce::Rectangle<int> getChipBounds (int chip) const;
    juce::String getChipText (int chip) const;
    /** Selects the choice of a chip (click). */
    void selectChip (int chip);
    /** Repaints when the parameter changed (polled by the owner's timer). */
    void refresh();

private:
    juce::RangedAudioParameter* param = nullptr;
    juce::StringArray labels;
    std::vector<int> values;
    juce::Colour accent;
    bool brand = false, grouped = false;
    int hover = -1, shown = -2;
};

/** v1.7 Performance view: a large pad tile - momentary (held = on), toggle or trigger - lit while its
    parameter is on. Title, optional second line and an amount bar while lit. Right-click = MIDI learn. */
class StagePad : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Mode { momentary, toggle, trigger };
    StagePad (APVTS& state, const juce::String& paramId, const juce::String& englishTitle, juce::Colour colour, Mode mode);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void setTitle (const juce::String& englishOrTranslated, bool translate = true) { title = englishOrTranslated; translateTitle = translate; repaint(); }
    juce::String getTitle() const { return translateTitle ? i18n::tr (title) : title; }
    bool isLit() const;
    /** Trigger mode: the action (the parameter id is only for MIDI learn). Also called on press in the other modes. */
    std::function<void()> onPress;
    /** Second line (already translated), e.g. "1/8 . HELD", "hold", "last 4 bars". */
    std::function<juce::String()> subtitle;
    /** 0 .. 1 bar while lit (punch amount), < 0 = none. */
    std::function<float()> amount;
    /** Small colour mark: 0 = none, 1 = dash top left (punch pads), 2 = bar at the bottom (stutter / fill). */
    int mark = 0;
    /** Lit tiles filled with the colour (default) or in the brand gradient. */
    bool brandLit = false;
    /** Neutral (grey) tile when off (ALL START / STOP, CAPTURE). */
    bool neutral = false;
    /** Repaints when the lit state changed (polled by the owner's timer). */
    void refresh();

private:
    juce::RangedAudioParameter* param = nullptr;
    juce::String title;
    juce::Colour colour;
    Mode mode;
    bool translateTitle = true, down = false, shownLit = false;
    juce::String shownSub;
};

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
    /** v1.7 views: the view switcher in the header (FULL . LIVE . LIVE - FX . PERFORM, host parameter View). */
    ChoiceChips& getViewSwitcher() noexcept { return *viewChips; }

    /** Horizontal level fader drawn as a meter with a level mark (MIDI learn on right-click). */
    class LevelBar : public LLSlider
    {
    public:
        LevelBar() : LLSlider (juce::Slider::LinearHorizontal, colours::green) {}
        void paint (juce::Graphics& g) override;
        float meter = 0.0f;
    };

    // shared with the Performance view
    /** The recessed LCD (dark in every theme). */
    static void drawDisplayBox (juce::Graphics& g, juce::Rectangle<float> r, float radius);
    /** Tempo, bar . beat and the beat dots on an LCD (scales with the box height, designed at 78 px). */
    static void drawTransport (juce::Graphics& g, LiveLooperProcessor& proc, juce::Rectangle<float> box);
    /** The state word of a track card ("PLAY", "REC WAIT" ..., English) and its colour. */
    static juce::String stateWord (LiveLooperProcessor& proc, int track, juce::Colour& colour);
    /** "4 BARS" / "1.5 BARS" / "-" (translated) and whether the track has a finished loop. */
    static juce::String lengthText (LiveLooperProcessor& proc, int track, bool& hasLength);
    /** The view switcher chips (Full, Live, Live without FX, Performance) - sets the views together. */
    static std::unique_ptr<ChoiceChips> makeViewSwitcher (LiveLooperProcessor& proc);
    /** "livelooper" + badge (brand gradient) at the top left; returns the right edge of the badge. */
    static float drawLogo (juce::Graphics& g, juce::Point<float> topLeft, const juce::String& badge, float size = 30.0f);

private:

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
    std::unique_ptr<ChoiceChips> viewChips;
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
