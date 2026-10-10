#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

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

// The handoff's one easing curve, cubic-bezier (0.2, 0.7, 0.2, 1): progress for a time from 0 to 1.
float ease (float time);

// Where a popping-in card is drawn at progress 0 to 1 (a Tween over dur2): from 3 px lower at 98.5 %
// scale about its centre, to where it is, with no transform at all once in.
juce::AffineTransform popInTransform (juce::Rectangle<float> card, float progress);

} // namespace staple
