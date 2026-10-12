#include "Measure.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

using namespace eq1;

namespace
{

// 24 Bands of every Shape, each Shape in three Stereo Placements, every one with a Shape that has
// dynamics a Dynamic Band: under Auto and set Threshold, one External, one with a Free Detection
// Range; the first one metered; Auto Gain and Output Pan on.
Settings everyShapeAndPlacement()
{
    auto settings = test::spreadBands ([] (size_t slot) {
        const Shape shape = test::allShapes[slot % test::allShapes.size()];
        return BandSettings { .shape = shape,
                              .gain = slot % 2 == 0 ? -6.0 : 6.0,
                              .q = 1.5,
                              .slope = 24.0,
                              .placement = test::allPlacements[(slot + slot / test::allShapes.size()) % test::allPlacements.size()],
                              .dynamicRange = hasDynamics (shape) ? (slot % 2 == 0 ? -9.0 : 9.0) : 0.0,
                              .threshold = -30.0,
                              .thresholdAuto = slot % 4 < 2 };
    });
    settings.bands[3].detectionSource = DetectionSource::External;
    settings.bands[18].detectionRange = DetectionRange::Free;
    settings.bands[18].detectionLow = 200.0;
    settings.bands[18].detectionHigh = 2000.0;
    settings.meteredSlot = 1;
    settings.autoGain = true;
    settings.outputPan = 0.3;
    return settings;
}

// What the Engine plays instead of the EQ's output while the editor holds it, if anything.
enum class Held
{
    Nothing,
    Solo,
    Audition,
};

Settings holding (Settings settings, Held held)
{
    if (held == Held::Solo)
        settings.soloSlot = 7;
    if (held == Held::Audition)
        settings.auditionSlot = 4;
    return settings;
}

// Half a second of stereo noise, loud and quiet in turn every 100 ms, and the Sidechain's, loud and
// quiet every 70 ms.
struct Material
{
    std::vector<float> left, right, sidechainLeft, sidechainRight;

    explicit Material (size_t length)
        : left (length), right (length), sidechainLeft (length), sidechainRight (length)
    {
        test::Noise noise (3);
        for (size_t i = 0; i < length; ++i)
        {
            const float level = i / 4800 % 2 == 0 ? 0.05f : 0.5f;
            left[i] = level * noise();
            right[i] = level * noise();
            const float sidechainLevel = i / 3360 % 2 == 0 ? 0.5f : 0.02f;
            sidechainLeft[i] = sidechainLevel * noise();
            sidechainRight[i] = sidechainLevel * noise();
        }
    }

