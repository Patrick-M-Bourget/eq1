#pragma once

#include "DisplayFrame.h"
#include "DisplayGeometry.h"

#include <vector>

namespace eq1::display
{

// A Dynamic Range grip: the ▲▼ on the display that sets a Band's Dynamic Range.
struct Grip
{
    int slot = 0;
    juce::Point<float> centre;
    float alpha = 1.0f;
};
// The grips to show: the selected Band's (one selected alone) while its Shape has Gain and it isn't
// Bypassed, so a range can be made by dragging, and every other Dynamic Band's not under Dynamics Bypass,
// at 55 % until hovered. Each sits at its Band's Frequency and heard Gain + Dynamic Range, or 26 px below
// its handle with none, and 14 px inside the top and bottom.
std::vector<Grip> dynamicRangeGrips (const DisplayGeometry& geometry, const DisplayFrame& frame);
// A grip's 18 x 26 px hit area.
juce::Rectangle<float> gripArea (juce::Point<float> centre);

// Over the curves: a handle per Band in use, numbered by Band Slot, with a Dynamic Band's Dynamic Range
// ring and the Solo cue; the values beside the Bands being dragged; the marquee; and the "All 24 Bands
// are in use" message.
void paintHandles (juce::Graphics& g, const DisplayGeometry& geometry, const DisplayFrame& frame);

} // namespace eq1::display
