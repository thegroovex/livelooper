// v1.6 editor parts for the K.O. II (header chip, one-time question, SETUP > Controllers), the
// PUNCH FX + SEND FX panels of the FX rows (12 punch FX, XY field, send FX) and the Controller GUI.

#include "PluginEditor.h"
#include "ControllerView.h"
#include "AppSettings.h"
#include "controllers/Ko2.h"
#include "push/PushDevice.h"

namespace livelooper
{
using namespace params;
using i18n::tr;

namespace
{
    // short pad labels of the 12 punch FX (the long names are in the tooltips)
    constexpr const char* kPunchShort[kNumPunchFx] = { "SCATTER", "SWAP", "GRAINS", "REPEAT", "TAPE STOP", "WOBBLE",
                                                       "LOW-PASS", "HIGH-PASS", "SEND", "TREMOLO", "OCT DROP", "DECIMATE" };

    /** XY field of the PUNCH FX panel: hold the mouse = the selected punch FX on, position = X / Y. */
    class PunchXYPad : public juce::Component, private juce::Timer
    {
    public:
        PunchXYPad (LiveLooperProcessor& p, std::function<int()> sel) : proc (p), selected (std::move (sel))
        {
            setTooltip ("XY: hold to play the selected punch FX, move for X / Y. Right-click: MIDI learn of X");
            startTimerHz (15);
        }
        void setTooltip (const juce::String& t) { tip = t; }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (1.0f);
            g.setColour (juce::Colour (0xff101116));
            g.fillRoundedRectangle (r, 10.0f);
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            for (int i = 1; i < 4; ++i)
            {
                g.fillRect (r.getX() + r.getWidth() * i / 4.0f, r.getY() + 4.0f, 1.0f, r.getHeight() - 8.0f);
                g.fillRect (r.getX() + 4.0f, r.getY() + r.getHeight() * i / 4.0f, r.getWidth() - 8.0f, 1.0f);
            }
            const float x = proc.getState().getRawParameterValue (gPfxX)->load();
            const float y = proc.getState().getRawParameterValue (gPfxY)->load();
            const auto c = juce::Point<float> (r.getX() + x * r.getWidth(), r.getBottom() - y * r.getHeight());
            const bool on = proc.getEngine().getPunchFx().isOn (selected());
            g.setColour ((on ? colours::learn : colours::textDim).withAlpha (on ? 0.35f : 0.18f));
            g.fillEllipse (juce::Rectangle<float> (36.0f, 36.0f).withCentre (c));
            g.setColour (on ? colours::learn : colours::text.withAlpha (0.7f));
            g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (c));
            g.setColour (colours::textDim);
            g.setFont (LooperLookAndFeel::font (r.getWidth() < 100.0f ? 9.0f : 10.0f, true));
            g.drawFittedText ((r.getWidth() < 100.0f ? juce::String() : juce::String ("XY  ")) + i18n::upper (tr (getPunchFxName (selected()))),
                              r.reduced (6.0f, 4.0f).toNearestInt(), juce::Justification::centredTop, 1, 0.7f);
            g.setColour (juce::Colours::white.withAlpha (0.10f));
            g.drawRoundedRectangle (r, 10.0f, 1.0f);
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
                return;
            set (punchHoldId (selected()), 1.0f);
            move (e);
        }
        void mouseDrag (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) move (e); }
        void mouseUp (const juce::MouseEvent& e) override
        {
            if (! e.mods.isPopupMenu())
                set (punchHoldId (selected()), 0.0f);
        }

    private:
        void timerCallback() override { repaint(); }
        void move (const juce::MouseEvent& e)
        {
            const auto r = getLocalBounds().toFloat();
            set (gPfxX, juce::jlimit (0.0f, 1.0f, (e.position.x - r.getX()) / r.getWidth()));
            set (gPfxY, juce::jlimit (0.0f, 1.0f, (r.getBottom() - e.position.y) / r.getHeight()));
        }
        void set (const juce::String& id, float v)
        {
            if (auto* p = proc.getState().getParameter (id))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (v);
                p->endChangeGesture();
            }
        }
        LiveLooperProcessor& proc;
        std::function<int()> selected;
        juce::String tip;
    };

    /** The header chip: dot (green = confirmed, yellow = name match), "K.O. II", role, arrow. */
    class Ko2Chip : public juce::Button
    {
    public:
        explicit Ko2Chip (LiveLooperProcessor& p) : juce::Button ("K.O. II"), proc (p) {}
        void paintButton (juce::Graphics& g, bool over, bool) override
        {
            auto r = getLocalBounds().toFloat().reduced (1.0f);
            g.setColour (juce::Colour (over ? 0xff2c2f36 : 0xff22252b));
            g.fillRoundedRectangle (r, r.getHeight() / 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.drawRoundedRectangle (r, r.getHeight() / 2.0f, 1.0f);
            auto* d = proc.getKo2Device();
            const bool confirmed = d != nullptr && d->isConfirmed();
            g.setColour (confirmed ? colours::green : colours::yellow);
            g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ r.getX() + 13.0f, r.getCentreY() }));
            const bool record = proc.getControllerAudioMode() == 1;
            g.setColour (colours::text);
            g.setFont (LooperLookAndFeel::font (12.0f, true));
            auto t = r.withTrimmedLeft (22.0f).withTrimmedRight (16.0f);
            g.drawText ("K.O. II", t.removeFromLeft (52.0f), juce::Justification::centredLeft);
            g.setColour (juce::Colours::white.withAlpha (0.2f));
            g.fillRect (t.getX(), t.getY() + 6.0f, 1.0f, t.getHeight() - 12.0f);
            g.setColour (record ? colours::red : colours::cyan);
            g.drawText (tr (record ? "Record" : "Controller"), t.withTrimmedLeft (7.0f), juce::Justification::centredLeft);
            g.setColour (colours::textDim);
            g.drawText (juce::String::fromUTF8 ("\xe2\x96\xbe"), r.removeFromRight (16.0f), juce::Justification::centred);
        }
    private:
        LiveLooperProcessor& proc;
    };

    /** "K.O. II connected - how should livelooper use it?" with three buttons; hides after 12 s. */
    class Ko2Toast : public juce::Component, private juce::Timer
    {
    public:
        explicit Ko2Toast (std::function<void (int)> choose) : choice (std::move (choose))
        {
            const char* names[] = { "Controller only", "Record K.O. II", "Ignore" };
            for (int i = 0; i < 3; ++i)
            {
                buttons[(size_t) i] = std::make_unique<LLButton> (names[i], i == 1 ? colours::red : colours::cyan);
                buttons[(size_t) i]->onClick = [this, i] { pick (i); };
                buttons[(size_t) i]->setWantsKeyboardFocus (false);
                addAndMakeVisible (*buttons[(size_t) i]);
            }
            settings = std::make_unique<LLButton> ("Settings ...", colours::cyan);
            settings->onClick = [this] { pick (3); };
            settings->setWantsKeyboardFocus (false);
            addAndMakeVisible (*settings);
            setWantsKeyboardFocus (false);
            setInterceptsMouseClicks (true, true);
            startTimer (12000);
        }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (2.0f);
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (r.translated (0.0f, 4.0f), 14.0f);
            g.setColour (juce::Colour (0xf0202228));
            g.fillRoundedRectangle (r, 14.0f);
            g.setColour (juce::Colours::white.withAlpha (0.14f));
            g.drawRoundedRectangle (r, 14.0f, 1.0f);
            g.setColour (colours::text);
            g.setFont (LooperLookAndFeel::font (14.0f, true));
            g.drawFittedText (tr ("K.O. II connected - how should livelooper use it?"), getLocalBounds().reduced (18, 12).removeFromTop (24),
                              juce::Justification::centredLeft, 1, 0.8f);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (16, 12);
            r.removeFromTop (30);
            auto row = r.removeFromTop (34);
            const int w = (row.getWidth() - 3 * 8 - 110) / 3;
            for (auto& b : buttons)
            {
                b->setBounds (row.removeFromLeft (w));
                row.removeFromLeft (8);
            }
            settings->setBounds (row);
        }
        /** Timeout: controller only for this session, asked again next time. */
        std::function<void()> onTimeout;
    private:
        void timerCallback() override
        {
            stopTimer();
            if (onTimeout) onTimeout();
        }
        void pick (int i)
        {
            stopTimer();
            if (choice) choice (i);
        }
        std::array<std::unique_ptr<LLButton>, 3> buttons;
        std::unique_ptr<LLButton> settings;
        std::function<void (int)> choice;
    };

    struct PunchSelect : juce::MouseListener
    {
        explicit PunchSelect (std::function<void (juce::Component*)> f) : fn (std::move (f)) {}
        void mouseDown (const juce::MouseEvent& e) override { fn (e.eventComponent); }
        std::function<void (juce::Component*)> fn;
    };
}

