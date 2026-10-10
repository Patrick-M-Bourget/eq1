#include "eq1/Engine.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
#include <vector>

using namespace eq1;
using Catch::Matchers::WithinAbs;

namespace
{
// Plays signals through the Engine, the same on every channel, in blocks whose sizes cycle through
// 1, 37, 512 and 160 samples.
struct Player
{
    Engine& engine;
    double sampleRate;
    int numChannels;
    long long position = 0;

    void sine (double seconds, double amplitude, double frequency = 997.0)
    {
        play (seconds, [&] (long long n) {
            return static_cast<float> (amplitude * std::sin (2.0 * std::numbers::pi * frequency * static_cast<double> (n) / sampleRate));
        });
    }

    void silence (double seconds) { sine (seconds, 0.0); }

    // White noise, uniform between -amplitude and amplitude.
    void noise (double seconds, double amplitude, unsigned seed)
    {
        std::mt19937 random (seed);
        std::uniform_real_distribution<float> uniform (static_cast<float> (-amplitude), static_cast<float> (amplitude));
        play (seconds, [&] (long long) { return uniform (random); });
    }

private:
    // Plays sampleAt(position) onwards for the given time.
    template <typename SampleAt>
    void play (double seconds, SampleAt sampleAt)
    {
        constexpr int blockSizes[] = { 1, 37, 512, 160 };
        std::vector<std::vector<float>> blocks (static_cast<size_t> (numChannels), std::vector<float> (512));
        std::vector<float*> channels;
        for (auto& block : blocks)
            channels.push_back (block.data());
        const auto total = std::llround (seconds * sampleRate);
        for (long long done = 0, cut = 0; done < total; ++cut)
        {
            const int count = static_cast<int> (std::min<long long> (blockSizes[cut % 4], total - done));
            for (int i = 0; i < count; ++i)
            {
                const float s = sampleAt (position + i);
                for (auto& block : blocks)
                    block[static_cast<size_t> (i)] = s;
            }
            engine.process ({ channels.data(), numChannels, count });
            position += count;
            done += count;
        }
    }
};
} // namespace

TEST_CASE ("Output Level of a full-scale sine is 0 dBFS peak and -3.01 dB RMS on each channel")
{
    const double sampleRate = GENERATE (44100.0, 48000.0, 96000.0);
    CAPTURE (sampleRate);

    Engine engine;
    engine.prepare (sampleRate, 512, 2);
    engine.setSettings ({});
    Player { engine, sampleRate, 2 }.sine (0.5, 1.0);

    REQUIRE (engine.outputLevelChannels() == 2);
    for (int ch = 0; ch < 2; ++ch)
    {
        CAPTURE (ch);
        const auto level = engine.readOutputLevel (ch);
        CHECK_THAT (level.peakDb, WithinAbs (0.0, 0.01));
        CHECK_THAT (level.rmsDb, WithinAbs (-3.01, 0.1));
    }
}

TEST_CASE ("Output Level is measured after Output Gain and Output Pan, and reads the input during Global Bypass")
{
    constexpr double sampleRate = 48000.0;
    Engine engine;
    engine.prepare (sampleRate, 512, 2);
    Settings settings;
    settings.outputGainDb = -12.0;
    settings.outputPan = -1.0; // hard left: the right channel is silent
    settings.panMode = PanMode::LeftRight;

    SECTION ("processing")
    {
        engine.setSettings (settings);
        Player { engine, sampleRate, 2 }.sine (0.5, 0.5);
        CHECK_THAT (engine.readOutputLevel (0).peakDb, WithinAbs (-6.02 - 12.0, 0.01));
        CHECK (engine.readOutputLevel (1).peakDb == outputLevelFloorDb);
    }

    SECTION ("Global Bypass")
    {
        settings.globalBypass = true;
        engine.setSettings (settings);
        Player { engine, sampleRate, 2 }.sine (0.5, 0.5);
        for (int ch = 0; ch < 2; ++ch)
            CHECK_THAT (engine.readOutputLevel (ch).peakDb, WithinAbs (-6.02, 0.01));
    }
}

TEST_CASE ("A single-sample over is reported by the next read, after quieter blocks, and only by that read")
{
    constexpr double sampleRate = 48000.0;
    Engine engine;
    engine.prepare (sampleRate, 512, 1);
    engine.setSettings ({});

    std::vector<float> block (512, 0.0f);
    block[100] = 1.5f; // +3.52 dBFS
    float* channels[] = { block.data() };
    engine.process ({ channels, 1, 512 });
    Player player { engine, sampleRate, 1 };
    player.sine (0.1, 0.25);

    CHECK_THAT (engine.readOutputLevel (0).peakDb, WithinAbs (3.52, 0.01));
    player.sine (0.1, 0.25);
    CHECK_THAT (engine.readOutputLevel (0).peakDb, WithinAbs (-12.04, 0.01));
}

TEST_CASE ("Output Level RMS covers only the last 300 ms, and silence reads exactly the floor")
{
    const double sampleRate = GENERATE (44100.0, 96000.0);
    CAPTURE (sampleRate);
    Engine engine;
    engine.prepare (sampleRate, 512, 1);
    engine.setSettings ({});
    REQUIRE (engine.outputLevelChannels() == 1);

    SECTION ("before anything is processed")
    {
        const auto level = engine.readOutputLevel (0);
        CHECK (level.peakDb == outputLevelFloorDb);
        CHECK (level.rmsDb == outputLevelFloorDb);
    }

    Player player { engine, sampleRate, 1 };
    player.sine (1.0, 0.9);

    SECTION ("half the window silent: half the power")
    {
        player.silence (0.15);
        CHECK_THAT (engine.readOutputLevel (0).rmsDb, WithinAbs (20.0 * std::log10 (0.9) - 3.01 - 3.01, 0.1));
    }

    SECTION ("a whole window of silence, even after noise far above full scale")
    {
        // Rounding in a running sum of squares would leave more than the floor after some of these.
        for (unsigned seed = 0; seed < 8; ++seed)
        {
            CAPTURE (seed);
            player.noise (0.1 + 0.07 * seed, 1000.0, seed);
            player.silence (0.3);
            engine.readOutputLevel (0); // the loud passage's peak
            const auto level = engine.readOutputLevel (0);
            CHECK (level.rmsDb == outputLevelFloorDb);
            CHECK (level.peakDb == outputLevelFloorDb);
        }
    }

    SECTION ("a read doesn't reset RMS")
    {
        const double first = engine.readOutputLevel (0).rmsDb;
        CHECK (engine.readOutputLevel (0).rmsDb == first);
    }
}
