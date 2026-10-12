#include "eq1/Engine.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
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

TEST_CASE ("Analysis taps hold the smallest power of two covering 150 ms at the prepared sample rate")
{
    struct Expected
    {
        double sampleRate;
        int capacity;
    };
    const auto expected = GENERATE (Expected { 44100.0, 8192 }, Expected { 48000.0, 8192 }, Expected { 96000.0, 16384 }, Expected { 192000.0, 32768 });
    CAPTURE (expected.sampleRate);

    Engine engine;
    engine.prepare (expected.sampleRate, 512, 2);
    CHECK (engine.analysisCapacity() == expected.capacity);
}

TEST_CASE ("Analysis taps keep samples in order while the ring wraps around")
{
    Engine engine;
    engine.prepare (48000.0, 512, 1);
    const int capacity = engine.analysisCapacity();
    // Blocks that don't divide the capacity, read in chunks that don't either, for over three turns.
    constexpr int blockSize = 1000, readSize = 777;
    std::vector<float> block (blockSize), read (readSize);
    float* main[] = { block.data() };
    long long pushed = 0, popped = 0;
    while (pushed < 3LL * capacity + blockSize)
    {
        for (int i = 0; i < blockSize; ++i)
            block[static_cast<size_t> (i)] = static_cast<float> (pushed + i); // exact in float up to 2^24
        engine.process ({ main, 1, blockSize });
        pushed += blockSize;
        for (int count = 1; count > 0;)
        {
            count = engine.readAnalysis (AnalysisTap::PreEq, read.data(), readSize);
            for (int i = 0; i < count; ++i)
                REQUIRE (read[static_cast<size_t> (i)] == static_cast<float> (popped + i));
            popped += count;
        }
    }
    CHECK (popped == pushed);
}

TEST_CASE ("A reader that skips a tap first reads only what the Engine processed after the skip")
{
    // Nothing reads while the editor is closed, so the taps fill up and then drop new samples.
    Engine engine;
    engine.prepare (48000.0, 1000, 1);
    std::vector<float> block (1000);
    float* main[] = { block.data() };
    const auto play = [&] (float from) {
        for (size_t i = 0; i < block.size(); ++i)
            block[i] = from + static_cast<float> (i);
        engine.process ({ main, 1, static_cast<int> (block.size()) });
    };
    for (int n = 0; n < 2 * engine.analysisCapacity() / 1000; ++n)
        play (static_cast<float> (n * 1000));

    for (auto tap : { AnalysisTap::PreEq, AnalysisTap::PostEq })
        engine.skipAnalysis (tap);
    play (1.0e6f);

    for (auto tap : { AnalysisTap::PreEq, AnalysisTap::PostEq })
    {
        const auto read = readAll (engine, tap);
        REQUIRE (read.size() == block.size());
        for (size_t i = 0; i < read.size(); ++i)
            REQUIRE (read[i] == 1.0e6f + static_cast<float> (i));
    }
    // A tap with nothing in it skips nothing.
    engine.skipAnalysis (AnalysisTap::Sidechain);
    CHECK (readAll (engine, AnalysisTap::Sidechain).empty());
}

TEST_CASE ("A reader can skip a tap while the audio thread pushes to it")
{
    // Each sample is its index, so what the reader pops after a skip must have been pushed after it
    // started, and in order. Under ThreadSanitizer, this also checks the skip races with nothing.
    Engine engine;
    constexpr int blockSize = 64;
    engine.prepare (48000.0, blockSize, 1);
    std::atomic<long long> pushed { 0 };
    std::atomic<bool> done { false };
    std::thread audio ([&] {
        std::vector<float> block (blockSize);
        float* main[] = { block.data() };
        for (long long n = 0; n < (1LL << 20); n += blockSize)
        {
            for (int i = 0; i < blockSize; ++i)
                block[static_cast<size_t> (i)] = static_cast<float> (n + i); // exact in float up to 2^24
            engine.process ({ main, 1, blockSize });
            pushed.store (n + blockSize, std::memory_order_release);
        }
        done = true;
    });

    std::vector<float> read (static_cast<size_t> (engine.analysisCapacity()));
    int skips = 0;
    while (! done)
    {
        const auto before = pushed.load (std::memory_order_acquire);
        engine.skipAnalysis (AnalysisTap::PreEq);
        ++skips;
        float last = -1.0f;
        for (int round = 0; round < 4; ++round)
        {
            const int count = engine.readAnalysis (AnalysisTap::PreEq, read.data(), static_cast<int> (read.size()));
            for (int i = 0; i < count; ++i)
            {
                REQUIRE (read[static_cast<size_t> (i)] >= static_cast<float> (before));
                REQUIRE (read[static_cast<size_t> (i)] > last);
                last = read[static_cast<size_t> (i)];
            }
        }
    }
    audio.join();
    CHECK (skips > 1);
}
