#include "Knob.h"

#include "../Painting.h"
#include "../TipWatchdog.h"
#include "KnobTooltip.h"
#include "Overlay.h"

#include <algorithm>
#include <cmath>
#include <tuple>
#include <vector>

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

// The convex hull of points, as a closed path (Andrew's monotone chain).
juce::Path convexHull (std::vector<juce::Point<float>> points)
{
    std::sort (points.begin(), points.end(), [] (auto a, auto b) { return std::tie (a.x, a.y) < std::tie (b.x, b.y); });
    const auto cross = [] (auto o, auto a, auto b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); };
    std::vector<juce::Point<float>> hull;
    for (int pass = 0; pass < 2; ++pass)
    {
        const auto start = hull.size();
        for (const auto p : points)
        {
            while (hull.size() >= start + 2 && cross (hull[hull.size() - 2], hull.back(), p) <= 0.0f)
                hull.pop_back();
            hull.push_back (p);
        }
        hull.pop_back(); // the next pass starts from it
        std::reverse (points.begin(), points.end());
    }
    juce::Path path;
    if (hull.size() < 3)
        return path;
    path.startNewSubPath (hull.front());
    for (size_t i = 1; i < hull.size(); ++i)
        path.lineTo (hull[i]);
    path.closeSubPath();
    return path;
}

// The way the pointer takes from a knob's hit area (the circle of its face, or of its ring's outer edge) to its
// Readout (card), both in one component's coordinates: the convex hull of the circle and the card, so every straight
// path from the circle onto the card stays on it. Beside the circle, it reaches out only between the points where the
// lines from the card's corners touch the circle: when the card is wider than the circle, those are on the circle's
// far side of its centre, and the way reaches out beside the circle level with its centre too.
juce::Path wayToReadout (juce::Point<float> centre, float radius, juce::Rectangle<float> card)
{
    constexpr int samples = 64; // round the circle; the slivers this cuts off its edge are the knob's own
    const auto onCircle = [&] (float angle) { return centre + juce::Point<float> (std::cos (angle), std::sin (angle)) * radius; };
    std::vector<juce::Point<float>> points;
    for (int i = 0; i < samples; ++i)
        points.push_back (onCircle (juce::MathConstants<float>::twoPi * static_cast<float> (i) / static_cast<float> (samples)));
    for (const auto corner : { card.getTopLeft(), card.getTopRight(), card.getBottomLeft(), card.getBottomRight() })
    {
        points.push_back (corner);
        // Where the lines from the corner touch the circle, exactly.
        const float distance = corner.getDistanceFrom (centre);
        if (distance <= radius)
            continue;
        const float towardCorner = std::atan2 (corner.y - centre.y, corner.x - centre.x);
        const float spread = std::acos (radius / distance);
        points.push_back (onCircle (towardCorner - spread));
        points.push_back (onCircle (towardCorner + spread));
    }
    return convexHull (std::move (points));
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
    ringHovered = ringDragging = readoutOnRing = false;
    ringWidth = width;
    ringOffset = offset;
    repaint();
}

void Knob::ringChanged()
{
    if (tooltip != nullptr && isTooltipOnRing())
        tooltip->refresh();
    repaint();
}

juce::Point<float> Knob::getFaceCentre() const { return getLocalBounds().toFloat().getCentre(); }

bool Knob::isOnRing (juce::Point<float> position) const
{
    if (ring == nullptr)
        return false;
    const float distance = position.getDistanceFrom (getFaceCentre());
    return distance > getFaceRadius() + 1.0f && distance <= hitRadius();
}

bool Knob::isOnRingAsHit (const juce::MouseEvent& e) const { return isOnRing (e.position.roundToInt().toFloat()); }

float Knob::hitRadius() const { return getFaceRadius() + (ring != nullptr ? ringOffset + ringWidth / 2.0f : 0.0f); }

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
    drawKnobFace (g, face, shadowFor (diameter), isSmall (diameter) ? knobs::rimSmall : knobs::rim,
                  isSmall (diameter) ? tokens::colour::knobRimSmall : tokens::colour::knobRim);

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
    mouseMove (e); // first, so the Readout shows what is under the pointer
    if (isEnabled())
        showTooltip();
}

