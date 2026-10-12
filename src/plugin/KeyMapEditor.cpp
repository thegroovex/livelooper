#include "KeyMapEditor.h"

namespace livelooper
{
using i18n::tr;

KeyMapEditor::KeyMapEditor (LiveLooperProcessor& p)
    : proc (p), actions (KeyMap::getAvailableActions())
{
    setLookAndFeel (&lookAndFeel);
    setWantsKeyboardFocus (true);
    list.setWantsKeyboardFocus (false);
    list.setRowHeight (24);
    list.setColour (juce::ListBox::backgroundColourId, colours::section);
    list.setColour (juce::ListBox::outlineColourId, colours::sectionEdge);
    list.setOutlineThickness (1);
    addAndMakeVisible (list);

    enableBtn.setClickingTogglesState (true);
    enableBtn.setToggleState (proc.getKeyMap().isEnabled(), juce::dontSendNotification);
    enableBtn.setTooltip ("Switch control by the computer keyboard on or off");
    enableBtn.onClick = [this] { proc.getKeyMap().setEnabled (enableBtn.getToggleState()); repaint(); };
    defaultsBtn.setTooltip ("Load the default keyboard mapping");
    defaultsBtn.onClick = [this] {
        proc.getKeyMap().setDefaults();
        info = tr ("Default keyboard mapping loaded");
        refresh();
    };
    clearBtn.setTooltip ("Remove every key assignment");
    clearBtn.onClick = [this] {
        proc.getKeyMap().clear();
        info = tr ("All key assignments removed");
        refresh();
    };
    for (auto* b : { &enableBtn, &defaultsBtn, &clearBtn })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }
    setSize (520, 600);
}

KeyMapEditor::~KeyMapEditor()
{
    setLookAndFeel (nullptr);
}

void KeyMapEditor::show (LiveLooperProcessor& p, juce::Component& centreAround)
{
    auto content = std::make_unique<KeyMapEditor> (p);
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (content.release());
    o.dialogTitle = tr ("Keyboard mapping");
    o.dialogBackgroundColour = colours::panel;
    o.componentToCentreAround = &centreAround;
    o.escapeKeyTriggersCloseButton = false;
    o.useNativeTitleBar = true;
    o.resizable = false;
    if (auto* w = o.launchAsync())
        w->getContentComponent()->grabKeyboardFocus();
}

void KeyMapEditor::refresh()
{
    list.updateContent();
    list.repaint();
    repaint();
}

void KeyMapEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
    auto b = getLocalBounds().reduced (14);
    g.setColour (colours::text);
    g.setFont (LooperLookAndFeel::font (15.0f, true));
    g.drawText (tr ("KEYBOARD MAPPING"), b.removeFromTop (22), juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (LooperLookAndFeel::font (12.0f));
    g.drawFittedText (tr ("Click a function, then press its new key. Esc cancels, right-click removes the key."),
                      b.removeFromTop (34), juce::Justification::centredLeft, 2);

    auto bottom = getLocalBounds().reduced (14).removeFromBottom (60).removeFromBottom (20);
    g.setColour (info.isNotEmpty() ? colours::yellow : colours::textDim);
    g.drawFittedText (info.isNotEmpty() ? info : (proc.getKeyMap().isEnabled() ? tr ("Keyboard control is on")
                                                                               : tr ("Keyboard control is off")),
                      bottom, juce::Justification::centredLeft, 1);
}

void KeyMapEditor::resized()
{
    auto b = getLocalBounds().reduced (14);
    b.removeFromTop (60);
    auto bottom = b.removeFromBottom (60);
    list.setBounds (b);
    auto row = bottom.removeFromTop (36).withTrimmedTop (6);
    enableBtn.setBounds (row.removeFromLeft (190));
    row.removeFromLeft (8);
    defaultsBtn.setBounds (row.removeFromLeft (140));
    row.removeFromLeft (8);
    clearBtn.setBounds (row.removeFromLeft (150));
}

void KeyMapEditor::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool)
{
    if (! juce::isPositiveAndBelow (row, actions.size()))
        return;
    const auto& action = actions[row];
    const bool capturing = row == captureRow;
    if (capturing)
        g.fillAll (colours::learn.withAlpha (0.35f));
    else if (row % 2 == 1)
        g.fillAll (juce::Colours::white.withAlpha (0.03f));

    auto r = juce::Rectangle<int> (0, 0, width, height).reduced (10, 0);
    auto keyArea = r.removeFromRight (190).reduced (0, 3);

    g.setColour (colours::text);
    g.setFont (LooperLookAndFeel::font (13.0f));
    g.drawFittedText (KeyMap::describeAction (action), r, juce::Justification::centredLeft, 1, 0.8f);

    const auto key = proc.getKeyMap().getKeyFor (action);
    if (capturing)
    {
        g.setColour (colours::learn);
        g.drawRoundedRectangle (keyArea.toFloat(), 4.0f, 1.5f);
        g.setColour (colours::text);
        g.setFont (LooperLookAndFeel::font (12.0f, true));
        g.drawFittedText (tr ("press a key ..."), keyArea, juce::Justification::centred, 1);
    }
    else if (key.isValid())
    {
        g.setColour (colours::panelLight);
        g.fillRoundedRectangle (keyArea.toFloat(), 4.0f);
        g.setColour (colours::sectionEdge);
        g.drawRoundedRectangle (keyArea.toFloat(), 4.0f, 1.0f);
        g.setColour (colours::yellow);
        g.setFont (LooperLookAndFeel::font (12.0f, true));
        g.drawFittedText (KeyMap::describeKey (key), keyArea, juce::Justification::centred, 1, 0.8f);
    }
    else
    {
        g.setColour (colours::textDim.withAlpha (0.6f));
        g.setFont (LooperLookAndFeel::font (12.0f));
        g.drawFittedText ("-", keyArea, juce::Justification::centred, 1);
    }
}

void KeyMapEditor::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        proc.getKeyMap().removeAction (actions[row]);
        captureRow = -1;
        info = tr ("Key removed: %1", KeyMap::describeAction (actions[row]));
        refresh();
        return;
    }
    startCapture (captureRow == row ? -1 : row);
}

void KeyMapEditor::startCapture (int row)
{
    captureRow = row;
    info = {};
    grabKeyboardFocus();
    refresh();
}

bool KeyMapEditor::keyPressed (const juce::KeyPress& key)
{
    if (captureRow < 0)
        return false;

    const auto action = actions[captureRow];
    captureRow = -1;
    if (key.isKeyCode (juce::KeyPress::escapeKey) && ! key.getModifiers().isAnyModifierKeyDown())
    {
        info = {};
        refresh();
        return true;
    }

    auto& km = proc.getKeyMap();
    const auto previous = km.getActionFor (key);
    km.assign (key, action);
    info = previous.isNotEmpty() && previous != action
               ? tr ("%1 moved from: %2", KeyMap::describeKey (key), KeyMap::describeAction (previous))
               : tr ("%1 assigned", KeyMap::describeKey (key));
    refresh();
    return true;
}
} // namespace livelooper
