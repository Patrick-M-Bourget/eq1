#include "EdgeFadeLayer.h"

#include "../staple/Tokens.h"

namespace eq1::display
{

void paintEdgeFades (juce::Graphics& g, const DisplayGeometry& geometry)
{
    namespace layout = staple::tokens::layout;
    const auto opaque = staple::tokens::colour::bg0, clear = opaque.withAlpha (0.0f);
    const auto bounds = geometry.bounds();
    // From the edge point inwards over distance.
    const auto fade = [&] (juce::Rectangle<float> area, juce::Point<float> edge, juce::Point<float> inner) {
        g.setGradientFill (juce::ColourGradient (opaque, edge, clear, inner, false));
        g.fillRect (area);
    };
    auto area = bounds;
    fade (area.removeFromTop (layout::fadeTop), bounds.getTopLeft(), bounds.getTopLeft().translated (0.0f, layout::fadeTop));
    area = bounds;
    fade (area.removeFromBottom (layout::fadeBottom), bounds.getBottomLeft(), bounds.getBottomLeft().translated (0.0f, -layout::fadeBottom));
    area = bounds;
    fade (area.removeFromLeft (layout::fadeLeft), bounds.getTopLeft(), bounds.getTopLeft().translated (layout::fadeLeft, 0.0f));
    area = bounds;
    fade (area.removeFromRight (layout::fadeRight), bounds.getTopRight(), bounds.getTopRight().translated (-layout::fadeRight, 0.0f));
}

} // namespace eq1::display
