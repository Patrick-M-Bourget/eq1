#pragma once

#include <juce_graphics/juce_graphics.h>

#include <cmath>
#include <optional>

namespace staple
{

// Whether a drawing whose logical (0, 0) is origin logical pixels from a whole device pixel (a
// component's place in the window, whose top left is one) starts on a whole device pixel at scale.
// Only there is an image of it drawn back pixel for pixel; elsewhere it would be resampled, softer.
inline bool startsOnWholeDevicePixel (juce::Point<float> origin, float scale)
{
    const auto whole = [scale] (float v) { return std::abs (v * scale - std::round (v * scale)) < 1.0e-3f; };
    return whole (origin.x) && whole (origin.y);
}

// A drawing that doesn't change from frame to frame, drawn once into an image at the device scale it
// is drawn at, and drawn back from it pixel for pixel until what it shows (Key, compared with ==), the
// area it covers or that scale changes. Where it doesn't start on a whole device pixel (at 75% or 125%
// on a 1x screen, startsOnWholeDevicePixel) it is drawn directly instead, as sharp as ever.
template <typename Key>
class CachedLayer
{
public:
    // Draws paint's drawing of area, in g's logical pixels, for key: from the image when it was drawn
    // for the same key, area and scale, else into a new one first. An RGB format is for a drawing that
    // covers all of area, an ARGB one for any other. origin is where g's logical (0, 0) lies from a
    // whole device pixel, in logical pixels: the component's place in the window.
    template <typename Paint>
    void draw (juce::Graphics& g, juce::Point<float> origin, juce::Rectangle<float> area, const Key& key, juce::Image::PixelFormat format, Paint&& paint)
    {
        const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
        if (! startsOnWholeDevicePixel (origin, scale))
        {
            paint (g);
            return;
        }
        if (! drawnFor || ! (drawnFor->key == key) || drawnFor->area != area || ! juce::exactlyEqual (drawnFor->scale, scale))
        {
            drawnFor = DrawnFor { key, area, scale };
            device = area.transformedBy (juce::AffineTransform::scale (scale)).getSmallestIntegerContainer();
            image = {};
            if (! device.isEmpty())
            {
                image = juce::Image (format, device.getWidth(), device.getHeight(), true);
                juce::Graphics into (image);
                into.addTransform (juce::AffineTransform::scale (scale).translated (static_cast<float> (-device.getX()), static_cast<float> (-device.getY())));
                paint (into);
            }
        }
        if (image.isValid())
        {
            // An image is drawn at the alpha of g's colour: here, whole.
            const juce::Graphics::ScopedSaveState saved (g);
            g.setOpacity (1.0f);
            g.drawImageTransformed (image, juce::AffineTransform::translation (device.getPosition().toFloat()).scaled (1.0f / scale));
        }
    }

private:
    struct DrawnFor
    {
        Key key;
        juce::Rectangle<float> area;
        float scale = 0.0f;
    };
    std::optional<DrawnFor> drawnFor;
    juce::Rectangle<int> device; // the image's place in device pixels
    juce::Image image;
};

} // namespace staple
