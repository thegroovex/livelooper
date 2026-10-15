#include "SettingsPanel.h"
#include "Localisation.h"

namespace livelooper
{
using i18n::tr;

namespace
{
    struct Item { int id = 0; juce::String text; bool enabled = true, ticked = false; const juce::PopupMenu* sub = nullptr; bool header = false, separator = false; };

    std::vector<Item> itemsOf (const juce::PopupMenu& m)
    {
        std::vector<Item> out;
        for (juce::PopupMenu::MenuItemIterator it (m); it.next();)
        {
            const auto& i = it.getItem();
            Item x;
            x.id = i.itemID;
            x.text = i.text;
            x.enabled = i.isEnabled;
            x.ticked = i.isTicked;
            x.sub = i.subMenu.get();
            x.header = i.isSectionHeader;
            x.separator = i.isSeparator;
            out.push_back (x);
        }
        return out;
    }

    enum class Kind { Switch, Combo, Action };
    struct Row
    {
        Kind kind = Kind::Action;
        juce::String label;
        int id = 0;                    // switch / action
        bool on = false, enabled = true;
        std::vector<Item> choices;     // combo
    };
    struct Group { juce::String heading; std::vector<Row> rows; juce::StringArray notes; };

    juce::String shortLabel (juce::String s)
    {
        // "Track 2: Out 3/4" -> "Track 2"; trailing " ..." stays (it opens a window)
        if (s.containsChar (':'))
            s = s.upToFirstOccurrenceOf (":", false, false);
        return s.trim();
    }

    bool isPicker (const std::vector<Item>& seg)
    {
        // a list of sets to choose from (no tick): ids 300 ... 319
        for (const auto& i : seg)
            if (i.id < 300 || i.id >= 320)
                return false;
        return seg.size() >= 1;
    }

    void addSubMenuRows (const juce::PopupMenu& sub, const juce::Array<int>& toggles, std::vector<Row>& rows, juce::StringArray& notes)
    {
        const auto items = itemsOf (sub);
        std::vector<Item> seg;
        juce::String segHeader;
        auto flush = [&] {
            std::vector<Item> chooseable;
            for (const auto& i : seg)
                if (i.sub == nullptr && i.text.isNotEmpty())
                    chooseable.push_back (i);
            bool anyTick = false, anyToggle = false;
            for (const auto& i : chooseable)
            {
                anyTick = anyTick || i.ticked;
                anyToggle = anyToggle || toggles.contains (i.id);
            }
            std::vector<Item> radio;
            if (! anyToggle && (anyTick || isPicker (chooseable)) && chooseable.size() >= 2)
            {
                for (const auto& i : chooseable)
                    if (i.enabled || i.ticked || anyTick)
                        if (i.id != 0 && (i.enabled || anyTick))
                            radio.push_back (i);
            }
            else if (isPicker (chooseable) && ! chooseable.empty())
                radio = chooseable;
            if (! radio.empty())
            {
                Row r;
                r.kind = Kind::Combo;
                r.label = segHeader;
                r.choices = radio;
                bool en = false;
                for (const auto& i : radio)
                    en = en || i.enabled;
                r.enabled = en;
                rows.push_back (r);
                for (const auto& i : chooseable)
                    if (std::find_if (radio.begin(), radio.end(), [&] (const Item& x) { return x.id == i.id; }) == radio.end() && ! i.enabled)
                        notes.add (i.text);
            }
            else
            {
                for (const auto& i : chooseable)
                {
                    if (! i.enabled && ! toggles.contains (i.id) && ! i.text.endsWith ("..."))
                    {
                        notes.add (i.text);
                        continue;
                    }
                    Row r;
                    r.kind = toggles.contains (i.id) ? Kind::Switch : Kind::Action;
                    r.label = i.text;
                    r.id = i.id;
                    r.on = i.ticked;
                    r.enabled = i.enabled;
                    rows.push_back (r);
                }
            }
            for (const auto& i : seg)
                if (i.sub != nullptr)
                {
                    Row r;
                    r.kind = Kind::Combo;
                    r.label = shortLabel (i.text);
                    for (const auto& c : itemsOf (*i.sub))
                        if (! c.header && ! c.separator && c.id != 0)
                            r.choices.push_back (c);
                    rows.push_back (r);
                }
            seg.clear();
        };
        for (const auto& i : items)
        {
            if (i.header || i.separator)
            {
                flush();
                segHeader = i.header ? i.text : juce::String();
                continue;
            }
            seg.push_back (i);
        }
        flush();
    }

