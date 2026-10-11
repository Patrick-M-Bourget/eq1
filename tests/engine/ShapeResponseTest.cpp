#include "Measure.h"

#include "eq1/Response.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <numbers>

using namespace eq1;

namespace
{

// The analog targets (ADR 0001). Shelves are Butterworth shelves of order Slope / 6 whose
// Frequency is where they reach half their Gain in dB. Q sets the corner resonance: each
// second-order section's Q is the Butterworth one scaled by (Q / 0.71)^(1 / sections), so
// Q 0.71 is a plain Butterworth shelf and the resonance stays alike at any Slope.
double lowShelfDb (double f, double frequency, double gain, int order, double q)
{
    const double g = std::pow (10.0, gain / 20.0);
    const double rz = std::pow (g, 1.0 / (2.0 * order)), rp = 1.0 / rz;
    const int pairs = order / 2;
    const double resonance = pairs > 0 ? std::pow (q / std::sqrt (0.5), 1.0 / pairs) : 1.0;
    const std::complex<double> s { 0.0, f / frequency };

    std::complex<double> h = 1.0;
    for (int k = 1; k <= pairs; ++k)
    {
        const double sectionQ = resonance / (2.0 * std::sin ((2 * k - 1) * std::numbers::pi / (2.0 * order)));
        h *= (s * s + s * rz / sectionQ + rz * rz) / (s * s + s * rp / sectionQ + rp * rp);
    }
    if (order % 2 == 1)
        h *= (s + rz) / (s + rp);
    return 20.0 * std::log10 (std::abs (h));
}

double highShelfDb (double f, double frequency, double gain, int order, double q)
{
    return gain + lowShelfDb (f, frequency, -gain, order, q);
}

double tiltShelfDb (double f, double frequency, double gain, int order, double q)
{
    return highShelfDb (f, frequency, gain, order, q) - gain / 2.0;
}

// Flat Tilt: a straight line through 0 dB at Frequency; Gain is the tilt across 20 Hz to 20 kHz.
double flatTiltDb (double f, double frequency, double gain)
{
    return gain / std::log2 (20000.0 / 20.0) * std::log2 (f / frequency);
}

// Low Cut and High Cut: Butterworth of the given order with Frequency at -3 dB, each second-order
// section's Q scaled by (Q / 0.71)^(1 / sections) as for shelves. Order 32 is Brickwall.
double cutDb (double f, double frequency, int order, double q, bool lowCut)
{
    const int pairs = order / 2;
    const double resonance = pairs > 0 ? std::pow (q / std::sqrt (0.5), 1.0 / pairs) : 1.0;
    const std::complex<double> s = lowCut ? std::complex<double> { 0.0, -frequency / f } : std::complex<double> { 0.0, f / frequency };

    std::complex<double> h = 1.0;
    for (int k = 1; k <= pairs; ++k)
    {
        const double sectionQ = resonance / (2.0 * std::sin ((2 * k - 1) * std::numbers::pi / (2.0 * order)));
        h /= s * s + s / sectionQ + 1.0;
    }
    if (order % 2 == 1)
        h /= s + 1.0;
    return 20.0 * std::log10 (std::abs (h));
}

// Allowed Cut error in dB, above and below the target, by where Frequency sits relative to Nyquist
// (docs/dsp/filter-design.md, "Test tolerances"). A steep Cut's dB error at its corner says little,
// so the target may shift by a twelfth of an octave either way.
struct CutTolerance
{
    double above, below;
};

CutTolerance cutTolerance (Shape s, double sampleRate, double frequency, double q)
{
    const double position = frequency / (sampleRate / 2.0);
    const int region = position <= 0.45 ? 0 : position <= 0.73 ? 1 : 2;
    constexpr CutTolerance lowCut[] = { { 1.5, 3.0 }, { 2.0, 3.0 }, { 4.0, 3.0 } };
    constexpr CutTolerance resonantLowCut[] = { { 4.5, 1.0 }, { 6.0, 1.0 }, { 6.0, 9.0 } };
    constexpr CutTolerance highCut[] = { { 0.5, 1.0 }, { 1.0, 5.0 }, { 1.0, 12.0 } };
    if (s == Shape::HighCut)
        return highCut[region];
    return q > 2.0 ? resonantLowCut[region] : lowCut[region];
}

// Butterworth low-pass of the given order (plain, Q 0.71), evaluated at the complex s given.
std::complex<double> butterworth (std::complex<double> s, int order)
{
    std::complex<double> h = 1.0;
    for (int k = 1; k <= order / 2; ++k)
        h /= s * s + s * 2.0 * std::sin ((2 * k - 1) * std::numbers::pi / (2.0 * order)) + 1.0;
    if (order % 2 == 1)
        h /= s + 1.0;
    return h;
}

// Band Pass: the Butterworth low-pass of order Slope / 6 at s -> Q (s + 1/s).
double bandPassDb (double f, double frequency, int order, double q)
{
    const double x = f / frequency;
    return 20.0 * std::log10 (std::abs (butterworth ({ 0.0, q * (x - 1.0 / x) }, order)));
}

// Notch: the Butterworth low-pass of order Slope / 12 at s -> 1 / (Q (s + 1/s)), silent at Frequency.
double notchDb (double f, double frequency, int order, double q)
{
    const double x = f / frequency;
    if (std::abs (x - 1.0) < 1.0e-12)
        return -400.0;
    return 20.0 * std::log10 (std::abs (butterworth (1.0 / std::complex<double> { 0.0, q * (x - 1.0 / x) }, order)));
}

// All Pass: Butterworth poles of order Slope / 6, Q scaled as for Cuts, with mirrored zeros. Its
// phase in radians, unwrapped: -pi/2 x order at Frequency.
double allPassPhase (double f, double frequency, int order, double q)
{
    const double x = f / frequency;
    const int pairs = order / 2;
    const double resonance = pairs > 0 ? std::pow (q / std::sqrt (0.5), 1.0 / pairs) : 1.0;
    double phase = order % 2 == 1 ? -2.0 * std::atan (x) : 0.0;
    for (int k = 1; k <= pairs; ++k)
    {
        const double sectionQ = resonance / (2.0 * std::sin ((2 * k - 1) * std::numbers::pi / (2.0 * order)));
        phase -= 2.0 * std::atan2 (x / sectionQ, 1.0 - x * x);
    }
    return phase;
}

// Allowed Band Pass and Notch error in dB, read as for Cuts (docs/dsp/filter-design.md, "Test tolerances").
CutTolerance bandTolerance (Shape s, double sampleRate, double frequency, int order)
{
    const double position = frequency / (sampleRate / 2.0);
    const int region = position <= 0.45 ? 0 : position <= 0.73 ? 1 : 2;
    constexpr CutTolerance bandPass[] = { { 0.5, 1.0 }, { 2.0, 2.5 }, { 5.0, 2.0 } };
    constexpr CutTolerance steepBandPass[] = { { 2.5, 5.5 }, { 3.0, 6.0 }, { 5.0, 9.0 } };
    constexpr CutTolerance notch[] = { { 0.1, 6.0 }, { 0.1, 6.5 }, { 0.1, 6.5 } };
    if (s == Shape::Notch)
        return notch[region];
    return order > 4 ? steepBandPass[region] : bandPass[region];
}

// Allowed All Pass phase error in degrees below Frequency.
double allPassToleranceDegrees (double sampleRate, double frequency, int order)
{
    const double position = frequency / (sampleRate / 2.0);
    return position <= 0.45 ? 6.0 * order + 6.0 : position <= 0.73 ? 20.0 * order + 10.0 : 50.0 * order + 10.0;
}

Settings shape (Shape s, double frequency, double gain, double q, double slope)
{
    Settings settings;
    settings.bands[0] = { .inUse = true, .shape = s, .frequency = frequency, .gain = gain, .q = q, .slope = slope };
    return settings;
}

// Allowed shelf error as a share of the curve's own span in dB (docs/dsp/filter-design.md, "Test
// tolerances"): resonant shelves (Q above 2) split each section in two biquads, and come within 12% up
// to 0.45 x Nyquist as gentle ones do. Nearer Nyquist shelves are the limits of a cascade of
// decramped biquads (ADR 0001): resonant ones a little more up to 0.73 x Nyquist, and as gentle ones
// do above it.
double shelfToleranceDb (double sampleRate, double frequency, double q, double spanDb)
{
    const double position = frequency / (sampleRate / 2.0);
    const bool resonant = q > 2.0;
    const double share = position <= 0.45 ? 0.12 : position <= 0.73 ? (resonant ? 0.15 : 0.12) : 0.45;
    return 0.6 * (position > 0.73 ? 1.0 : 0.1) + share * spanDb;
}

std::vector<float> responseOf (double sampleRate, const Settings& settings)
{
    Engine engine;
    engine.prepare (sampleRate, 4096, 1);
    engine.setSettings (settings);
    return test::impulseResponse (engine);
}

} // namespace

