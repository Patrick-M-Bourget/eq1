#pragma once

#include "EditHistory.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

namespace eq1
{

// The keys every control in the editor shares, whatever its kind. Each component that keyboard focus
// reaches gets a focus ring (staple::LookAndFeel draws it), shown once Tab, Shift+Tab, an arrow,
// Space or Return is pressed in it. Space or Return toggles a toggle button or presses a button; a
// button stays down while Space is held, so Detection Audition plays until it is released. Up and
// down (left and right too) step a ComboBox to the previous or next item at once, a held key being
// one undo step. Sliders and the EQ display step themselves (KeyboardSlider, EqDisplay). A click on a
// button or a ComboBox doesn't take keyboard focus.
//
// A held key's step ends with the key's release, or with a key pressed in another control.
class KeyboardControl final : private juce::KeyListener
{
public:
    explicit KeyboardControl (EditHistory& history);
    ~KeyboardControl() override;

    // Takes in every component under root, at any depth, that wants keyboard focus.
    void adopt (juce::Component& root);

private:
    bool keyPressed (const juce::KeyPress& key, juce::Component* origin) override;
    bool keyStateChanged (bool isKeyDown, juce::Component* origin) override;

    // Space or Return on a button: what a click does, at once.
    static void click (juce::Button& button);
    bool stepComboBox (juce::ComboBox& box, int direction);
    void endHeld();

    EditHistory& history;
    std::vector<juce::Component::SafePointer<juce::Component>> adopted;
    juce::Component::SafePointer<juce::Component> heldIn; // where the held key was pressed
    juce::Component::SafePointer<juce::Button> pressed;   // held down by Space
    bool stepping = false;                                 // a ComboBox's undo step is open
};

} // namespace eq1
