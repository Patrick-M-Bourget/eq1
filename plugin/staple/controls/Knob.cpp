#include "Knob.h"

#include "KnobTooltip.h"
#include "Overlay.h"
#include "ParseValue.h"

#include <cmath>

namespace staple
{

namespace
{
namespace knobs = tokens::knob;

bool isSmall (float diameter) { return diameter <= knobs::small; }

const juce::DropShadow& shadowFor (float diameter) { return isSmall (diameter) ? tokens::shadow::knobSmall : tokens::shadow::knob; }

// A position from 0 to 1 as an angle clockwise from 12 o'clock, in radians.
float angleOf (double proportion)
{
    return juce::degreesToRadians (knobs::sweepDegrees * (static_cast<float> (proportion) - 0.5f));
}
} // namespace

Knob::Knob (float d, const juce::String& name) : eq1::KeyboardSlider (name), diameter (d)
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setPopupMenuEnabled (false);
    setHasFocusOutline (true);
}

Knob::~Knob() = default;

int Knob::getIdealSize() const
{
    const auto& shadow = shadowFor (diameter);
    float margin = static_cast<float> (shadow.radius + shadow.offset.y);
    if (ring != nullptr)
        margin = std::max (margin, ringOffset + ringWidth / 2.0f);
    return static_cast<int> (std::ceil (diameter + 2.0f * margin));
}

void Knob::setArcColour (juce::Colour colour)
{
    arcColour = colour;
    repaint();
}

void Knob::setBipolar (bool b)
{
    bipolar = b;
    repaint();
}

void Knob::setArcOrigin (double value)
{
    arcOrigin = value;
    repaint();
}

void Knob::setRing (RingHandler* handler, float width, float offset)
{
    ring = handler;
    ringWidth = width;
    ringOffset = offset;
    repaint();
}

juce::Point<float> Knob::getFaceCentre() const { return getLocalBounds().toFloat().getCentre(); }

bool Knob::isOnRing (juce::Point<float> position) const
{
    if (ring == nullptr)
        return false;
    const float distance = position.getDistanceFrom (getFaceCentre());
    return distance > getFaceRadius() + 1.0f && distance <= getFaceRadius() + ringOffset + ringWidth / 2.0f;
}

float Knob::arcRadius() const
{
    return getFaceRadius() - std::max (knobs::arcInset, std::round (diameter * knobs::arcInsetProportion));
}

float Knob::arcStartProportion()
{
    if (arcOrigin.has_value())
        return static_cast<float> (valueToProportionOfLength (juce::jlimit (getMinimum(), getMaximum(), *arcOrigin)));
    return bipolar ? 0.5f : 0.0f;
}

//==============================================================================
void Knob::paint (juce::Graphics& g)
{
    if (! isEnabled())
        g.beginTransparencyLayer (tokens::motion::disabledAlpha);

    const auto centre = getFaceCentre();
    const float r = getFaceRadius();
    if (ring != nullptr)
        ring->paintRing (g, *this, centre, r + ringOffset - ringWidth / 2.0f, r + ringOffset + ringWidth / 2.0f);

    // The face, inset 1 px, on its shadow, lit from 12 % below its top.
    const auto face = juce::Rectangle<float> (diameter, diameter).withCentre (centre).reduced (1.0f);
    drawSoftShadow (g, face, face.getWidth() / 2.0f, shadowFor (diameter));
    juce::ColourGradient gradient (tokens::colour::knobFaceTop, face.getCentreX(), face.getY() + 0.12f * face.getHeight(),
                                   tokens::colour::knobFaceEdge, face.getCentreX(), face.getY() + 1.32f * face.getHeight(), true);
    gradient.addColour (0.45, tokens::colour::knobFaceMid);
    g.setGradientFill (gradient);
    g.fillEllipse (face);
    const float rim = isSmall (diameter) ? knobs::rimSmall : knobs::rim;
    g.setColour (isSmall (diameter) ? tokens::colour::knobRimSmall : tokens::colour::knobRim);
    g.drawEllipse (face.reduced (rim / 2.0f), rim);

    // The value arc, from its origin, on a wider translucent stroke as its glow.
    if (isEnabled())
    {
        float from = angleOf (arcStartProportion());
        float to = angleOf (valueToProportionOfLength (getValue()));
        if (to < from)
            std::swap (from, to);
        to = std::max (to, from + juce::degreesToRadians (knobs::originStubDegrees));
        const float radius = arcRadius();
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, from, to, true);
        const float width = isSmall (diameter) ? knobs::arcSmall : knobs::arc;
        const auto stroke = [&] (float w) { return juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded); };
        g.setColour (arcColour.withMultipliedAlpha (knobs::arcGlowAlpha));
        g.strokePath (arc, stroke (width + 2.0f * knobs::arcGlow));
        g.setColour (arcColour);
        g.strokePath (arc, stroke (width));
    }

    if (! isEnabled())
        g.endTransparencyLayer();
}

bool Knob::hitTest (int x, int y)
{
    const auto p = juce::Point<int> (x, y).toFloat();
    return p.getDistanceFrom (getFaceCentre()) <= getFaceRadius() || isOnRing (p);
}

