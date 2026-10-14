#include "TooltipWindow.h"

namespace staple
{

namespace
{
constexpr int gap = 6;
// A control at least this many Tooltips tall has it at its side.
constexpr int sideways = 4;
} // namespace

TooltipWindow::TooltipWindow (juce::Component* parent) : juce::TooltipWindow (parent) {}

juce::String TooltipWindow::getTipFor (juce::Component& c)
{
    control = &c;
    return juce::TooltipWindow::getTipFor (c);
}

void TooltipWindow::moved() { placeByControl(); }

void TooltipWindow::resized() { placeByControl(); }

void TooltipWindow::placeByControl()
{
    auto* parent = getParentComponent();
    if (placing || parent == nullptr || getWidth() == 0)
        return;
    const juce::ScopedValueSetter<bool> guard (placing, true);
    const int width = getWidth(), height = getHeight();
    // staple::LookAndFeel::getTooltipBounds centres JUCE's placement on the pointer.
    const auto pointer = getBounds().getCentre();
    const auto at = control != nullptr && control->isShowing() ? parent->getLocalArea (control, control->getLocalBounds())
                                                               : juce::Rectangle<int> (pointer, pointer);
    juce::Rectangle<int> bounds (width, height);
    if (at.getHeight() >= sideways * height)
        bounds.setPosition (at.getX() - gap - width, pointer.y - height / 2);
    else
    {
        bounds.setPosition (at.getCentreX() - width / 2, at.getBottom() + gap);
        if (bounds.getBottom() > parent->getHeight())
            bounds.setY (at.getY() - gap - height);
    }
    setBounds (bounds.constrainedWithin (parent->getLocalBounds()));
}

} // namespace staple
