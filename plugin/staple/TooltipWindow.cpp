#include "TooltipWindow.h"

#include "controls/Overlay.h"

namespace staple
{

namespace
{
constexpr int gap = 6;
// A control at least this many Tooltips tall has the Tooltip at its side.
constexpr int sidewaysHeightRatio = 4;
// For this long after hiding a tip, JUCE's timer shows the tip under the pointer at once, without a rest.
constexpr juce::uint32 reshowAtOnceMs = 500;
} // namespace

TooltipWindow::TooltipWindow (juce::Component* parent, TipSource tipFor) : juce::TooltipWindow (parent), tipSource (std::move (tipFor))
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
    if (isDismissed (c) || ! TipWatchdog::isPointerOver (c, TipWatchdog::pointer()))
        return {};
    return tipSource != nullptr ? tipSource (c) : juce::TooltipWindow::getTipFor (c);
}

bool TooltipWindow::isDismissed (juce::Component& c)
{
    if (dismissed.control != &c)
        return false;
    if (! dismissed.moved || juce::Time::getApproximateMillisecondCounter() < dismissed.time + reshowAtOnceMs)
        return true;
    dismissed.control = nullptr;
    return false;
}

void TooltipWindow::dismiss (const juce::MouseEvent& e)
{
    endFade();
    hideTip();
    // After hideTip, which starts JUCE's reshowAtOnceMs.
    dismissed = { e.eventComponent, e.getScreenPosition(), juce::Time::getApproximateMillisecondCounter(), false };
}

void TooltipWindow::moved() { placeByControl(); }

void TooltipWindow::resized() { placeByControl(); }

void TooltipWindow::visibilityChanged()
{
    // JUCE asks for a control's tip just before showing it.
    if (isVisible())
    {
        showingFor = control;
        watchdog.start();
    }
    else
    {
        watchdog.stop();
    }
}

void TooltipWindow::watch (juce::Point<float> pointer)
{
    if (isVisible() && (showingFor == nullptr || ! TipWatchdog::isPointerOver (*showingFor, pointer)))
        fadeOut();
}

void TooltipWindow::mouseMove (const juce::MouseEvent& e)
{
    if (dismissed.control != nullptr
        && (e.eventComponent != dismissed.control || e.getScreenPosition().toFloat().getDistanceFrom (dismissed.at.toFloat()) > dismissalMovePx))
        dismissed.moved = true;
    pointerOver (e.eventComponent);
}

void TooltipWindow::mouseExit (const juce::MouseEvent& e)
{
    if (e.eventComponent == showingFor)
        pointerOver (nullptr);
}

void TooltipWindow::mouseDown (const juce::MouseEvent& e) { dismiss (e); }

void TooltipWindow::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails&) { dismiss (e); }

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
