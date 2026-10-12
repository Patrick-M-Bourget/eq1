#pragma once

#include "Double2.h"

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

// One step of a biquad in transposed direct form II: x in, with the section's state s1 and s2, in
// Sample arithmetic. Sample is double for one channel, or Double2 for a Stereo Band's two channels in
// one step, a lane each. The one place the arithmetic is written: the compiler sees the same
// expressions for both, and fuses or rounds them the same way (Double2.h), so a lane is bit for bit
// the scalar path.
template <typename Sample>
inline Sample biquadStep (Sample b0, Sample b1, Sample b2, Sample a1, Sample a2, Sample x, Sample& s1, Sample& s2)
{
    const Sample y = b0 * x + s1;
    s1 = b1 * x - a1 * y + s2;
    s2 = b2 * x - a2 * y;
    return y;
}

// The fallback Double2 is the scalar step on each lane.
inline ScalarDouble2 biquadStep (ScalarDouble2 b0, ScalarDouble2 b1, ScalarDouble2 b2, ScalarDouble2 a1, ScalarDouble2 a2,
                                 ScalarDouble2 x, ScalarDouble2& s1, ScalarDouble2& s2)
{
    return { biquadStep (b0.lane0, b1.lane0, b2.lane0, a1.lane0, a2.lane0, x.lane0, s1.lane0, s2.lane0),
             biquadStep (b0.lane1, b1.lane1, b2.lane1, a1.lane1, a2.lane1, x.lane1, s1.lane1, s2.lane1) };
}

// One channel of a biquad.
struct BiquadState
{
    double s1 = 0.0, s2 = 0.0;

    float process (const BiquadCoefficients& c, float input)
    {
        return static_cast<float> (biquadStep (c.b0, c.b1, c.b2, c.a1, c.a2, static_cast<double> (input), s1, s2));
    }
};

} // namespace eq1
