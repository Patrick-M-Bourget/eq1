#include "ValueText.h"

#include <cmath>

namespace eq1::valueText
{

namespace
{
// value to a fixed number of decimals, a half rounded away from zero ("2.3" for 2.25 to 1 decimal).
// juce::String (value, 0) would write every digit it has.
juce::String fixed (double value, int decimals)
{
    const double scale = std::pow (10.0, decimals);
    const double rounded = std::round (value * scale) / scale;
    return decimals > 0 ? juce::String (rounded, decimals) : juce::String (juce::roundToInt (rounded));
}

// value to decimals, as a number: what it reads as.
double shown (double value, int decimals) { return fixed (value, decimals).getDoubleValue(); }
} // namespace

juce::String frequency (double hz, FrequencyDecimals decimals)
{
    const int hzDecimals = shown (hz, decimals.belowHundredHz) < 100.0 ? decimals.belowHundredHz : decimals.hz;
    if (shown (hz, hzDecimals) < 1000.0)
        return fixed (hz, hzDecimals) + " Hz";
    const double kHz = hz / 1000.0;
    return fixed (kHz, shown (kHz, decimals.kHz) < 10.0 ? decimals.kHz : decimals.fromTenKHz) + " kHz";
}

juce::String frequencyLabel (double hz)
{
    return hz >= 1000.0 ? juce::String (juce::roundToInt (hz / 1000.0)) + "k" : juce::String (juce::roundToInt (hz));
}

juce::String signedDb (double db, DbFormat format)
{
    const auto minus = juce::String::charToString (0x2212);
    const juce::String unit = format.unit ? " dB" : "";
    if (std::isinf (db))
        return (db < 0.0 ? minus : "+") + "inf" + unit;
    const auto magnitude = fixed (std::abs (db), format.decimals);
    const bool zero = juce::exactlyEqual (magnitude.getDoubleValue(), 0.0);
    const juce::String sign = zero ? (format.plusAtZero ? "+" : "") : (db < 0.0 ? minus : "+");
    return sign + magnitude + unit;
}

juce::String slope (double dbPerOctave)
{
    const bool whole = std::abs (dbPerOctave - std::round (dbPerOctave)) < 0.05;
    return fixed (dbPerOctave, whole ? 0 : 1) + " dB/oct";
}

juce::String percent (double value)
{
    const int rounded = juce::roundToInt (value);
    return (rounded < 0 ? juce::String::charToString (0x2212) : juce::String()) + juce::String (std::abs (rounded)) + "%";
}

} // namespace eq1::valueText
