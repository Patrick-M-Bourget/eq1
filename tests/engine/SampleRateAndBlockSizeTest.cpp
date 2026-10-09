#include "eq1/Engine.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <numbers>
#include <random>
#include <vector>

using namespace eq1;

namespace
{

// 24 Bands of every Shape and Stereo Placement, with Auto Gain and Output Pan; every third one a
// Dynamic Band when dynamic.
Settings scene (bool dynamic)
{
    constexpr Shape shapes[] = { Shape::Bell,  Shape::LowShelf, Shape::LowCut,    Shape::HighShelf, Shape::HighCut,
                                 Shape::Notch, Shape::BandPass, Shape::TiltShelf, Shape::FlatTilt,  Shape::AllPass };
    constexpr StereoPlacement placements[] = { StereoPlacement::Stereo, StereoPlacement::Left, StereoPlacement::Right,
                                               StereoPlacement::Mid, StereoPlacement::Side };
    Settings settings;
    for (size_t slot = 0; slot < settings.bands.size(); ++slot)
        settings.bands[slot] = { .inUse = true,
                                 .shape = shapes[slot % 10],
                                 .frequency = 30.0 * std::pow (1.3, static_cast<double> (slot)),
                                 .gain = slot % 2 == 0 ? -6.0 : 6.0,
                                 .q = 1.5,
                                 .slope = 24.0,
                                 .placement = placements[slot % 5],
                                 .dynamicRange = dynamic && slot % 3 == 0 ? -9.0 : 0.0,
                                 .threshold = -30.0,
                                 .thresholdAuto = slot % 2 == 0 };
    settings.autoGain = true;
    settings.outputPan = 0.3;
    return settings;
}

// Plays half a second of stereo noise, loud and quiet in turn every 100 ms, through the scene, in
// the blocks blockAt(n) gives for the nth block, and returns the left output then the right.
std::vector<float> play (double sampleRate, bool dynamic, const std::function<int (int)>& blockAt)
{
    const auto length = static_cast<size_t> (0.5 * sampleRate);
    const auto burst = static_cast<size_t> (0.1 * sampleRate);
    std::vector<float> left (length), right (length);
    std::mt19937 random (3);
    std::uniform_real_distribution<float> noise (-1.0f, 1.0f);
    for (size_t i = 0; i < length; ++i)
    {
        const float level = i / burst % 2 == 0 ? 0.05f : 0.5f;
        left[i] = level * noise (random);
        right[i] = level * noise (random);
    }

    Engine engine;
    engine.prepare (sampleRate, 4096, 2);
    engine.setSettings (scene (dynamic));
    for (size_t start = 0, n = 0; start < length; ++n)
    {
        const auto count = std::min (static_cast<size_t> (blockAt (static_cast<int> (n))), length - start);
        float* main[] = { left.data() + start, right.data() + start };
        engine.process ({ main, 2, static_cast<int> (count) });
        start += count;
    }
    left.insert (left.end(), right.begin(), right.end());
    return left;
}

// The largest difference between two outputs, in dB below the first's peak.
double differenceDb (const std::vector<float>& reference, const std::vector<float>& output)
{
    double peak = 0.0, difference = 0.0;
    for (size_t i = 0; i < reference.size(); ++i)
    {
        peak = std::max (peak, static_cast<double> (std::abs (reference[i])));
        difference = std::max (difference, static_cast<double> (std::abs (output[i] - reference[i])));
    }
    return 20.0 * std::log10 (difference / peak + 1.0e-30);
}

struct Blocks
{
    const char* name;
    std::function<int (int)> at;
};

// From a sample at a time to more than the Engine was prepared for, and sizes changing every block.
Blocks anyBlocks()
{
    return GENERATE (Blocks { "1", [] (int) { return 1; } }, Blocks { "7", [] (int) { return 7; } },
                     Blocks { "100", [] (int) { return 100; } }, Blocks { "8192", [] (int) { return 8192; } },
                     Blocks { "1 to 1000, changing", [] (int n) { return 1 + n * 7919 % 1000; } });
}

double anySampleRate() { return GENERATE (44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0); }

// A Dynamic Bell's Live Gain every 10 ms for two seconds, on a tone at its Frequency that is loud
// for 200 ms of every 500.
std::vector<double> liveGainCourse (double sampleRate, const BandSettings& band)
{
    Settings settings;
    settings.bands[0] = band;
    const auto blockSize = static_cast<int> (std::lround (0.01 * sampleRate)); // whole at every sample rate checked
    Engine engine;
    engine.prepare (sampleRate, blockSize, 1);
    engine.setSettings (settings);

    std::vector<float> block (static_cast<size_t> (blockSize));
    float* main[] = { block.data() };
    std::vector<double> course;
    long n = 0;
    for (int b = 0; b < 200; ++b)
    {
        for (auto& sample : block)
        {
            const double seconds = static_cast<double> (n) / sampleRate;
            const double level = std::fmod (seconds, 0.5) < 0.2 ? 0.5 : 0.01;
            sample = static_cast<float> (level * std::sin (2.0 * std::numbers::pi * band.frequency * seconds));
            ++n;
        }
        engine.process ({ main, 1, blockSize });
        course.push_back (engine.liveGainDb (1));
    }
    return course;
}

} // namespace

TEST_CASE ("Without Dynamic Bands, the output is the same sample for sample however the host cuts it into blocks", "[sweep]")
{
    const double sampleRate = anySampleRate();
    const auto blocks = anyBlocks();
    const auto reference = play (sampleRate, false, [] (int) { return 512; });

    CAPTURE (sampleRate, blocks.name);
    REQUIRE (play (sampleRate, false, blocks.at) == reference);
}

TEST_CASE ("Dynamic Bands sound the same, to within 40 dB below the peak, however the host cuts the audio into blocks", "[sweep]")
{
    const double sampleRate = anySampleRate();
    const auto blocks = anyBlocks();
    const auto reference = play (sampleRate, true, [] (int) { return 512; });

    // Not to the sample: a Dynamic Band's gain moves once per run the Engine processes, which is
    // shorter than its usual 16 samples where a host block ends (eq1/Engine.h).
    CAPTURE (sampleRate, blocks.name);
    REQUIRE (differenceDb (reference, play (sampleRate, true, blocks.at)) < -40.0);
}

TEST_CASE ("A Dynamic Band's Live Gain follows the same course in time at every sample rate", "[sweep]")
{
    const double sampleRate = anySampleRate();
    const auto timing = GENERATE (0.0, 50.0, 100.0); // fastest, Auto and slowest Attack and Release
    const bool thresholdAuto = GENERATE (false, true);
    const BandSettings band { .inUse = true, .shape = Shape::Bell, .frequency = 1000.0, .q = 1.0, .dynamicRange = -12.0,
                              .threshold = -30.0, .thresholdAuto = thresholdAuto, .attack = timing, .release = timing };

    const auto reference = liveGainCourse (48000.0, band);
    const auto course = liveGainCourse (sampleRate, band);
    double difference = 0.0;
    for (size_t i = 0; i < course.size(); ++i)
        difference = std::max (difference, std::abs (course[i] - reference[i]));

    CAPTURE (sampleRate, timing, thresholdAuto);
    REQUIRE (difference < 0.5);
}