TEST_CASE ("Low Shelf, High Shelf and Tilt Shelf match their analog targets up to Nyquist across Slopes", "[response]")
{
    const double sampleRate = GENERATE (44100.0, 48000.0, 96000.0);
    const double frequency = GENERATE (20.0, 200.0, 2000.0, 9000.0, 15000.0, 20000.0);
    const double gain = GENERATE (-30.0, -6.0, 3.0, 18.0);
    const int order = GENERATE (1, 2, 5, 16);
    const double q = GENERATE (0.1, std::sqrt (0.5), 2.0, 10.0, 40.0);
    const Shape s = GENERATE (Shape::LowShelf, Shape::HighShelf, Shape::TiltShelf);
    if (frequency > 0.91 * sampleRate / 2.0)
        return;

    const auto target = [&] (double f) {
        switch (s)
        {
            case Shape::LowShelf: return lowShelfDb (f, frequency, gain, order, q);
            case Shape::HighShelf: return highShelfDb (f, frequency, gain, order, q);
            default: return tiltShelfDb (f, frequency, gain, order, q);
        }
    };

    const auto response = responseOf (sampleRate, shape (s, frequency, gain, q, 6.0 * order));
    const auto frequencies = test::frequenciesUpToNyquist (sampleRate, 64);

    double span = 0.0;
    for (double f : frequencies)
        span = std::max (span, std::abs (target (f) - target (frequencies.front())));
    span = std::max (span, std::abs (gain));

    for (double f : frequencies)
    {
        CAPTURE (sampleRate, frequency, gain, order, q, static_cast<int> (s), f);
        const double error = test::magnitudeDb (response, f, sampleRate) - target (f);
        REQUIRE (std::abs (error) <= shelfToleranceDb (sampleRate, frequency, q, span));
    }
}

