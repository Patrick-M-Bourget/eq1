#pragma once

namespace eq1
{

struct BiquadCoefficients
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0; // a0 normalised to 1
};

inline BiquadCoefficients interpolate (const BiquadCoefficients& from, const BiquadCoefficients& to, double amount)
{
    const auto mix = [amount] (double a, double b) { return a + amount * (b - a); };
    return { mix (from.b0, to.b0), mix (from.b1, to.b1), mix (from.b2, to.b2), mix (from.a1, to.a1), mix (from.a2, to.a2) };
}

// One channel of a biquad, transposed direct form II. In double throughout: a cascade carries the
// signal from one section to the next without rounding it to float in between.
struct BiquadState
{
    double s1 = 0.0, s2 = 0.0;

    double process (const BiquadCoefficients& c, double x)
    {
        const double y = c.b0 * x + s1;
        s1 = c.b1 * x - c.a1 * y + s2;
        s2 = c.b2 * x - c.a2 * y;
        return y;
    }
};

} // namespace eq1
