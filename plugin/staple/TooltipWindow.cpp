#include "TooltipWindow.h"

#include "controls/Overlay.h"

namespace staple
{

namespace
{
constexpr int gap = 6;
// A control at least this many Tooltips tall has the Tooltip at its side.
constexpr int sidewaysHeightRatio = 4;
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
    const auto controlArea = control != nullptr && control->isShowing() ? parent->getLocalArea (control, control->getLocalBounds())
                                                                        : juce::Rectangle<int> (pointer, pointer);
    if (controlArea.getHeight() >= sidewaysHeightRatio * height)
        setBounds (juce::Rectangle<int> (controlArea.getX() - gap - width, pointer.y - height / 2, width, height).constrainedWithin (parent->getLocalBounds()));
    else
        setBounds (placeCardBy (controlArea, { width, height }, parent->getLocalBounds()));
}

} // namespace staple
