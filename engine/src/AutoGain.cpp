#include "AutoGain.h"

#include "Units.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace eq1
{

void AutoGainEstimate::prepare (double sampleRate)
{
    rate = sampleRate;
    // Pink noise from 20 Hz up to 20 kHz, or up to Nyquist at low sample rates, at the middle of each
    // of numPoints equal steps in log frequency.
    constexpr double lowest = 20.0;
    const double highest = std::min (20000.0, 0.49 * sampleRate);
    const double ratio = std::pow (highest / lowest, 1.0 / numPoints);
    for (int point = 0; point < numPoints; ++point)
    {
        const double frequency = lowest * std::pow (ratio, point + 0.5);
        sineSquared[static_cast<size_t> (point)] = Phi::sineSquaredHalf (2.0 * std::numbers::pi * frequency / sampleRate);
    }
    next = numPoints;
}

void AutoGainEstimate::start (const Settings& settings)
{
    numCascades = 0;
    for (const auto& band : settings.bands)
    {
        if (! band.inUse || band.bypass)
            continue;
        const auto cascade = designBand (scaledByGainScale (band, settings.gainScale), rate);
        auto& power = cascades[static_cast<size_t> (numCascades++)];
        power.count = cascade.count;
        for (int i = 0; i < cascade.count; ++i)
        {
            const auto& c = cascade.sections[static_cast<size_t> (i)];
            power.sections[static_cast<size_t> (i)] = { SquaredMagnitude (c.b0, c.b1, c.b2), SquaredMagnitude (1.0, c.a1, c.a2) };
        }
    }
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
        const auto phi = Phi::fromSineSquared (sineSquared[static_cast<size_t> (next)]);
        double power = 1.0;
        for (int c = 0; c < numCascades; ++c)
        {
            const auto& cascade = cascades[static_cast<size_t> (c)];
            for (int i = 0; i < cascade.count; ++i)
            {
                const auto& section = cascade.sections[static_cast<size_t> (i)];
                power *= section.numerator.at (phi) / section.denominator.at (phi);
            }
        }
        sum += power;
    }
    if (running())
        return false;
    const double db = numCascades == 0 ? 0.0 : -powerToDecibels (sum / numPoints);
    // A degenerate curve (no power at all, or not a number) asks for no compensation it can't have.
    resultDb = std::isnan (db) ? 0.0 : std::clamp (db, -autoGainLimitDb, autoGainLimitDb);
    return true;
}

} // namespace eq1
