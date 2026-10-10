#include "DisplayRangeChip.h"

#include "Accessibility.h"
#include "PluginProcessor.h"

namespace eq1
{

namespace
{
constexpr int ranges[] = { 6, 12, 30 };
// The menu's width and rows (HANDOFF.md, the Display Range listbox).
constexpr int menuWidth = 93, menuRowHeight = 26;

juce::String rangeText (int rangeDb)
{
    return juce::String::charToString (0x00B1) + juce::String (rangeDb) + " dB";
}
} // namespace

DisplayRangeChip::DisplayRangeChip (PluginProcessor& p)
    : staple::TextChip (rangeText (p.displayRangeDb()), Look::filled, staple::tokens::size::fs2), processor (p)
{
    setName ("Display Range");
    setTitle ("Display Range");
    setChevron (true);
    startTimerHz (10);
}

juce::PopupMenu DisplayRangeChip::menu()
{
    juce::PopupMenu popup;
    const juce::Component::SafePointer<DisplayRangeChip> chip (this);
    for (int range : ranges)
    {
        juce::PopupMenu::Item item (rangeText (range));
        item.setTicked (range == processor.displayRangeDb());
        item.setAction ([chip, range] {
            if (chip == nullptr)
                return;
            chip->processor.setDisplayRangeDb (range);
            chip->timerCallback();
        });
        popup.addItem (std::move (item));
    }
    return popup;
}

void DisplayRangeChip::clicked()
{
    auto popup = menu();
    // A menu is a window of its own: it draws with the editor's look only when given it.
    popup.setLookAndFeel (&getLookAndFeel());
    popup.showMenuAsync (juce::PopupMenu::Options()
                             .withTargetComponent (this)
                             .withDeletionCheck (*this)
                             .withMinimumWidth (menuWidth)
                             .withStandardItemHeight (menuRowHeight));
}

void DisplayRangeChip::timerCallback()
{
    const auto text = rangeText (processor.displayRangeDb());
    if (text == getButtonText())
        return;
    setButtonText (text);
    // Its width follows its text; it keeps its right edge.
    if (! getBounds().isEmpty())
        setBounds (getBounds().withLeft (getRight() - getIdealWidth()));
    if (auto* handler = getAccessibilityHandler())
        handler->notifyAccessibilityEvent (juce::AccessibilityEvent::valueChanged);
}

std::unique_ptr<juce::AccessibilityHandler> DisplayRangeChip::createAccessibilityHandler()
{
    return accessibility::handler (*this, juce::AccessibilityRole::button, [this] { return getButtonText(); }, [this] { triggerClick(); });
}

} // namespace eq1
