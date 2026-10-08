#include "Measure.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <atomic>
#include <thread>
#include <vector>

using namespace eq1;
using Catch::Matchers::WithinAbs;

TEST_CASE ("Settings set on another thread while audio is processing reach the Engine")
{
    constexpr double sampleRate = 48000.0;
    Engine engine;
    engine.prepare (sampleRate, 256, 2);

    std::atomic<bool> done { false };
    std::thread editor ([&] {
        // Every slot changes on every edit, so a torn snapshot would leave slots out of step.
        for (int edit = 0; edit < 2000; ++edit)
        {
            Settings settings;
            for (auto& band : settings.bands)
                band = { .inUse = true, .shape = Shape::Bell, .frequency = 100.0 + edit, .gain = edit % 2 == 0 ? 0.5 : -0.5, .q = 1.0 };
            engine.setSettings (settings);
        }
        Settings final;
        final.bands[11] = { .inUse = true, .shape = Shape::Bell, .frequency = 2000.0, .gain = -10.0, .q = 1.5 };
        engine.setSettings (final);
        done = true;
    });

    std::vector<float> left (256), right (256);
    float* channels[] = { left.data(), right.data() };
    while (! done)
        engine.process ({ channels, 2, 256 });
    editor.join();

    // Let the last edit settle (Bands glide and crossfade for about 50 ms), then measure.
    for (int block = 0; block < 64; ++block)
    {
        std::fill (left.begin(), left.end(), 0.0f);
        std::fill (right.begin(), right.end(), 0.0f);
        engine.process ({ channels, 2, 256 });
    }

    Engine mono;
    mono.prepare (sampleRate, 4096, 1);
    Settings final;
    final.bands[11] = { .inUse = true, .shape = Shape::Bell, .frequency = 2000.0, .gain = -10.0, .q = 1.5 };
    mono.setSettings (final);
    const auto expected = test::impulseResponse (mono);

    std::vector<float> impulse (8192, 0.0f);
    impulse[0] = 1.0f;
    std::vector<float> silent (8192, 0.0f);
    float* impulseChannels[] = { impulse.data(), silent.data() };
    engine.process ({ impulseChannels, 2, 8192 });

    for (double f : { 200.0, 2000.0, 8000.0 })
    {
        CAPTURE (f);
        CHECK_THAT (test::magnitudeDb (impulse, f, sampleRate), WithinAbs (test::magnitudeDb (expected, f, sampleRate), 0.01));
    }
}
