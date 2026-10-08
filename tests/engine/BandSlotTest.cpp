#include "Measure.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
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

TEST_CASE ("Changing Shape keeps the Band's Frequency, Gain and Q")
{
    Settings bellSettings;
    bellSettings.bands[2] = bellBand (700.0, -8.0, 3.0);
    Settings shelfSettings = bellSettings;
    shelfSettings.bands[2].shape = Shape::HighShelf;

    Engine engine;
    engine.prepare (sampleRate, 4096, 1);
    engine.setSettings (bellSettings);
    settle (engine);
    engine.setSettings (shelfSettings);
    settle (engine);
    const auto asShelf = test::impulseResponse (engine);
    engine.setSettings (bellSettings);
    settle (engine);
    const auto backToBell = test::impulseResponse (engine);

    const auto shelf = responseOf (shelfSettings);
    const auto bell = responseOf (bellSettings);
    for (double f : test::frequenciesUpToNyquist (sampleRate, 32))
    {
        CAPTURE (f);
        CHECK_THAT (test::magnitudeDb (asShelf, f, sampleRate), WithinAbs (test::magnitudeDb (shelf, f, sampleRate), 0.001));
        CHECK_THAT (test::magnitudeDb (backToBell, f, sampleRate), WithinAbs (test::magnitudeDb (bell, f, sampleRate), 0.001));
    }
}

namespace
{
// Plays settingsInTurn one after another, settling after each, and returns the final response.
std::vector<float> responseAfter (std::initializer_list<Settings> settingsInTurn)
{
    Engine engine;
    engine.prepare (sampleRate, 4096, 1);
    for (const auto& settings : settingsInTurn)
    {
        engine.setSettings (settings);
        settle (engine);
    }
    return test::impulseResponse (engine);
}

Settings withBand (Shape shape, double gain, double slope)
{
    Settings settings;
    settings.bands[0] = { .inUse = true, .shape = shape, .frequency = 1000.0, .gain = gain, .q = 1.0, .slope = slope };
    return settings;
}

void checkSameResponse (const std::vector<float>& actual, const std::vector<float>& expected)
{
    for (double f : test::frequenciesUpToNyquist (sampleRate, 32))
    {
        CAPTURE (f);
        CHECK_THAT (test::magnitudeDb (actual, f, sampleRate), WithinAbs (test::magnitudeDb (expected, f, sampleRate), 0.001));
    }
}
} // namespace

TEST_CASE ("A Slope below the Shape's minimum is raised to it")
{
    // Bell's minimum is 12 dB/oct, a shelf's 6 (ADR 0003). Bell ignores Slope until Bell Slope (#19).
    checkSameResponse (responseOf (withBand (Shape::Bell, 6.0, 0.0)), responseOf (withBand (Shape::Bell, 6.0, 12.0)));
    checkSameResponse (responseOf (withBand (Shape::LowShelf, 6.0, 0.0)), responseOf (withBand (Shape::LowShelf, 6.0, 6.0)));
}

TEST_CASE ("A Slope between whole orders is rounded to the nearest one")
{
    checkSameResponse (responseOf (withBand (Shape::HighShelf, 9.0, 8.9)), responseOf (withBand (Shape::HighShelf, 9.0, 6.0)));
    checkSameResponse (responseOf (withBand (Shape::HighShelf, 9.0, 9.1)), responseOf (withBand (Shape::HighShelf, 9.0, 12.0)));
}

TEST_CASE ("Switching Shape and back restores a Slope below the other Shape's minimum")
{
    // The stored Slope of 6 stays 6 while the Bell raises it to 12.
    checkSameResponse (responseAfter ({ withBand (Shape::LowShelf, 6.0, 6.0), withBand (Shape::Bell, 6.0, 6.0), withBand (Shape::LowShelf, 6.0, 6.0) }),
                       responseOf (withBand (Shape::LowShelf, 6.0, 6.0)));
}

TEST_CASE ("Switching to a Shape without Gain and back restores the Gain")
{
    const Shape withoutGain = GENERATE (Shape::LowCut, Shape::HighCut, Shape::Notch, Shape::BandPass, Shape::AllPass);
    CAPTURE (static_cast<int> (withoutGain));

    const auto restored = responseAfter ({ withBand (Shape::Bell, 6.0, 12.0), withBand (withoutGain, 6.0, 12.0), withBand (Shape::Bell, 6.0, 12.0) });
    CHECK_THAT (test::magnitudeDb (restored, 1000.0, sampleRate), WithinAbs (6.0, 0.01));
    checkSameResponse (restored, responseOf (withBand (Shape::Bell, 6.0, 12.0)));
}

TEST_CASE ("Cuts, Notch, Band Pass and All Pass have no Gain; Bell and Flat Tilt have no Slope; Brickwall is for Cuts")
{
    for (auto shape : { Shape::Bell, Shape::LowShelf, Shape::HighShelf, Shape::TiltShelf, Shape::FlatTilt })
        CHECK (hasGain (shape));
    for (auto shape : { Shape::LowCut, Shape::HighCut, Shape::Notch, Shape::BandPass, Shape::AllPass })
        CHECK_FALSE (hasGain (shape));
    for (auto shape : { Shape::Bell, Shape::FlatTilt })
        CHECK_FALSE (hasSlope (shape));
    CHECK (isCut (Shape::LowCut));
    CHECK (isCut (Shape::HighCut));
    CHECK_FALSE (isCut (Shape::BandPass));
}
