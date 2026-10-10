#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace eq1
{

// The editor's look. It draws the keyboard focus ring around the focused control or Band handle
// (every control that keyboard focus reaches has setHasFocusOutline) once the keyboard is in use:
// after Tab, Shift+Tab or an arrow key, and not after a mouse click.
class EditorLookAndFeel : public juce::LookAndFeel_V4
{
public:
    std::unique_ptr<juce::FocusOutline> createFocusOutlineForComponent (juce::Component& component) override;

    bool isFocusRingShown() const { return focusRingShown; }
    // Shows the ring after a key, hides it after a click.
    void showFocusRing (bool shown);

    // A key that moves focus or steps a control, pressed in component: shows the ring if component
    // is drawn with an EditorLookAndFeel.
    static void keyUsed (juce::Component& component);

private:
    bool focusRingShown = false;
};

} // namespace eq1
