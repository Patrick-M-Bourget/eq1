#include "Overlay.h"

#include "../Tokens.h"

namespace staple
{

namespace
{
const juce::Identifier overlayLayerProperty { "stapleOverlayLayer" };
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