//==============================================================================
void Knob::mouseEnter (const juce::MouseEvent& e)
{
    if (isEnabled())
        showTooltip();
    mouseMove (e);
}

void Knob::mouseMove (const juce::MouseEvent& e)
{
    const bool over = isEnabled() && isOnRing (e.position);
    if (over != ringHovered)
    {
        ringHovered = over;
        ring->ringHover (*this, over);
    }
}

void Knob::mouseExit (const juce::MouseEvent& e)
{
    if (ringHovered)
    {
        ringHovered = false;
        if (ring != nullptr)
            ring->ringHover (*this, false);
    }
    if (drag == nullptr && ! ringDragging)
        hideTooltipUnlessHovered (e.getScreenPosition());
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    if (! isEnabled() || ! e.mods.isLeftButtonDown())
        return;
    if (isOnRing (e.position))
    {
        ringDragging = true;
        ring->ringMouseDown (*this, e);
        return;
    }
    drag = std::make_unique<Drag> (*this, e.position.y, e.mods.isShiftDown());
    showTooltip();
}

void Knob::mouseDrag (const juce::MouseEvent& e)
{
    if (ringDragging)
    {
        ring->ringMouseDrag (*this, e);
        return;
    }
    if (drag == nullptr)
        return;
    const double here = valueToProportionOfLength (getValue());
    // Shift pressed or let go mid-drag carries on from where the knob is, at the new speed.
    if (e.mods.isShiftDown() != drag->fine)
    {
        drag->fine = e.mods.isShiftDown();
        drag->startY = e.position.y;
        drag->startProportion = here;
    }
    const float pixels = drag->fine ? knobs::fineDragPixels : knobs::dragPixels;
    const double proportion = juce::jlimit (0.0, 1.0, drag->startProportion + (drag->startY - e.position.y) / pixels);
    setValue (proportionOfLengthToValue (proportion), juce::sendNotificationSync);
}

void Knob::mouseUp (const juce::MouseEvent& e)
{
    if (ringDragging)
    {
        ringDragging = false;
        ring->ringMouseUp (*this, e);
    }
    drag.reset();
    if (! isMouseOver (true))
        hideTooltipUnlessHovered (e.getScreenPosition());
}

void Knob::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! isEnabled())
        return;
    if (isOnRing (e.position))
    {
        ring->ringDoubleClick (*this, e);
        return;
    }
    if (! isDoubleClickReturnEnabled())
        return;
    // Inside the second press's gesture if it is open, so the reset is one undo step either way.
    std::optional<ScopedDragNotification> gesture;
    if (drag == nullptr)
        gesture.emplace (*this);
    setValue (getDoubleClickReturnValue(), juce::sendNotificationSync);
    if (drag != nullptr)
    {
        drag->startY = e.position.y;
        drag->startProportion = valueToProportionOfLength (getValue());
    }
}

void Knob::valueChanged()
{
    if (tooltip != nullptr)
        tooltip->refresh();
    repaint();
}

void Knob::enablementChanged()
{
    if (! isEnabled())
    {
        drag.reset();
        hideTooltip();
    }
    eq1::KeyboardSlider::enablementChanged();
    repaint();
}

void Knob::parentHierarchyChanged()
{
    if (! isShowing())
        hideTooltip();
    eq1::KeyboardSlider::parentHierarchyChanged();
}

//==============================================================================
juce::String Knob::tooltipTitle() const { return getTitle().isNotEmpty() ? getTitle() : getName(); }

juce::String Knob::tooltipValue() { return getTextFromValue (getValue()); }

bool Knob::isTooltipShown() const { return tooltip != nullptr && tooltip->isVisible() && tooltip->getParentComponent() != nullptr; }

void Knob::showTooltip()
{
    auto& layer = overlayLayerFor (*this);
    if (&layer == this || ! isShowing())
        return;
    if (tooltip == nullptr)
        tooltip = std::make_unique<KnobTooltip> (*this);
    if (tooltip->getParentComponent() != &layer)
        layer.addChildComponent (*tooltip);
    tooltip->refresh();
    if (! tooltip->isVisible())
        tooltip->appear();
}

void Knob::hideTooltipUnlessHovered (juce::Point<int> screenPosition)
{
    if (tooltip == nullptr || tooltip->isEditing())
        return;
    if (getScreenBounds().contains (screenPosition) && isEnabled())
        return;
    if (tooltip->isVisible() && tooltip->getScreenBounds().contains (screenPosition))
        return;
    hideTooltip();
}

void Knob::hideTooltip()
{
    if (tooltip != nullptr)
        tooltip->disappear();
}

bool Knob::commitTypedText (const juce::String& text)
{
    if (! isEnabled() || text.trim().isEmpty())
        return false;
    const auto parsed = parseValue (text, getTextValueSuffix().trim());
    const double value = juce::jlimit (getMinimum(), getMaximum(), parsed.has_value() ? *parsed : getValueFromText (text));
    const ScopedDragNotification gesture (*this);
    setValue (value, juce::sendNotificationSync);
    return true;
}

} // namespace staple
