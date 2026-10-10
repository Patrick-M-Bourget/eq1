#pragma once

#include "DisplayGeometry.h"

#include <array>
#include <functional>

namespace eq1::display
{

// The display's edges dissolve into what lies behind it, as the handoff's mask does: whatever the
// display draws under the fades shows fully inside and not at all at each edge, over layout::fadeTop,
// fadeBottom, fadeLeft and fadeRight, and where two edges' fades meet they multiply. Drawing what lies
// behind over the display with the opposite alpha looks the same and costs a few small images a frame.
struct EdgeFadeOverlay
{
    // One edge's fade, in device pixels: its image and where its top left sits from the display's.
    struct Strip
    {
        juce::Image image;
        juce::Point<int> at;
    };
    std::array<Strip, 4> strips; // top, bottom, left, right; they don't overlap
    float scale = 1.0f;          // device pixels per logical pixel
};

// paintBehind's drawing (in the display's logical pixels) in the fades only, at scale, each strip
// whole device pixels, so it is drawn back pixel for pixel at any scale; its alpha is 1 at each edge
// and 0 where the fades end.
EdgeFadeOverlay edgeFadeOverlay (const DisplayGeometry& geometry, float scale, const std::function<void (juce::Graphics&)>& paintBehind);

// Draws the overlay over the grid, the Analyzer and the curves; the handles, labels and the Display
// Range chip go over it, unfaded.
void paintEdgeFades (juce::Graphics& g, const EdgeFadeOverlay& overlay);

} // namespace eq1::display
