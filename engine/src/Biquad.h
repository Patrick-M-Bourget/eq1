#pragma once

namespace eq1
{

struct BiquadCoefficients
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0; // a0 normalised to 1
};

// One channel of a biquad, transposed direct form II.
struct BiquadState
{
    double s1 = 0.0, s2 = 0.0;

    float process (const BiquadCoefficients& c, float input)
    {
        const double x = input;
        const double y = c.b0 * x + s1;
        s1 = c.b1 * x - c.a1 * y + s2;
        s2 = c.b2 * x - c.a2 * y;
        return static_cast<float> (y);
    }
};

} // namespace eq1
