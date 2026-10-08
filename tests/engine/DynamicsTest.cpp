#include "Measure.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <functional>
#include <numbers>
#include <random>
#include <vector>

using namespace eq1;
using Catch::Matchers::WithinAbs;

namespace
{
constexpr double sampleRate = 48000.0;

// A full-scale sine reads 0 dB on the detector, as on the Analyzer.
double amplitudeOf (double db) { return std::pow (10.0, db / 20.0); }

double sine (double frequency, double db, int n) { return amplitudeOf (db) * std::sin (2.0 * std::numbers::pi * frequency * n / sampleRate); }

// A Dynamic Bell in slot 1, its Threshold set by hand.
BandSettings dynamicBell (double gain, double dynamicRange, double threshold)
{
    auto band = test::bellBand (1000.0, gain, 1.0);
    band.dynamicRange = dynamicRange;
    band.thresholdAuto = false;
    band.threshold = threshold;
    return band;
}

struct Run
{
    std::vector<double> liveGain;              // slot 1's Live Gain after each block
    std::vector<std::vector<float>> output;    // per channel
};

// Plays signal (channel, sample index) through an Engine with the given channels for seconds, in
// blocks of blockSize, with settings that change(seconds, settings) may edit before each block.
Run play (int numChannels,
          double seconds,
          const Settings& start,
          const std::function<double (int, int)>& signal,
          const std::function<void (double, Settings&)>& change = {},
          int blockSize = 64)
{
    Engine engine;
    engine.prepare (sampleRate, blockSize, numChannels);
    Settings settings = start;
    Run run;
    run.output.resize (static_cast<size_t> (numChannels));
    std::vector<std::vector<float>> block (static_cast<size_t> (numChannels), std::vector<float> (blockSize));
    std::vector<float*> channels;
    for (auto& channel : block)
        channels.push_back (channel.data());
    const int numBlocks = static_cast<int> (seconds * sampleRate / blockSize);
    for (int b = 0, n = 0; b < numBlocks; ++b, n += blockSize)
    {
        if (change)
            change (n / sampleRate, settings);
        engine.setSettings (settings);
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < blockSize; ++i)
                block[static_cast<size_t> (ch)][static_cast<size_t> (i)] = static_cast<float> (signal (ch, n + i));
        engine.process ({ channels.data(), numChannels, blockSize });
        run.liveGain.push_back (engine.liveGainDb (1));
        for (int ch = 0; ch < numChannels; ++ch)
            run.output[static_cast<size_t> (ch)].insert (run.output[static_cast<size_t> (ch)].end(), block[static_cast<size_t> (ch)].begin(),
                                                         block[static_cast<size_t> (ch)].end());
    }
    return run;
}

Settings withBand (const BandSettings& band)
{
    Settings settings;
    settings.bands[0] = band;
    return settings;
}

double last (const Run& run) { return run.liveGain.back(); }

// Timing runs in blocks of 16 samples, the Engine's sub-block, so Live Gain is seen at every step.
constexpr int timingBlock = 16;
constexpr double timingBlockSeconds = timingBlock / sampleRate;

// How long Live Gain takes from block from to cover half of its movement from its value before from
// to its value at block to, interpolated between blocks.
double secondsToHalfway (const std::vector<double>& liveGain, size_t from, size_t to)
{
    const double start = liveGain[from - 1], end = liveGain[to - 1], halfway = 0.5 * (start + end);
    for (size_t b = from; b < to; ++b)
        if ((liveGain[b] - halfway) * (end - start) >= 0.0)
        {
            const double before = liveGain[b - 1];
            return (static_cast<double> (b - from) + (halfway - before) / (liveGain[b] - before)) * timingBlockSeconds;
        }
    FAIL ("Live Gain never got halfway");
    return 0.0;
}

// Silence, then from onset a tone at Frequency, levelDb loud, for length seconds, then silence again.
std::function<double (int, int)> burst (double onset, double length, double levelDb)
{
    return [=] (int, int n) { return n >= onset * sampleRate && n < (onset + length) * sampleRate ? sine (1000.0, levelDb, n) : 0.0; };
}

size_t blockAt (double seconds) { return static_cast<size_t> (seconds / timingBlockSeconds); }

// The attack time, halfway, of a Dynamic Bell with Threshold -30 dB to a tone overshootDb above it.
double attackSeconds (double attack, double overshootDb)
{
    auto band = dynamicBell (0.0, -9.0, -30.0);
    band.attack = attack;
    const auto run = play (1, 1.0, withBand (band), burst (0.25, 1.0, -30.0 + overshootDb), {}, timingBlock);
    return secondsToHalfway (run.liveGain, blockAt (0.25), run.liveGain.size());
}

