#pragma once

#include "Biquad.h"

namespace eq1
{

// A decramped (matched-response) Bell, per ADR 0001.
BiquadCoefficients designBell (double sampleRate, double frequency, double gain, double q);

} // namespace eq1
