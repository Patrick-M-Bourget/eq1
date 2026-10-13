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

// The steps an image drawn once and drawn back pixel for pixel takes (CachedLayer, SoftShadowCache).
// g's device scale where a drawing whose logical (0, 0) is origin from a whole device pixel can be
// drawn from an image; none where it can't (startsOnWholeDevicePixel), and is drawn directly instead.
inline std::optional<float> imageScaleFor (juce::Graphics& g, juce::Point<float> origin)
{
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    return startsOnWholeDevicePixel (origin, scale) ? std::optional<float> (scale) : std::nullopt;
}

// The device pixels area, in logical pixels, covers at scale: the image's place.
inline juce::Rectangle<int> devicePixelsOf (juce::Rectangle<float> area, float scale)
{
    return area.transformedBy (juce::AffineTransform::scale (scale)).getSmallestIntegerContainer();
}

// An image of device's pixels of paint's drawing, which is in logical pixels, at scale. An RGB format is
// for a drawing that covers all of device, an ARGB one for any other.
template <typename Paint>
juce::Image drawnAtDevicePixels (juce::Rectangle<int> device, float scale, juce::Image::PixelFormat format, Paint&& paint)
{
    juce::Image image (format, device.getWidth(), device.getHeight(), true);
    juce::Graphics into (image);
    into.addTransform (juce::AffineTransform::scale (scale).translated (static_cast<float> (-device.getX()), static_cast<float> (-device.getY())));
    paint (into);
    return image;
}

// Draws image back at device, pixel for pixel, at g's scale.
inline void drawBackAtDevicePixels (juce::Graphics& g, const juce::Image& image, juce::Rectangle<int> device, float scale)
{
    // An image is drawn at the alpha of g's colour: here, whole.
    const juce::Graphics::ScopedSaveState saved (g);
    g.setOpacity (1.0f);
    g.drawImageTransformed (image, juce::AffineTransform::translation (device.getPosition().toFloat()).scaled (1.0f / scale));
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
        const auto imageScale = imageScaleFor (g, origin);
        if (! imageScale)
        {
            paint (g);
            return;
        }
        const float scale = *imageScale;
        if (! drawnFor || ! (drawnFor->key == key) || drawnFor->area != area || ! juce::exactlyEqual (drawnFor->scale, scale))
        {
            drawnFor = DrawnFor { key, area, scale };
            device = devicePixelsOf (area, scale);
            image = device.isEmpty() ? juce::Image() : drawnAtDevicePixels (device, scale, format, paint);
        }
        if (image.isValid())
            drawBackAtDevicePixels (g, image, device, scale);
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