    std::vector<Group> groupsOf (const juce::PopupMenu& pageMenu, const juce::Array<int>& toggles)
    {
        std::vector<Group> groups;
        auto plain = [&]() -> Group& {
            if (groups.empty() || groups.back().heading.isNotEmpty())
                groups.push_back ({});
            return groups.back();
        };
        for (const auto& i : itemsOf (pageMenu))
        {
            if (i.header || i.separator)
                continue;
            if (i.sub != nullptr)
            {
                Group g;
                addSubMenuRows (*i.sub, toggles, g.rows, g.notes);
                if (g.rows.size() == 1 && g.rows[0].label.isEmpty())
                {
                    // one control: a plain row labelled with the sub-menu name
                    g.rows[0].label = i.text;
                    auto& p = plain();
                    p.rows.push_back (g.rows[0]);
                    p.notes.addArray (g.notes);
                    continue;
                }
                for (auto& r : g.rows)
                    if (r.label.isEmpty())
                        r.label = i.text;
                g.heading = i.text;
                if (! g.rows.empty() || ! g.notes.isEmpty())
                    groups.push_back (g);
                continue;
            }
            Row r;
            if (! i.enabled && ! toggles.contains (i.id) && ! i.text.endsWith ("..."))
            {
                plain().notes.add (i.text);
                continue;
            }
            r.kind = toggles.contains (i.id) ? Kind::Switch : Kind::Action;
            r.label = i.text;
            r.id = i.id;
            r.on = i.ticked;
            r.enabled = i.enabled;
            plain().rows.push_back (r);
        }
        return groups;
    }

    constexpr int kRowH = 44, kMargin = 26, kGroupGap = 22;
}

//============================================================================== rows
class ActionRow : public juce::Component
{
public:
    ActionRow (const juce::String& t, std::function<void()> fn) : text (t), onClick (std::move (fn)) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics& g) override
    {
        if (isMouseOverOrDragging())
        {
            g.setColour (juce::Colours::white.withAlpha (isMouseButtonDown() ? 0.10f : 0.05f));
            g.fillRect (getLocalBounds());
        }
        g.setColour (isEnabled() ? colours::text : colours::textDim);
        g.setFont (LooperLookAndFeel::font (14.0f));
        g.drawFittedText (text, getLocalBounds().reduced (16, 0).withTrimmedRight (20), juce::Justification::centredLeft, 1, 0.8f);
        juce::Path p;
        const float x = (float) getWidth() - 22.0f, y = (float) getHeight() * 0.5f;
        p.startNewSubPath (x, y - 5.0f);
        p.lineTo (x + 5.0f, y);
        p.lineTo (x, y + 5.0f);
        g.setColour (colours::textDim);
        g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseDown (const juce::MouseEvent&) override { repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        repaint();
        if (isEnabled() && getLocalBounds().contains (e.getPosition()) && onClick)
            onClick();
    }
private:
    juce::String text;
    std::function<void()> onClick;
};

class SettingsPanel::Content : public juce::Component
{
public:
    explicit Content (SettingsPanel& p) : owner (p) {}

    void build (const juce::PopupMenu& pageMenu, const juce::String& title, int width)
    {
        controls.clear();
        layoutCards.clear();
        groups = groupsOf (pageMenu, owner.toggles);
        pageTitle = title;
        int y = kMargin + 46;
        owner.numRows = 0;
        const int w = width - 2 * kMargin;
        for (const auto& g : groups)
        {
            Card card;
            card.heading = g.heading;
            if (g.heading.isNotEmpty())
            {
                card.headingY = y;
                y += 24;
            }
            card.box = { kMargin, y, w, (int) g.rows.size() * kRowH };
            for (size_t i = 0; i < g.rows.size(); ++i)
            {
                const auto& r = g.rows[i];
                const juce::Rectangle<int> rowB (kMargin, y + (int) i * kRowH, w, kRowH);
                addRow (r, rowB);
                ++owner.numRows;
            }
            y += (int) g.rows.size() * kRowH;
            if (! g.notes.isEmpty())
            {
                y += 6;
                const auto f = LooperLookAndFeel::font (12.0f);
                for (const auto& n : g.notes)
                {
                    juce::GlyphArrangement ga;
                    ga.addJustifiedText (f, n, 0.0f, 0.0f, (float) w - 28.0f, juce::Justification::topLeft);
                    const int h = (int) std::ceil (ga.getBoundingBox (0, -1, true).getHeight()) + 4;
                    card.notes.push_back ({ n, juce::Rectangle<int> (kMargin + 14, y, w - 28, h) });
                    y += h;
                }
            }
            layoutCards.push_back (card);
            y += kGroupGap;
        }
        setSize (width, y + kMargin);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colours::white);
        g.setFont (LooperLookAndFeel::font (24.0f, true));
        g.drawText (pageTitle, kMargin, kMargin - 4, getWidth() - 2 * kMargin, 34, juce::Justification::centredLeft);
        for (const auto& c : layoutCards)
        {
            if (c.heading.isNotEmpty())
            {
                g.setColour (colours::textDim);
                g.setFont (LooperLookAndFeel::font (12.0f, true));
                g.drawText (c.heading, c.box.getX() + 4, c.headingY, c.box.getWidth(), 20, juce::Justification::centredLeft);
            }
            if (! c.box.isEmpty())
            {
                const auto r = c.box.toFloat();
                g.setColour (juce::Colours::white.withAlpha (0.055f));
                g.fillRoundedRectangle (r, 12.0f);
                g.setColour (juce::Colours::white.withAlpha (0.09f));
                g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);
                g.setColour (juce::Colours::white.withAlpha (0.07f));
                for (int y = c.box.getY() + kRowH; y < c.box.getBottom() - 1; y += kRowH)
                    g.fillRect ((float) c.box.getX() + 16.0f, (float) y, (float) c.box.getWidth() - 32.0f, 1.0f);
            }
            g.setColour (colours::textDim.withAlpha (0.9f));
            g.setFont (LooperLookAndFeel::font (12.0f));
            for (const auto& n : c.notes)
                g.drawFittedText (n.first, n.second, juce::Justification::topLeft, 4, 1.0f);
        }
        // labels of switch / pop-up rows
        g.setFont (LooperLookAndFeel::font (14.0f));
        for (const auto& l : labels)
        {
            g.setColour (l.enabled ? colours::text : colours::textDim);
            g.drawFittedText (l.text, l.area, juce::Justification::centredLeft, 2, 0.75f);
        }
    }

