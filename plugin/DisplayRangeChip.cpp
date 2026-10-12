#include "DisplayRangeChip.h"

#include "PluginProcessor.h"
#include "ShowMenu.h"

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
    : staple::TextChip (rangeText (p.displayRangeDb()), Look::filled, staple::tokens::size::fs2),
      EditorTick::Client (static_cast<juce::Component&> (*this)), processor (p)
{
    setName ("Display Range");
    setTitle ("Display Range");
    setChevron (true);
    setTextIsValue (true);
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
            chip->follow();
        });
        popup.addItem (std::move (item));
    }
    return popup;
}

void DisplayRangeChip::clicked()
{
    showMenu (menu(), *this,
              juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (menuWidth).withStandardItemHeight (menuRowHeight));
}

void DisplayRangeChip::follow()
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

} // namespace eq1