// The release time, halfway, of the same Bell after a tone far above Threshold lasting length seconds.
double releaseSeconds (double release, double length)
{
    auto band = dynamicBell (0.0, -9.0, -30.0);
    band.release = release;
    const auto run = play (1, 0.25 + length + 8.0, withBand (band), burst (0.25, length, -6.0), {}, timingBlock);
    return secondsToHalfway (run.liveGain, blockAt (0.25 + length), run.liveGain.size());
}
} // namespace

TEST_CASE ("A Dynamic Band's Live Gain moves by its signed Dynamic Range when detection crosses Threshold")
{
    const double dynamicRange = GENERATE (-9.0, 6.0);
    CAPTURE (dynamicRange);
    const auto settings = withBand (dynamicBell (3.0, dynamicRange, -30.0));

    // Far below Threshold: Live Gain stays at Gain.
    const auto quiet = play (1, 1.0, settings, [] (int, int n) { return sine (1000.0, -60.0, n); });
    CHECK_THAT (last (quiet), WithinAbs (3.0, 0.01));

    // Far above it: Live Gain reaches Gain plus Dynamic Range.
    const auto loud = play (1, 1.0, settings, [] (int, int n) { return sine (1000.0, -3.0, n); });
    CHECK_THAT (last (loud), WithinAbs (3.0 + dynamicRange, 0.05));
}

TEST_CASE ("A Dynamic Range of 0 means no movement")
{
    const auto settings = withBand (dynamicBell (3.0, 0.0, -30.0));
    const auto loud = play (1, 1.0, settings, [] (int, int n) { return sine (1000.0, -3.0, n); });
    for (double g : loud.liveGain)
        REQUIRE (g == 3.0);
}

TEST_CASE ("A Dynamic Band sounds at its Live Gain")
{
    // A steady tone at Frequency, far above Threshold: the Bell's boost of 3 dB becomes a cut of 6 dB.
    const auto settings = withBand (dynamicBell (3.0, -9.0, -40.0));
    const auto run = play (1, 1.0, settings, [] (int, int n) { return sine (1000.0, -10.0, n); });
    double input = 0.0, output = 0.0;
    const auto& out = run.output[0];
    for (size_t i = out.size() / 2; i < out.size(); ++i)
    {
        input += std::pow (sine (1000.0, -10.0, static_cast<int> (i)), 2.0);
        output += static_cast<double> (out[i]) * out[i];
    }
    CHECK_THAT (10.0 * std::log10 (output / input), WithinAbs (-6.0, 0.1));
}

TEST_CASE ("Dynamics Bypass holds Live Gain at Gain, keeping the Band's dynamics for when it is released")
{
    // The whole time above Threshold: Bypassed for a second, released for a second, Bypassed again.
    auto band = dynamicBell (3.0, -9.0, -30.0);
    band.dynamicsBypass = true;
    const auto run = play (1, 3.0, withBand (band), [] (int, int n) { return sine (1000.0, -3.0, n); },
                           [] (double seconds, Settings& s) { s.bands[0].dynamicsBypass = seconds < 1.0 || seconds >= 2.0; });
    const auto blocksPerSecond = run.liveGain.size() / 3;
    for (size_t b = 0; b < blocksPerSecond; ++b)
        REQUIRE (run.liveGain[b] == 3.0);
    CHECK_THAT (run.liveGain[2 * blocksPerSecond - 1], WithinAbs (-6.0, 0.05));
    CHECK (last (run) == 3.0);
}

TEST_CASE ("Live Gain never goes beyond +/-30 dB")
{
    const double gain = GENERATE (-24.0, 24.0);
    CAPTURE (gain);
    const double dynamicRange = gain > 0.0 ? 30.0 : -30.0;
    const auto run = play (1, 1.0, withBand (dynamicBell (gain, dynamicRange, -40.0)), [] (int, int n) { return sine (1000.0, -3.0, n); });
    for (double g : run.liveGain)
        REQUIRE (std::abs (g) <= 30.0);
    CHECK_THAT (last (run), WithinAbs (gain > 0.0 ? 30.0 : -30.0, 1.0e-9));
}

TEST_CASE ("Attack and Release get strictly faster below 50% and slower above it")
{
    const double settings[] = { 0.0, 25.0, 49.0, 50.0, 51.0, 75.0, 100.0 };
    double previousAttack = 0.0, previousRelease = 0.0;
    for (double setting : settings)
    {
        CAPTURE (setting);
        const double attack = attackSeconds (setting, 9.0), release = releaseSeconds (setting, 1.0);
        CAPTURE (attack, release);
        CHECK (attack > previousAttack);
        CHECK (release > previousRelease);
        previousAttack = attack;
        previousRelease = release;
    }
}

TEST_CASE ("Auto Attack catches a detection signal far above Threshold faster than one just above it")
{
    const double far = attackSeconds (50.0, 30.0), near = attackSeconds (50.0, 6.0);
    CAPTURE (far, near);
    CHECK (far < 0.5 * near);
}

