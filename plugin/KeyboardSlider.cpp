#include "KeyboardSlider.h"

#include "staple/LookAndFeel.h"

namespace eq1
{

KeyboardSlider::KeyboardSlider (const juce::String& name) : juce::Slider (name)
{
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (false);
    // One stop for Tab: its text box, which a double-click edits, isn't another.
    setFocusContainerType (FocusContainerType::keyboardFocusContainer);
}

KeyboardSlider::~KeyboardSlider()
{
    endHeldStep();
}

bool KeyboardSlider::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    if ((mods.getRawFlags() & ~juce::ModifierKeys::shiftModifier & juce::ModifierKeys::allKeyboardModifiers) != 0)
        return false;
    const int code = key.getKeyCode();
    const int direction = code == juce::KeyPress::rightKey || code == juce::KeyPress::upKey     ? 1
                          : code == juce::KeyPress::leftKey || code == juce::KeyPress::downKey ? -1
                                                                                                : 0;
    if (direction == 0 || ! isEnabled())
        return false;
    staple::LookAndFeel::keyUsed (*this);

    const double from = getValue();
    const double proportion = valueToProportionOfLength (from) + direction * (mods.isShiftDown() ? 0.002 : 0.01);
    double to = proportionOfLengthToValue (juce::jlimit (0.0, 1.0, proportion));
    // A step smaller than the interval would round back to where it started.
    if (const double interval = getInterval(); interval > 0.0 && std::abs (to - from) < interval)
        to = juce::jlimit (getMinimum(), getMaximum(), from + direction * interval);
    if (landStep != nullptr)
        to = landStep (from, to);
    if (! held)
        held.emplace (*this);
    setValue (to, juce::sendNotificationSync);
    return true;
}

bool KeyboardSlider::keyStateChanged (bool isKeyDown)
{
    if (! isKeyDown)
        endHeldStep();
    return false;
}

void KeyboardSlider::focusLost (FocusChangeType cause)
{
    endHeldStep();
    juce::Slider::focusLost (cause);
}

void KeyboardSlider::endHeldStep()
{
    held.reset();
}

} // namespace eq1
