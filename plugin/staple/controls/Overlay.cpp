#include "Overlay.h"

#include "../CachedLayer.h"
#include "../Tokens.h"

#include <algorithm>
#include <cmath>

namespace staple
{

namespace
{
const juce::Identifier overlayLayerProperty { "stapleOverlayLayer" };
constexpr int shadowLayers = 8;
// SoftShadowCache's images: as many as a display of every Band's handle, hovered and selected, uses;
// past that it starts again.
constexpr size_t maxCachedShadows = 64;

bool operator== (const juce::DropShadow& a, const juce::DropShadow& b)
{
    return a.colour == b.colour && a.radius == b.radius && a.offset == b.offset;
}
} // namespace

juce::Component& overlayLayerFor (juce::Component& component)
{
    juce::Component* top = &component;
    juce::Component* transformed = nullptr;
    for (auto* c = component.getParentComponent(); c != nullptr; c = c->getParentComponent())
    {
        if (c->getProperties().contains (overlayLayerProperty))
            return *c;
        if (c->isTransformed())
            transformed = c;
        top = c;
    }
    return transformed != nullptr ? *transformed : *top;
}

void markOverlayLayer (juce::Component& layer) { layer.getProperties().set (overlayLayerProperty, true); }

void drawSoftShadow (juce::Graphics& g, juce::Rectangle<float> shape, float cornerRadius, const juce::DropShadow& shadow)
{
    const auto base = shape.translated (static_cast<float> (shadow.offset.x), static_cast<float> (shadow.offset.y));
    const float blur = static_cast<float> (shadow.radius);
    // Each layer's alpha, so that all of them together reach the shadow's own.
    const float alpha = 1.0f - std::pow (1.0f - shadow.colour.getFloatAlpha(), 1.0f / shadowLayers);
    g.setColour (shadow.colour.withAlpha (alpha));
    for (int i = 0; i < shadowLayers; ++i)
    {
        const float spread = blur * (static_cast<float> (i) / (shadowLayers - 1) - 0.5f);
        const auto r = base.expanded (spread);
        if (r.isEmpty())
            continue;
        g.fillRoundedRectangle (r, std::max (0.0f, cornerRadius + spread));
    }
}

void SoftShadowCache::draw (juce::Graphics& g, juce::Point<float> origin, juce::Rectangle<float> shape, float cornerRadius, const juce::DropShadow& shadow)
{
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (! startsOnWholeDevicePixel (origin, scale))
    {
        drawSoftShadow (g, shape, cornerRadius, shadow);
        return;
    }
    // drawSoftShadow's widest layer reaches half the blur past the offset shape; a pixel more for its
    // anti-aliased edge.
    const auto reach = shape.translated (static_cast<float> (shadow.offset.x), static_cast<float> (shadow.offset.y))
                           .expanded (static_cast<float> (shadow.radius) / 2.0f + 1.0f);
    const auto device = reach.transformedBy (juce::AffineTransform::scale (scale)).getSmallestIntegerContainer();
    if (device.isEmpty())
        return;
    const auto relative = shape - device.getPosition().toFloat() / scale;
    auto entry = std::find_if (entries.begin(), entries.end(), [&] (const Entry& e) {
        return e.shape == relative && juce::exactlyEqual (e.cornerRadius, cornerRadius) && e.shadow == shadow && juce::exactlyEqual (e.scale, scale)
               && e.image.getBounds() == device.withZeroOrigin();
    });
    if (entry == entries.end())
    {
        if (entries.size() >= maxCachedShadows)
            entries.clear();
        juce::Image image (juce::Image::ARGB, device.getWidth(), device.getHeight(), true);
        {
            juce::Graphics into (image);
            into.addTransform (juce::AffineTransform::scale (scale));
            drawSoftShadow (into, relative, cornerRadius, shadow);
        }
        entries.push_back ({ relative, cornerRadius, shadow, scale, image });
        entry = std::prev (entries.end());
    }
    // An image is drawn at the alpha of g's colour: here, whole.
    const juce::Graphics::ScopedSaveState saved (g);
    g.setOpacity (1.0f);
    g.drawImageTransformed (entry->image, juce::AffineTransform::translation (device.getPosition().toFloat()).scaled (1.0f / scale));
}

juce::AffineTransform popInTransform (juce::Rectangle<float> card, float progress)
{
    using namespace tokens::motion;
    if (progress >= 1.0f)
        return {};
    const float scale = popInScale + (1.0f - popInScale) * progress;
    const auto centre = card.getCentre();
    return juce::AffineTransform::scale (scale, scale, centre.x, centre.y).translated (0.0f, popInOffset * (1.0f - progress));
}

float ease (float time)
{
    using namespace tokens::motion;
    const float t = juce::jlimit (0.0f, 1.0f, time);
    // Solve x (u) = t for the curve's parameter u by bisection, then return y (u).
    const auto bezier = [] (float u, float p1, float p2) {
        const float v = 1.0f - u;
        return 3.0f * v * v * u * p1 + 3.0f * v * u * u * p2 + u * u * u;
    };
    float lo = 0.0f, hi = 1.0f, u = t;
    for (int i = 0; i < 24; ++i)
    {
        u = (lo + hi) / 2.0f;
        if (bezier (u, easeX1, easeX2) < t)
            lo = u;
        else
            hi = u;
    }
    return bezier (u, easeY1, easeY2);
}

} // namespace staple
