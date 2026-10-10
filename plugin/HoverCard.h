#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace eq1
{

class PluginProcessor;
class BandEditing;

// The Hover Card.
class HoverCard final : public juce::Component
{
public:
    HoverCard (PluginProcessor& processor, BandEditing& editing);

    // The Band it shows, or 0 while hidden.
    int shownSlot() const { return slot; }

    // Shows the card for a Band whose handle is at handle, inside within, both in the card's parent's
    // coordinates; hides it.
    void show (int slot, juce::Point<float> handle, juce::Rectangle<int> within);
    void hide();

    // The pointer is over the card.
    bool isPointerOver() const { return pointerOver; }

    void mouseEnter (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;

private:
    PluginProcessor& processor;
    BandEditing& editing;
    int slot = 0;
    bool pointerOver = false;
};

} // namespace eq1
