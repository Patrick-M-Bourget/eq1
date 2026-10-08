#include "Measure.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cmath>
#include <random>
#include <vector>

using namespace eq1;

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int length = 8192;

std::vector<float> noise (unsigned seed)
{
    std::mt19937 random (seed);
    std::uniform_real_distribution<float> unit (-0.5f, 0.5f);
    std::vector<float> samples (length);
    for (auto& s : samples)
        s = unit (random);
    return samples;
}

Settings bandWith (StereoPlacement placement)
{
    Settings settings;
    settings.bands[0] = test::bellBand (1000.0, 12.0, 1.0);
    settings.bands[0].placement = placement;
    return settings;
}

// Processes signal in place through an Engine with as many channels as signal has.
void process (const Settings& settings, std::vector<std::vector<float>>& signal)
{
    Engine engine;
    engine.prepare (sampleRate, length, static_cast<int> (signal.size()));
    engine.setSettings (settings);
    std::vector<float*> channels;
    for (auto& channel : signal)
        channels.push_back (channel.data());
    engine.process ({ channels.data(), static_cast<int> (channels.size()), length });
}

// What a Stereo Band does to one channel on its own: the reference for every Placement.
std::vector<float> filtered (std::vector<float> samples)
{
    std::vector<std::vector<float>> mono { std::move (samples) };
    process (bandWith (StereoPlacement::Stereo), mono);
    return mono[0];
}

void checkClose (const std::vector<float>& actual, const std::vector<float>& expected, float tolerance)
{
    REQUIRE (actual.size() == expected.size());
    for (size_t i = 0; i < actual.size(); ++i)
    {
        CAPTURE (i);
        REQUIRE (std::abs (actual[i] - expected[i]) <= tolerance);
    }
}

std::vector<float> combine (const std::vector<float>& a, const std::vector<float>& b, float sign)
{
    std::vector<float> result (a.size());
    for (size_t i = 0; i < a.size(); ++i)
        result[i] = 0.5f * (a[i] + sign * b[i]);
    return result;
}
std::vector<float> midOf (const std::vector<float>& left, const std::vector<float>& right) { return combine (left, right, 1.0f); }
std::vector<float> sideOf (const std::vector<float>& left, const std::vector<float>& right) { return combine (left, right, -1.0f); }
} // namespace

TEST_CASE ("A Stereo Band processes both channels")
{
    const auto left = noise (1), right = noise (2);
    std::vector<std::vector<float>> signal { left, right };
    process (bandWith (StereoPlacement::Stereo), signal);

    checkClose (signal[0], filtered (left), 1.0e-6f);
    checkClose (signal[1], filtered (right), 1.0e-6f);
}

TEST_CASE ("A Left or Right Band processes only its channel and leaves the other untouched")
{
    const bool leftBand = GENERATE (true, false);
    CAPTURE (leftBand);
    const auto left = noise (1), right = noise (2);
    std::vector<std::vector<float>> signal { left, right };
    process (bandWith (leftBand ? StereoPlacement::Left : StereoPlacement::Right), signal);

    const auto& processed = signal[leftBand ? 0 : 1];
    const auto& untouched = signal[leftBand ? 1 : 0];
    checkClose (processed, filtered (leftBand ? left : right), 1.0e-6f);
    REQUIRE (untouched == (leftBand ? right : left));
}

TEST_CASE ("A Mid or Side Band processes only its part of the signal and leaves the other unchanged")
{
    const bool midBand = GENERATE (true, false);
    CAPTURE (midBand);
    const auto left = noise (1), right = noise (2);
    std::vector<std::vector<float>> signal { left, right };
    process (bandWith (midBand ? StereoPlacement::Mid : StereoPlacement::Side), signal);

    const auto mid = midOf (signal[0], signal[1]), side = sideOf (signal[0], signal[1]);
    const auto originalMid = midOf (left, right), originalSide = sideOf (left, right);
    // The untouched part comes back to within float rounding of the encode and decode.
    checkClose (midBand ? mid : side, filtered (midBand ? originalMid : originalSide), 1.0e-6f);
    checkClose (midBand ? side : mid, midBand ? originalSide : originalMid, 1.0e-6f);
}

TEST_CASE ("On a mono track, a Side Band has no effect and every other Stereo Placement processes the signal")
{
    const auto placement = GENERATE (StereoPlacement::Stereo, StereoPlacement::Left, StereoPlacement::Right, StereoPlacement::Mid,
                                     StereoPlacement::Side);
    CAPTURE (static_cast<int> (placement));
    const auto input = noise (3);
    std::vector<std::vector<float>> signal { input };
    process (bandWith (placement), signal);

    if (placement == StereoPlacement::Side)
        REQUIRE (signal[0] == input);
    else
        checkClose (signal[0], filtered (input), 1.0e-6f);
}

TEST_CASE ("A stored Stereo Placement comes back when the Engine goes from stereo to mono and back")
{
    const auto left = noise (1), right = noise (2);
    std::vector<std::vector<float>> before { left, right };
    process (bandWith (StereoPlacement::Side), before);

    Engine engine;
    engine.setSettings (bandWith (StereoPlacement::Side));
    std::vector<float> mono = noise (3);
    float* monoChannels[] = { mono.data() };
    engine.prepare (sampleRate, length, 1);
    engine.process ({ monoChannels, 1, length });
    REQUIRE (mono == noise (3)); // the Side Band has nothing to process on mono

    std::vector<std::vector<float>> after { left, right };
    float* stereoChannels[] = { after[0].data(), after[1].data() };
    engine.prepare (sampleRate, length, 2);
    engine.process ({ stereoChannels, 2, length });

    checkClose (after[0], before[0], 1.0e-6f);
    checkClose (after[1], before[1], 1.0e-6f);
}