//============================================================================== init / layout
void LiveLooperEditor::initKo2()
{
    auto& st = proc.getState();
    // ---- PUNCH FX panel
    punchSelectListener = std::make_unique<PunchSelect> ([this] (juce::Component* c) {
        for (int i = 0; i < kNumPunchFx; ++i)
            if (punchBtns[(size_t) i].get() == c)
                selectedPunch = i;
        if (punchXY != nullptr)
            punchXY->repaint();
    });
    for (int i = 0; i < kNumPunchFx; ++i)
    {
        auto& b = punchBtns[(size_t) i];
        b = std::make_unique<MomentaryButton> (st, punchHoldId (i), kPunchShort[i], colours::learn);
        b->tooltipFn = [i] { return tr ("Punch FX %1: %2 - hold to play, right-click: MIDI learn", juce::String (i + 1), tr (getPunchFxName (i))); };
        b->addMouseListener (punchSelectListener.get(), false);
        surface.addChildComponent (*b);
    }
    pfxAmountKnob = std::make_unique<Knob> (st, gPfxAmount, "Amount", colours::learn);
    sfxXKnob = std::make_unique<Knob> (st, gSfxX, "X", colours::orange);
    sfxYKnob = std::make_unique<Knob> (st, gSfxY, "Y", colours::orange);
    sfxLevelKnob = std::make_unique<Knob> (st, gSfxLevel, "Send", colours::orange);
    for (auto* k : { pfxAmountKnob.get(), sfxXKnob.get(), sfxYKnob.get(), sfxLevelKnob.get() })
        surface.addChildComponent (k);
    pfxTargetBox.setEnglishItems (punchTargetChoices());
    pfxTargetAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, gPfxTarget, pfxTargetBox);
    pfxTargetBox.setTooltip ("Where the punch FX work: loops, input, master or drums");
    sfxTypeBox.setEnglishItems (sendFxChoices());
    sfxTypeAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, gSfxType, sfxTypeBox);
    sfxTypeBox.setTooltip ("Send FX on the loops: SEND = how much goes in, X / Y = its two knobs");
    pfxLatchBtn = std::make_unique<LLButton> ("LATCH", colours::yellow);
    pfxLatchBtn->setClickingTogglesState (true);
    pfxLatchAtt = std::make_unique<APVTS::ButtonAttachment> (st, gPfxLatch, *pfxLatchBtn);
    pfxLatchBtn->setTooltip ("Latch: a press switches a punch FX on, the next press off");
    for (juce::Component* c : { (juce::Component*) &pfxTargetBox, (juce::Component*) &sfxTypeBox, (juce::Component*) pfxLatchBtn.get() })
        surface.addChildComponent (c);
    punchXY = std::make_unique<PunchXYPad> (proc, [this] { return selectedPunch; });
    surface.addChildComponent (*punchXY);

    // ---- header chip (only while a K.O. II is connected)
    ko2Chip = std::make_unique<Ko2Chip> (proc);
    ko2Chip->setTooltip (tr ("K.O. II: role, resample, clock - click for options"));
    ko2Chip->onClick = [this] { showKo2ChipMenu(); };
    ko2Chip->setWantsKeyboardFocus (false);
    surface.addChildComponent (*ko2Chip);

    // ---- Controller GUI
    controllerView = std::make_unique<ControllerView> (proc);
    controllerView->onStandard = [this] { setControllerGui (false); };
    controllerView->onSwitchDevice = [this] (ControllerView::Device d) {
        appsettings::setValue ("gui.controller", d == ControllerView::Device::Ko2 ? "ko2" : "push");
        updateKo2Ui();
    };
    surface.addChildComponent (*controllerView);
}

