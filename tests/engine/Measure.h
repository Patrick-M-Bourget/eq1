#pragma once

// The Engine tests' shared helpers: what the Engine is driven with (every Shape and Stereo
// Placement, the sample rates checked) and how what comes out of its public interface is measured.
// A helper a second test needs lives here (CODING_STANDARDS.md, "Shared behaviour lives once").

#include "eq1/Engine.h"

#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <numbers>
#include <vector>

namespace eq1::test
{

// Every Shape and every Stereo Placement, in their enums' order.
inline constexpr std::array allShapes = { Shape::Bell,  Shape::LowShelf, Shape::LowCut,    Shape::HighShelf, Shape::HighCut,
                                          Shape::Notch, Shape::BandPass, Shape::TiltShelf, Shape::FlatTilt,  Shape::AllPass };
inline constexpr std::array allPlacements = { StereoPlacement::Stereo, StereoPlacement::Left, StereoPlacement::Right,
                                              StereoPlacement::Mid, StereoPlacement::Side };

// The sample rates eq1 is checked at, one Catch2 section each.
inline double anySampleRate() { return GENERATE (44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0); }

inline BandSettings bellBand (double frequency, double gain, double q)
{
    return { .inUse = true, .shape = Shape::Bell, .frequency = frequency, .gain = gain, .q = q };
}

// Settings with one Bell in the first Band slot.
inline Settings bell (double frequency, double gain, double q)
{
    Settings settings;
    settings.bands[0] = bellBand (frequency, gain, q);
    return settings;
}

// Processes a unit impulse through a mono Engine until the response has decayed.
inline std::vector<float> impulseResponse (Engine& engine)
{
    constexpr int blockSize = 4096;
    constexpr size_t maxLength = size_t { 1 } << 22;

    std::vector<float> response;
    std::vector<float> block (blockSize);
    float peak = 0.0f;

    while (response.size() < maxLength)
    {
        std::fill (block.begin(), block.end(), 0.0f);
        if (response.empty())
            block[0] = 1.0f;

        float* channels[] = { block.data() };
        engine.process ({ channels, 1, blockSize });
        response.insert (response.end(), block.begin(), block.end());

        float blockPeak = 0.0f;
        for (float s : block)
            blockPeak = std::max (blockPeak, std::abs (s));
        peak = std::max (peak, blockPeak);

        if (response.size() > blockSize && blockPeak < peak * 1.0e-9f)
            break;
    }
    return response;
}

// Magnitude in dB of an impulse response at frequency (Hz), by direct DFT.
inline double magnitudeDb (const std::vector<float>& response, double frequency, double sampleRate)
{
    const double w = 2.0 * std::numbers::pi * frequency / sampleRate;
    const std::complex<double> step = std::polar (1.0, -w);
    std::complex<double> rotation = 1.0;
    std::complex<double> sum = 0.0;
    for (float s : response)
    {
        sum += static_cast<double> (s) * rotation;
        rotation *= step;
    }
    return 20.0 * std::log10 (std::abs (sum));
}

// Phase in radians, wrapped to +/-pi, of an impulse response at frequency (Hz), by direct DFT.
inline double phase (const std::vector<float>& response, double frequency, double sampleRate)
{
    const double w = 2.0 * std::numbers::pi * frequency / sampleRate;
    const std::complex<double> step = std::polar (1.0, -w);
    std::complex<double> rotation = 1.0;
    std::complex<double> sum = 0.0;
    for (float s : response)
    {
        sum += static_cast<double> (s) * rotation;
        rotation *= step;
    }
    return std::arg (sum);
}

// Log-spaced frequencies from 10 Hz up to and including Nyquist.
inline std::vector<double> frequenciesUpToNyquist (double sampleRate, int count)
{
    std::vector<double> frequencies;
    const double nyquist = sampleRate / 2.0;
    for (int i = 0; i < count; ++i)
        frequencies.push_back (10.0 * std::pow (nyquist / 10.0, i / double (count - 1)));
    return frequencies;
}

// How much sharper the sharpest corner of a sine of toneFrequency is, from sample `from` on, than a
// pure sine of the same peak level would allow: a sine A sin(wn) has a second difference of at most
// A w^2. Clicks show up far above 1.
inline double discontinuity (const std::vector<float>& output, double sampleRate, double toneFrequency, size_t from)
{
    double peak = 0.0, sharpest = 0.0;
    for (auto i = std::max (from, size_t { 2 }); i < output.size(); ++i)
    {
        peak = std::max (peak, static_cast<double> (std::abs (output[i])));
        sharpest = std::max (sharpest, std::abs (static_cast<double> (output[i]) - 2.0 * output[i - 1] + output[i - 2]));
    }
    const double w = 2.0 * std::numbers::pi * toneFrequency / sampleRate;
    return sharpest / (peak * w * w);
}

} // namespace eq1::test
