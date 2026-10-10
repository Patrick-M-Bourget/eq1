#include "Overlay.h"

#include "../Tokens.h"

#include <cmath>

namespace staple
{

namespace
{
const juce::Identifier overlayLayerProperty { "stapleOverlayLayer" };
constexpr int shadowLayers = 8;
} // namespace

juce::Component& overlayLayerFor (juce::Component& component)
{
    auto* top = &component;
    for (auto* c = component.getParentComponent(); c != nullptr; c = c->getParentComponent())
    {
        if (c->getProperties().contains (overlayLayerProperty))
            return *c;
        top = c;
    }
    return *top;
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