TEST_CASE ("Auto Release recovers faster after a short burst than after sustained movement")
{
    const double shortBurst = releaseSeconds (50.0, 0.05), sustained = releaseSeconds (50.0, 1.5);
    CAPTURE (shortBurst, sustained);
    CHECK (shortBurst < 0.5 * sustained);
}

TEST_CASE ("Auto Threshold lets a Band rest on steady material and move on what stands out of it, at any level")
{
    // Reference material: white noise with a tone at the Bell's Frequency bursting out of it about
    // 15 dB louder in the Bell's region, 100 ms in every 500 ms. The same material is played at two levels
    // 24 dB apart: Auto Threshold follows the level, so the Band acts the same.
    const double levelDb = GENERATE (-36.0, -12.0);
    CAPTURE (levelDb);
    constexpr double seconds = 6.0, period = 0.5, burstLength = 0.1;
    std::mt19937 random (7);
    std::normal_distribution<double> gaussian;
    std::vector<double> material (static_cast<size_t> (seconds * sampleRate));
    for (size_t n = 0; n < material.size(); ++n)
    {
        const bool bursting = std::fmod (n / sampleRate, period) < burstLength;
        material[n] = amplitudeOf (levelDb) * gaussian (random) + (bursting ? sine (1000.0, levelDb + 6.0, static_cast<int> (n)) : 0.0);
    }

    auto band = test::bellBand (1000.0, 0.0, 1.0);
    band.dynamicRange = -6.0;
    REQUIRE (band.thresholdAuto);
    const auto run = play (1, seconds, withBand (band), [&] (int, int n) { return material[static_cast<size_t> (n)]; }, {}, timingBlock);

    // After the first two seconds: more than half the Dynamic Range at the end of each burst, and
    // back near Gain just before the next one.
    for (double start = 2.0; start + period < seconds; start += period)
    {
        CAPTURE (start);
        CHECK (run.liveGain[blockAt (start + burstLength) - 1] < -3.0);
        CHECK (run.liveGain[blockAt (start + period) - 1] > -1.0);
    }
}

TEST_CASE ("A Mid or Side Dynamic Band reacts only to Mid or Side content")
{
    const auto placement = GENERATE (StereoPlacement::Mid, StereoPlacement::Side);
    CAPTURE (static_cast<int> (placement));
    auto band = dynamicBell (0.0, -9.0, -30.0);
    band.placement = placement;
    // A loud tone in the Mid (the same on both channels) or in the Side (inverted on the right).
    const auto toneIn = [] (StereoPlacement part) {
        return [part] (int ch, int n) { return (ch == 1 && part == StereoPlacement::Side ? -1.0 : 1.0) * sine (1000.0, -6.0, n); };
    };
    const auto other = placement == StereoPlacement::Mid ? StereoPlacement::Side : StereoPlacement::Mid;

    CHECK (last (play (2, 1.0, withBand (band), toneIn (other))) == 0.0);
    CHECK_THAT (last (play (2, 1.0, withBand (band), toneIn (placement))), WithinAbs (-9.0, 0.05));
}

TEST_CASE ("A Stereo Dynamic Band applies one Live Gain to both channels, driven by the louder one")
{
    const auto band = dynamicBell (0.0, -9.0, -30.0);
    // A loud tone on one channel and a quiet one, far below Threshold, on the other.
    const auto loudOn = [] (int loudChannel) {
        return [loudChannel] (int ch, int n) { return sine (1000.0, ch == loudChannel ? -6.0 : -50.0, n); };
    };
    const auto leftLoud = play (2, 1.0, withBand (band), loudOn (0));
    const auto rightLoud = play (2, 1.0, withBand (band), loudOn (1));
    CHECK (leftLoud.liveGain == rightLoud.liveGain);
    CHECK_THAT (last (leftLoud), WithinAbs (-9.0, 0.05));

    // The quiet channel is cut as much as the loud one.
    for (int ch = 0; ch < 2; ++ch)
    {
        CAPTURE (ch);
        const auto& out = leftLoud.output[static_cast<size_t> (ch)];
        double input = 0.0, output = 0.0;
        for (size_t i = out.size() / 2; i < out.size(); ++i)
        {
            input += std::pow (sine (1000.0, ch == 0 ? -6.0 : -50.0, static_cast<int> (i)), 2.0);
            output += static_cast<double> (out[i]) * out[i];
        }
        CHECK_THAT (10.0 * std::log10 (output / input), WithinAbs (-9.0, 0.1));
    }
}

