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
// numerator chosen so the magnitude equals the analog section's at DC, at matchFrequency (by default
// the reference frequency) and at Nyquist (first-order sections: at DC and Nyquist).
BiquadCoefficients matchSection (const AnalogSection& section, double referenceFrequency, double sampleRate);
BiquadCoefficients matchSection (const AnalogSection& section, double referenceFrequency, double sampleRate, double matchFrequency);

// Where a high-pass section's gain is matched to the analog section's.
enum class HighPassGain
{
    atReference,
    // The geometric mean of the gains matching at the reference frequency and at Nyquist.
    betweenReferenceAndNyquist,
};

// A high-pass section, s^2 / (d2 s^2 + d1 s + d0) or s / (d1 s + d0): the poles as matchSection's,
// the zeros exactly at DC, and the magnitude equal to the analog section's where gain says.
BiquadCoefficients matchHighPass (const AnalogSection& section,
                                  double referenceFrequency,
                                  double sampleRate,
                                  HighPassGain gain = HighPassGain::atReference);

// A notch section, (n2 s^2 + n0) / (d2 s^2 + d1 s + d0): the poles as matchSection's, the zeros on the
// unit circle exactly at zeroFrequency, and the magnitude equal to the analog section's at DC.
BiquadCoefficients matchNotch (const AnalogSection& section, double referenceFrequency, double sampleRate, double zeroFrequency);

// A second-order section with the matched poles of poles (an analog section, around referenceFrequency)
// and a numerator chosen so its squared magnitude is squaredAtDc at DC, squaredAtMatch at matchFrequency
// and squaredAtNyquist at Nyquist: for a target other than the poles' own analog section.
BiquadCoefficients matchMagnitudes (const AnalogSection& poles,
                                    double referenceFrequency,
                                    double sampleRate,
                                    double matchFrequency,
                                    double squaredAtDc,
                                    double squaredAtMatch,
                                    double squaredAtNyquist);

// The analog section's squared magnitude at normalisedFrequency (a multiple of its reference frequency).
double analogSquared (const AnalogSection& section, double normalisedFrequency);

// The section with its poles and zeros swapped, so its magnitude is the reciprocal.
BiquadCoefficients inverse (const BiquadCoefficients& c);

} // namespace eq1
