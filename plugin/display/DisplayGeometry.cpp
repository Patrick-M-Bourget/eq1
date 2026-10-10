#include "DisplayGeometry.h"

#include <cmath>

namespace eq1::display
{

float DisplayGeometry::xOf (double frequency) const
{
    return static_cast<float> (std::log (frequency / lowestFrequency) / std::log (highestFrequency / lowestFrequency) * width);
}

double DisplayGeometry::frequencyAt (float x) const
{
    return lowestFrequency * std::pow (highestFrequency / lowestFrequency, juce::jlimit (0.0, 1.0, static_cast<double> (x) / width));
}

float DisplayGeometry::yOf (double db) const
{
    const auto range = static_cast<float> (rangeDb);
    const float half = static_cast<float> (height) * 0.5f;
    return half - static_cast<float> (db) / range * (half - handleRadius);
}

double DisplayGeometry::dbAt (float y) const
{
    const auto range = static_cast<double> (rangeDb);
    const double half = height * 0.5;
    return (half - y) / (half - handleRadius) * range;
}

juce::Point<float> DisplayGeometry::handleOf (const BandSettings& band) const
{
    const auto range = static_cast<double> (rangeDb);
    const double gain = hasGain (band.shape) ? juce::jlimit (-range, range, band.gain) : 0.0;
    return { xOf (band.frequency), yOf (gain) };
}

} // namespace eq1::display
