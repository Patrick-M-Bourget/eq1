#include "IconButton.h"

namespace staple
{

namespace
{
namespace colour = tokens::colour;
namespace motion = tokens::motion;

juce::Colour brightened (juce::Colour c, bool highlighted, bool down)
{
    return c.withMultipliedBrightness (down ? motion::pressedBrightness : highlighted ? motion::hoverBrightness : 1.0f);
}
} // namespace

IconButton::IconButton (const juce::String& name, Icon i) : juce::Button (name), icon (i)
{
    setHasFocusOutline (true);
}

void IconButton::setIcon (Icon newIcon)
{
    icon = newIcon;
    repaint();
}

void IconButton::setLitColour (juce::Colour colour)
{
    litColour = colour;
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
    const float alpha = isEnabled() ? 1.0f : motion::disabledAlpha;
    const float side = static_cast<float> (std::min (getWidth(), getHeight()));
    const auto square = getLocalBounds().toFloat().withSizeKeepingCentre (side, side);
    // The icon's 16 px grid in a 24 px button, in proportion at other sizes.
    const auto iconArea = square.withSizeKeepingCentre (side * 2.0f / 3.0f, side * 2.0f / 3.0f);

    juce::Colour ink;
    if (isOff())
    {
        g.setColour (brightened (colour::stateOffBg, highlighted, down).withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (square, tokens::size::r2);
        ink = brightened (colour::stateOff, highlighted, down);
    }
    else if (isLit())
        ink = brightened (litColour, highlighted, down);
    else
        ink = highlighted || down ? colour::text1 : colour::text3;
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