TEST_CASE ("A Left or Right Dynamic Band reacts only to its own channel")
{
    const auto placement = GENERATE (StereoPlacement::Left, StereoPlacement::Right);
    auto band = dynamicBell (0.0, -9.0, -30.0);
    band.placement = placement;
    const int own = placement == StereoPlacement::Left ? 0 : 1;
    const auto loudOn = [] (int loudChannel) { return [loudChannel] (int ch, int n) { return ch == loudChannel ? sine (1000.0, -6.0, n) : 0.0; }; };
    CHECK (last (play (2, 1.0, withBand (band), loudOn (1 - own))) == 0.0);
    CHECK_THAT (last (play (2, 1.0, withBand (band), loudOn (own))), WithinAbs (-9.0, 0.05));
}

namespace
{
// The Live Gain a Dynamic Band of the given Shape at 1 kHz, Threshold -30 dB, reaches on a tone at
// toneFrequency, toneDb loud.
double liveGainOnTone (Shape shape, double q, double toneFrequency, double toneDb)
{
    auto band = dynamicBell (0.0, -9.0, -30.0);
    band.shape = shape;
    band.q = q;
    band.slope = 12.0;
    return last (play (1, 1.0, withBand (band), [=] (int, int n) { return sine (toneFrequency, toneDb, n); }));
}
} // namespace

TEST_CASE ("Bell detection is limited to its Frequency and Q")
{
    // The same tone moves the Band at Frequency, and not three octaves away.
    CHECK (liveGainOnTone (Shape::Bell, 2.0, 1000.0, -12.0) < -8.0);
    CHECK (liveGainOnTone (Shape::Bell, 2.0, 125.0, -12.0) == 0.0);
    CHECK (liveGainOnTone (Shape::Bell, 2.0, 8000.0, -12.0) == 0.0);
    // An octave away: heard by a wide Bell, not a narrow one.
    CHECK (liveGainOnTone (Shape::Bell, 0.5, 2000.0, -20.0) < -3.0);
    CHECK (liveGainOnTone (Shape::Bell, 4.0, 2000.0, -20.0) == 0.0);
}

TEST_CASE ("Low Shelf detection ignores content above its Frequency, and High Shelf detection content below it")
{
    CHECK_THAT (liveGainOnTone (Shape::LowShelf, 1.0, 100.0, -6.0), WithinAbs (-9.0, 0.05));
    CHECK (liveGainOnTone (Shape::LowShelf, 1.0, 8000.0, -6.0) == 0.0);
    CHECK_THAT (liveGainOnTone (Shape::HighShelf, 1.0, 10000.0, -6.0), WithinAbs (-9.0, 0.05));
    CHECK (liveGainOnTone (Shape::HighShelf, 1.0, 125.0, -6.0) == 0.0);
}

TEST_CASE ("Tilt Shelf and Flat Tilt detect on the whole spectrum")
{
    const auto shape = GENERATE (Shape::TiltShelf, Shape::FlatTilt);
    for (double frequency : { 30.0, 1000.0, 18000.0 })
    {
        CAPTURE (static_cast<int> (shape), frequency);
        CHECK_THAT (liveGainOnTone (shape, 1.0, frequency, -6.0), WithinAbs (-9.0, 0.05));
    }
}

TEST_CASE ("A Cut, Notch, Band Pass or All Pass ignores its dynamics settings")
{
    const auto shape = GENERATE (Shape::LowCut, Shape::HighCut, Shape::Notch, Shape::BandPass, Shape::AllPass);
    CAPTURE (static_cast<int> (shape));
    auto dynamic = dynamicBell (0.0, -9.0, -30.0);
    dynamic.shape = shape;
    auto plain = dynamic;
    plain.dynamicRange = 0.0;
    const auto tone = [] (int, int n) { return sine (1000.0, -6.0, n); };
    const auto withDynamics = play (1, 1.0, withBand (dynamic), tone), without = play (1, 1.0, withBand (plain), tone);
    CHECK (withDynamics.output == without.output);
    for (double g : withDynamics.liveGain)
        REQUIRE (g == 0.0);
}

TEST_CASE ("Bell to Notch and back to Bell restores the Band's dynamics unchanged")
{
    const auto band = dynamicBell (2.0, -9.0, -30.0);
    const auto run = play (1, 3.0, withBand (band), [] (int, int n) { return sine (1000.0, -6.0, n); },
                           [] (double seconds, Settings& s) { s.bands[0].shape = seconds >= 1.0 && seconds < 2.0 ? Shape::Notch : Shape::Bell; });
    const auto blocksPerSecond = run.liveGain.size() / 3;
    CHECK_THAT (run.liveGain[blocksPerSecond - 1], WithinAbs (-7.0, 0.05));
    CHECK (run.liveGain[2 * blocksPerSecond - 1] == 2.0); // Notch: no movement
    CHECK_THAT (last (run), WithinAbs (-7.0, 0.05));
}
