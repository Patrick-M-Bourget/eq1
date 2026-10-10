#include "KeyboardSlider.h"

#include "Accessibility.h"
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

void KeyboardSlider::describe (const juce::RangedAudioParameter& parameter)
{
    setTitle (parameter.getName (100));
    spokenValue = [&parameter] (double value) { return accessibility::spokenValue (parameter, parameter.convertTo0to1 (static_cast<float> (value))); };
}

void KeyboardSlider::childrenChanged()
{
    // The text box shows the value the slider itself is read with.
    for (auto* child : getChildren())
        child->setAccessible (false);
}

namespace
{
class SliderHandler final : public juce::AccessibilityHandler
{
public:
    explicit SliderHandler (KeyboardSlider& s)
        : juce::AccessibilityHandler (s, juce::AccessibilityRole::slider, {}, Interfaces { std::make_unique<Value> (s) })
    {
    }

private:
    class Value final : public juce::AccessibilityValueInterface
    {
    public:
        explicit Value (KeyboardSlider& s) : slider (s) {}
        bool isReadOnly() const override { return false; }
        double getCurrentValue() const override { return slider.getValue(); }
        void setValue (double value) override
        {
            juce::Slider::ScopedDragNotification drag (slider);
            slider.setValue (value, juce::sendNotificationSync);
        }
        juce::String getCurrentValueAsString() const override
        {
            return slider.spokenValue != nullptr ? slider.spokenValue (slider.getValue()) : slider.getTextFromValue (slider.getValue());
        }
        void setValueAsString (const juce::String& text) override { setValue (slider.getValueFromText (text)); }
        AccessibleValueRange getRange() const override
        {
            const double interval = slider.getInterval();
            return { { slider.getMinimum(), slider.getMaximum() }, interval > 0.0 ? interval : (slider.getMaximum() - slider.getMinimum()) * 0.01 };
        }

    private:
        KeyboardSlider& slider;
    };
};
} // namespace

std::unique_ptr<juce::AccessibilityHandler> KeyboardSlider::createAccessibilityHandler()
{
    return std::make_unique<SliderHandler> (*this);
}

} // namespace eq1
