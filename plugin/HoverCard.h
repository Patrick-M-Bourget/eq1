#pragma once

#include "staple/controls/IconButton.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace eq1
{

class PluginProcessor;
class BandEditing;

// The Hover Card (HANDOFF.md §2 "HoverCard", §5.2): quick controls for the Band whose handle the
// pointer rests on, which edit that Band alone and never select it. Left, Bypass and the Shape; in the
// middle Frequency, Gain and Q; right, Delete and ▾, which opens the Band menu (BandMenu.h) for that
// one Band. Every edit is one undo step. The EQ display shows, places and hides it (EqDisplay.h). It is
// for the mouse only: neither it nor its controls take keyboard focus, and the Band panel serves the
// keyboard. A screen reader reads it as a group, "Band 4 quick controls".
class HoverCard final : public juce::Component
{
public:
    HoverCard (PluginProcessor& processor, BandEditing& editing);
    ~HoverCard() override;

    // The Band it shows, or 0 while hidden.
    int shownSlot() const { return slot; }

    // Shows the card for a Band whose handle is at handle, or moves it there: centred 18 px above it, or
    // below it when the handle is near the top of within (the display), and kept 6 px inside its left and
    // right; both in the card's parent's coordinates. It reads the Band afresh each time. Hides it.
    void show (int slot, juce::Point<float> handle, juce::Rectangle<int> within);
    void hide();

    // The card itself, in its parent's coordinates: the component is wider, for its shadow and arrow tip.
    juce::Rectangle<int> body() const { return bodyArea() + getPosition(); }

    // The pointer is over the card.
    bool isPointerOver() const { return pointerOver; }
    // It stays up, whatever the pointer does, while its menu is open.
    bool isHeld() const { return menuOpen; }

    // What ▾ opens: the Band menu for the shown Band alone. Its Delete deletes that Band, and its Split
    // and Paste leave the selection as it is.
    juce::PopupMenu menu();
    // The Band menu's Select All.
    std::function<void()> onSelectAll;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool hitTest (int x, int y) override;
    void mouseEnter (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;

private:
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
    // The names and states of its controls for the shown Band.
    void refresh();
    void openMenu();

    // Room around the body for its shadow and arrow tip, which the pointer passes through.
    static constexpr int margin = 32;
    juce::Rectangle<int> bodyArea() const { return getLocalBounds().reduced (margin); }

    PluginProcessor& processor;
    BandEditing& editing;
    int slot = 0;
    bool pointerOver = false, menuOpen = false;
    bool above = true; // the card is above its handle, its tip pointing down
    float tipX = 0.0f; // the handle's x, in the card's coordinates

    // Named apart from the Band panel's buttons; titled for the shown Band by refresh().
    staple::IconButton bypass { "Hover Card Bypass", staple::Icon::power }, deleteButton { "Hover Card Delete", staple::Icon::close },
        more { "Hover Card Menu", staple::Icon::dropdown };
};

} // namespace eq1
