#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <optional>

namespace staple
{

// Where the kit puts what floats over the editor (the knob tooltip, popovers): a child of the overlay
// layer, so it scales with the editor's content rather than being a desktop window. The layer is the
// nearest ancestor marked with markOverlayLayer (the editor's scaled content component), else the
// top-level component.
juce::Component& overlayLayerFor (juce::Component& component);
void markOverlayLayer (juce::Component& layer);

// Where a Tooltip or a Knob Readout of size goes by control, both in layer's coordinates: centred on
// control, its top at top (over the control's label), or 6 px under control without one, or 6 px above
// control where that would leave layer; kept inside layer.
juce::Rectangle<int> placeCardBy (juce::Rectangle<int> control, juce::Point<int> size, juce::Rectangle<int> layer, std::optional<int> top = {});

// The handoff's one easing curve, cubic-bezier (0.2, 0.7, 0.2, 1): progress for a time from 0 to 1.
float ease (float time);

// Where a popping-in card is drawn at progress 0 to 1 (a Tween over dur2): from 3 px lower at 98.5 %
// scale about its centre, to where it is, with no transform at all once in. The card is in the pixels
// of the transformed component's parent, where juce::Component::setTransform applies.
juce::AffineTransform popInTransform (juce::Rectangle<float> card, float progress);

} // namespace staple
