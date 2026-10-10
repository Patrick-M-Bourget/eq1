#include "CurvesLayer.h"

#include "eq1/Response.h"
#include "../staple/Tokens.h"

#include <vector>

namespace eq1::display
{

namespace
{
// A curve through db, one value every pixelStep pixels from the left edge.
juce::Path curve (const DisplayGeometry& geometry, const std::vector<double>& db)
{
    juce::Path path;
    const auto range = static_cast<double> (geometry.rangeDb);
    for (size_t i = 0; i < db.size(); ++i)
    {
        const juce::Point<float> point { static_cast<float> (i) * DisplayGeometry::pixelStep,
                                         geometry.yOf (juce::jlimit (-range * 1.5, range * 1.5, db[i])) };
        if (i == 0)
            path.startNewSubPath (point);
        else
            path.lineTo (point);
    }
    return path;
}
} // namespace

void paintCurves (juce::Graphics& g, const DisplayGeometry& geometry, const DisplayFrame& frame)
{
    const double sampleRate = frame.sampleRate;
    std::vector<double> frequencies;
    for (float x = 0.0f; x <= static_cast<float> (geometry.width); x += DisplayGeometry::pixelStep)
        frequencies.push_back (std::min (geometry.frequencyAt (x), 0.4999 * sampleRate));
    // On mono a Side Band has nothing to process (#6): it plays no part in the whole EQ's curve.
    std::vector<double> total (frequencies.size(), 0.0), bandDb (frequencies.size());
    for (int slot = 1; slot <= numBandSlots; ++slot)
    {
        const auto& band = frame.bands.bands[static_cast<size_t> (slot - 1)];
        if (! band.inUse)
            continue;
        const bool silent = band.bypass || (frame.mono && band.placement == StereoPlacement::Side);
        auto live = band;
        live.gain = frame.drawnGains[static_cast<size_t> (slot - 1)];
        bandResponseDb (live, frequencies.data(), bandDb.data(), static_cast<int> (bandDb.size()), sampleRate);
        if (! silent)
            for (size_t i = 0; i < total.size(); ++i)
                total[i] += bandDb[i];
        g.setColour (bandColour (slot).withAlpha (silent ? 0.12f : frame.selected.contains (slot) ? 0.6f : 0.3f));
        g.strokePath (curve (geometry, bandDb), juce::PathStrokeType (1.2f));
    }
    g.setColour (staple::tokens::colour::curveMain);
    g.strokePath (curve (geometry, total), juce::PathStrokeType (2.0f));
}

} // namespace eq1::display
