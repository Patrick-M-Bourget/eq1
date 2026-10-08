#include "BellDesign.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

namespace eq1
{

namespace
{

// Squared magnitude of a biquad polynomial p0 + p1 z^-1 + p2 z^-2 is a linear combination of
// phi0 = cos^2(w/2), phi1 = sin^2(w/2), phi2 = 4 phi0 phi1 with weights
// P0 = (p0 + p1 + p2)^2, P1 = (p0 - p1 + p2)^2, P2 = -4 p0 p2.
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

    double at (const Phi& phi) const { return p0 * phi.phi0 + p1 * phi.phi1 + p2 * phi.phi2; }
};

// The positive-root, minimum-phase factorisation back to polynomial coefficients. A cut is the
// inverse of a boost, so these roots become its poles: they are kept inside the unit circle.
void factor (const SquaredMagnitude& m, double& c0, double& c1, double& c2)
{
    const double root0 = std::sqrt (std::max (m.p0, 0.0));
    const double root1 = std::sqrt (std::max (m.p1, 0.0));
    const double w = 0.5 * (root0 + root1);
    c0 = 0.5 * (w + std::sqrt (std::max (w * w + m.p2, 0.0)));
    c1 = 0.5 * (root0 - root1);
    c2 = -m.p2 / (4.0 * c0);

    // Near and above Nyquist the target can be out of a biquad's reach and the roots land outside
    // the unit circle; reflecting them inside (swapping c0 and c2) keeps the magnitude unchanged.
    if (std::abs (c2) > c0)
        std::swap (c0, c2);
}

// |H(j 2 pi f)|^2 of the analog Bell (s^2 + s A/Q + 1) / (s^2 + s/(A Q) + 1), s normalised to frequency.
double analogBellSquared (double f, double frequency, double a, double q)
{
    const std::complex<double> s { 0.0, f / frequency };
    return std::norm ((s * s + s * (a / q) + 1.0) / (s * s + s / (a * q) + 1.0));
}

// Matched boost: poles by the matched z-transform of the analog poles, numerator chosen so the
// squared magnitude equals the analog Bell's at DC, at the Bell's Frequency and at Nyquist.
BiquadCoefficients designBoost (double sampleRate, double frequency, double a, double q)
{
    // Above ~0.98 Nyquist the matched poles fold back; hold them, and the match point, just below it.
    const double w0 = std::min (2.0 * std::numbers::pi * frequency / sampleRate, 0.98 * std::numbers::pi);

    const double zeta = 1.0 / (2.0 * a * q);
    const double decay = std::exp (-zeta * w0);
    const double a1 = zeta <= 1.0 ? -2.0 * decay * std::cos (std::sqrt (1.0 - zeta * zeta) * w0)
                                  : -2.0 * decay * std::cosh (std::sqrt (zeta * zeta - 1.0) * w0);
    const double a2 = decay * decay;

    const SquaredMagnitude den { (1.0 + a1 + a2) * (1.0 + a1 + a2), (1.0 - a1 + a2) * (1.0 - a1 + a2), -4.0 * a2 };

    const Phi atMatch (w0);
    const double nyquist = sampleRate / 2.0;
    const double matchFrequency = w0 * sampleRate / (2.0 * std::numbers::pi);

    SquaredMagnitude num;
    num.p0 = den.p0;
    num.p1 = den.p1 * analogBellSquared (nyquist, frequency, a, q);
    num.p2 = (den.at (atMatch) * analogBellSquared (matchFrequency, frequency, a, q)
              - num.p0 * atMatch.phi0 - num.p1 * atMatch.phi1)
             / atMatch.phi2;

    BiquadCoefficients c;
    factor (num, c.b0, c.b1, c.b2);
    c.a1 = a1;
    c.a2 = a2;
    return c;
}

} // namespace

BiquadCoefficients designBell (double sampleRate, double frequency, double gain, double q)
{
    // A cut is exactly the inverse of the boost of the same size, so design the boost and swap.
    const double a = std::pow (10.0, std::abs (gain) / 40.0);
    const auto boost = designBoost (sampleRate, frequency, a, q);
    if (gain >= 0.0)
        return boost;

    const double g = 1.0 / boost.b0;
    return { g, boost.a1 * g, boost.a2 * g, boost.b1 * g, boost.b2 * g };
}

} // namespace eq1