TEST_CASE ("Butterworth shelves reach half their Gain at Frequency and their full Gain away from it")
{
    const double sampleRate = GENERATE (44100.0, 96000.0);
    const double gain = GENERATE (-12.0, 9.0);
    const int order = GENERATE (1, 2, 4, 8);

    const auto low = responseOf (sampleRate, shape (Shape::LowShelf, 1000.0, gain, std::sqrt (0.5), 6.0 * order));
    const auto high = responseOf (sampleRate, shape (Shape::HighShelf, 1000.0, gain, std::sqrt (0.5), 6.0 * order));

    CAPTURE (sampleRate, gain, order);
    CHECK (std::abs (test::magnitudeDb (low, 1000.0, sampleRate) - gain / 2.0) < 0.05);
    CHECK (std::abs (test::magnitudeDb (high, 1000.0, sampleRate) - gain / 2.0) < 0.05);
    CHECK (std::abs (test::magnitudeDb (low, 0.0, sampleRate) - gain) < 0.01);
    CHECK (std::abs (test::magnitudeDb (high, 0.0, sampleRate)) < 0.01);
    CHECK (std::abs (test::magnitudeDb (high, 10000.0, sampleRate) - highShelfDb (10000.0, 1000.0, gain, order, std::sqrt (0.5))) < 0.05);
}

