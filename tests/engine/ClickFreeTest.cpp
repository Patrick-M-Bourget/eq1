#include "Measure.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <numbers>
#include <vector>

using namespace eq1;

namespace
{

using test::bellBand;

constexpr double toneFrequency = 200.0;
constexpr double toneSeconds = 2.0;
constexpr double onsetSeconds = 0.1;

struct Host
{
    double sampleRate;
    int blockSize;
};

// Plays a sine through the Engine as a host would, letting change(seconds, settings) edit the
// settings before each block, and returns the output.
std::vector<float> playTone (Host host, const std::function<void (double, Settings&)>& change)
{
    const auto [sampleRate, blockSize] = host;
    const int numBlocks = static_cast<int> (toneSeconds * sampleRate / blockSize);

    Engine engine;
    engine.prepare (sampleRate, blockSize, 1);

    Settings settings;
    std::vector<float> output;
    std::vector<float> block (static_cast<size_t> (blockSize));
    float* channels[] = { block.data() };
    int n = 0;
    for (int b = 0; b < numBlocks; ++b)
    {
        change (static_cast<double> (n) / sampleRate, settings);
        engine.setSettings (settings);
        for (auto& s : block)
            s = static_cast<float> (0.25 * std::sin (2.0 * std::numbers::pi * toneFrequency * n++ / sampleRate));
        engine.process ({ channels, 1, blockSize });
        output.insert (output.end(), block.begin(), block.end());
    }
    return output;
}

// As playTone, in stereo: the tone at different levels and phases on the left and right, so it has both
// Mid and Side. Returns the left and right outputs.
std::array<std::vector<float>, 2> playStereoTone (Host host, const std::function<void (double, Settings&)>& change)
{
    const auto [sampleRate, blockSize] = host;
    const int numBlocks = static_cast<int> (toneSeconds * sampleRate / blockSize);

    Engine engine;
    engine.prepare (sampleRate, blockSize, 2);

    Settings settings;
    std::array<std::vector<float>, 2> output;
    std::vector<float> left (static_cast<size_t> (blockSize)), right (static_cast<size_t> (blockSize));
    float* channels[] = { left.data(), right.data() };
    int n = 0;
    for (int b = 0; b < numBlocks; ++b)
    {
        change (static_cast<double> (n) / sampleRate, settings);
        engine.setSettings (settings);
        for (size_t i = 0; i < left.size(); ++i, ++n)
        {
            const double phase = 2.0 * std::numbers::pi * toneFrequency * n / sampleRate;
            left[i] = static_cast<float> (0.25 * std::sin (phase));
            right[i] = static_cast<float> (0.15 * std::sin (phase + 1.0));
        }
        engine.process ({ channels, 2, blockSize });
        output[0].insert (output[0].end(), left.begin(), left.end());
        output[1].insert (output[1].end(), right.begin(), right.end());
    }
    return output;
}

// How much sharper the output's sharpest corner is than a pure sine of the same peak level would
// allow: a sine A sin(wn) has a second difference of at most A w^2. Clicks show up far above 1.
// The onset is skipped: the tone itself starts abruptly, and steep filters ring as it does. Every
// change a test makes comes after it.
double discontinuity (const std::vector<float>& output, double sampleRate)
{
    double peak = 0.0, sharpest = 0.0;
    for (auto i = static_cast<size_t> (onsetSeconds * sampleRate); i < output.size(); ++i)
    {
        peak = std::max (peak, static_cast<double> (std::abs (output[i])));
        sharpest = std::max (sharpest, std::abs (static_cast<double> (output[i]) - 2.0 * output[i - 1] + output[i - 2]));
    }
    const double w = 2.0 * std::numbers::pi * toneFrequency / sampleRate;
    return sharpest / (peak * w * w);
}

constexpr double threshold = 2.0;

// False until the onset has passed, then flips every 50 ms.
bool alternating (double seconds) { return seconds >= onsetSeconds && static_cast<int> ((seconds - onsetSeconds) / 0.05) % 2 == 0; }

// Odd and large blocks, at the common sample rates.
Host anyHost()
{
    return GENERATE (Host { 44100.0, 17 }, Host { 48000.0, 64 }, Host { 48000.0, 512 }, Host { 96000.0, 17 }, Host { 96000.0, 512 });
}

} // namespace

TEST_CASE ("Jumps in Gain do not click")
{
    const auto host = anyHost();
    CAPTURE (host.sampleRate, host.blockSize);
    const auto output = playTone (host, [] (double time, Settings& s) {
        s.bands[0] = bellBand (toneFrequency, alternating (time) ? 24.0 : -24.0, 1.0);
    });
    CHECK (discontinuity (output, host.sampleRate) < threshold);
}

TEST_CASE ("Jumps in Frequency do not click")
{
    const auto host = anyHost();
    CAPTURE (host.sampleRate, host.blockSize);
    const auto output = playTone (host, [] (double time, Settings& s) {
        s.bands[0] = bellBand (alternating (time) ? 5000.0 : 50.0, 18.0, 2.0);
    });
    CHECK (discontinuity (output, host.sampleRate) < threshold);
}

