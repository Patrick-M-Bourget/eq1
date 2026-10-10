#include "IconButton.h"

#include "../Light.h"

namespace staple
{

namespace
{
namespace colour = tokens::colour;
} // namespace

IconButton::IconButton (const juce::String& name, Icon i) : juce::Button (name), icon (i)
{
    setHasFocusOutline (true);
    setMouseClickGrabsKeyboardFocus (false);
}

float IconButton::getIconSide() const
{
    return iconSize > 0.0f ? iconSize : static_cast<float> (std::min (getWidth(), getHeight())) * 2.0f / 3.0f;
}

void IconButton::setLitColour (juce::Colour colour)
{
    litColour = colour;
    repaint();
}

void IconButton::setRestColour (juce::Colour colour)
{
    restColour = colour;
    repaint();
}

void IconButton::setIconSize (float size)
{
    iconSize = size;
    repaint();
}

void IconButton::setIconRotation (float radians)
{
    iconRotation = radians;
    repaint();
}

void IconButton::setOffLook (bool o)
{
    offLook = o;
    repaint();
}

void IconButton::setMomentary (bool m)
{
    momentary = m;
    if (momentary)
        setClickingTogglesState (false);
}

bool IconButton::isLit() const { return momentary ? held : (getToggleState() && ! offLook); }

bool IconButton::isOff() const { return offLook && getToggleState(); }

void IconButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const float alpha = enabledAlpha (*this);
    const float side = static_cast<float> (std::min (getWidth(), getHeight()));
    const auto square = getLocalBounds().toFloat().withSizeKeepingCentre (side, side);
    // The icon's 16 px grid in a 24 px button, in proportion at other sizes.
    const float iconSide = getIconSide();
    const auto iconArea = square.withSizeKeepingCentre (iconSide, iconSide);

    juce::Colour ink;
    if (isOff())
    {
        g.setColour (lit (colour::stateOffBg, highlighted, down).withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (square, tokens::size::r2);
        ink = lit (colour::stateOff, highlighted, down);
    }
    else if (isLit())
        ink = lit (litColour, highlighted, down);
    else
        ink = highlighted || down ? colour::text1 : restColour;
    const juce::Graphics::ScopedSaveState saved (g);
    if (iconRotation != 0.0f)
        g.addTransform (juce::AffineTransform::rotation (iconRotation, iconArea.getCentreX(), iconArea.getCentreY()));
    drawIcon (g, icon, iconArea, ink.withMultipliedAlpha (alpha));
}

void IconButton::mouseDown (const juce::MouseEvent& e)
{
    juce::Button::mouseDown (e);
    if (momentary && isEnabled() && e.mods.isLeftButtonDown())
        setHeld (true);
}

void IconButton::mouseUp (const juce::MouseEvent& e)
{
    juce::Button::mouseUp (e);
    setHeld (false);
}

void IconButton::enablementChanged()
{
    if (! isEnabled())
        setHeld (false);
    juce::Button::enablementChanged();
}

void IconButton::setHeld (bool nowHeld)
{
    if (nowHeld == held)
        return;
    held = nowHeld;
    repaint();
    if (held && onPress != nullptr)
        onPress();
    else if (! held && onRelease != nullptr)
        onRelease();
}

} // namespace staple
