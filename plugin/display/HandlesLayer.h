#pragma once

#include "DisplayFrame.h"
#include "DisplayGeometry.h"

#include <vector>

namespace eq1::display
{

// How a Band's handle is drawn.
struct HandleState
{
    int slot = 1;
    bool selected = false;
    bool bypassed = false;     // Bypassed, or a Side Band on mono, which has nothing to process
    float hover = 0.0f;        // 0 to 1, through the hover fade
    bool dragged = false;      // being dragged
    float globalBypass = 0.0f; // 0 to 1, through Global Bypass's fade
};
struct HandleStyle
{
    float diameter = 0.0f;
    juce::Colour fill, ring;
    float ringWidth = 0.0f;
    float glowAlpha = 0.0f; // of the Band's colour, around a selected handle
    juce::DropShadow shadow;
};
// A flat 16 px dot in the Band's colour with a 1 px dark ring, x1.15 on hover; selected 22 px with a
// 2 px white ring and a 40 % glow; x1.15 while dragged. Bypassed, and every handle under Global Bypass,
// in the bypassed colour at 85 % with no glow, a selected one's ring at 60 % white.
HandleStyle handleStyle (const HandleState& state);

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

// What the readout beside a dragged Band says: "Band 4" over "1.00 kHz  +3.0 dB  Q 1.00", Gain left out
// on Shapes without it.
struct Readout
{
    juce::String title, value;
};
Readout dragReadout (int slot, const BandSettings& band);

// Over the curves: the grips, a handle per Band in use with the Solo cue on a Soloed one, the readouts
// beside the Bands being dragged, the marquee, and the "All 24 Bands are in use" message.
void paintHandles (juce::Graphics& g, const DisplayGeometry& geometry, const DisplayFrame& frame);

} // namespace eq1::display
