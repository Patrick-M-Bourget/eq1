#include "KeyboardControl.h"

#include "staple/LookAndFeel.h"

namespace eq1
{

KeyboardControl::KeyboardControl (EditHistory& h) : history (h) {}

KeyboardControl::~KeyboardControl()
{
    endHeld();
    for (auto& component : adopted)
        if (component != nullptr)
            component->removeKeyListener (this);
}

void KeyboardControl::adopt (juce::Component& root)
{
    for (auto* child : root.getChildren())
    {
        if (child->getWantsKeyboardFocus())
        {
            child->setHasFocusOutline (true);
            child->addKeyListener (this);
            adopted.emplace_back (child);
        }
        adopt (*child);
    }
}

bool KeyboardControl::keyPressed (const juce::KeyPress& key, juce::Component* origin)
{
    if (origin == nullptr)
        return false;
    if (origin != heldIn)
        endHeld();

    const int code = key.getKeyCode();
    const bool arrow = code == juce::KeyPress::upKey || code == juce::KeyPress::downKey || code == juce::KeyPress::leftKey
                       || code == juce::KeyPress::rightKey;
    const bool press = code == juce::KeyPress::spaceKey || code == juce::KeyPress::returnKey;
    // In a text field the arrows, Space and Return are typing, not moving around the editor.
    if (code == juce::KeyPress::tabKey || ((arrow || press) && dynamic_cast<juce::TextEditor*> (origin) == nullptr))
        staple::LookAndFeel::keyUsed (*origin);

    const bool plain = ! key.getModifiers().isAnyModifierKeyDown();
    if (auto* button = dynamic_cast<juce::Button*> (origin); button != nullptr && press && plain && button->isEnabled())
    {
        if (code == juce::KeyPress::returnKey)
        {
            click (*button);
            return true;
        }
        if (pressed == nullptr)
        {
            heldIn = origin;
            pressed = button;
            button->setState (juce::Button::buttonDown);
        }
        return true;
    }
    if (auto* box = dynamic_cast<juce::ComboBox*> (origin); box != nullptr && arrow && plain && box->isEnabled())
    {
        heldIn = origin;
        stepComboBox (*box, code == juce::KeyPress::upKey || code == juce::KeyPress::leftKey ? -1 : 1);
        return true;
    }
    return false;
}

bool KeyboardControl::keyStateChanged (bool isKeyDown, juce::Component*)
{
    if (! isKeyDown)
        endHeld();
    return false;
}

void KeyboardControl::click (juce::Button& button)
{
    if (button.getClickingTogglesState())
        button.setToggleState (! button.getToggleState(), juce::sendNotificationSync);
    else
        juce::NullCheckedInvocation::invoke (button.onClick);
}

bool KeyboardControl::stepComboBox (juce::ComboBox& box, int direction)
{
    for (int i = box.getSelectedItemIndex() + direction; i >= 0 && i < box.getNumItems(); i += direction)
    {
        if (! box.isItemEnabled (box.getItemId (i)))
            continue;
        if (! stepping)
        {
            history.beginTransaction();
            stepping = true;
        }
        box.setSelectedItemIndex (i, juce::sendNotificationSync);
        return true;
    }
    return false;
}

void KeyboardControl::endHeld()
{
    heldIn = nullptr;
    if (std::exchange (stepping, false))
        history.endTransaction();
    if (auto* button = pressed.getComponent())
    {
        pressed = nullptr;
        button->setState (juce::Button::buttonNormal);
        click (*button);
    }
}

} // namespace eq1
