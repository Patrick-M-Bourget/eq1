#pragma once

#include "DisplayFrame.h"
#include "DisplayGeometry.h"

#include <array>
#include <optional>
#include <vector>

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

// The Band whose filled curve contains a point, or 0: between 0 dB and its curve as drawn (at its Live
// Gain) at the point's Frequency, within 0.3 dB, where the curve is at least 0.4 dB from 0 dB. Bypassed
// Bands count. Where several contain it, the one nearest 0 dB there wins.
int bandAreaAt (const DisplayGeometry& geometry, const DisplayFrame& frame, juce::Point<float> point);

// The curves as paths, kept from frame to frame: each Band's own curve with its fill down to the 0 dB
// line, the selected Dynamic Band's wash between its curves at Gain and Gain + Dynamic Range, and the
// whole EQ's curve, the sum of the Bands that are playing, each Dynamic Band at its Live Gain. The
// curves come from the Engine's own response maths (eq1/Response.h), evaluated every pixelStep pixels
// at frequencies tabled once per width; nothing plays above Nyquist, so they stay level from there.
// A Band's curve is designed again only when what it was built from changes: the Band's settings at
// its drawn Gain, the geometry (width, height, Display Range) or the sample rate. The sum is built
// again when any Band's curve was, or which Bands play changes; the wash when its Band's settings or
// the geometry do. A frame that changes none of them, an Analyzer frame, only draws the paths it has.
// The colours and alphas (selection, hover, Global Bypass) are applied when painting, not cached.
class CurveCache
{
public:
    // Brings the paths up to date with what frame shows, designing only what changed.
    void update (const DisplayGeometry& geometry, const DisplayFrame& frame);
    // Paints the curves (update first): each Band's fill and line, the selected ones on top with the
    // wash, then the sum with its halo.
    void paint (juce::Graphics& g, const DisplayFrame& frame) const;

    // How many times a Band's curve, and the sum, have been built: what a test of a frame's work reads.
    int builds (int slot) const { return bands[static_cast<size_t> (slot - 1)].builds; }
    int sumBuilds() const { return sum.builds; }
    // Counts up on each update that changed anything, the Bands in use included: a key for what is
    // found from the curves, such as the Band whose filled curve is under the pointer.
    int generation() const { return changes; }

private:
    // What a Band's curve was built from: the Band at its drawn Gain.
    struct BandKey
    {
        BandSettings designed;
        DisplayGeometry geometry;
        double sampleRate = 0.0;
        bool operator== (const BandKey& other) const;
    };
    struct BandCurve
    {
        std::optional<BandKey> key;
        std::vector<double> db; // at each tabled frequency
        juce::Path line, fill;
        int builds = 0;
    };
    struct WashKey
    {
        int slot = 0;
        BandSettings band; // as heard
        DisplayGeometry geometry;
        double sampleRate = 0.0;
        bool operator== (const WashKey& other) const;
    };
    struct Sum
    {
        bool built = false;
        DisplayGeometry geometry;
        std::array<bool, numBandSlots> inUse {}, playing {}; // playing: in use and not silent
        juce::Path path;
        int builds = 0;
    };

    std::vector<double> frequencies; // every pixelStep pixels from the left edge, held below Nyquist
    int frequenciesWidth = -1;
    double frequenciesSampleRate = 0.0;
    std::array<BandCurve, numBandSlots> bands;
    std::optional<WashKey> washKey;
    juce::Path wash;
    Sum sum;
    int changes = 0;
};

} // namespace eq1::display
