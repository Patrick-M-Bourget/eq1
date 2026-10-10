#pragma once

#include "DisplayGeometry.h"

#include <functional>

namespace eq1::display
{

// The display's edges dissolve into what lies behind it, as the handoff's mask does: whatever the
// display draws under the fades shows fully inside and not at all at each edge, over layout::fadeTop,
// fadeBottom, fadeLeft and fadeRight, and where two edges' fades meet they multiply. Drawing what lies
// behind over the display with the opposite alpha looks the same and costs one image a frame.
//
// The overlay: paintBehind's drawing (in the display's logical pixels) at scale, its alpha 1 at each
// edge and 0 inside the fades. Built again only when the display's size, place or scale changes.
juce::Image edgeFadeOverlay (const DisplayGeometry& geometry, float scale, const std::function<void (juce::Graphics&)>& paintBehind);

// Draws the overlay over the grid, the Analyzer and the curves; the handles, labels and the Display
// Range chip go over it, unfaded.
void paintEdgeFades (juce::Graphics& g, const DisplayGeometry& geometry, const juce::Image& overlay);

} // namespace eq1::display