void LiveLooperEditor::layoutPunchTab (juce::Rectangle<int> c)
{
    // v1.6 final: PUNCH FX and SEND FX are panels of their own in the FX rows (always visible)
    for (auto& b : punchBtns)
        b->setVisible (true);
    for (juce::Component* comp : { (juce::Component*) pfxAmountKnob.get(), (juce::Component*) sfxXKnob.get(), (juce::Component*) sfxYKnob.get(),
                                   (juce::Component*) sfxLevelKnob.get(), (juce::Component*) &pfxTargetBox, (juce::Component*) &sfxTypeBox,
                                   (juce::Component*) pfxLatchBtn.get(), punchXY.get() })
        comp->setVisible (true);

    // PUNCH FX: target + LATCH in the title row; 4 x 3 pads | XY field | AMOUNT
    {
        auto t = juce::Rectangle<int> (punchArea.getX() + 12, punchArea.getY() + 8, punchArea.getWidth() - 24, 22);
        pfxLatchBtn->setBounds (t.removeFromRight (64));
        t.removeFromRight (6);
        pfxTargetBox.setBounds (t.removeFromRight (112));
    }
    auto r = c;
    const int amountW = 56;
    const int xy = juce::jmin (r.getHeight(), 96);
    auto grid = r.removeFromLeft (r.getWidth() - xy - amountW - 16);
    const int pw = grid.getWidth() / 4, ph = (grid.getHeight() + 3) / 3;
    for (int i = 0; i < kNumPunchFx; ++i)
        punchBtns[(size_t) i]->setBounds (grid.getX() + (i % 4) * pw, grid.getY() + (i / 4) * ph, pw - 3, ph - 3);
    r.removeFromLeft (8);
    punchXY->setBounds (r.removeFromLeft (xy).withSizeKeepingCentre (xy, xy));
    r.removeFromLeft (8);
    pfxAmountKnob->setBounds (r);

    // SEND FX: type in the title row; SEND | X | Y
    {
        auto t = juce::Rectangle<int> (sendArea.getX() + 12, sendArea.getY() + 8, sendArea.getWidth() - 24, 22);
        sfxTypeBox.setBounds (t.removeFromRight (juce::jmin (124, t.getWidth() - 80)));
        auto k = sendArea.reduced (12, 8).withTrimmedTop (26);
        const int kw = k.getWidth() / 3;
        sfxLevelKnob->setBounds (k.removeFromLeft (kw));
        sfxXKnob->setBounds (k.removeFromLeft (kw));
        sfxYKnob->setBounds (k);
    }
    punchCaption = sendCaption = {};
}