TEST_CASE ("Jumps in Q do not click")
{
    const auto host = anyHost();
    CAPTURE (host.sampleRate, host.blockSize);
    const auto output = playTone (host, [] (double time, Settings& s) {
        s.bands[0] = bellBand (toneFrequency, 18.0, alternating (time) ? 20.0 : 0.2);
    });
    CHECK (discontinuity (output, host.sampleRate) < threshold);
}

TEST_CASE ("A knob swept block by block does not zipper")
{
    const auto host = anyHost();
    CAPTURE (host.sampleRate, host.blockSize);
    const auto output = playTone (host, [] (double time, Settings& s) {
        const double position = 0.5 + 0.5 * std::sin (time * 15.0);
        s.bands[0] = bellBand (50.0 * std::pow (40.0, position), -24.0 + 48.0 * position, 0.3 * std::pow (30.0, position));
    });
    CHECK (discontinuity (output, host.sampleRate) < threshold);
}

TEST_CASE ("Toggling Bypass does not click")
{
    const auto host = anyHost();
    CAPTURE (host.sampleRate, host.blockSize);
    const auto output = playTone (host, [] (double time, Settings& s) {
        s.bands[0] = bellBand (toneFrequency, 18.0, 1.0);
        s.bands[0].bypass = alternating (time);
    });
    CHECK (discontinuity (output, host.sampleRate) < threshold);
}

TEST_CASE ("Putting a Band slot in and out of use does not click")
{
    const auto host = anyHost();
    CAPTURE (host.sampleRate, host.blockSize);
    const auto output = playTone (host, [] (double time, Settings& s) {
        s.bands[7] = bellBand (toneFrequency, -18.0, 1.0);
        s.bands[7].inUse = alternating (time);
    });
    CHECK (discontinuity (output, host.sampleRate) < threshold);
}

TEST_CASE ("Changing Shape does not click")
{
    const auto host = anyHost();
    const bool brickwall = GENERATE (false, true);
    CAPTURE (host.sampleRate, host.blockSize, brickwall);
    const auto output = playTone (host, [brickwall] (double time, Settings& s) {
        constexpr Shape shapes[] = { Shape::LowShelf, Shape::LowCut,   Shape::HighShelf, Shape::HighCut, Shape::Notch,
                                     Shape::BandPass, Shape::TiltShelf, Shape::FlatTilt, Shape::AllPass, Shape::Bell };
        s.bands[0] = bellBand (toneFrequency, 12.0, 1.0);
        s.bands[0].shape = time < onsetSeconds ? Shape::Bell : shapes[static_cast<int> ((time - onsetSeconds) / 0.05) % 10];
        s.bands[0].brickwall = brickwall;
    });
    CHECK (discontinuity (output, host.sampleRate) < threshold);
}

TEST_CASE ("Changing Slope does not click")
{
    const auto host = anyHost();
    CAPTURE (host.sampleRate, host.blockSize);
    const auto output = playTone (host, [] (double time, Settings& s) {
        s.bands[0] = bellBand (toneFrequency, -18.0, 1.0);
        s.bands[0].shape = Shape::LowShelf;
        s.bands[0].slope = alternating (time) ? 96.0 : 6.0;
    });
    CHECK (discontinuity (output, host.sampleRate) < threshold);
}

TEST_CASE ("A Cut swept block by block does not zipper")
{
    const auto host = anyHost();
    const Shape cut = GENERATE (Shape::LowCut, Shape::HighCut);
    const bool brickwall = GENERATE (false, true);
    CAPTURE (host.sampleRate, host.blockSize, static_cast<int> (cut), brickwall);
    // A 192 dB/oct Brickwall swept through the tone as fast as the Bell sweep reshapes it faster than
    // a sine turns, even with settings changed every sample, so it is swept a third as fast.
    const double speed = brickwall ? 5.0 : 15.0;
    const auto output = playTone (host, [&] (double time, Settings& s) {
        const double position = 0.5 + 0.5 * std::sin (time * speed);
        s.bands[0] = bellBand (50.0 * std::pow (40.0, position), 0.0, 0.3 * std::pow (30.0, position));
        s.bands[0].shape = cut;
        s.bands[0].slope = 6.0 + 90.0 * position;
        s.bands[0].brickwall = brickwall;
    });
    CHECK (discontinuity (output, host.sampleRate) < threshold);
}

TEST_CASE ("Switching a Cut's Brickwall does not click")
{
    const auto host = anyHost();
    const Shape cut = GENERATE (Shape::LowCut, Shape::HighCut);
    CAPTURE (host.sampleRate, host.blockSize, static_cast<int> (cut));
    const auto output = playTone (host, [&] (double time, Settings& s) {
        s.bands[0] = bellBand (cut == Shape::LowCut ? 100.0 : 400.0, 0.0, 1.0);
        s.bands[0].shape = cut;
        s.bands[0].slope = 12.0;
        s.bands[0].brickwall = alternating (time);
    });
    CHECK (discontinuity (output, host.sampleRate) < threshold);
}

