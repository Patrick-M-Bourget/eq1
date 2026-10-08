#include "Measure.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <random>
#include <vector>

using namespace eq1;
using Catch::Matchers::WithinAbs;

namespace
{
constexpr double sampleRate = 48000.0;

// One Band in slot 3, Soloed.
Settings soloed (BandSettings band)
{
    Settings settings;
    band.inUse = true;
    settings.bands[2] = band;
    settings.soloSlot = 3;
    return settings;
}

std::vector<float> responseOf (const Settings& settings)
{
    Engine engine;
    engine.prepare (sampleRate, 4096, 1);
    engine.setSettings (settings);
    return test::impulseResponse (engine);
}

double dbAt (const std::vector<float>& response, double frequency) { return test::magnitudeDb (response, frequency, sampleRate); }

std::vector<float> noise (unsigned seed, size_t length)
{
    std::mt19937 random (seed);
    std::uniform_real_distribution<float> unit (-0.5f, 0.5f);
    std::vector<float> samples (length);
    for (auto& s : samples)
        s = unit (random);
    return samples;
}

// Processes signal in place through an Engine with as many channels as signal has.
void process (const Settings& settings, std::vector<std::vector<float>>& signal)
{
    Engine engine;
    engine.prepare (sampleRate, static_cast<int> (signal[0].size()), static_cast<int> (signal.size()));
    engine.setSettings (settings);
    std::vector<float*> channels;
    for (auto& channel : signal)
        channels.push_back (channel.data());
    engine.process ({ channels.data(), static_cast<int> (channels.size()), static_cast<int> (signal[0].size()) });
}
} // namespace

TEST_CASE ("Soloing a Bell, Notch, Band Pass or All Pass plays the input around its Frequency, as wide as Q")
{
    const auto shape = GENERATE (Shape::Bell, Shape::Notch, Shape::BandPass, Shape::AllPass);
    CAPTURE (static_cast<int> (shape));
    // Solo plays the region, not the Band's effect: its Gain of +9 dB doesn't show.
    const auto response = responseOf (soloed ({ .shape = shape, .frequency = 1000.0, .gain = 9.0, .q = 2.0, .slope = 24.0 }));

    CHECK_THAT (dbAt (response, 1000.0), WithinAbs (0.0, 0.1));
    CHECK (dbAt (response, 125.0) < -20.0);
    CHECK (dbAt (response, 8000.0) < -20.0);
    // As wide as Q: -3 dB 1/Q of Frequency apart.
    const double half = 1.0 / (2.0 * 2.0);
    const double lower = 1000.0 * (std::sqrt (1.0 + half * half) - half), upper = 1000.0 * (std::sqrt (1.0 + half * half) + half);
    CHECK_THAT (dbAt (response, lower), WithinAbs (-3.0, 0.2));
    CHECK_THAT (dbAt (response, upper), WithinAbs (-3.0, 0.2));
}

TEST_CASE ("Soloing a Low Shelf plays the input below its Frequency, and a High Shelf above it")
{
    const auto low = responseOf (soloed ({ .shape = Shape::LowShelf, .frequency = 1000.0, .gain = -6.0, .q = 1.0, .slope = 6.0 }));
    CHECK_THAT (dbAt (low, 50.0), WithinAbs (0.0, 0.1));
    CHECK (dbAt (low, 10000.0) < -35.0);

    const auto high = responseOf (soloed ({ .shape = Shape::HighShelf, .frequency = 1000.0, .gain = 6.0, .q = 1.0, .slope = 6.0 }));
    CHECK_THAT (dbAt (high, 15000.0), WithinAbs (0.0, 0.1));
    CHECK (dbAt (high, 100.0) < -35.0);
}

TEST_CASE ("Soloing a Tilt Shelf or Flat Tilt plays the whole input")
{
    const auto shape = GENERATE (Shape::TiltShelf, Shape::FlatTilt);
    const auto response = responseOf (soloed ({ .shape = shape, .frequency = 1000.0, .gain = 12.0, .q = 1.0 }));
    for (double f : test::frequenciesUpToNyquist (sampleRate, 16))
    {
        CAPTURE (static_cast<int> (shape), f);
        CHECK_THAT (dbAt (response, f), WithinAbs (0.0, 1.0e-4));
    }
}

