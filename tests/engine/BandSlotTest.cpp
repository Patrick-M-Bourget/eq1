#include "Measure.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <vector>

using namespace eq1;
using Catch::Matchers::WithinAbs;

namespace
{
constexpr double sampleRate = 48000.0;
using test::bellBand;

std::vector<float> responseOf (const Settings& settings)
{
    Engine engine;
    engine.prepare (sampleRate, 4096, 1);
    engine.setSettings (settings);
    return test::impulseResponse (engine);
}

// Processes silence until any crossfade or glide has settled.
void settle (Engine& engine)
{
    std::vector<float> silence (4096, 0.0f);
    float* channels[] = { silence.data() };
    for (int i = 0; i < 4; ++i)
        engine.process ({ channels, 1, 4096 });
}

bool isUnitImpulse (const std::vector<float>& response)
{
    for (size_t i = 0; i < response.size(); ++i)
        if (response[i] != (i == 0 ? 1.0f : 0.0f))
            return false;
    return true;
}
} // namespace

TEST_CASE ("Every one of the 24 Band slots shapes the sound")
{
    for (int slot = 0; slot < numBandSlots; ++slot)
    {
        const double frequency = 50.0 * std::pow (1.3, slot);
        Settings settings;
        settings.bands[static_cast<size_t> (slot)] = bellBand (frequency, 9.0, 2.0);

        CAPTURE (slot, frequency);
        CHECK_THAT (test::magnitudeDb (responseOf (settings), frequency, sampleRate), WithinAbs (9.0, 0.01));
    }
}

TEST_CASE ("Bands in different slots combine")
{
    Settings low, high, both;
    low.bands[3] = bellBand (200.0, 6.0, 1.0);
    high.bands[17] = bellBand (3000.0, -9.0, 0.7);
    both.bands[3] = low.bands[3];
    both.bands[17] = high.bands[17];

    const auto lowResponse = responseOf (low);
    const auto highResponse = responseOf (high);
    const auto bothResponse = responseOf (both);

    for (double f : test::frequenciesUpToNyquist (sampleRate, 32))
    {
        CAPTURE (f);
        CHECK_THAT (test::magnitudeDb (bothResponse, f, sampleRate),
                    WithinAbs (test::magnitudeDb (lowResponse, f, sampleRate) + test::magnitudeDb (highResponse, f, sampleRate), 0.001));
    }
}

TEST_CASE ("A slot not in use has no effect")
{
    Settings settings;
    settings.bands[5] = bellBand (1000.0, 12.0, 1.0);
    settings.bands[5].inUse = false;

    CHECK (isUnitImpulse (responseOf (settings)));
}

TEST_CASE ("Bypass removes a Band's effect without discarding its settings")
{
    Settings active;
    active.bands[0] = bellBand (1000.0, 12.0, 1.0);
    Settings bypassed = active;
    bypassed.bands[0].bypass = true;

    CHECK (isUnitImpulse (responseOf (bypassed)));

    Engine engine;
    engine.prepare (sampleRate, 4096, 1);
    engine.setSettings (bypassed);
    settle (engine);
    engine.setSettings (active);
    settle (engine);
    const auto restored = test::impulseResponse (engine);

    CHECK_THAT (test::magnitudeDb (restored, 1000.0, sampleRate), WithinAbs (12.0, 0.01));
    CHECK_THAT (test::magnitudeDb (restored, 100.0, sampleRate),
                WithinAbs (test::magnitudeDb (responseOf (active), 100.0, sampleRate), 0.001));
}
