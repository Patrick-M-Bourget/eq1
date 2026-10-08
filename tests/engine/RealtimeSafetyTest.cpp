#include "AllocationGuard.h"

#include "eq1/Engine.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace eq1;

TEST_CASE ("Engine does not allocate while processing or taking new settings, with all 24 Bands in use")
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
    for (auto& band : settings.bands)
        band = { .inUse = true, .shape = Shape::Bell, .frequency = 1000.0, .gain = 6.0, .q = 1.0 };

    test::AllocationGuard guard;
    for (int block = 0; block < 64; ++block)
    {
        // Glides, crossfades, Shape and Slope changes and slots coming in and out of use, all at once.
        for (size_t slot = 0; slot < settings.bands.size(); ++slot)
        {
            auto& band = settings.bands[slot];
            band.frequency = 100.0 + 200.0 * block + 10.0 * static_cast<double> (slot);
            band.gain = block % 2 == 0 ? 12.0 : -12.0;
            band.bypass = (block + static_cast<int> (slot)) % 5 == 0;
            band.inUse = (block + static_cast<int> (slot)) % 7 != 0;
            constexpr Shape shapes[] = { Shape::Bell,  Shape::LowShelf, Shape::LowCut,    Shape::HighShelf, Shape::HighCut,
                                         Shape::Notch, Shape::BandPass, Shape::TiltShelf, Shape::FlatTilt,  Shape::AllPass };
            band.shape = shapes[(static_cast<size_t> (block / 8) + slot) % 10];
            band.slope = 6.0 * static_cast<double> (1 + (static_cast<size_t> (block) + slot) % 16);
        }
        engine.setSettings (settings);
        engine.process ({ main, 2, blockSize }, block % 2 == 0 ? &sidechainBlock : nullptr);
        engine.readAnalysis (AnalysisTap::PostEq, analysis.data(), blockSize);
    }

    REQUIRE (guard.allocations() == 0);
}
