#pragma once

#include <cmath>

namespace eq1
{

// The Engine's conversions, each defined once so every site rounds the same way.

// An amplitude gain from decibels (20 dB per factor 10), and back.
inline double decibelsToGain (double db) { return std::pow (10.0, db / 20.0); }
inline double gainToDecibels (double gain) { return 20.0 * std::log10 (gain); }

// Decibels from a power (a squared amplitude, 10 dB per factor 10).
inline double powerToDecibels (double power) { return 10.0 * std::log10 (power); }

// The coefficient k of a one-pole smoother, x += k * (target - x) once a sample, whose step response
// reaches 1 - 1/e after timeConstant samples.
inline double onePoleCoefficient (double timeConstantSamples) { return 1.0 - std::exp (-1.0 / timeConstantSamples); }

} // namespace eq1
