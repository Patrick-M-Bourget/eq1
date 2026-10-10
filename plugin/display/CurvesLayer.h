#pragma once

#include "DisplayFrame.h"
#include "DisplayGeometry.h"

namespace eq1::display
{

// Each Band's own curve, then the whole EQ's: the sum of the Bands that are playing, each Dynamic Band
// at its Live Gain. Nothing plays above Nyquist, so the curves stay level from there. The curves come
// from the Engine's own response maths (eq1/Response.h).
void paintCurves (juce::Graphics& g, const DisplayGeometry& geometry, const DisplayFrame& frame);

} // namespace eq1::display