private:
    struct Card
    {
        juce::String heading;
        int headingY = 0;
        juce::Rectangle<int> box;
        std::vector<std::pair<juce::String, juce::Rectangle<int>>> notes;
    };
    struct Label { juce::String text; juce::Rectangle<int> area; bool enabled; };

    void addRow (const Row& r, juce::Rectangle<int> b)
    {
        if (r.kind == Kind::Action)
        {
            auto a = std::make_unique<ActionRow> (r.label, [this, id = r.id] { owner.act (id); });
            a->setBounds (b);
            a->setEnabled (r.enabled);
            addAndMakeVisible (*a);
            controls.push_back (std::move (a));
            return;
        }
        if (r.kind == Kind::Switch)
        {
            auto t = std::make_unique<juce::ToggleButton> (r.label);
            t->setToggleState (r.on, juce::dontSendNotification);
            t->setEnabled (r.enabled);
            t->setBounds (b.reduced (16, 6));
            t->onClick = [this, id = r.id] { owner.act (id); };
            addAndMakeVisible (*t);
            controls.push_back (std::move (t));
            return;
        }
        const int cw = juce::jmin (280, b.getWidth() / 2);
        labels.push_back ({ r.label, b.reduced (16, 0).withTrimmedRight (cw + 12), r.enabled });
        auto c = std::make_unique<juce::ComboBox>();
        int selected = 0;
        for (const auto& i : r.choices)
        {
            c->addItem (i.text, i.id);
            c->setItemEnabled (i.id, i.enabled);
            if (i.ticked)
                selected = i.id;
        }
        c->setTextWhenNothingSelected (tr ("Choose ..."));
        c->setSelectedId (selected, juce::dontSendNotification);
        c->setEnabled (r.enabled);
        c->setBounds (b.removeFromRight (cw + 12).reduced (12, 7).withTrimmedLeft (0));
        auto* raw = c.get();
        c->onChange = [this, raw] { if (raw->getSelectedId() != 0) owner.act (raw->getSelectedId()); };
        addAndMakeVisible (*c);
        controls.push_back (std::move (c));
    }

    SettingsPanel& owner;
    std::vector<Group> groups;
    std::vector<std::unique_ptr<juce::Component>> controls;
    std::vector<Card> layoutCards;
    std::vector<Label> labels;
    juce::String pageTitle;

public:
    void clearLabels() { labels.clear(); }
};

