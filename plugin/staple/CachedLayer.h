#pragma once

#include <juce_graphics/juce_graphics.h>

#include <optional>

namespace staple
{

// A drawing that doesn't change from frame to frame, drawn once into an image at the device scale it
// is drawn at, and drawn back from it pixel for pixel until what it shows (Key, compared with ==), the
// area it covers or that scale changes. Drawn back pixel for pixel where g's logical pixels start on a
// whole device pixel, as a component's do at a UI Scale and display scale whose product is whole;
// elsewhere the image is drawn back resampled, slightly softer.
template <typename Key>
class CachedLayer
{
public:
    // Draws paint's drawing of area, in g's logical pixels, for key: from the image when it was drawn
    // for the same key, area and scale, else into a new one first. An RGB format is for a drawing that
    // covers all of area, an ARGB one for any other.
    template <typename Paint>
    void draw (juce::Graphics& g, juce::Rectangle<float> area, const Key& key, juce::Image::PixelFormat format, Paint&& paint)
    {
        const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
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