    // Writes value into every channel, the Sidechain's included, from sample `from` on, count long.
    void burst (size_t from, size_t count, float value)
    {
        for (auto* channel : { &left, &right, &sidechainLeft, &sidechainRight })
            std::fill_n (channel->begin() + static_cast<std::ptrdiff_t> (from), count, value);
    }
};

// What came out of a Played: the output in place, and what the levels and the taps read after each block.
struct Played
{
    std::vector<float> left, right;
    std::vector<double> detectionLevels, outputLevels;
    std::vector<float> analysis;
};

// Plays the material from sample `from` to its end through a fresh Engine at 48 kHz, in blocks of
// blockSize, reading the Detection Level, the Output Level and the Analyzer taps after each block.
Played play (const Material& material, const Settings& settings, size_t from, int blockSize)
{
    Played played { .left = { material.left.begin() + static_cast<std::ptrdiff_t> (from), material.left.end() },
                    .right = { material.right.begin() + static_cast<std::ptrdiff_t> (from), material.right.end() } };
    const size_t length = played.left.size();
    Engine engine;
    engine.prepare (48000.0, blockSize, 2);
    engine.setSettings (settings);
    std::vector<float> tap (4096);
    for (size_t start = 0; start < length;)
    {
        const auto count = std::min (static_cast<size_t> (blockSize), length - start);
        float* main[] = { played.left.data() + start, played.right.data() + start };
        const float* sidechain[] = { material.sidechainLeft.data() + from + start, material.sidechainRight.data() + from + start };
        const ConstAudioBlock sidechainBlock { sidechain, 2, static_cast<int> (count) };
        engine.process ({ main, 2, static_cast<int> (count) }, &sidechainBlock);
        played.detectionLevels.push_back (engine.readDetectionLevel());
        for (int ch = 0; ch < 2; ++ch)
        {
            const auto level = engine.readOutputLevel (ch);
            played.outputLevels.push_back (level.peakDb);
            played.outputLevels.push_back (level.rmsDb);
        }
        for (const auto which : { AnalysisTap::PreEq, AnalysisTap::PostEq, AnalysisTap::Sidechain })
        {
            const int read = engine.readAnalysis (which, tap.data(), static_cast<int> (tap.size()));
            played.analysis.insert (played.analysis.end(), tap.begin(), tap.begin() + read);
        }
        start += count;
    }
    return played;
}

template <typename T>
size_t nonFiniteIn (const std::vector<T>& values)
{
    return static_cast<size_t> (std::count_if (values.begin(), values.end(), [] (T v) { return ! std::isfinite (v); }));
}

// Where the first of two outputs differs from the other, bit for bit, or their size when they don't.
size_t firstDifference (const std::vector<float>& a, const std::vector<float>& b)
{
    REQUIRE (a.size() == b.size());
    const auto mismatch = std::mismatch (a.begin(), a.end(), b.begin(), [] (float x, float y) {
        return std::bit_cast<std::uint32_t> (x) == std::bit_cast<std::uint32_t> (y);
    });
    return static_cast<size_t> (mismatch.first - a.begin());
}

} // namespace

TEST_CASE ("A non-finite input sample never reaches the output, and every Band recovers within one run")
{
    const float bad = GENERATE (std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                                -std::numeric_limits<float>::infinity());
    const auto held = GENERATE (Held::Nothing, Held::Solo, Held::Audition);
    // Blocks that cut the burst's run in two, and ones that hold several runs.
    const int blockSize = GENERATE (37, 512);
    CAPTURE (bad, static_cast<int> (held), blockSize);
    const auto settings = holding (everyShapeAndPlacement(), held);

    // A quarter second in, five bad samples in the middle of one 16-sample run, on every channel.
    constexpr size_t length = 24000, burstRun = 750, burstStart = burstRun * 16 + 7, burstLength = 5;
    constexpr size_t nextRun = (burstRun + 1) * 16;
    Material material (length);
    material.burst (burstStart, burstLength, bad);

    const auto played = play (material, settings, 0, blockSize);
    CHECK (nonFiniteIn (played.left) == 0);
    CHECK (nonFiniteIn (played.right) == 0);

    // From the next run on, the output is what a clean Engine, started at that run on the same
    // material, plays: every Band and detector has started afresh.
    const Material clean (length);
    const auto reference = play (clean, settings, nextRun, 512);
    const std::vector<float> leftAfter (played.left.begin() + nextRun, played.left.end());
    const std::vector<float> rightAfter (played.right.begin() + nextRun, played.right.end());
    CHECK (firstDifference (leftAfter, reference.left) == reference.left.size());
    CHECK (firstDifference (rightAfter, reference.right) == reference.right.size());
}

TEST_CASE ("The Output Level, the Detection Level and the Analyzer taps never read a non-finite value, burst or not")
{
    const float bad = GENERATE (std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity());
    CAPTURE (bad);
    Material material (24000);
    material.burst (12007, 5, bad);

    const auto played = play (material, everyShapeAndPlacement(), 0, 512);
    CHECK (nonFiniteIn (played.outputLevels) == 0);
    CHECK (nonFiniteIn (played.detectionLevels) == 0);
    CHECK (nonFiniteIn (played.analysis) == 0);
    // The metered Band's detector heard the burst's run and listens again from the next.
    CHECK (std::count (played.detectionLevels.begin(), played.detectionLevels.end(), levelFloorDb) == 0);
}
