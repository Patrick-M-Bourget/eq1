#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

namespace staple
{

// Where the kit puts what floats over the editor (the knob tooltip, popovers): a child of the overlay
// layer, so it scales with the editor's content rather than being a desktop window. The layer is the
// nearest ancestor marked with markOverlayLayer (the editor's scaled content component), else the
// top-level component.
juce::Component& overlayLayerFor (juce::Component& component);
void markOverlayLayer (juce::Component& layer);

// A shadow drawn without blur (HANDOFF.md §4): stacked translucent rounded rectangles, each a step
// wider, that add up to the shadow's colour inside the shape and fade out over its blur radius.
void drawSoftShadow (juce::Graphics& g, juce::Rectangle<float> shape, float cornerRadius, const juce::DropShadow& shadow);

// drawSoftShadow from images, for shadows drawn on every frame (the EQ display's handles): each shadow
// is drawn once per shape size, corner, shadow, scale and place within a device pixel, and drawn back
// from its image pixel for pixel. origin is where g's logical (0, 0) lies from a whole device pixel (the
// component's place in the window); where that isn't one at g's scale (startsOnWholeDevicePixel), the
// shadow is drawn directly.
class SoftShadowCache
{
public:
    void draw (juce::Graphics& g, juce::Point<float> origin, juce::Rectangle<float> shape, float cornerRadius, const juce::DropShadow& shadow);

private:
    struct Entry
    {
        juce::Rectangle<float> shape; // from the image's top left, in logical pixels
        float cornerRadius = 0.0f;
        juce::DropShadow shadow;
        float scale = 0.0f;
        juce::Image image;
    };
    std::vector<Entry> entries;
};

// The handoff's one easing curve, cubic-bezier (0.2, 0.7, 0.2, 1): progress for a time from 0 to 1.
float ease (float time);

// Where a popping-in card is drawn at progress 0 to 1 (a Tween over dur2): from 3 px lower at 98.5 %
// scale about its centre, to where it is, with no transform at all once in. The card is in the pixels
// of the transformed component's parent, where juce::Component::setTransform applies.
juce::AffineTransform popInTransform (juce::Rectangle<float> card, float progress);

} // namespace staple
