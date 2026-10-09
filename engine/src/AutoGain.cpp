#include "AutoGain.h"

#include <algorithm>
#include <cmath>
#include <complex>

namespace eq1
{

void AutoGainEstimate::start (const Settings& settings, double sampleRate)
{
    rate = sampleRate;
    numCascades = 0;
    for (const auto& band : settings.bands)
        if (band.inUse && ! band.bypass)
            cascades[static_cast<size_t> (numCascades++)] = designBand (scaledByGainScale (band, settings.gainScale), sampleRate);
    // Pink noise up to 20 kHz, or up to Nyquist at low sample rates.
    const double highest = std::min (20000.0, 0.49 * sampleRate);
    ratio = std::pow (highest / lowest, 1.0 / numPoints);
    next = 0;
    sum = 0.0;
}

bool AutoGainEstimate::advance (int points)
{
    if (! running())
        return true;
    const int end = std::min (numPoints, next + points);
    for (; next < end; ++next)
    {
        // The middle of each of numPoints equal steps in log frequency.
        const double frequency = lowest * std::pow (ratio, next + 0.5);
        double power = 1.0;
        for (int c = 0; c < numCascades; ++c)
            power *= std::norm (responseAt (cascades[static_cast<size_t> (c)], frequency, rate));
        sum += power;
    }
    if (running())
        return false;
    resultDb = numCascades == 0 ? 0.0 : -10.0 * std::log10 (sum / numPoints);
    return true;
}

} // namespace eq1
