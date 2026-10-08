#pragma once

// Measurement helpers for Engine tests: drive the Engine through its public interface and
// measure what comes out.

#include "eq1/Engine.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <vector>

namespace eq1::test
{

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

} // namespace eq1::test
