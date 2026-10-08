#include "MatchedDesign.h"

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

    explicit SquaredMagnitude (double c0, double c1, double c2)
        : p0 ((c0 + c1 + c2) * (c0 + c1 + c2)), p1 ((c0 - c1 + c2) * (c0 - c1 + c2)), p2 (-4.0 * c0 * c2)
    {
    }
    SquaredMagnitude() = default;

    double at (const Phi& phi) const { return p0 * phi.phi0 + p1 * phi.phi1 + p2 * phi.phi2; }
};

// The positive-root, minimum-phase factorisation back to polynomial coefficients. A cut is the
// inverse of a boost, so these roots may become poles: they are kept inside the unit circle.
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

double analogSquared (const AnalogSection& a, double normalisedFrequency)
{
    const std::complex<double> s { 0.0, normalisedFrequency };
    return std::norm ((a.n2 * s * s + a.n1 * s + a.n0) / (a.d2 * s * s + a.d1 * s + a.d0));
}

// Above ~0.98 Nyquist the matched poles fold back; they, and the match points, are held just below it.
double digitalFrequency (double frequency, double sampleRate)
{
    return std::min (2.0 * std::numbers::pi * frequency / sampleRate, 0.98 * std::numbers::pi);
}

// Poles: the analog roots, scaled to the digital frequency w of the reference and mapped by z = e^(sT).
void matchPoles (const AnalogSection& section, double w, BiquadCoefficients& c)
{
    if (section.d2 == 0.0)
    {
        c.a1 = -std::exp (-section.d0 / section.d1 * w);
        return;
    }
    const double disc = section.d1 * section.d1 - 4.0 * section.d2 * section.d0;
    const double re = -section.d1 / (2.0 * section.d2) * w;
    if (disc < 0.0)
    {
        const double im = std::sqrt (-disc) / (2.0 * section.d2) * w;
        c.a1 = -2.0 * std::exp (re) * std::cos (im);
        c.a2 = std::exp (2.0 * re);
    }
    else
    {
        const double spread = std::sqrt (disc) / (2.0 * section.d2) * w;
        c.a1 = -(std::exp (re + spread) + std::exp (re - spread));
        c.a2 = std::exp (2.0 * re);
    }
}

} // namespace

BiquadCoefficients matchSection (const AnalogSection& section, double referenceFrequency, double sampleRate)
{
    return matchSection (section, referenceFrequency, sampleRate, referenceFrequency);
}

BiquadCoefficients matchSection (const AnalogSection& section, double referenceFrequency, double sampleRate, double matchFrequency)
{
    const double w = digitalFrequency (referenceFrequency, sampleRate);
    const double nyquist = 0.5 * sampleRate / referenceFrequency;

    BiquadCoefficients c;
    matchPoles (section, w, c);
    const SquaredMagnitude den (1.0, c.a1, c.a2);
    if (section.d2 == 0.0)
    {
        const double root0 = std::sqrt (den.p0 * analogSquared (section, 0.0));
        const double root1 = std::sqrt (den.p1 * analogSquared (section, nyquist));
        c.b0 = 0.5 * (root0 + root1);
        c.b1 = 0.5 * (root0 - root1);
        return c;
    }

    const double wMatch = digitalFrequency (matchFrequency, sampleRate);
    const Phi atMatch (wMatch);

    SquaredMagnitude num;
    num.p0 = den.p0 * analogSquared (section, 0.0);
    num.p1 = den.p1 * analogSquared (section, nyquist);
    num.p2 = (den.at (atMatch) * analogSquared (section, wMatch / (2.0 * std::numbers::pi) * sampleRate / referenceFrequency)
              - num.p0 * atMatch.phi0 - num.p1 * atMatch.phi1)
             / atMatch.phi2;
    factor (num, c.b0, c.b1, c.b2);
    return c;
}

// The three-point match can't place two zeros exactly at DC, so a high-pass section fixes them
// there, (1 - z^-1)^2 or (1 - z^-1), and scales to the analog magnitude at the reference.
BiquadCoefficients matchHighPass (const AnalogSection& section, double referenceFrequency, double sampleRate)
{
    const double w = digitalFrequency (referenceFrequency, sampleRate);

    BiquadCoefficients c;
    matchPoles (section, w, c);
    c.b0 = 1.0;
    c.b1 = section.d2 == 0.0 ? -1.0 : -2.0;
    c.b2 = section.d2 == 0.0 ? 0.0 : 1.0;

    const Phi atReference (w);
    const double digitalSquared = SquaredMagnitude (c.b0, c.b1, c.b2).at (atReference) / SquaredMagnitude (1.0, c.a1, c.a2).at (atReference);
    const double k = std::sqrt (analogSquared (section, w / (2.0 * std::numbers::pi) * sampleRate / referenceFrequency) / digitalSquared);
    c.b0 *= k;
    c.b1 *= k;
    c.b2 *= k;
    return c;
}

BiquadCoefficients inverse (const BiquadCoefficients& c)
{
    const double g = 1.0 / c.b0;
    return { g, c.a1 * g, c.a2 * g, c.b1 * g, c.b2 * g };
}

} // namespace eq1
