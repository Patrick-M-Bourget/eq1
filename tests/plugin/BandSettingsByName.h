#pragma once

#include "eq1/Settings.h"

#include <cmath>
#include <map>
#include <string>
#include <vector>

// A Band's settings by name, for tests to compare Bands with: BandSettings' own == warns on its
// doubles (-Wfloat-equal), and a failure prints each setting.
inline std::map<std::string, double> byName (const eq1::BandSettings& band)
{
    return { { "in_use", band.inUse },
             { "bypass", band.bypass },
             { "shape", static_cast<double> (band.shape) },
             { "frequency", band.frequency },
             { "gain", band.gain },
             { "q", band.q },
             { "slope", band.slope },
             { "brickwall", band.brickwall },
             { "placement", static_cast<double> (band.placement) },
             { "detection_source", static_cast<double> (band.detectionSource) },
             { "detection_range", static_cast<double> (band.detectionRange) },
             { "detection_low", band.detectionLow },
             { "detection_high", band.detectionHigh },
             { "dynamic_range", band.dynamicRange },
             { "threshold", band.threshold },
             { "threshold_auto", band.thresholdAuto },
             { "attack", band.attack },
             { "release", band.release },
             { "dynamics_bypass", band.dynamicsBypass } };
}

inline std::vector<std::map<std::string, double>> byName (const std::vector<eq1::BandSettings>& bands)
{
    std::vector<std::map<std::string, double>> named;
    for (const auto& band : bands)
        named.push_back (byName (band));
    return named;
}

// The same Band to within a parameter's round trip through a normalised value, as a Preset loads.
inline bool near (const eq1::BandSettings& a, const eq1::BandSettings& b)
{
    const auto x = byName (a), y = byName (b);
    for (const auto& [name, value] : y)
        if (std::abs (x.at (name) - value) > 1.0e-5 * std::max (1.0, std::abs (value)))
            return false;
    return true;
}
