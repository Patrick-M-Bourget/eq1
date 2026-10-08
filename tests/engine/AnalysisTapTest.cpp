#include "eq1/Engine.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <atomic>
#include <cmath>
#include <thread>
#include <vector>

using namespace eq1;
using Catch::Matchers::WithinAbs;

namespace
{
std::vector<float> readAll (Engine& engine, AnalysisTap tap)
{
    std::vector<float> samples (1 << 16);
    samples.resize (static_cast<size_t> (engine.readAnalysis (tap, samples.data(), static_cast<int> (samples.size()))));
    return samples;
}
} // namespace

TEST_CASE ("Analysis taps carry the mono mix of the input before and after the EQ, and of the Sidechain")
{
    constexpr int blockSize = 256;
    constexpr int numBlocks = 3;

    Engine engine;
    engine.prepare (48000.0, blockSize, 2);
    Settings settings;
    settings.bands[0] = { .inUse = true, .shape = Shape::Bell, .frequency = 1000.0, .gain = 12.0, .q = 1.0 };
    engine.setSettings (settings);

    std::vector<float> expectedPre, expectedPost, expectedSidechain;
    for (int block = 0; block < numBlocks; ++block)
    {
        std::vector<float> left (blockSize), right (blockSize), sidechainLeft (blockSize), sidechainRight (blockSize);
        for (int i = 0; i < blockSize; ++i)
        {
            const int n = block * blockSize + i;
            left[i] = std::sin (0.13f * n);
            right[i] = 0.5f * std::cos (0.07f * n);
            sidechainLeft[i] = 0.25f;
            sidechainRight[i] = -0.75f;
            expectedPre.push_back (0.5f * (left[i] + right[i]));
            expectedSidechain.push_back (-0.25f);
        }

        float* main[] = { left.data(), right.data() };
        const float* sidechainChannels[] = { sidechainLeft.data(), sidechainRight.data() };
        const ConstAudioBlock sidechain { sidechainChannels, 2, blockSize };
        engine.process ({ main, 2, blockSize }, &sidechain);

        for (int i = 0; i < blockSize; ++i)
            expectedPost.push_back (0.5f * (left[i] + right[i]));
    }

    const auto pre = readAll (engine, AnalysisTap::PreEq);
    const auto post = readAll (engine, AnalysisTap::PostEq);
    const auto sidechain = readAll (engine, AnalysisTap::Sidechain);

    REQUIRE (pre.size() == expectedPre.size());
    REQUIRE (post.size() == expectedPost.size());
    REQUIRE (sidechain.size() == expectedSidechain.size());
    for (size_t i = 0; i < pre.size(); ++i)
    {
        CHECK_THAT (pre[i], WithinAbs (expectedPre[i], 1.0e-6));
        CHECK_THAT (post[i], WithinAbs (expectedPost[i], 1.0e-6));
        CHECK_THAT (sidechain[i], WithinAbs (expectedSidechain[i], 1.0e-6));
    }

    SECTION ("a tap that has been read is empty until more audio is processed")
    {
        CHECK (readAll (engine, AnalysisTap::PreEq).empty());
    }
}

TEST_CASE ("Sidechain tap stays empty when no Sidechain is connected")
{
    Engine engine;
    engine.prepare (44100.0, 64, 1);
    std::vector<float> mono (64, 0.5f);
    float* main[] = { mono.data() };
    engine.process ({ main, 1, 64 });

    CHECK (readAll (engine, AnalysisTap::PreEq).size() == 64);
    CHECK (readAll (engine, AnalysisTap::Sidechain).empty());
}

TEST_CASE ("The Analyzer can keep reading the taps while the host prepares the Engine again")
{
    // Hosts call prepare when the sample rate or block size changes, while the editor goes on reading.
    Engine engine;
    engine.prepare (48000.0, 256, 2);
    std::atomic<bool> done { false };
    std::thread reader ([&] {
        std::vector<float> samples (4096);
        while (! done)
            for (auto tap : { AnalysisTap::PreEq, AnalysisTap::PostEq, AnalysisTap::Sidechain })
                engine.readAnalysis (tap, samples.data(), static_cast<int> (samples.size()));
    });

    std::vector<float> left (256, 0.25f), right (256, -0.25f);
    float* main[] = { left.data(), right.data() };
    for (int round = 0; round < 50; ++round)
    {
        engine.prepare (round % 2 == 0 ? 44100.0 : 96000.0, 256, 2);
        for (int block = 0; block < 4; ++block)
            engine.process ({ main, 2, 256 });
    }
    done = true;
    reader.join();
    SUCCEED();
}