void LiveLooperEditor::layoutKo2 (int dx)
{
    // the chip sits left of the round record button
    ko2Chip->setBounds (1168 + dx - 112 - 10 - 168, 50, 168, 28);
    if (ko2Toast != nullptr)
        ko2Toast->setBounds (surface.getWidth() - 560 - 24, surface.getHeight() - 112 - 24, 560, 112);
    controllerView->setBounds (surface.getLocalBounds());
}

//============================================================================== timer
bool LiveLooperEditor::isControllerGuiShown() const noexcept
{
    return controllerView != nullptr && controllerView->isVisible();
}

bool LiveLooperEditor::isKo2ToastVisible() const noexcept
{
    return ko2Toast != nullptr && ko2Toast->isVisible();
}

void LiveLooperEditor::setControllerGui (bool on)
{
    appsettings::setValue ("gui.layout", on ? "controller" : "standard");
    updateKo2Ui();
}

void LiveLooperEditor::updateKo2Ui()
{
    // track colours (Push encoder page) tint the strips
    for (int t = 0; t < 8; ++t)
    {
        const int c = proc.getTrackColour (t);
        if (c != shownTrackColours[(size_t) t])
        {
            shownTrackColours[(size_t) t] = c;
            colours::setTrackOverride (t, c >= 0 ? getTrackColourInfo (c).argb : 0u);
            if (t < (int) strips.size() && strips[(size_t) t] != nullptr)
                strips[(size_t) t]->repaint();
            surface.repaint();
        }
    }

    const bool ko2 = proc.isKo2Connected();
    const bool pushOn = proc.isPushConnected();
    if (ko2Chip->isVisible() != ko2)
        ko2Chip->setVisible (ko2);
    ko2Chip->repaint();

    // first connection: ask once (or apply the remembered choice)
    if (ko2 && ! ko2WasConnected && proc.wrapperType != juce::AudioProcessor::wrapperType_Undefined)
    {
        const auto role = appsettings::getValue ("ko2.role", "ask");
        if (role == "ask")
            showKo2Toast();
        else
        {
            applyKo2Choice (role == "record" ? 1 : role == "ignore" ? 2 : 0, false);
            showStatus (role == "record" ? tr ("K.O. II: RECORDING INPUT") : role == "ignore" ? tr ("K.O. II: IGNORED") : tr ("K.O. II: CONTROLLER ONLY"));
        }
    }
    if (! ko2 && ko2WasConnected && ko2Toast != nullptr)
        ko2Toast.reset();
    ko2WasConnected = ko2;

    // notices of the processor (no free track, KEYS mode ...)
    for (auto n = proc.takeControllerNotice(); n.isNotEmpty(); n = proc.takeControllerNotice())
        showStatus (i18n::upper (n));

    // Controller GUI: only while a controller is connected (falls back to the standard GUI)
    const bool want = appsettings::getValue ("gui.layout", "standard") == "controller" && (ko2 || pushOn);
    if (want)
    {
        const auto pref = appsettings::getValue ("gui.controller", "ko2");
        const auto dev = (ko2 && (pref == "ko2" || ! pushOn)) ? ControllerView::Device::Ko2 : ControllerView::Device::Push;
        controllerView->bothConnected = ko2 && pushOn;
        if (controllerView->getDevice() != dev || ! controllerView->isVisible())
            controllerView->setDevice (dev);
        controllerView->resized();
    }
    if (controllerView->isVisible() != want)
    {
        controllerView->setBounds (surface.getLocalBounds());
        controllerView->setVisible (want);
        if (want)
            controllerView->toFront (false);
    }
}

