#include "eq1/Engine.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

using namespace eq1;

namespace
{

// Every Shape across the spectrum, steep and resonant, half of them Dynamic Bands: the filters that
// ring longest, and so take longest to decay into subnormal numbers.
Settings everyShapeRinging()
{
    constexpr Shape shapes[] = { Shape::Bell,  Shape::LowShelf, Shape::LowCut,    Shape::HighShelf, Shape::HighCut,
                                 Shape::Notch, Shape::BandPass, Shape::TiltShelf, Shape::FlatTilt,  Shape::AllPass };
    Settings settings;
    for (size_t slot = 0; slot < settings.bands.size(); ++slot)
        settings.bands[slot] = { .inUse = true,
                                 .shape = shapes[slot % 10],
                                 .frequency = 20.0 * std::pow (1.33, static_cast<double> (slot)),
                                 .gain = slot % 2 == 0 ? 12.0 : -12.0,
                                 .q = 20.0,
                                 .slope = 96.0,
                                 .dynamicRange = slot % 2 == 0 ? -12.0 : 0.0 };
    return settings;
}

int subnormalsIn (const std::vector<float>& samples)
{
    int count = 0;
    for (float s : samples)
        count += std::fpclassify (s) == FP_SUBNORMAL ? 1 : 0;
    return count;
}

// The sample rates eq1 is checked at.
double anySampleRate() { return GENERATE (44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0); }

// All 24 Band slots with one Shape, at every combination of the ends of Frequency, Q, Gain and
// Slope (2 x 2 x 2 x 3 = 24), as Dynamic Bands moving the full +/-30 dB at the fastest and slowest
// timing, with their Free Detection Range turned inside out; the output controls at their ends too.
Settings extremes (Shape shape)
{
    Settings settings;
    for (size_t slot = 0; slot < settings.bands.size(); ++slot)
    {
        const bool low = slot % 2 == 0, wide = slot / 2 % 2 == 0, raised = slot / 4 % 2 == 0;
        const size_t slope = slot / 8; // 0: the Shape's minimum, 1: 96 dB/oct, 2: Brickwall
        settings.bands[slot] = { .inUse = true,
                                 .shape = shape,
                                 .frequency = low ? 10.0 : 30000.0,
                                 .gain = raised ? 30.0 : -30.0,
                                 .q = wide ? 0.025 : 40.0,
                                 .slope = slope == 0 ? 0.0 : 96.0,
                                 .brickwall = slope == 2,
                                 .detectionRange = DetectionRange::Free,
                                 .detectionLow = 30000.0,
                                 .detectionHigh = 10.0,
                                 .dynamicRange = raised ? -30.0 : 30.0,
                                 .threshold = -60.0,
                                 .thresholdAuto = slot % 3 == 0,
                                 .attack = low ? 0.0 : 100.0,
                                 .release = wide ? 0.0 : 100.0 };
    }
    settings.gainScale = 2.0;
    settings.autoGain = true;
    settings.outputGainDb = 36.0;
    return settings;
}

} // namespace

TEST_CASE ("Silence after a signal decays to true silence, never through subnormal numbers")
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    Engine engine;
    engine.prepare (sampleRate, blockSize, 2);
    engine.setSettings (everyShapeRinging());

    std::vector<float> left (blockSize), right (blockSize);
    float* main[] = { left.data(), right.data() };
    int subnormals = 0;
    // An impulse, then 20 s of silence: long enough for the ringing to fall below the smallest normal float.
    for (int block = 0; block < static_cast<int> (20.0 * sampleRate / blockSize); ++block)
    {
        std::fill (left.begin(), left.end(), 0.0f);
        std::fill (right.begin(), right.end(), 0.0f);
        if (block == 0)
            left[0] = right[0] = 1.0f;
        engine.process ({ main, 2, blockSize });
        subnormals += subnormalsIn (left) + subnormalsIn (right);
    }
    REQUIRE (subnormals == 0);
}

TEST_CASE ("Processing leaves the caller's floating-point mode as it found it")
{
    Engine engine;
    engine.prepare (48000.0, 64, 1);
    engine.setSettings (everyShapeRinging());
    std::vector<float> block (64, 0.5f);
    float* main[] = { block.data() };
    engine.process ({ main, 1, 64 });

    // Read through volatile, so the compiler can't work it out ahead of time.
    volatile float smallestNormal = 1.17549435e-38f;
    REQUIRE (std::fpclassify (smallestNormal / 4.0f) == FP_SUBNORMAL);
}

TEST_CASE ("Every Shape at the ends of every setting stays finite and bounded, through the input and the silence after it")
{
    const double sampleRate = anySampleRate();
    const auto shape = GENERATE (Shape::Bell, Shape::LowShelf, Shape::LowCut, Shape::HighShelf, Shape::HighCut,
                                 Shape::Notch, Shape::BandPass, Shape::TiltShelf, Shape::FlatTilt, Shape::AllPass);
    constexpr int blockSize = 256;
    Engine engine;
    engine.prepare (sampleRate, blockSize, 2);
    engine.setSettings (extremes (shape));

    // Full-scale noise for 0.25 s, then silence. The tail is not required to fall yet: Bands resonating
    // together at 10 Hz stack into a ringing that builds for seconds before it decays. An
    // unstable filter grows without limit instead.
    std::mt19937 random (7);
    std::uniform_real_distribution<float> noise (-1.0f, 1.0f);
    const auto inputSamples = static_cast<int> (0.25 * sampleRate);
    std::vector<float> left (blockSize), right (blockSize);
    float* main[] = { left.data(), right.data() };
    float peak = 0.0f;
    bool finite = true;
    for (int n = 0; n < static_cast<int> (1.5 * sampleRate); n += blockSize)
    {
        for (size_t i = 0; i < left.size(); ++i)
        {
            const bool playing = n + static_cast<int> (i) < inputSamples;
            left[i] = playing ? noise (random) : 0.0f;
            right[i] = playing ? noise (random) : 0.0f;
        }
        engine.process ({ main, 2, blockSize });
        for (size_t i = 0; i < left.size(); ++i)
        {
            finite = finite && std::isfinite (left[i]) && std::isfinite (right[i]);
            peak = std::max ({ peak, std::abs (left[i]), std::abs (right[i]) });
        }
    }

    CAPTURE (sampleRate, static_cast<int> (shape));
    REQUIRE (finite);
    REQUIRE (peak < 1.0e6f);
}