TEST_CASE ("Soloing a Cut plays the content it removes")
{
    const bool brickwall = GENERATE (false, true);
    CAPTURE (brickwall);
    const auto lowCut = responseOf (soloed ({ .shape = Shape::LowCut, .frequency = 1000.0, .q = 0.71, .slope = 24.0, .brickwall = brickwall }));
    CHECK_THAT (dbAt (lowCut, 100.0), WithinAbs (0.0, 0.1));
    CHECK (dbAt (lowCut, 10000.0) < -60.0);

    const auto highCut = responseOf (soloed ({ .shape = Shape::HighCut, .frequency = 1000.0, .q = 0.71, .slope = 24.0, .brickwall = brickwall }));
    CHECK_THAT (dbAt (highCut, 10000.0), WithinAbs (0.0, 0.1));
    CHECK (dbAt (highCut, 100.0) < -60.0);

    // A Cut at a Slope of 0 removes nothing; its Solo still plays the side it would remove.
    const auto flat = responseOf (soloed ({ .shape = Shape::LowCut, .frequency = 1000.0, .q = 0.71, .slope = 0.0 }));
    CHECK (dbAt (flat, 20000.0) < -20.0);
}

TEST_CASE ("Soloing a slot not in use leaves the output alone")
{
    auto settings = soloed (test::bellBand (1000.0, 12.0, 1.0));
    settings.bands[2].inUse = false;
    const auto response = responseOf (settings);
    for (size_t i = 0; i < response.size(); ++i)
        REQUIRE (response[i] == (i == 0 ? 1.0f : 0.0f));
}

TEST_CASE ("A Soloed Band plays the part of the signal it processes, and the rest is silent")
{
    const auto placement = GENERATE (StereoPlacement::Left, StereoPlacement::Right, StereoPlacement::Mid, StereoPlacement::Side);
    CAPTURE (static_cast<int> (placement));
    constexpr size_t length = 8192;
    const auto left = noise (1, length), right = noise (2, length);

    auto band = test::bellBand (1000.0, 6.0, 0.7);
    band.placement = placement;
    std::vector<std::vector<float>> stereo { left, right };
    process (soloed (band), stereo);

    // The region of one signal: a Stereo Band's Solo on mono.
    const auto region = [&] (std::vector<float> samples) {
        std::vector<std::vector<float>> mono { std::move (samples) };
        auto stereoBand = band;
        stereoBand.placement = StereoPlacement::Stereo;
        process (soloed (stereoBand), mono);
        return mono[0];
    };
    std::vector<float> part (length);
    for (size_t i = 0; i < length; ++i)
        part[i] = placement == StereoPlacement::Left    ? left[i]
                  : placement == StereoPlacement::Right ? right[i]
                  : placement == StereoPlacement::Mid   ? 0.5f * (left[i] + right[i])
                                                        : 0.5f * (left[i] - right[i]);
    const auto expected = region (part);

    for (size_t i = 0; i < length; ++i)
    {
        CAPTURE (i);
        switch (placement)
        {
            case StereoPlacement::Left:
                REQUIRE_THAT (stereo[0][i], WithinAbs (expected[i], 1.0e-6));
                REQUIRE (stereo[1][i] == 0.0f);
                break;
            case StereoPlacement::Right:
                REQUIRE (stereo[0][i] == 0.0f);
                REQUIRE_THAT (stereo[1][i], WithinAbs (expected[i], 1.0e-6));
                break;
            case StereoPlacement::Mid:
                REQUIRE_THAT (stereo[0][i], WithinAbs (expected[i], 1.0e-6));
                REQUIRE (stereo[1][i] == stereo[0][i]);
                break;
            default:
                REQUIRE_THAT (stereo[0][i], WithinAbs (expected[i], 1.0e-6));
                REQUIRE (stereo[1][i] == -stereo[0][i]);
                break;
        }
    }
}

TEST_CASE ("On a mono track a Side Band's Solo is silent")
{
    auto band = test::bellBand (1000.0, 6.0, 1.0);
    band.placement = StereoPlacement::Side;
    std::vector<std::vector<float>> mono { noise (3, 4096) };
    process (soloed (band), mono);
    for (float s : mono[0])
        REQUIRE (s == 0.0f);
}
