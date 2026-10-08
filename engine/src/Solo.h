#pragma once

#include "eq1/Settings.h"

#include <algorithm>
#include <cmath>

namespace eq1
{

// The region of the input a Soloed Band plays (docs/dsp/filter-design.md, "Detection"), as the
// settings of a filter: around Frequency, as wide as Q, for Bell, Notch, Band Pass and All Pass;
// below Frequency for a Low Shelf and above it for a High Shelf; everything for Tilt Shelf and Flat
// Tilt; what a Cut removes for a Cut. Solo plays the region, not the Band's effect, so Gain plays no
// part.
inline BandSettings soloRegionOf (const BandSettings& band)
{
    BandSettings region { .inUse = true, .frequency = band.frequency, .q = std::sqrt (0.5) };
    switch (band.shape)
    {
        case Shape::Bell:
        case Shape::Notch:
        case Shape::BandPass:
        case Shape::AllPass:
            region.shape = Shape::BandPass;
            region.slope = 6.0;
            region.q = band.q;
            break;
        case Shape::LowShelf:
            region.shape = Shape::HighCut;
            region.slope = 12.0;
            break;
        case Shape::HighShelf:
            region.shape = Shape::LowCut;
            region.slope = 12.0;
            break;
        case Shape::TiltShelf:
        case Shape::FlatTilt:
            region.inUse = false; // a filter not in use passes everything
            break;
        case Shape::LowCut:
        case Shape::HighCut:
            region.shape = band.shape == Shape::LowCut ? Shape::HighCut : Shape::LowCut;
            region.slope = std::max (band.slope, 6.0);
            region.brickwall = band.brickwall;
            break;
    }
    return region;
}

} // namespace eq1
