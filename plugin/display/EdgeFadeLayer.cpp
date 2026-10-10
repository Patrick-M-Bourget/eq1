#include "EdgeFadeLayer.h"

#include "../staple/Tokens.h"

#include <vector>

namespace eq1::display
{

namespace layout = staple::tokens::layout;

juce::Image edgeFadeOverlay (const DisplayGeometry& geometry, float scale, const std::function<void (juce::Graphics&)>& paintBehind)
{
    const int width = juce::roundToInt (static_cast<float> (geometry.width) * scale);
    const int height = juce::roundToInt (static_cast<float> (geometry.height) * scale);
    if (width <= 0 || height <= 0)
        return {};
    juce::Image overlay (juce::Image::ARGB, width, height, true);
    {
        juce::Graphics g (overlay);
        g.addTransform (juce::AffineTransform::scale (scale));
        paintBehind (g);
    }
    // How much of the display shows at a distance along one axis of extent: 0 at either edge, 1 past
    // both fades.
    const auto shown = [] (float at, float extent, float fadeIn, float fadeOut) {
        return juce::jlimit (0.0f, 1.0f, at / fadeIn) * juce::jlimit (0.0f, 1.0f, (extent - at) / fadeOut);
    };
    const auto w = static_cast<float> (geometry.width), h = static_cast<float> (geometry.height);
    std::vector<float> across (static_cast<size_t> (width));
    for (int x = 0; x < width; ++x)
        across[static_cast<size_t> (x)] = shown ((static_cast<float> (x) + 0.5f) / scale, w, layout::fadeLeft, layout::fadeRight);
    const juce::Image::BitmapData pixels (overlay, juce::Image::BitmapData::readWrite);
    for (int y = 0; y < height; ++y)
    {
        const float down = shown ((static_cast<float> (y) + 0.5f) / scale, h, layout::fadeTop, layout::fadeBottom);
        for (int x = 0; x < width; ++x)
            reinterpret_cast<juce::PixelARGB*> (pixels.getPixelPointer (x, y))->multiplyAlpha (1.0f - down * across[static_cast<size_t> (x)]);
    }
    return overlay;
}

void paintEdgeFades (juce::Graphics& g, const DisplayGeometry& geometry, const juce::Image& overlay)
{
    if (! overlay.isValid())
        return;
    const auto bounds = geometry.bounds();
    const juce::Graphics::ScopedSaveState saved (g);
    // Only the fades: inside them the overlay is clear.
    g.excludeClipRegion (bounds.withTrimmedLeft (layout::fadeLeft)
                             .withTrimmedRight (layout::fadeRight)
                             .withTrimmedTop (layout::fadeTop)
                             .withTrimmedBottom (layout::fadeBottom)
                             .getSmallestIntegerContainer()
                             .reduced (1));
    g.drawImageTransformed (overlay, juce::AffineTransform::scale (bounds.getWidth() / static_cast<float> (overlay.getWidth()),
                                                                   bounds.getHeight() / static_cast<float> (overlay.getHeight())));
}

} // namespace eq1::display
