#include "ShapeDesign.h"

#include "MatchedDesign.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

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
        const double sectionQ = 1.0 / section.d1;
        const double damped = p.frequency * std::sqrt (std::max (1.0 - 1.0 / (4.0 * sectionQ * sectionQ), 0.01));
        add (cascade, matchSection (section, p.frequency, sampleRate, std::min (damped, sampleRate / 4.0)));
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
    // Only the shelves' and Cuts' filters depend on Slope so far; Bell Slope is #19, the rest #22.
    const bool cut = settings.shape == Shape::LowCut || settings.shape == Shape::HighCut;
    const bool usesSlope = cut || settings.shape == Shape::LowShelf || settings.shape == Shape::HighShelf
                           || settings.shape == Shape::TiltShelf;
    if (! usesSlope)
        return { settings.shape, 0 };
    if (cut && settings.brickwall)
        return { settings.shape, brickwallOrder };

    // The stored Slope is raised to the Shape's minimum and rounded to the nearest whole order
    // (ADR 0003; docs/dsp/filter-design.md, "Slopes between whole orders").
    const double slope = std::clamp (settings.slope, minimumSlope (settings.shape), 6.0 * maxSlopeOrder);
    return { settings.shape, static_cast<int> (std::lround (slope / 6.0)) };
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
        // Not built yet: no sections, so the signal passes unchanged.
        case Shape::Notch:
        case Shape::BandPass:
        case Shape::AllPass: return {};
    }
    return {};
}

} // namespace eq1
