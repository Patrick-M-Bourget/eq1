#include "AnalyzerSpectrum.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>
#include <vector>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using eq1::AnalyzerResolution;
using eq1::AnalyzerSpeed;
using eq1::AnalyzerSpectrum;

namespace
{
constexpr double sampleRate = 48000.0;

// Feeds seconds of the sum of sines (frequency, amplitude) to the spectrum, as the editor does at
// 60 fps, and lets it settle.
void play (AnalyzerSpectrum& spectrum, std::vector<std::pair<double, double>> sines, double seconds, AnalyzerSpeed speed)
{
    const int perFrame = static_cast<int> (sampleRate / 60.0);
    std::vector<float> block (static_cast<size_t> (perFrame));
    static long n = 0;
    for (int frame = 0; frame < static_cast<int> (seconds * 60.0); ++frame)
    {
        for (auto& s : block)
        {
            double sum = 0.0;
            for (auto [frequency, amplitude] : sines)
                sum += amplitude * std::sin (2.0 * std::numbers::pi * frequency * static_cast<double> (n) / sampleRate);
            s = static_cast<float> (sum);
            ++n;
        }
        spectrum.push (block.data(), perFrame);
        spectrum.update (1.0 / 60.0, speed);
    }
}
} // namespace

TEST_CASE ("A full-scale sine shows at 0 dB at its Frequency, and the rest of the spectrum far below")
{
    const auto resolution = GENERATE (AnalyzerResolution::low, AnalyzerResolution::medium, AnalyzerResolution::high,
                                      AnalyzerResolution::maximum);
    AnalyzerSpectrum spectrum;
    spectrum.prepare (sampleRate, resolution);
    play (spectrum, { { 1000.0, 1.0 } }, 3.0, AnalyzerSpeed::fast);

    CAPTURE (static_cast<int> (resolution));
    // Between FFT bins a Hann window reads up to 1.5 dB low.
    CHECK_THAT (spectrum.levelDb (spectrum.peakNear (1000.0), 0.0), WithinAbs (0.0, 1.5));
    CHECK (spectrum.levelDb (100.0, 0.0) < -60.0);
    CHECK (spectrum.levelDb (10000.0, 0.0) < -60.0);
}

TEST_CASE ("Analyzer Tilt tilts the display around 1 kHz and nothing else")
{
    AnalyzerSpectrum spectrum;
    spectrum.prepare (sampleRate, AnalyzerResolution::medium);
    play (spectrum, { { 300.0, 0.5 }, { 3000.0, 0.5 } }, 2.0, AnalyzerSpeed::fast);

    for (double f : { 100.0, 300.0, 1000.0, 3000.0, 12000.0 })
    {
        CAPTURE (f);
        CHECK_THAT (spectrum.levelDb (f, 4.5) - spectrum.levelDb (f, 0.0), WithinAbs (4.5 * std::log2 (f / 1000.0), 1.0e-9));
    }
}

TEST_CASE ("Higher resolution separates tones that a lower one merges")
{
    const auto dipBetweenTones = [] (AnalyzerResolution resolution) {
        AnalyzerSpectrum spectrum;
        spectrum.prepare (sampleRate, resolution);
        play (spectrum, { { 150.0, 0.5 }, { 180.0, 0.5 } }, 3.0, AnalyzerSpeed::fast);
        const double peaks = std::min (spectrum.levelDb (spectrum.peakNear (150.0), 0.0), spectrum.levelDb (spectrum.peakNear (180.0), 0.0));
        return peaks - spectrum.levelDb (165.0, 0.0);
    };
    CHECK (dipBetweenTones (AnalyzerResolution::maximum) > 20.0);
    CHECK (dipBetweenTones (AnalyzerResolution::low) < 3.0);
}

TEST_CASE ("A faster speed follows a new signal sooner")
{
    const auto levelAfterOneTenth = [] (AnalyzerSpeed speed) {
        AnalyzerSpectrum spectrum;
        spectrum.prepare (sampleRate, AnalyzerResolution::medium);
        play (spectrum, { { 1000.0, 1.0e-4 } }, 3.0, speed);
        play (spectrum, { { 1000.0, 1.0 } }, 0.1, speed);
        return spectrum.levelDb (1000.0, 0.0);
    };
    const AnalyzerSpeed speeds[] = { AnalyzerSpeed::verySlow, AnalyzerSpeed::slow, AnalyzerSpeed::medium, AnalyzerSpeed::fast,
                                     AnalyzerSpeed::veryFast };
    for (size_t i = 1; i < std::size (speeds); ++i)
    {
        CAPTURE (i);
        CHECK (levelAfterOneTenth (speeds[i]) > levelAfterOneTenth (speeds[i - 1]) + 1.0);
    }
    CHECK_THAT (levelAfterOneTenth (AnalyzerSpeed::veryFast), WithinAbs (0.0, 2.0));
}

TEST_CASE ("The peak near a Frequency is the top of the spectrum around it")
{
    AnalyzerSpectrum spectrum;
    spectrum.prepare (sampleRate, AnalyzerResolution::high);
    play (spectrum, { { 1234.0, 0.3 }, { 5000.0, 0.3 } }, 2.0, AnalyzerSpeed::fast);

    CHECK_THAT (spectrum.peakNear (1100.0), WithinRel (1234.0, 0.01));
    CHECK_THAT (spectrum.peakNear (5400.0), WithinRel (5000.0, 0.01));
}

TEST_CASE ("Resolution's FFT size follows the sample rate, so bins stay the same width in Hz")
{
    CHECK (AnalyzerSpectrum::fftSize (44100.0, AnalyzerResolution::low) == 1024);
    CHECK (AnalyzerSpectrum::fftSize (48000.0, AnalyzerResolution::maximum) == 8192);
    CHECK (AnalyzerSpectrum::fftSize (96000.0, AnalyzerResolution::medium) == 4096);
    CHECK (AnalyzerSpectrum::fftSize (192000.0, AnalyzerResolution::high) == 16384);
}

TEST_CASE ("With no peak within reach, the peak near a Frequency is that Frequency")
{
    AnalyzerSpectrum spectrum;
    spectrum.prepare (sampleRate, AnalyzerResolution::high);
    // Rising all the way across the sixth of an octave around 1 kHz: its top is an edge, not a peak.
    play (spectrum, { { 1500.0, 1.0 } }, 2.0, AnalyzerSpeed::fast);

    CHECK (spectrum.peakNear (1000.0) == 1000.0);
}