TEST_CASE ("Flat Tilt is a straight line in dB per octave through Frequency", "[response]")
{
    const double sampleRate = GENERATE (44100.0, 48000.0, 96000.0);
    const double frequency = GENERATE (30.0, 1000.0, 12000.0);
    const double gain = GENERATE (-30.0, -4.0, 9.0, 30.0);

    const auto response = responseOf (sampleRate, shape (Shape::FlatTilt, frequency, gain, 1.0, 12.0));
    for (double f : test::frequenciesUpToNyquist (sampleRate, 64))
    {
        // The line holds from 20 Hz until the cascade ends at 40 kHz, or near Nyquist.
        if (f < 20.0 || f > std::min (30000.0, 0.9 * sampleRate / 2.0))
            continue;
        CAPTURE (sampleRate, frequency, gain, f);
        const double error = test::magnitudeDb (response, f, sampleRate) - flatTiltDb (f, frequency, gain);
        REQUIRE (std::abs (error) <= 0.05 + 0.03 * std::abs (gain));
    }
}

TEST_CASE ("Low Cut and High Cut match their analog targets up to Nyquist at every Slope and Brickwall", "[response]")
{
    const double sampleRate = GENERATE (44100.0, 48000.0, 96000.0);
    const double frequency = GENERATE (20.0, 200.0, 2000.0, 9000.0, 15000.0, 20000.0);
    const int order = GENERATE (range (1, 17), 32);
    const double q = GENERATE (0.1, std::sqrt (0.5), 2.0, 10.0, 40.0);
    const Shape s = GENERATE (Shape::LowCut, Shape::HighCut);
    if (frequency > 0.91 * sampleRate / 2.0)
        return;

    // Cuts ignore Gain, so a stored Gain must not show.
    auto settings = shape (s, frequency, 9.0, q, order == 32 ? 96.0 : 6.0 * order);
    settings.bands[0].brickwall = order == 32;
    const auto response = responseOf (sampleRate, settings);
    const auto tolerance = cutTolerance (s, sampleRate, frequency, q);
    const double twelfth = std::exp2 (1.0 / 12.0);

    for (double f : test::frequenciesUpToNyquist (sampleRate, 64))
    {
        const auto target = [&] (double at) { return cutDb (at, frequency, order, q, s == Shape::LowCut); };
        const double lowest = std::min ({ target (f / twelfth), target (f), target (f * twelfth) });
        const double highest = std::max ({ target (f / twelfth), target (f), target (f * twelfth) });
        const double measured = test::magnitudeDb (response, f, sampleRate);

        CAPTURE (sampleRate, frequency, order, q, static_cast<int> (s), f, target (f), measured);
        // Never louder than the curve; below -60 dB anything quieter will do.
        REQUIRE (measured <= std::max (highest, -60.0) + tolerance.above);
        // Never quieter than the curve while it is still within 24 dB of the passband.
        if (lowest > -24.0)
            REQUIRE (measured >= lowest - tolerance.below);
    }
}

TEST_CASE ("A Cut or Band Pass at a Slope of 0 passes the signal unchanged")
{
    const Shape s = GENERATE (Shape::LowCut, Shape::HighCut, Shape::BandPass);
    const double slope = GENERATE (0.0, 2.9);
    const auto response = responseOf (48000.0, shape (s, 1000.0, 12.0, 1.0, slope));

    CAPTURE (static_cast<int> (s), slope);
    for (size_t i = 0; i < response.size(); ++i)
        REQUIRE (response[i] == (i == 0 ? 1.0f : 0.0f));
}

