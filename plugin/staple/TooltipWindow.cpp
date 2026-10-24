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

TooltipWindow::TooltipWindow (juce::Component* parent) : juce::TooltipWindow (parent)
{
    fading.setInterceptsMouseClicks (false, false);
    fading.setAccessible (false);
    fading.setAlwaysOnTop (true);
    if (parent != nullptr)
        parent->addChildComponent (fading);
    fade.apply = [this] (float alpha) {
        fading.setAlpha (alpha);
        if (alpha <= 0.0f)
            fading.setVisible (false);
    };
}

juce::String TooltipWindow::getTipFor (juce::Component& c)
{
    control = &c;
    return juce::TooltipWindow::getTipFor (c);
}

void TooltipWindow::moved() { placeByControl(); }

void TooltipWindow::resized() { placeByControl(); }

void TooltipWindow::visibilityChanged()
{
    // JUCE asks for a control's tip just before showing it.
    if (isVisible())
        showingFor = control;
}

void TooltipWindow::mouseMove (const juce::MouseEvent& e) { pointerOver (e.eventComponent); }

void TooltipWindow::mouseExit (const juce::MouseEvent& e)
{
    if (e.eventComponent == showingFor)
        pointerOver (nullptr);
}

void TooltipWindow::mouseDown (const juce::MouseEvent&)
{
    endFade();
    hideTip();
}

void TooltipWindow::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&)
{
    endFade();
    hideTip();
}

void TooltipWindow::pointerOver (juce::Component* under)
{
    if (isVisible() && under != showingFor)
        fadeOut();
}

void TooltipWindow::fadeOut()
{
    if (getParentComponent() != nullptr)
    {
        fading.picture = createComponentSnapshot (getLocalBounds(), true, juce::Component::getApproximateScaleFactorForComponent (this));
        fading.setBounds (getBounds());
        fading.setVisible (true);
        fading.toFront (false);
        fade.jump (1.0f);
        fade.towards (0.0f);
    }
    hideTip();
}

void TooltipWindow::endFade() { fade.jump (0.0f); }

void TooltipWindow::Fading::paint (juce::Graphics& g) { g.drawImage (picture, getLocalBounds().toFloat()); }

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
