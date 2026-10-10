#pragma once

#include "../Icons.h"
#include "../Tokens.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <map>
#include <optional>

namespace staple
{

// Staple's Edge selector (HANDOFF.md §4 "Band panel"): a ComboBox 104 x 34 flush with one side of a
// panel and rounded (r3) only on its inner side, on edgeSelectorBase with a hairline in a caller-set
// colour, brightest at the inner edge, and a 12 % wash of it from the inner top corner. It shows the
// selected item's icon (20 x 12) and dot (5 px) before its name in fs4 at 500, and lists its items in
// the ComboBox's own PopupMenu with the same icons. The arrow keys step it, at once, one undo step each.
class EdgeSelector : public juce::ComboBox
{
public:
    // The panel side it is flush with.
    enum class Side
    {
        left,
        right
    };

    explicit EdgeSelector (const juce::String& name = {}, Side side = Side::left);

    void setSide (Side newSide);
    Side getSide() const { return side; }
    // The hairline's and the wash's colour (the Band colour).
    void setEdgeColour (juce::Colour colour);
    // The icon's colour: the Band colour for Shape, text1 for Stereo Placement.
    void setIconColour (juce::Colour colour);

    using juce::ComboBox::addItem;
    // An item with its icon, and optionally a dot in front of its name and a context icon drawn under
    // the icon at 30 % (a Stereo Placement's unprocessed side).
    void addItem (const juce::String& text, int itemId, Icon icon, std::optional<juce::Colour> dot = std::nullopt,
                  std::optional<Icon> context = std::nullopt);

    void paint (juce::Graphics& g) override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    Side side;
    juce::Colour edgeColour = tokens::colour::text1, iconColour = tokens::colour::text1;
    std::map<int, Icon> icons, contexts;
    std::map<int, juce::Colour> dots;
};

} // namespace staple
