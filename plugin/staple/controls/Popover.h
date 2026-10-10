#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace staple
{

// Staple's popover (HANDOFF.md §4 "Menus / popovers / cards"): a card on menu with a 1 px line2 edge,
// r3 corners and shadow1, opened beside the component that opens it, inside that component's overlay
// layer (Overlay.h), popping in over dur2 from 3 px lower at 98.5 % scale. Opened from the keyboard,
// focus moves into it; opened by a click, focus stays where it was. A click anywhere outside it and its
// opener, or Esc, closes it, and focus inside it goes back to the opener. Its contents (the children a
// caller adds, laid out in getCardBounds()) are in the focus order only while it is open; its own
// chrome is never a focus stop.
class Popover : public juce::Component, private juce::Timer, private juce::KeyListener
{
public:
    enum class Placement
    {
        below,
        above
    };

    Popover();
    ~Popover() override;

    // The card's size; the component is larger by the shadow's margins.
    void setCardSize (int width, int height);
    juce::Rectangle<int> getCardBounds() const;

    // Opens beside opener, on the preferred side if there is room there, else on the other.
    void open (juce::Component& opener, Placement preferred = Placement::below);
    void close();
    bool isOpen() const { return opener != nullptr; }

    std::function<void()> onClose;

    void paint (juce::Graphics& g) override;
    bool hitTest (int x, int y) override;
    bool keyPressed (const juce::KeyPress& key) override;
    // Every press in the app, as a global mouse listener, while it is open.
    void mouseDown (const juce::MouseEvent& event) override;

private:
    void timerCallback() override;
    // Esc from wherever focus is in the opener's window.
    bool keyPressed (const juce::KeyPress& key, juce::Component* origin) override;
    juce::Component::SafePointer<juce::Component> escapeFrom;

    juce::Component::SafePointer<juce::Component> opener;
    double openedAt = 0.0;
};

} // namespace staple
