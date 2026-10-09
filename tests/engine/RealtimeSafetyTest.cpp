#include "AllocationGuard.h"

#include "eq1/Engine.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
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
            constexpr StereoPlacement placements[] = { StereoPlacement::Stereo, StereoPlacement::Left, StereoPlacement::Right,
                                                       StereoPlacement::Mid, StereoPlacement::Side };
            band.placement = placements[(static_cast<size_t> (block / 4) + slot) % 5];
        }
        // Solo moving from Band to Band, and off.
        settings.soloSlot = block % 6 == 5 ? 0 : 1 + block % numBandSlots;
        engine.setSettings (settings);
        engine.process ({ main, 2, blockSize }, block % 2 == 0 ? &sidechainBlock : nullptr);
        engine.readAnalysis (AnalysisTap::PostEq, analysis.data(), blockSize);
    }

    REQUIRE (guard.allocations() == 0);
}

TEST_CASE ("Engine does not allocate with 24 Brickwall Bands")
{
    constexpr int blockSize = 512;
    Engine engine;
    engine.prepare (48000.0, blockSize, 2);

    std::vector<float> left (blockSize, 0.5f), right (blockSize, -0.5f);
    float* main[] = { left.data(), right.data() };

    Settings settings;
    test::AllocationGuard guard;
    for (int block = 0; block < 64; ++block)
    {
        // Gliding Frequency and Q, and Brickwall switching on and off, on both Cuts.
        for (size_t slot = 0; slot < settings.bands.size(); ++slot)
            settings.bands[slot] = { .inUse = true,
                                     .shape = slot % 2 == 0 ? Shape::LowCut : Shape::HighCut,
                                     .frequency = 100.0 + 300.0 * block + 10.0 * static_cast<double> (slot),
                                     .q = 0.5 + 0.1 * block,
                                     .slope = 96.0,
                                     .brickwall = block % 16 != 15 };
        engine.setSettings (settings);
        engine.process ({ main, 2, blockSize });
    }

    REQUIRE (guard.allocations() == 0);
}

TEST_CASE ("Engine does not allocate with 24 Dynamic Bands")
{
    constexpr int blockSize = 512;
    Engine engine;
    engine.prepare (48000.0, blockSize, 2);

    std::vector<float> left (blockSize), right (blockSize), sidechainLeft (blockSize, 0.3f), sidechainRight (blockSize, -0.1f);
    float* main[] = { left.data(), right.data() };
    const float* sidechainChannels[] = { sidechainLeft.data(), sidechainRight.data() };

    Settings settings;
    test::AllocationGuard guard;
    for (int block = 0; block < 64; ++block)
    {
        // Loud and quiet in turn, so every Band moves; every dynamic Shape, Stereo Placement, Auto and
        // set Threshold, timing, and Dynamics Bypass switching; Internal and External detection, Band
        // and Free Detection Range, a stereo, mono and no Sidechain, and Detection Audition moving
        // from Band to Band.
        for (size_t i = 0; i < left.size(); ++i)
        {
            const float level = block % 4 < 2 ? 0.5f : 0.001f;
            left[i] = level * static_cast<float> ((i * 7919) % 101) / 101.0f;
            right[i] = -0.5f * left[i];
        }
        for (size_t slot = 0; slot < settings.bands.size(); ++slot)
        {
            constexpr Shape shapes[] = { Shape::Bell, Shape::LowShelf, Shape::HighShelf, Shape::TiltShelf, Shape::FlatTilt };
            constexpr StereoPlacement placements[] = { StereoPlacement::Stereo, StereoPlacement::Left, StereoPlacement::Right,
                                                       StereoPlacement::Mid, StereoPlacement::Side };
            settings.bands[slot] = { .inUse = true,
                                     .shape = shapes[slot % 5],
                                     .frequency = 100.0 + 400.0 * static_cast<double> (slot) + 10.0 * block,
                                     .gain = 3.0,
                                     .q = 1.0,
                                     .placement = placements[(slot / 5) % 5],
                                     .detectionSource = (block + static_cast<int> (slot)) % 4 < 2 ? DetectionSource::Internal : DetectionSource::External,
                                     .detectionRange = (block / 3 + static_cast<int> (slot)) % 2 == 0 ? DetectionRange::Band : DetectionRange::Free,
                                     .detectionLow = 40.0 + 5.0 * block,
                                     .detectionHigh = 2000.0 + 100.0 * static_cast<double> (slot),
                                     .dynamicRange = slot % 2 == 0 ? -12.0 : 9.0,
                                     .threshold = -40.0,
                                     .thresholdAuto = slot % 3 == 0,
                                     .attack = static_cast<double> ((slot * 13) % 101),
                                     .release = static_cast<double> ((slot * 29) % 101),
                                     .dynamicsBypass = (block + static_cast<int> (slot)) % 9 == 0 };
        }
        settings.auditionSlot = block % 5 == 4 ? 0 : 1 + (block / 5) % numBandSlots;
        const ConstAudioBlock sidechain { sidechainChannels, block % 3 == 0 ? 1 : 2, blockSize };
        engine.setSettings (settings);
        engine.process ({ main, 2, blockSize }, block % 7 == 6 ? nullptr : &sidechain);
        REQUIRE (std::isfinite (engine.liveGainDb (1)));
    }

    REQUIRE (guard.allocations() == 0);
}
