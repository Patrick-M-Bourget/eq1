#pragma once

#include "DisplayFrame.h"
#include "DisplayGeometry.h"

namespace eq1::display
{

// How a Band's own curve is drawn.
struct CurveState
{
    int slot = 1;
    bool selected = false;
    bool bypassed = false;     // Bypassed, or a Side Band on mono, which has nothing to process
    float hover = 0.0f;        // 0 to 1, through the hover fade
    float globalBypass = 0.0f; // 0 to 1, through Global Bypass's fade
};
struct CurveStyle
{
    juce::Colour colour; // from the 24-slot palette by Band Slot, or the bypassed palette
    float lineWidth = 1.0f, lineAlpha = 0.0f, fillAlpha = 0.0f;
    float glowAlpha = 0.0f; // a wider translucent stroke under the selected Band's line
};
// Others: 1 px at 50 % over a 10 % fill (1.5 px, 95 % and 26 % on hover); the selected Band 1.5 px, full,
// a 30 % fill and a glow. Bypassed: the bypassed palette, the selected one at 50 % and 14 % with no
// glow, others at 60 % of their alphas. Global Bypass draws every Band bypassed at 45 % of its alphas.
CurveStyle bandCurveStyle (const CurveState& state);
// The Dynamic Range wash on the selected Dynamic Band: dynRange at 20 %, 10 % when Bypassed, 45 % of
// that under Global Bypass.
float dynamicRangeWashAlpha (bool bypassed, float globalBypass);
// The sum curve and its halo: full, 30 % under Global Bypass.
float sumCurveAlpha (float globalBypass);

// Each Band's own curve with its fill down to the 0 dB line, the selected one on top with the Dynamic
// Range wash between its curves at Gain and Gain + Dynamic Range, then the whole EQ's curve with its
// halo: the sum of the Bands that are playing, each Dynamic Band at its Live Gain. Nothing plays above
// Nyquist, so the curves stay level from there. The curves come from the Engine's own response maths
// (eq1/Response.h).
void paintCurves (juce::Graphics& g, const DisplayGeometry& geometry, const DisplayFrame& frame);

} // namespace eq1::display