TEST_CASE ("Brickwall has no effect on Shapes other than the Cuts")
{
    const Shape s = GENERATE (Shape::Bell, Shape::LowShelf, Shape::HighShelf, Shape::Notch, Shape::BandPass, Shape::TiltShelf,
                              Shape::FlatTilt, Shape::AllPass);
    auto brickwall = shape (s, 1000.0, 9.0, 1.0, 24.0);
    brickwall.bands[0].brickwall = true;

    const auto with = responseOf (48000.0, brickwall);
    const auto without = responseOf (48000.0, shape (s, 1000.0, 9.0, 1.0, 24.0));

    CAPTURE (static_cast<int> (s));
    REQUIRE (with == without);
}

TEST_CASE ("Cuts, Notch, Band Pass and All Pass stay stable across their whole range, including Frequency above Nyquist")
{
    const double sampleRate = GENERATE (44100.0, 96000.0);
    const double frequency = GENERATE (10.0, 1000.0, 20000.0, 22000.0, 30000.0);
    const double q = GENERATE (0.025, 0.71, 40.0);
    const int order = GENERATE (1, 2, 16, 32);
    const Shape s = GENERATE (Shape::LowCut, Shape::HighCut, Shape::Notch, Shape::BandPass, Shape::AllPass);

    auto settings = shape (s, frequency, 0.0, q, order == 32 ? 96.0 : 6.0 * order);
    settings.bands[0].brickwall = order == 32;
    const auto response = responseOf (sampleRate, settings);

    CAPTURE (sampleRate, frequency, q, order, static_cast<int> (s));
    REQUIRE (std::all_of (response.begin(), response.end(), [] (float x) { return std::isfinite (x); }));
    const auto tail = response.end() - 4096;
    const auto magnitude = [] (float a, float b) { return std::abs (a) < std::abs (b); };
    const float peak = std::abs (*std::max_element (response.begin(), tail, magnitude));
    const float tailPeak = std::abs (*std::max_element (tail, response.end(), magnitude));
    // A Q 40 Band Pass or Notch at 10 Hz rings for longer than the response is measured: it need only be decaying.
    const bool ringsOn = frequency == 10.0 && q == 40.0 && (s == Shape::Notch || s == Shape::BandPass);
    REQUIRE (tailPeak < (ringsOn ? 0.1f * peak : 1.0e-4f));
}

TEST_CASE ("Band Pass and Notch match their analog targets up to Nyquist at every whole-order Slope", "[response]")
{
    const double sampleRate = GENERATE (44100.0, 48000.0, 96000.0);
    const double frequency = GENERATE (20.0, 200.0, 2000.0, 9000.0, 15000.0, 20000.0);
    const double q = GENERATE (0.1, std::sqrt (0.5), 2.0, 10.0, 40.0);
    const Shape s = GENERATE (Shape::BandPass, Shape::Notch);
    // Band Pass is of order Slope / 6, Notch of order Slope / 12.
    const int order = GENERATE (range (1, 17));
    if (frequency > 0.91 * sampleRate / 2.0 || (s == Shape::Notch && order > 8))
        return;

    // Neither has Gain, so a stored Gain must not show.
    const double slope = (s == Shape::Notch ? 12.0 : 6.0) * order;
    const auto response = responseOf (sampleRate, shape (s, frequency, 9.0, q, slope));
    const auto tolerance = bandTolerance (s, sampleRate, frequency, order);
    const double twelfth = std::exp2 (1.0 / 12.0);
    const auto target = [&] (double at) { return s == Shape::Notch ? notchDb (at, frequency, order, q) : bandPassDb (at, frequency, order, q); };

    for (double f : test::frequenciesUpToNyquist (sampleRate, 64))
    {
        const double lowest = std::min ({ target (f / twelfth), target (f), target (f * twelfth) });
        const double highest = std::max ({ target (f / twelfth), target (f), target (f * twelfth) });
        const double measured = test::magnitudeDb (response, f, sampleRate);

        CAPTURE (sampleRate, frequency, order, q, static_cast<int> (s), f, target (f), measured);
        REQUIRE (measured <= std::max (highest, -60.0) + tolerance.above);
        if (lowest > -24.0)
            REQUIRE (measured >= lowest - tolerance.below);
    }
}

