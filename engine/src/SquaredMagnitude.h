#pragma once

#include <cmath>

namespace eq1
{

// The squared magnitude of a biquad polynomial p0 + p1 z^-1 + p2 z^-2 on the unit circle, at
// z = e^(-iw), in real arithmetic: a linear combination of phi0 = cos^2(w/2), phi1 = sin^2(w/2),
// phi2 = 4 phi0 phi1 with weights P0 = (p0 + p1 + p2)^2, P1 = (p0 - p1 + p2)^2, P2 = -4 p0 p2. The
// matched design (MatchedDesign.cpp) sets magnitudes with it; Auto Gain's estimate (AutoGain.cpp)
// reads a cascade's power gain with it, three multiply-adds per polynomial and one real division
// per section, instead of a complex division.
struct Phi
{
    double phi0, phi1, phi2;

    explicit Phi (double w)
    {
        const double s = std::sin (w / 2.0);
        phi1 = s * s;
        phi0 = 1.0 - phi1;
        phi2 = 4.0 * phi0 * phi1;
    }
};

struct SquaredMagnitude
{
    double p0, p1, p2;

    explicit SquaredMagnitude (double c0, double c1, double c2)
        : p0 ((c0 + c1 + c2) * (c0 + c1 + c2)), p1 ((c0 - c1 + c2) * (c0 - c1 + c2)), p2 (-4.0 * c0 * c2)
    {
    }
    SquaredMagnitude() = default;

    double at (const Phi& phi) const { return p0 * phi.phi0 + p1 * phi.phi1 + p2 * phi.phi2; }
};

} // namespace eq1