TEST_CASE ("A Notch, Band Pass or All Pass swept block by block does not zipper")
{
    const auto host = anyHost();
    const Shape shape = GENERATE (Shape::Notch, Shape::BandPass, Shape::AllPass);
    CAPTURE (host.sampleRate, host.blockSize, static_cast<int> (shape));
    const auto output = playTone (host, [&] (double time, Settings& s) {
        const double position = 0.5 + 0.5 * std::sin (time * 15.0);
        s.bands[0] = bellBand (50.0 * std::pow (40.0, position), 0.0, 0.3 * std::pow (30.0, position));
        s.bands[0].shape = shape;
        s.bands[0].slope = 6.0 + 90.0 * position;
    });
    CHECK (discontinuity (output, host.sampleRate) < threshold);
}

TEST_CASE ("Changing Stereo Placement does not click")
{
    const auto host = anyHost();
    CAPTURE (host.sampleRate, host.blockSize);
    const auto output = playStereoTone (host, [] (double time, Settings& s) {
        constexpr StereoPlacement placements[] = { StereoPlacement::Left, StereoPlacement::Right, StereoPlacement::Mid,
                                                   StereoPlacement::Side, StereoPlacement::Stereo };
        s.bands[0] = bellBand (toneFrequency, 12.0, 1.0);
        s.bands[0].placement = time < onsetSeconds ? StereoPlacement::Stereo : placements[static_cast<int> ((time - onsetSeconds) / 0.05) % 5];
    });
    CHECK (discontinuity (output[0], host.sampleRate) < threshold);
    CHECK (discontinuity (output[1], host.sampleRate) < threshold);
}

TEST_CASE ("Engaging and releasing Solo does not click")
{
    const auto host = anyHost();
    const auto placement = GENERATE (StereoPlacement::Stereo, StereoPlacement::Left, StereoPlacement::Side);
    CAPTURE (host.sampleRate, host.blockSize, static_cast<int> (placement));
    // A High Shelf boosting the tone, Soloed on and off: Solo plays the region above 2 kHz, so the
    // tone at 200 Hz drops away and comes back.
    const auto output = playStereoTone (host, [placement] (double time, Settings& s) {
        s.bands[0] = bellBand (2000.0, 12.0, 1.0);
        s.bands[0].shape = Shape::HighShelf;
        s.bands[0].placement = placement;
        s.soloSlot = alternating (time) ? 1 : 0;
    });
    CHECK (discontinuity (output[0], host.sampleRate) < threshold);
    CHECK (discontinuity (output[1], host.sampleRate) < threshold);
}

TEST_CASE ("Moving Solo to another Band, or changing the Soloed Band's Stereo Placement, does not click")
{
    const auto host = anyHost();
    const bool movePlacement = GENERATE (false, true);
    CAPTURE (host.sampleRate, host.blockSize, movePlacement);
    const auto output = playStereoTone (host, [movePlacement] (double time, Settings& s) {
        s.bands[0] = bellBand (toneFrequency, 6.0, 1.0);
        s.bands[1] = bellBand (toneFrequency * 4.0, 6.0, 1.0); // its region barely has the tone
        if (movePlacement)
        {
            s.bands[0].placement = alternating (time) ? StereoPlacement::Left : StereoPlacement::Side;
            s.soloSlot = time < onsetSeconds ? 0 : 1;
        }
        else
        {
            s.soloSlot = time < onsetSeconds ? 0 : alternating (time) ? 1 : 2;
        }
    });
    CHECK (discontinuity (output[0], host.sampleRate) < threshold);
    CHECK (discontinuity (output[1], host.sampleRate) < threshold);
}

namespace
{
// A Dynamic Bell on the tone, cutting by up to 18 dB above a Threshold of -40 dB.
BandSettings dynamicBellOnTone (double timing)
{
    auto band = bellBand (toneFrequency, 0.0, 1.0);
    band.dynamicRange = -18.0;
    band.thresholdAuto = false;
    band.threshold = -40.0;
    band.attack = band.release = timing;
    return band;
}
} // namespace

TEST_CASE ("A Dynamic Band moving with Auto Attack and Release does not click")
{
    const auto host = anyHost();
    CAPTURE (host.sampleRate, host.blockSize);
    // The tone crosses Threshold every 50 ms as Threshold jumps.
    const auto output = playTone (host, [] (double seconds, Settings& s) {
        s.bands[0] = dynamicBellOnTone (50.0);
        s.bands[0].threshold = alternating (seconds) ? -40.0 : 0.0;
    });
    CHECK (discontinuity (output, host.sampleRate) < threshold);
}

TEST_CASE ("Switching Dynamics Bypass does not click, even at the fastest Attack and Release")
{
    const auto host = anyHost();
    CAPTURE (host.sampleRate, host.blockSize);
    const auto output = playTone (host, [] (double seconds, Settings& s) {
        s.bands[0] = dynamicBellOnTone (0.0);
        s.bands[0].dynamicsBypass = alternating (seconds);
    });
    CHECK (discontinuity (output, host.sampleRate) < threshold);
}