TEST_CASE ("All Pass is flat and turns the phase by 90 degrees per order at Frequency, matching the analog phase below it", "[response]")
{
    const double sampleRate = GENERATE (44100.0, 48000.0, 96000.0);
    const double frequency = GENERATE (20.0, 200.0, 2000.0, 9000.0, 15000.0, 20000.0);
    const double q = GENERATE (0.1, std::sqrt (0.5), 2.0, 10.0, 40.0);
    const int order = GENERATE (range (1, 17));
    if (frequency > 0.91 * sampleRate / 2.0)
        return;

    const auto response = responseOf (sampleRate, shape (Shape::AllPass, frequency, 9.0, q, 6.0 * order));
    const auto wrappedDegrees = [] (double radians) { return std::remainder (radians, 2.0 * std::numbers::pi) * 180.0 / std::numbers::pi; };
    const double tolerance = allPassToleranceDegrees (sampleRate, frequency, order);
    CAPTURE (sampleRate, frequency, order, q);

    CHECK (std::abs (wrappedDegrees (test::phase (response, frequency, sampleRate) - allPassPhase (frequency, frequency, order, q))) < 0.1);
    for (double f : test::frequenciesUpToNyquist (sampleRate, 64))
    {
        CAPTURE (f);
        REQUIRE (std::abs (test::magnitudeDb (response, f, sampleRate)) < 0.01);
        // Wrapped phase can't show an error beyond 180 degrees, so the bound holds where it is smaller.
        if (f <= frequency && tolerance < 180.0)
            REQUIRE (std::abs (wrappedDegrees (test::phase (response, f, sampleRate) - allPassPhase (f, frequency, order, q))) <= tolerance);
    }
}

TEST_CASE ("A steep Band Pass's response moves smoothly as Frequency or Q crosses where its upper sections are held", "[response]")
{
    // Swept in steps of 5 Hz in Frequency, or 0.2% in Q, the response changes evenly from step to step,
    // whether or not an upper section is held near Nyquist: no step stands out from the ones on either
    // side (the second difference, in dB, stays small), so the design has no jumps to click on. A steep
    // skirt may still move by a fraction of a dB per step.
    const double sampleRate = GENERATE (44100.0, 48000.0, 96000.0);
    const double q = GENERATE (0.1, std::sqrt (0.5), 2.0);
    const int order = GENERATE (5, 16);
    const bool sweepQ = GENERATE (false, true);
    CAPTURE (sampleRate, q, order, sweepQ);
    const double nyquist = sampleRate / 2.0;
    const double probes[] = { 0.2 * nyquist, 0.6 * nyquist, 0.8 * nyquist, 0.92 * nyquist, 0.98 * nyquist };
    std::array<std::array<double, std::size (probes)>, 3> last {}; // the last three steps' responses
    // Frequency from 0.6 to 1.2 x Nyquist, across the hold at 0.95; Q from half to twice q.
    const int steps = sweepQ ? 700 : static_cast<int> (0.6 * nyquist / 5.0);
    for (int step = 0; step < steps; ++step)
    {
        const double frequency = sweepQ ? 0.9 * nyquist : 0.6 * nyquist + 5.0 * step;
        const double bandQ = sweepQ ? q * 0.5 * std::pow (1.002, step) : q;
        CAPTURE (frequency, bandQ);
        BandSettings band { .inUse = true, .shape = Shape::BandPass, .frequency = frequency, .q = bandQ, .slope = 6.0 * order };
        std::rotate (last.begin(), last.begin() + 1, last.end());
        bandResponseDb (band, probes, last[2].data(), static_cast<int> (std::size (probes)), sampleRate);
        if (step < 2)
            continue;
        // Deep in the stopband the dB values are numerical noise. The limit sits between the smoothest
        // design's worst (0.26 dB) and a design that jumps where a section is held (1.3 dB).
        for (size_t i = 0; i < std::size (probes); ++i)
            if (std::max ({ last[0][i], last[1][i], last[2][i] }) > -100.0)
                REQUIRE (std::abs (last[0][i] - 2.0 * last[1][i] + last[2][i]) < 0.6);
    }
}
