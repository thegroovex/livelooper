#pragma once

#include "LooperLookAndFeel.h"

namespace livelooper
{
/**
    v1.6 SETUP window in the style of Apple's System Settings: a sidebar with the pages, grouped
    rows on the right (switches, pop-up buttons, actions), short labels, quiet notes.

    It is generated from the SETUP menu structure (section header = page, sub-menu = group,
    ticked radio items = pop-up, toggle ids = switch, disabled items = notes), so every
    setting stays reachable and is handled by exactly the same code as before.
*/
class SettingsPanel : public juce::Component
{
public:
    using Builder = std::function<juce::PopupMenu (juce::Array<int>& toggleIds)>;
    using Handler = std::function<void (int result)>;

    SettingsPanel (Builder build, Handler handle);
    ~SettingsPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    juce::StringArray getPageNames() const;
    void showPage (int index);
    void showPage (const juce::String& name);
    int getPage() const noexcept { return page; }
    /** Re-reads the menu (after a change) and keeps the page. */
    void rebuild();
    /** Number of rows on the current page (tests). */
    int getNumRows() const noexcept { return numRows; }

private:
    struct Page { juce::String name; juce::PopupMenu menu; };
    class Content;
    class Sidebar;

    Builder builder;
    Handler handler;
    juce::Array<int> toggles;
    std::vector<Page> pages;
    int page = 0, numRows = 0;
    std::unique_ptr<Sidebar> sidebar;
    juce::Viewport view;
    std::unique_ptr<Content> content;
    void readMenu();
    void act (int id);
};
} // namespace livelooper
