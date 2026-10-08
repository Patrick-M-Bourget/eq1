#include "AllocationGuard.h"

#include "eq1/Engine.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace eq1;

TEST_CASE ("Engine does not allocate while processing or taking new settings")
{
    constexpr int blockSize = 512;
    Engine engine;
    engine.prepare (48000.0, blockSize, 2);

    std::vector<float> left (blockSize, 0.5f), right (blockSize, -0.5f), key (blockSize, 0.25f);
    float* main[] = { left.data(), right.data() };
    const float* sidechain[] = { key.data() };
    const ConstAudioBlock sidechainBlock { sidechain, 1, blockSize };
    std::vector<float> analysis (blockSize);

    Settings settings;
    settings.bands[0] = { .inUse = true, .shape = Shape::Bell, .frequency = 1000.0, .gain = 6.0, .q = 1.0 };

    test::AllocationGuard guard;
    for (int block = 0; block < 64; ++block)
    {
        settings.bands[0].frequency = 100.0 + 200.0 * block;
        settings.bands[0].gain = block % 2 == 0 ? 12.0 : -12.0;
        engine.setSettings (settings);
        engine.process ({ main, 2, blockSize }, block % 2 == 0 ? &sidechainBlock : nullptr);
        engine.readAnalysis (AnalysisTap::PostEq, analysis.data(), blockSize);
    }

    REQUIRE (guard.allocations() == 0);
}
