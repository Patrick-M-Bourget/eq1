#include "HoverCard.h"

namespace eq1
{

HoverCard::HoverCard (PluginProcessor& p, BandEditing& e) : processor (p), editing (e) {}

void HoverCard::show (int newSlot, juce::Point<float> handle, juce::Rectangle<int> within)
{
    juce::ignoreUnused (handle, within);
    slot = newSlot;
    setVisible (true);
}

void HoverCard::hide()
{
    slot = 0;
    pointerOver = false;
    setVisible (false);
}

void HoverCard::mouseEnter (const juce::MouseEvent&) { pointerOver = true; }

void HoverCard::mouseExit (const juce::MouseEvent& e)
{
    // Leaving one of its controls for the card itself is still on it.
    pointerOver = getLocalBounds().contains (e.getEventRelativeTo (this).position.toInt());
}

} // namespace eq1
