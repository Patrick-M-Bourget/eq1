#pragma once

#include "Biquad.h"

namespace eq1
{

// An analog filter section, (n2 s^2 + n1 s + n0) / (d2 s^2 + d1 s + d0), with s normalised to a
// reference frequency. A first-order section has n2 = d2 = 0.
struct AnalogSection
{
    double n2, n1, n0;
    double d2, d1, d0;
};

// The decramped digital section (ADR 0001): poles by the matched z-transform of the analog poles,
// numerator chosen so the magnitude equals the analog section's at DC, at the reference frequency
// and at Nyquist (first-order sections: at DC and Nyquist).
BiquadCoefficients matchSection (const AnalogSection& section, double referenceFrequency, double sampleRate);

// The section with its poles and zeros swapped, so its magnitude is the reciprocal.
BiquadCoefficients inverse (const BiquadCoefficients& c);

} // namespace eq1
