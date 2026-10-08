#include "ShapeDesign.h"

#include "MatchedDesign.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <numbers>
#include <utility>

namespace eq1
{

namespace
{

// Flat Tilt is built from first-order shelves, one per octave from 5 Hz to 40 kHz.
constexpr double tiltLowest = 5.0, tiltHighest = 40000.0;
constexpr int tiltSections = 13;
static_assert (tiltSections <= maxSections);
constexpr int maxSlopeOrder = 16;    // 96 dB/oct
constexpr int brickwallOrder = 32; // about 192 dB/oct: the most the section limit allows
static_assert (maxSlopeOrder / 2 + maxSlopeOrder % 2 <= maxSections);
static_assert (brickwallOrder / 2 <= maxSections);

double decibelsToGain (double db) { return std::pow (10.0, db / 20.0); }

void add (Cascade& cascade, const BiquadCoefficients& section) { cascade.sections[static_cast<size_t> (cascade.count++)] = section; }

void scale (Cascade& cascade, double factor)
{
    auto& first = cascade.sections[0];
    first.b0 *= factor;
    first.b1 *= factor;
    first.b2 *= factor;
}

// Bell: (s^2 + s A/Q + 1) / (s^2 + s/(A Q) + 1) with A = 10^(Gain/40). A cut is the inverse of
// the boost of the same size, whose high-Q poles the matched z-transform follows well.
Cascade designBell (const ShapeParameters& p, double sampleRate)
{
    const double a = std::pow (10.0, std::abs (p.gain) / 40.0);
    const auto boost = matchSection ({ 1.0, a / p.q, 1.0, 1.0, 1.0 / (a * p.q), 1.0 }, p.frequency, sampleRate);
    Cascade cascade;
    add (cascade, p.gain >= 0.0 ? boost : inverse (boost));
    return cascade;
}

// The analog sections of a Butterworth Low Shelf with the given DC gain (linear): zeros on a circle
// of radius g^(1/2N), poles on the reciprocal circle, so it reaches half its gain in dB at s = 1.
// Q scales every second-order section's Q by (Q / 0.71)^(1 / sections).
template <typename Use>
void lowShelfSections (int order, double dcGain, double q, Use use)
{
    const double rz = std::pow (dcGain, 1.0 / (2.0 * order)), rp = 1.0 / rz;
    const int pairs = order / 2;
    const double resonance = pairs > 0 ? std::pow (q / std::sqrt (0.5), 1.0 / pairs) : 1.0;
    for (int k = 1; k <= pairs; ++k)
    {
        const double sectionQ = resonance / (2.0 * std::sin ((2 * k - 1) * std::numbers::pi / (2.0 * order)));
        use (AnalogSection { 1.0, rz / sectionQ, rz * rz, 1.0, rp / sectionQ, rp * rp });
    }
    if (order % 2 == 1)
        use (AnalogSection { 0.0, 1.0, rz, 0.0, 1.0, rp });
}

// Each shelf is designed in the direction whose poles sit at or below Frequency, where the
// matched z-transform is accurate, and inverted for the other direction: Low Shelves as boosts,
// High Shelves as cuts.
Cascade designLowShelf (const ShapeParameters& p, double sampleRate)
{
    Cascade cascade;
    lowShelfSections (p.structure.order, decibelsToGain (std::abs (p.gain)), p.q, [&] (const AnalogSection& section) {
        const auto boost = matchSection (section, p.frequency, sampleRate);
        add (cascade, p.gain >= 0.0 ? boost : inverse (boost));
    });
    return cascade;
}

// A High Shelf is the Low Shelf mirrored in frequency (s -> 1/s). Mirrored, a boost's poles sit
// above Frequency, so the cut (the inverse) is designed and inverted back for a boost.
Cascade designHighShelf (const ShapeParameters& p, double sampleRate)
{
    Cascade cascade;
    lowShelfSections (p.structure.order, decibelsToGain (std::abs (p.gain)), p.q, [&] (const AnalogSection& low) {
        const AnalogSection boost = low.d2 == 0.0 ? AnalogSection { 0.0, low.n0, low.n1, 0.0, low.d0, low.d1 }
                                                  : AnalogSection { low.n0, low.n1, low.n2, low.d0, low.d1, low.d2 };
        const AnalogSection cut { boost.d2, boost.d1, boost.d0, boost.n2, boost.n1, boost.n0 };
        const auto designed = matchSection (cut, p.frequency, sampleRate);
        add (cascade, p.gain >= 0.0 ? inverse (designed) : designed);
    });
    return cascade;
}

// The analog sections of a Butterworth low-pass of the given order, -3 dB at s = 1. Q scales each
// second-order section's Q as for shelves.
template <typename Use>
void butterworthSections (int order, double q, Use use)
{
    const int pairs = order / 2;
    const double resonance = pairs > 0 ? std::pow (q / std::sqrt (0.5), 1.0 / pairs) : 1.0;
    for (int k = 1; k <= pairs; ++k)
    {
        const double sectionQ = resonance / (2.0 * std::sin ((2 * k - 1) * std::numbers::pi / (2.0 * order)));
        use (AnalogSection { 0.0, 0.0, 1.0, 1.0, 1.0 / sectionQ, 1.0 });
    }
    if (order % 2 == 1)
        use (AnalogSection { 0.0, 0.0, 1.0, 0.0, 1.0, 1.0 });
}

// Where a High Cut section is matched: its damped natural frequency, naturalFrequency x sqrt (1 - 1/(4 Q^2))
// with d1 = 1/Q (at least 0.1 x naturalFrequency, and at most atMost x it), held at or below half Nyquist.
double dampedMatchFrequency (double naturalFrequency, double d1, double sampleRate, double atMost = 1.0)
{
    const double damped = std::min (std::sqrt (std::max (1.0 - d1 * d1 / 4.0, 0.01)), atMost);
    return std::min (naturalFrequency * damped, sampleRate / 4.0);
}

// High Cut: each section matched at its damped natural frequency, which keeps the passband from
// bulging near Nyquist (at least 0.1 x Frequency, for sections with Q below 0.5). The match point is
// held at or below half Nyquist, where a High Cut would otherwise boost before it cuts: it rolls off
// early instead.
Cascade designHighCut (const ShapeParameters& p, double sampleRate)
{
    Cascade cascade;
    butterworthSections (p.structure.order, p.q, [&] (const AnalogSection& section) {
        if (section.d2 == 0.0)
        {
            add (cascade, matchSection (section, p.frequency, sampleRate));
            return;
        }
        add (cascade, matchSection (section, p.frequency, sampleRate, dampedMatchFrequency (p.frequency, section.d1, sampleRate)));
    });
    return cascade;
}

// Low Cut: the High Cut mirrored in frequency (s -> 1/s), with its zeros exactly at DC.
Cascade designLowCut (const ShapeParameters& p, double sampleRate)
{
    Cascade cascade;
    butterworthSections (p.structure.order, p.q, [&] (const AnalogSection& low) {
        const AnalogSection high = low.d2 == 0.0 ? AnalogSection { 0.0, 1.0, 0.0, 0.0, 1.0, 1.0 }
                                                 : AnalogSection { 1.0, 0.0, 0.0, 1.0, low.d1, 1.0 };
        add (cascade, matchHighPass (high, p.frequency, sampleRate));
    });
    return cascade;
}

// A second-order section rescaled to its own natural frequency w0 (s -> w0 s), so it is designed
// around w0 x Frequency, where its poles are, and held below Nyquist there if need be.
struct Normalised
{
    AnalogSection section;
    double naturalFrequency; // w0, as a multiple of Frequency
};

Normalised normalise (const AnalogSection& a)
{
    const double w0 = std::sqrt (a.d0 / a.d2);
    return { { a.n2 / a.d2, a.n1 / (a.d2 * w0), a.n0 / a.d0, 1.0, a.d1 / (a.d2 * w0), 1.0 }, w0 };
}

// The Butterworth low-pass of the given order (plain, Q 0.71) transformed pole by pole: each
// upper-half-plane pole p becomes the two poles roots (p); use ({ first, second }) gets them, each to be
// paired with its conjugate into a real s^2 + d1 s + d0. oddPole () is called for the real pole.
template <typename Roots, typename Use, typename OddPole>
void transformedButterworth (int order, Roots roots, Use use, OddPole oddPole)
{
    butterworthSections (order, std::sqrt (0.5), [&] (const AnalogSection& lowPass) {
        if (lowPass.d2 == 0.0)
        {
            oddPole();
            return;
        }
        const std::complex<double> pole { -lowPass.d1 / 2.0, std::sqrt (std::max (1.0 - lowPass.d1 * lowPass.d1 / 4.0, 0.0)) };
        const auto [first, second] = roots (pole);
        use (std::array { first, second });
    });
}

// Roots of s^2 - b s + 1.
std::pair<std::complex<double>, std::complex<double>> reciprocalRoots (std::complex<double> b)
{
    const auto d = std::sqrt (b * b - 4.0);
    return { (b + d) / 2.0, (b - d) / 2.0 };
}

// Band Pass: s -> Q (s + 1/s) of a Butterworth low-pass of order Slope / 6. Each low-pass pole pair
// becomes a lower and an upper pole pair, with s / Q for each. Together they are designed as a Low
// Cut section on the lower pair (s^2) and a High Cut section on the upper one (1 / Q^2): split this
// way a wide Band Pass stays accurate. The odd pole gives one band-pass section.
Cascade designBandPass (const ShapeParameters& p, double sampleRate)
{
    Cascade cascade;
    const double q = p.q;
    transformedButterworth (
        p.structure.order,
        [q] (std::complex<double> pole) { return reciprocalRoots (pole / q); },
        [&] (const std::array<std::complex<double>, 2>& roots) {
            auto lowerRoot = roots[0], upperRoot = roots[1];
            if (std::norm (lowerRoot) > std::norm (upperRoot))
                std::swap (lowerRoot, upperRoot);

            const auto lower = normalise ({ 1.0, 0.0, 0.0, 1.0, -2.0 * lowerRoot.real(), std::norm (lowerRoot) });
            add (cascade,
                 matchHighPass (lower.section, p.frequency * lower.naturalFrequency, sampleRate, HighPassGain::betweenReferenceAndNyquist));

            // Matched as a High Cut section, but at most 0.9 of its natural frequency: a very sharp
            // section matched at its peak takes the poles' tiny error as gain.
            const auto upper = normalise ({ 0.0, 0.0, 1.0 / (q * q), 1.0, -2.0 * upperRoot.real(), std::norm (upperRoot) });
            const double reference = p.frequency * upper.naturalFrequency;
            add (cascade, matchSection (upper.section, reference, sampleRate, dampedMatchFrequency (reference, upper.section.d1, sampleRate, 0.9)));
        },
        [&] { add (cascade, matchSection ({ 0.0, 1.0 / q, 0.0, 1.0, 1.0 / q, 1.0 }, p.frequency, sampleRate)); });
    return cascade;
}

// Notch: s -> 1 / (Q (s + 1/s)) of a Butterworth low-pass of order Slope / 12. Every section's zeros
// sit exactly at Frequency; each is designed around its own poles.
Cascade designNotch (const ShapeParameters& p, double sampleRate)
{
    Cascade cascade;
    const auto addSection = [&] (double d1, double d0) {
        const auto section = normalise ({ 1.0, 0.0, 1.0, 1.0, d1, d0 });
        add (cascade, matchNotch (section.section, p.frequency * section.naturalFrequency, sampleRate, p.frequency));
    };
    const double q = p.q;
    transformedButterworth (
        p.structure.order,
        [q] (std::complex<double> pole) { return reciprocalRoots (1.0 / (pole * q)); },
        [&] (const std::array<std::complex<double>, 2>& roots) {
            for (const auto& root : roots)
                addSection (-2.0 * root.real(), std::norm (root));
        },
        [&] { addSection (1.0 / q, 1.0); });
    return cascade;
}

// All Pass: Butterworth poles of order Slope / 6, Q scaled as for Cuts, with zeros mirrored. Bilinear,
// prewarped at Frequency, so its phase there is exactly -90 degrees per order. ADR 0001 rules out
// bilinear designs because they cramp magnitude; an all-pass's magnitude is flat by construction.
Cascade designAllPass (const ShapeParameters& p, double sampleRate)
{
    const double k = std::tan (std::min (std::numbers::pi * p.frequency / sampleRate, 0.49 * std::numbers::pi));
    Cascade cascade;
    butterworthSections (p.structure.order, p.q, [&] (const AnalogSection& section) {
        if (section.d2 == 0.0)
        {
            const double a1 = (k - 1.0) / (k + 1.0);
            add (cascade, { a1, 1.0, 0.0, a1, 0.0 });
            return;
        }
        const double a0 = 1.0 + section.d1 * k + k * k;
        const double a1 = 2.0 * (k * k - 1.0) / a0, a2 = (1.0 - section.d1 * k + k * k) / a0;
        add (cascade, { a2, a1, 1.0, a1, a2 });
    });
    return cascade;
}

// Tilt Shelf: the High Shelf moved down by half its Gain, so it passes 0 dB at Frequency.
Cascade designTiltShelf (const ShapeParameters& p, double sampleRate)
{
    auto cascade = designHighShelf (p, sampleRate);
    scale (cascade, decibelsToGain (-p.gain / 2.0));
    return cascade;
}

// Flat Tilt: a straight line of Gain / 10 dB per octave (Gain is the tilt across 20 Hz to 20 kHz),
// built from first-order shelves each rising its share over one step, and set to 0 dB at Frequency.
Cascade designFlatTilt (const ShapeParameters& p, double sampleRate)
{
    const double octaves = std::log2 (tiltHighest / tiltLowest);
    const double step = octaves / tiltSections;
    const double perOctave = p.gain / std::log2 (20000.0 / 20.0);
    const double k = std::pow (10.0, perOctave * step / 40.0);

    Cascade cascade;
    double analogGainAtFrequency = 1.0;
    for (int i = 0; i < tiltSections; ++i)
    {
        const double centre = tiltLowest * std::exp2 ((i + 0.5) * step);
        const double zero = centre / k, pole = centre * k;
        // (s/zero + 1) / (s/pole + 1), with s normalised to the pole.
        add (cascade, matchSection ({ 0.0, pole / zero, 1.0, 0.0, 1.0, 1.0 }, pole, sampleRate));
        const double ratio = p.frequency / zero, poleRatio = p.frequency / pole;
        analogGainAtFrequency *= std::sqrt ((1.0 + ratio * ratio) / (1.0 + poleRatio * poleRatio));
    }

    // Pass exactly 0 dB at Frequency: measured on the digital cascade when Frequency is below Nyquist.
    double gainAtFrequency = analogGainAtFrequency;
    if (p.frequency < sampleRate / 2.0)
    {
        const std::complex<double> z = std::polar (1.0, -2.0 * std::numbers::pi * p.frequency / sampleRate);
        std::complex<double> h = 1.0;
        for (int i = 0; i < cascade.count; ++i)
        {
            const auto& c = cascade.sections[static_cast<size_t> (i)];
            h *= (c.b0 + c.b1 * z) / (1.0 + c.a1 * z);
        }
        gainAtFrequency = std::abs (h);
    }
    scale (cascade, 1.0 / gainAtFrequency);
    return cascade;
}

// The lowest Slope each Shape plays, in dB/oct (ADR 0003).
double minimumSlope (Shape shape)
{
    switch (shape)
    {
        case Shape::LowCut:
        case Shape::HighCut:
        case Shape::BandPass: return 0.0;
        case Shape::Bell:
        case Shape::Notch: return 12.0;
        case Shape::LowShelf:
        case Shape::HighShelf:
        case Shape::TiltShelf:
        case Shape::FlatTilt:
        case Shape::AllPass: return 6.0;
    }
    return 6.0;
}

} // namespace

Cascade interpolate (const Cascade& from, const Cascade& to, double amount)
{
    Cascade result;
    result.count = to.count;
    for (size_t i = 0; i < static_cast<size_t> (to.count); ++i)
        result.sections[i] = interpolate (from.sections[i], to.sections[i], amount);
    return result;
}

Structure structureOf (const BandSettings& settings)
{
    if (! hasSlope (settings.shape))
        return { settings.shape, 0 };
    if (isCut (settings.shape) && settings.brickwall)
        return { settings.shape, brickwallOrder };

    // The stored Slope is raised to the Shape's minimum and rounded to the nearest whole order
    // (ADR 0003; docs/dsp/filter-design.md, "Slopes between whole orders").
    // A Notch's order counts 12 dB/oct steps.
    const double slope = std::clamp (settings.slope, minimumSlope (settings.shape), 6.0 * maxSlopeOrder);
    const double step = settings.shape == Shape::Notch ? 12.0 : 6.0;
    return { settings.shape, static_cast<int> (std::lround (slope / step)) };
}

Cascade designShape (const ShapeParameters& p, double sampleRate)
{
    switch (p.structure.shape)
    {
        case Shape::Bell: return designBell (p, sampleRate);
        case Shape::LowShelf: return designLowShelf (p, sampleRate);
        case Shape::HighShelf: return designHighShelf (p, sampleRate);
        case Shape::TiltShelf: return designTiltShelf (p, sampleRate);
        case Shape::FlatTilt: return designFlatTilt (p, sampleRate);
        case Shape::LowCut: return designLowCut (p, sampleRate);
        case Shape::HighCut: return designHighCut (p, sampleRate);
        case Shape::Notch: return designNotch (p, sampleRate);
        case Shape::BandPass: return designBandPass (p, sampleRate);
        case Shape::AllPass: return designAllPass (p, sampleRate);
    }
    return {};
}

} // namespace eq1