//============================================================================== sidebar
class SettingsPanel::Sidebar : public juce::Component
{
public:
    explicit Sidebar (SettingsPanel& p) : owner (p) {}
    void paint (juce::Graphics& g) override
    {
        const auto names = owner.getPageNames();
        const juce::Colour icons[] = { colours::brand0, colours::green, colours::orange, colours::red, colours::cyan,
                                       colours::learn, colours::blue, juce::Colour (0xff8e8e93) };
        for (int i = 0; i < names.size(); ++i)
        {
            const auto r = rowBounds (i).toFloat();
            if (i == owner.getPage())
            {
                g.setGradientFill (LooperLookAndFeel::brandGradient (r, 0.55f));
                g.fillRoundedRectangle (r, 8.0f);
            }
            else if (i == hover)
            {
                g.setColour (juce::Colours::white.withAlpha (0.05f));
                g.fillRoundedRectangle (r, 8.0f);
            }
            const auto icon = juce::Rectangle<float> (r.getX() + 10.0f, r.getCentreY() - 11.0f, 22.0f, 22.0f);
            juce::ColourGradient ig (icons[i % 8].brighter (0.2f), icon.getX(), icon.getY(), icons[i % 8].darker (0.3f), icon.getX(), icon.getBottom(), false);
            g.setGradientFill (ig);
            g.fillRoundedRectangle (icon, 6.0f);
            g.setColour (juce::Colours::white);
            g.setFont (LooperLookAndFeel::font (12.0f, true));
            g.drawText (names[i].substring (0, 1).toUpperCase(), icon, juce::Justification::centred);
            g.setFont (LooperLookAndFeel::font (14.0f, i == owner.getPage()));
            g.drawText (names[i], r.withTrimmedLeft (42.0f), juce::Justification::centredLeft, true);
        }
    }
    juce::Rectangle<int> rowBounds (int i) const { return { 8, 12 + i * 36, getWidth() - 16, 32 }; }
    int rowAt (juce::Point<int> p) const
    {
        for (int i = 0; i < owner.getPageNames().size(); ++i)
            if (rowBounds (i).contains (p))
                return i;
        return -1;
    }
    void mouseMove (const juce::MouseEvent& e) override { const int h = rowAt (e.getPosition()); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override { const int r = rowAt (e.getPosition()); if (r >= 0) owner.showPage (r); }
private:
    SettingsPanel& owner;
    int hover = -1;
};

//============================================================================== SettingsPanel
SettingsPanel::SettingsPanel (Builder build, Handler handle) : builder (std::move (build)), handler (std::move (handle))
{
    sidebar = std::make_unique<Sidebar> (*this);
    addAndMakeVisible (*sidebar);
    content = std::make_unique<Content> (*this);
    view.setViewedComponent (content.get(), false);
    view.setScrollBarsShown (true, false);
    view.setScrollBarThickness (8);
    addAndMakeVisible (view);
    readMenu();
    setSize (820, 600);
}

SettingsPanel::~SettingsPanel()
{
    view.setViewedComponent (nullptr, false);
}

void SettingsPanel::readMenu()
{
    toggles.clear();
    const auto menu = builder ? builder (toggles) : juce::PopupMenu();
    pages.clear();
    for (const auto& i : itemsOf (menu))
    {
        if (i.header)
        {
            pages.push_back ({ i.text, {} });
            continue;
        }
        if (pages.empty())
            pages.push_back ({ tr ("General"), {} });
        auto& m = pages.back().menu;
        if (i.separator)
            m.addSeparator();
        else if (i.sub != nullptr)
            m.addSubMenu (i.text, *i.sub, i.enabled);
        else
            m.addItem (i.id, i.text, i.enabled, i.ticked);
    }
    page = juce::jlimit (0, juce::jmax (0, (int) pages.size() - 1), page);
}

juce::StringArray SettingsPanel::getPageNames() const
{
    juce::StringArray s;
    for (const auto& p : pages)
        s.add (p.name);
    return s;
}

void SettingsPanel::showPage (int index)
{
    page = juce::jlimit (0, juce::jmax (0, (int) pages.size() - 1), index);
    content->clearLabels();
    if (! pages.empty())
        content->build (pages[(size_t) page].menu, pages[(size_t) page].name, juce::jmax (360, view.getWidth() - view.getScrollBarThickness()));
    sidebar->repaint();
}

void SettingsPanel::showPage (const juce::String& name)
{
    const int i = getPageNames().indexOf (name);
    showPage (i >= 0 ? i : 0);
}

void SettingsPanel::rebuild()
{
    const auto pos = view.getViewPosition();
    readMenu();
    showPage (page);
    view.setViewPosition (pos);
}

void SettingsPanel::act (int id)
{
    if (handler)
        handler (id);
    juce::Component::SafePointer<SettingsPanel> safe (this);
    juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->rebuild(); });
}

void SettingsPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff131318));
    const auto side = sidebar->getBounds().toFloat().reduced (6.0f);
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.fillRoundedRectangle (side, 14.0f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawRoundedRectangle (side, 14.0f, 1.0f);
}

void SettingsPanel::resized()
{
    auto b = getLocalBounds();
    sidebar->setBounds (b.removeFromLeft (210).reduced (4, 4));
    view.setBounds (b);
    showPage (page);
}
} // namespace livelooper