//============================================================================== K.O. II question / chip
void LiveLooperEditor::showKo2Toast (bool force)
{
    if (! force && ! proc.isKo2Connected())
        return;
    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    auto t = std::make_unique<Ko2Toast> ([safe] (int i) {
        if (safe == nullptr)
            return;
        if (i == 3)
            safe->showSettingsWindow();
        else
            safe->applyKo2Choice (i, true);
        safe->ko2Toast.reset();
    });
    static_cast<Ko2Toast*> (t.get())->onTimeout = [safe] {
        if (safe == nullptr)
            return;
        safe->applyKo2Choice (0, false);      // controller only for this session
        safe->ko2Toast.reset();
    };
    ko2Toast = std::move (t);
    surface.addAndMakeVisible (*ko2Toast);
    ko2Toast->setBounds (surface.getWidth() - 560 - 24, surface.getHeight() - 112 - 24, 560, 112);
    ko2Toast->toFront (false);
}

void LiveLooperEditor::applyKo2Choice (int choice, bool remember)
{
    if (remember)
        appsettings::setValue ("ko2.role", choice == 1 ? "record" : choice == 2 ? "ignore" : "controller");
    if (choice == 2)
    {
        proc.setKo2Groups (0);                 // ignore: no pad controls livelooper
        return;
    }
    if (proc.getKo2Groups() == 0)
        proc.setKo2Groups (-1);
    proc.setControllerAudioMode (choice == 1 ? 1 : 0);
    if (choice == 1 && proc.wrapperType != juce::AudioProcessor::wrapperType_Standalone)
        showStatus (tr ("IN THE PLUG-IN: CHOOSE THE K.O. II AS AUDIO INPUT IN YOUR DAW"));
}

void LiveLooperEditor::showKo2ChipMenu()
{
    juce::PopupMenu m;
    const bool record = proc.getControllerAudioMode() == 1;
    m.addSectionHeader ("K.O. II");
    m.addItem (700, tr ("Controller only"), true, ! record);
    m.addItem (701, tr ("Record K.O. II"), true, record);
    m.addSeparator();
    m.addItem (770, tr ("Resample K.O. pattern"));
    m.addItem (740, tr ("Send clock to K.O. II"), true, proc.getKo2ClockOut());
    m.addItem (781, tr ("Controller GUI"), true, appsettings::getValue ("gui.layout", "standard") == "controller");
    m.addItem (790, tr ("Settings ..."));
    if (record && proc.wrapperType != juce::AudioProcessor::wrapperType_Standalone)
    {
        m.addSeparator();
        m.addItem (798, tr ("Record K.O. II in the plug-in: choose 'EP-133 ...' as audio input in your DAW's audio settings and set livelooper's track input."), false);
    }
    juce::Component::SafePointer<LiveLooperEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (ko2Chip.get()), [safe] (int r) {
        if (safe == nullptr || r == 0)
            return;
        if (r == 781)
            safe->setControllerGui (appsettings::getValue ("gui.layout", "standard") != "controller");
        else if (r == 790)
            safe->showSettingsWindow();
        else
            safe->handleControllerMenu (r);
    });
}

