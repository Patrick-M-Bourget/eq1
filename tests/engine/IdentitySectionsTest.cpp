#include "../../engine/src/RunGrid.h"
#include "Measure.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

using namespace eq1;

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = runSamples; // one run per block, one piece each, so pieces are counted exactly
constexpr double toneFrequency = 200.0;
constexpr double onsetSeconds = 0.1;

// Plays a 200 Hz tone (or silence) through the Engine for seconds, appending the first channel's output.
struct Player
{
    Engine engine;
    int numChannels;
    std::vector<float> output;
    long n = 0;

    explicit Player (int channels) : numChannels (channels) { engine.prepare (sampleRate, blockSize, numChannels); }

    int runs (double seconds) const { return static_cast<int> (seconds * sampleRate / blockSize); }

    void play (double seconds, bool silent = false)
    {
        std::vector<std::vector<float>> block (static_cast<size_t> (numChannels), std::vector<float> (blockSize));
        std::vector<float*> channels;
        for (auto& ch : block)
            channels.push_back (ch.data());
        for (int b = 0; b < runs (seconds); ++b)
        {
            for (int i = 0; i < blockSize; ++i, ++n)
                for (size_t ch = 0; ch < block.size(); ++ch)
                    block[ch][static_cast<size_t> (i)] = silent ? 0.0f
                                                                : static_cast<float> ((0.25 - 0.1 * static_cast<double> (ch))
                                                                                      * std::sin (2.0 * std::numbers::pi * toneFrequency * n / sampleRate));
            engine.process ({ channels.data(), numChannels, blockSize });
            output.insert (output.end(), block[0].begin(), block[0].end());
        }
    }
};

Settings shelf (Shape shape, double q)
{
    Settings settings;
    auto& band = settings.bands[0];
    band.inUse = true;
    band.shape = shape;
    band.frequency = 1000.0;
    band.gain = 9.0;
    band.q = q;
    band.slope = 24.0; // two second-order sections, two biquads each: two identity biquads when unsplit
    return settings;
}
} // namespace

TEST_CASE ("A shelf below the split Q skips its identity biquads, and glides click-free out of and back into them")
{
    // Unsplit (Q 2 and below), each second-order shelf section's second biquad is exactly the identity
    // (docs/dsp/filter-design.md, "Resonant shelves"): it is skipped on every piece of every run while
    // its state is clear. Split, it filters, and runs again. Back below the split it rings down first,
    // then is skipped again once its state has drained; the ring-down is checked in silence, which
    // drains it on every platform.
    const Shape shape = GENERATE (Shape::LowShelf, Shape::HighShelf, Shape::TiltShelf);
    const int numChannels = GENERATE (1, 2); // one line, and a Stereo Band's two lanes
    CAPTURE (static_cast<int> (shape), numChannels);
    constexpr std::uint64_t identityBiquads = 2;

    Player player (numChannels);
    player.engine.setSettings (shelf (shape, 1.0));
    player.play (0.5);
    const auto unsplit = player.engine.sectionsSkipped();
    CHECK (unsplit == identityBiquads * static_cast<std::uint64_t> (player.runs (0.5)));

    // Q glides across the split: the biquads keep being skipped until Q passes 2, then filter.
    player.engine.setSettings (shelf (shape, 3.0));
    player.play (0.2);
    const auto split = player.engine.sectionsSkipped();
    CHECK (split > unsplit);
    CHECK (split < unsplit + identityBiquads * static_cast<std::uint64_t> (player.runs (0.2)));
    player.play (0.2);
    CHECK (player.engine.sectionsSkipped() == split);

    // Back below the split, they are the identity again but still ringing, so they run.
    player.engine.setSettings (shelf (shape, 1.0));
    player.play (0.2);
    const auto onset = static_cast<size_t> (onsetSeconds * sampleRate);
    CHECK (test::discontinuity (player.output, sampleRate, toneFrequency, onset) < 2.0);

    // Drained in silence, they are skipped again on every run.
    player.play (1.0, true);
    const auto drained = player.engine.sectionsSkipped();
    player.play (0.2, true);
    CHECK (player.engine.sectionsSkipped() == drained + identityBiquads * static_cast<std::uint64_t> (player.runs (0.2)));
}
