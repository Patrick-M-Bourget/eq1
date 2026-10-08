#include "Measure.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
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

Settings shape (Shape s, double frequency, double gain, double q, double slope)
{
    Settings settings;
    settings.bands[0] = { .inUse = true, .shape = s, .frequency = frequency, .gain = gain, .q = q, .slope = slope };
    return settings;
}

// Allowed shelf error as a share of the curve's own span in dB: shelves near Nyquist and
// resonant shelves (Q above 2) are the limits of a cascade of decramped biquads (ADR 0001).
double shelfToleranceDb (double sampleRate, double frequency, double q, double spanDb)
{
    const double position = frequency / (sampleRate / 2.0);
    const double share = q > 2.0 ? 0.75 : position <= 0.73 ? 0.12 : 0.45;
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
    const double q = GENERATE (0.1, std::sqrt (0.5), 2.0, 40.0);
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
