#include "Measure.h"

#include "eq1/Response.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cmath>

using namespace eq1;

namespace
{
std::vector<float> measured (double sampleRate, const Settings& settings)
{
    Engine engine;
    engine.prepare (sampleRate, 4096, 1);
    engine.setSettings (settings);
    return test::impulseResponse (engine);
}
} // namespace

TEST_CASE ("The display's response is the Engine's: every Shape, Slope and Q matches what the Engine plays", "[sweep]")
{
    const double sampleRate = GENERATE (44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0);
    const auto shape = GENERATE (Shape::Bell, Shape::LowShelf, Shape::LowCut, Shape::HighShelf, Shape::HighCut, Shape::Notch,
                                 Shape::BandPass, Shape::TiltShelf, Shape::FlatTilt, Shape::AllPass);
    const double slope = GENERATE (0.0, 12.0, 48.0);
    const double q = GENERATE (0.3, 4.0);
    const bool brickwall = GENERATE (false, true);
    const double frequency = GENERATE (100.0, 15000.0);

    Settings settings;
    settings.bands[0] = { .inUse = true, .shape = shape, .frequency = frequency, .gain = -7.5, .q = q, .slope = slope, .brickwall = brickwall };
    const auto response = measured (sampleRate, settings);

    for (double f : test::frequenciesUpToNyquist (sampleRate, 48))
    {
        const double played = test::magnitudeDb (response, f, sampleRate);
        CAPTURE (sampleRate, static_cast<int> (shape), slope, q, brickwall, frequency, f, played);
        // Below -80 dB the Engine's float output can't be measured to 0.01 dB.
        if (played < -80.0)
            continue;
        REQUIRE (std::abs (bandResponseDb (settings.bands[0], f, sampleRate) - played) < 0.01);
        REQUIRE (std::abs (responseDb (settings, f, sampleRate) - played) < 0.01);
    }
}

TEST_CASE ("The display's response adds up the Bands in use and leaves out Bypassed ones")
{
    Settings settings;
    settings.bands[2] = test::bellBand (200.0, 6.0, 1.0);
    settings.bands[9] = test::bellBand (3000.0, -4.0, 2.0);
    settings.bands[15] = test::bellBand (1000.0, 12.0, 1.0);
    settings.bands[15].bypass = true;
    settings.bands[20] = test::bellBand (1000.0, 12.0, 1.0);
    settings.bands[20].inUse = false;

    const auto response = measured (48000.0, settings);
    for (double f : test::frequenciesUpToNyquist (48000.0, 32))
    {
        CAPTURE (f);
        REQUIRE (std::abs (responseDb (settings, f, 48000.0) - test::magnitudeDb (response, f, 48000.0)) < 0.01);
    }
    CHECK (std::abs (responseDb (settings, 200.0, 48000.0)
                     - bandResponseDb (settings.bands[2], 200.0, 48000.0) - bandResponseDb (settings.bands[9], 200.0, 48000.0))
           < 1.0e-9);
}

TEST_CASE ("The display's response at many frequencies at once is the same as one at a time")
{
    const auto band = BandSettings { .inUse = true, .shape = Shape::BandPass, .frequency = 2000.0, .q = 3.0, .slope = 48.0 };
    const auto frequencies = test::frequenciesUpToNyquist (48000.0, 40);
    std::vector<double> db (frequencies.size());
    bandResponseDb (band, frequencies.data(), db.data(), static_cast<int> (frequencies.size()), 48000.0);
    for (size_t i = 0; i < frequencies.size(); ++i)
        CHECK (db[i] == bandResponseDb (band, frequencies[i], 48000.0));
}
