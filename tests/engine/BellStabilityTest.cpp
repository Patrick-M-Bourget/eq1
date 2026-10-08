#include "Measure.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

using namespace eq1;

TEST_CASE ("Bell is stable across its whole range, including Frequency above Nyquist")
{
    const double sampleRate = GENERATE (44100.0, 48000.0, 96000.0, 192000.0);
    const double frequency = GENERATE (10.0, 1000.0, 15000.0, 20000.0, 21000.0, 22000.0, 23500.0, 26000.0, 30000.0);
    const double gain = GENERATE (-30.0, -6.0, -0.1, 0.0, 0.1, 6.0, 30.0);
    const double q = GENERATE (0.025, 0.3, 1.0, 5.0, 40.0);

    Engine engine;
    engine.prepare (sampleRate, 4096, 1);
    engine.setSettings (test::bell (frequency, gain, q));
    const auto response = test::impulseResponse (engine);

    CAPTURE (sampleRate, frequency, gain, q);
    REQUIRE (std::all_of (response.begin(), response.end(), [] (float s) { return std::isfinite (s); }));
    float tailPeak = 0.0f;
    for (size_t i = response.size() - 4096; i < response.size(); ++i)
        tailPeak = std::max (tailPeak, std::abs (response[i]));
    REQUIRE (tailPeak < 1.0e-4f); // decaying, though a Q 40 boost at 10 Hz rings for seconds
}

TEST_CASE ("Bell stays finite when its settings jump at random between blocks")
{
    const double sampleRate = GENERATE (44100.0, 96000.0);

    Engine engine;
    engine.prepare (sampleRate, 64, 2);

    std::mt19937 random (1234);
    std::uniform_real_distribution<double> unit (0.0, 1.0);
    std::uniform_real_distribution<float> noise (-1.0f, 1.0f);
    std::vector<float> left (64), right (64);
    float* main[] = { left.data(), right.data() };

    for (int block = 0; block < 20000; ++block)
    {
        const double frequency = 10.0 * std::pow (3000.0, unit (random));
        const double gain = -30.0 + 60.0 * unit (random);
        const double q = 0.025 * std::pow (1600.0, unit (random));
        engine.setSettings (test::bell (frequency, gain, q));

        for (int i = 0; i < 64; ++i)
        {
            left[i] = noise (random);
            right[i] = noise (random);
        }
        engine.process ({ main, 2, 64 });

        // Unsmoothed jumps may ring hard, but never blow up.
        const bool bounded = std::all_of (left.begin(), left.end(), [] (float s) { return std::isfinite (s) && std::abs (s) < 1.0e6f; });
        CAPTURE (block, frequency, gain, q);
        REQUIRE (bounded);
    }
}
