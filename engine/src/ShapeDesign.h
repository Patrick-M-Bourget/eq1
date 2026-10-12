#pragma once

#include "Biquad.h"
#include "eq1/Settings.h"

#include <array>
#include <complex>
#include <numbers>

namespace eq1
{

// A Band's filter: a cascade of biquads.
inline constexpr int maxSections = 16;

// The Q of a plain Butterworth section, 1 / sqrt 2 (0.71): -3 dB at its natural frequency, where the
// Cuts, the shelves and the Detection Range filters are designed from. (sqrt 2 / 2 is the same double
// as sqrt (0.5): halving is exact.)
inline constexpr double butterworthQ = std::numbers::sqrt2 / 2.0;

struct Cascade
{
    std::array<BiquadCoefficients, maxSections> sections {};
    int count = 0;
};

Cascade interpolate (const Cascade& from, const Cascade& to, double amount);

// What sets a Band's number of sections: its Shape and, for Shapes with a Slope, the order (Slope / 6,
// Slope / 12 for Notch, or 32 for Brickwall).
// Settings with the same structure can glide into each other; a new structure crossfades.
struct Structure
{
    Shape shape = Shape::Bell;
    int order = 0;

    bool operator== (const Structure&) const = default;
};

Structure structureOf (const BandSettings& settings);

struct ShapeParameters
{
    Structure structure;
    double frequency, gain, q;
};

// The decramped cascade for a Shape (ADR 0001). The section count depends only on the structure.
Cascade designShape (const ShapeParameters& parameters, double sampleRate);

// A Band's filter at its Gain, held to +/-30 dB as Live Gain is: what it plays without dynamics.
Cascade designBand (const BandSettings& band, double sampleRate);

// The cascade's complex response at frequency (Hz).
std::complex<double> responseAt (const Cascade& cascade, double frequency, double sampleRate);

} // namespace eq1
