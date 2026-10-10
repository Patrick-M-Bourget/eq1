#pragma once

#include "DisplayGeometry.h"

namespace eq1::display
{

// The display's edges dissolve into bg0: a gradient from opaque at each edge to transparent over
// layout::fadeTop, fadeBottom, fadeLeft and fadeRight. Painted over the grid, the Analyzer and the
// curves; the handles, labels and the Display Range chip go over it, unfaded.
void paintEdgeFades (juce::Graphics& g, const DisplayGeometry& geometry);

} // namespace eq1::display