void Knob::mouseMove (const juce::MouseEvent& e)
{
    const bool over = isEnabled() && isOnRingAsHit (e);
    if (over != ringHovered)
    {
        ringHovered = over;
        ring->ringHover (*this, over);
    }
    // Only here, on the face or ring: off them, the Readout keeps the content (and so the size, and the way to it) it had
    // as the pointer left.
    if (over != readoutOnRing)
    {
        readoutOnRing = over;
        if (tooltip != nullptr)
            tooltip->refresh();
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
    if (! isMouseDragging() && ! ringDragging)
        hideTooltipUnlessHovered (e.getScreenPosition());
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    if (! isEnabled() || ! e.mods.isLeftButtonDown())
        return;
    if (isOnRingAsHit (e))
    {
        ringDragging = true;
        ring->ringMouseDown (*this, e);
        showTooltip();
        return;
    }
    startMouseDrag (e);
    showTooltip();
}

void Knob::mouseDrag (const juce::MouseEvent& e)
{
    if (ringDragging)
    {
        ring->ringMouseDrag (*this, e);
        return;
    }
    continueMouseDrag (e);
}

double draggedAlongRange (juce::Slider& slider, double from, float pixels, bool fine)
{
    const double proportion = slider.valueToProportionOfLength (from) + pixels / (fine ? knobs::fineDragPixels : knobs::dragPixels);
    return slider.proportionOfLengthToValue (juce::jlimit (0.0, 1.0, proportion));
}

double Knob::valueDraggedBy (double from, float pixels, bool fine) { return draggedAlongRange (*this, from, pixels, fine); }

void Knob::mouseUp (const juce::MouseEvent& e)
{
    if (ringDragging)
    {
        ringDragging = false;
        ring->ringMouseUp (*this, e);
        if (tooltip != nullptr)
            tooltip->refresh();
    }
    endMouseDrag();
    if (! isMouseOver (true))
        hideTooltipUnlessHovered (e.getScreenPosition());
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
        endMouseDrag();
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
juce::String Knob::tooltipTitle() const
{
    if (isTooltipOnRing())
        return ring->ringTitle();
    return getTitle().isNotEmpty() ? getTitle() : getName();
}

juce::String Knob::tooltipValue()
{
    if (isTooltipOnRing())
        return ring->ringValue();
    return spokenValue != nullptr ? spokenValue (getValue()) : getTextFromValue (getValue());
}

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
    // One Knob Readout at a time on the layer: another knob's, kept on the way to it across this knob, goes, unless a
    // value is being typed into it.
    for (auto* child : layer.getChildren())
        if (auto* other = dynamic_cast<KnobTooltip*> (child); other != nullptr && other != tooltip.get() && other->isVisible() && ! other->isEditing())
            other->disappear();
}

void Knob::hideTooltipUnlessHovered (juce::Point<int> screenPosition)
{
    if (tooltip == nullptr || tooltip->isEditing())
        return;
    // Over the face (or the ring), not the shadow margin round it: JUCE sends the exit as the pointer leaves the face,
    // usually still inside the knob's bounds, and the knob hears nothing more. A hidden knob or parent sends one inside
    // them too. On the way to the Readout, in the margin or over the label, the pointer is crossing to the Readout.
    const auto pointer = screenPosition.toFloat();
    if ((isEnabled() && (TipWatchdog::isPointerOver (*this, pointer) || isOnWayToReadout (pointer)))
        || TipWatchdog::isPointerOver (*tooltip, pointer))
        return;
    hideTooltip();
}

void Knob::watchTooltip (juce::Point<float> pointer)
{
    if (! isMouseButtonDown())
        hideTooltipUnlessHovered (pointer.roundToInt());
}

juce::Rectangle<float> Knob::faceArea() const
{
    const float r = getFaceRadius();
    return juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (getFaceCentre());
}

bool Knob::isOnWayToReadout (juce::Point<float> screenPosition)
{
    auto* layer = tooltip != nullptr ? tooltip->getParentComponent() : nullptr;
    if (layer == nullptr || ! tooltip->isVisible() || ! isShowing())
        return false;
    const auto centre = layer->getLocalPoint (this, getFaceCentre());
    const float radius = layer->getLocalPoint (this, getFaceCentre().translated (hitRadius(), 0.0f)).getDistanceFrom (centre);
    const auto way = wayToReadout (centre, radius, tooltip->getBounds().toFloat());
    return TipWatchdog::isPointerWithin (*layer, way, screenPosition);
}

void Knob::hideTooltip()
{
    if (tooltip != nullptr)
        tooltip->disappear();
}

} // namespace staple