//============================================================================== SETUP > Controllers
void LiveLooperEditor::addControllerMenu (juce::PopupMenu& menu, juce::Array<int>& toggles)
{
    const bool connected = proc.isKo2Connected();
    const bool record = proc.getControllerAudioMode() == 1;
    juce::PopupMenu k;
    k.addItem (700, tr ("Controller only"), true, ! record);
    k.addItem (701, tr ("Record K.O. II"), true, record);
    const int groups = proc.getKo2Groups() < 0 ? ko2::defaultGroups (record ? ko2::Role::Record : ko2::Role::Controller) : proc.getKo2Groups();
    for (int g = 0; g < 4; ++g)
        k.addItem (710 + g, tr ("Control group %1", juce::String::charToString ((juce::juce_wchar) ('A' + g))), true, (groups >> g) & 1);
    k.addItem (714, tr ("Default control groups for the role"));
    k.addItem (740, tr ("Send clock to K.O. II"), true, proc.getKo2ClockOut());
    juce::PopupMenu ch;
    ch.addItem (720, tr ("Any"), true, proc.getKo2Channel() == 0);
    for (int c = 1; c <= 16; ++c)
        ch.addItem (720 + c, juce::String (c), true, proc.getKo2Channel() == c);
    k.addSubMenu (tr ("K.O. II MIDI channel"), ch);
    juce::PopupMenu off;
    const int offsets[] = { -1, 0, 5, 10, 20, 30, 50, 80, 100, 150, 200 };
    for (int i = 0; i < 11; ++i)
        off.addItem (750 + i, offsets[i] < 0 ? tr ("Automatic (reported latency)") : juce::String (offsets[i]) + " ms", true, proc.getKo2OffsetMs() == offsets[i]);
    k.addSubMenu (tr ("Latency offset (recording)"), off);
    k.addItem (770, tr ("Resample K.O. pattern"), connected);
    k.addItem (771, tr ("Ask again when connected"));
    k.addSeparator();
    auto* dev = proc.getKo2Device();
    k.addItem (797, connected ? tr ("Connected: %1", dev->getDeviceName()) + (dev->isConfirmed() ? juce::String() : " (" + tr ("not confirmed") + ")")
                              : tr ("No K.O. II found"), false);
    k.addItem (796, tr (juce::String::fromUTF8 ("Experimental \xe2\x80\x93 please test (see the K.O. II page of the docs)")), false);
    k.addItem (795, tr ("On the K.O. II: SHIFT + ERASE, type 1 0 1, ENTER (clock in)"), false);
    menu.addSubMenu (tr ("K.O. II"), k);

    // GUI: only offered while a K.O. II or Push 1 is connected
    if (connected || proc.isPushConnected())
    {
        juce::PopupMenu gui;
        const bool ctl = appsettings::getValue ("gui.layout", "standard") == "controller";
        gui.addItem (780, tr ("Standard"), true, ! ctl);
        gui.addItem (781, tr ("Controller"), true, ctl);
        if (connected && proc.isPushConnected())
        {
            const bool ko = appsettings::getValue ("gui.controller", "ko2") == "ko2";
            gui.addItem (782, tr ("Controller view: K.O. II"), true, ko);
            gui.addItem (783, tr ("Controller view: Push"), true, ! ko);
        }
        menu.addSubMenu (tr ("GUI"), gui);
        toggles.addArray ({ -780, -781 });
    }
    toggles.addArray ({ 710, 711, 712, 713, 740, -700, -701 });
}

bool LiveLooperEditor::handleControllerMenu (int r)
{
    if (r < 700 || r > 799)
        return false;
    const bool record = proc.getControllerAudioMode() == 1;
    if (r == 700 || r == 701)
    {
        applyKo2Choice (r - 700, true);
        showStatus (r == 701 ? tr ("K.O. II: RECORDING INPUT") : tr ("K.O. II: CONTROLLER ONLY"));
    }
    else if (r >= 710 && r <= 713)
    {
        int g = proc.getKo2Groups();
        if (g < 0)
            g = ko2::defaultGroups (record ? ko2::Role::Record : ko2::Role::Controller);
        proc.setKo2Groups (g ^ (1 << (r - 710)));
    }
    else if (r == 714)
        proc.setKo2Groups (-1);
    else if (r >= 720 && r <= 736)
        proc.setKo2Channel (r - 720);
    else if (r == 740)
        proc.setKo2ClockOut (! proc.getKo2ClockOut());
    else if (r >= 750 && r <= 760)
    {
        const int offsets[] = { -1, 0, 5, 10, 20, 30, 50, 80, 100, 150, 200 };
        proc.setKo2OffsetMs (offsets[r - 750]);
    }
    else if (r == 770)
        proc.performKo2Resample();
    else if (r == 771)
        appsettings::setValue ("ko2.role", "ask");
    else if (r == 780 || r == 781)
        setControllerGui (r == 781);
    else if (r == 782 || r == 783)
        appsettings::setValue ("gui.controller", r == 782 ? "ko2" : "push");
    updateKo2Ui();
    return true;
}
} // namespace livelooper
