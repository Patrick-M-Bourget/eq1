#include "Measure.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
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

// How much sharper the output's sharpest corner is than a pure sine of the same peak level would
// allow: a sine A sin(wn) has a second difference of at most A w^2. Clicks show up far above 1.
double discontinuity (const std::vector<float>& output, double sampleRate)
{
    double peak = 0.0, sharpest = 0.0;
    for (size_t i = 2; i < output.size(); ++i)
    {
        peak = std::max (peak, static_cast<double> (std::abs (output[i])));
        sharpest = std::max (sharpest, std::abs (static_cast<double> (output[i]) - 2.0 * output[i - 1] + output[i - 2]));
    }
    const double w = 2.0 * std::numbers::pi * toneFrequency / sampleRate;
    return sharpest / (peak * w * w);
}

constexpr double threshold = 2.0;

// Flips between false and true every 50 ms.
bool alternating (double seconds) { return static_cast<int> (seconds / 0.05) % 2 == 1; }

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
