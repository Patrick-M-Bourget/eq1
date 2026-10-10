#pragma once

#include "DisplayFrame.h"
#include "DisplayGeometry.h"

namespace eq1::display
{

// Over the curves: a handle per Band in use, numbered by Band Slot, with a Dynamic Band's Dynamic Range
// ring and the Solo cue; the values beside the Bands being dragged; the marquee; and the "All 24 Bands
// are in use" message.
void paintHandles (juce::Graphics& g, const DisplayGeometry& geometry, const DisplayFrame& frame);

} // namespace eq1::display
