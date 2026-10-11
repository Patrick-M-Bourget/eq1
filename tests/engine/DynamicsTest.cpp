#include "Measure.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
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

// A signal for the Sidechain: its channel count, and its sample (channel, sample index).
struct Sidechain
{
    int numChannels = 2;
    std::function<double (int, int)> signal;
};

struct Run
{
    std::vector<double> liveGain;              // slot 1's Live Gain after each block
    std::vector<double> detectionLevel;        // the metered Band's Detection Level, read after each block
    std::vector<std::vector<float>> output;    // per channel
};

// Plays signal (channel, sample index) through an Engine with the given channels for seconds, in
// blocks of blockSize, with settings that change(seconds, settings) may edit before each block.
Run play (int numChannels,
          double seconds,
          const Settings& start,
          const std::function<double (int, int)>& signal,
          const std::function<void (double, Settings&)>& change = {},
          int blockSize = 64,
          const Sidechain* sidechain = nullptr)
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
    const int sidechainChannels = sidechain != nullptr ? sidechain->numChannels : 0;
    std::vector<std::vector<float>> sidechainSamples (static_cast<size_t> (sidechainChannels), std::vector<float> (blockSize));
    std::vector<const float*> sidechainChannelPointers;
    for (auto& channel : sidechainSamples)
        sidechainChannelPointers.push_back (channel.data());
    const ConstAudioBlock sidechainBlock { sidechainChannelPointers.data(), sidechainChannels, blockSize };
    const int numBlocks = static_cast<int> (seconds * sampleRate / blockSize);
    for (int b = 0, n = 0; b < numBlocks; ++b, n += blockSize)
    {
        if (change)
            change (n / sampleRate, settings);
        engine.setSettings (settings);
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < blockSize; ++i)
                block[static_cast<size_t> (ch)][static_cast<size_t> (i)] = static_cast<float> (signal (ch, n + i));
        for (int ch = 0; ch < sidechainChannels; ++ch)
            for (int i = 0; i < blockSize; ++i)
                sidechainSamples[static_cast<size_t> (ch)][static_cast<size_t> (i)] = static_cast<float> (sidechain->signal (ch, n + i));
        engine.process ({ channels.data(), numChannels, blockSize }, sidechain != nullptr ? &sidechainBlock : nullptr);
        run.liveGain.push_back (engine.liveGainDb (1));
        run.detectionLevel.push_back (engine.readDetectionLevel());
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

TEST_CASE ("A larger Dynamic Range moves a Band further for the same overshoot, not slower")
{
    // Detection 6 dB above Threshold.
    const auto moved = [] (double dynamicRange)
    { return last (play (1, 1.0, withBand (dynamicBell (0.0, dynamicRange, -30.0)), [] (int, int n) { return sine (1000.0, -24.0, n); })); };
    const double small = moved (-6.0), large = moved (-12.0);
    CAPTURE (small, large);
    CHECK (small < 0.0);
    CHECK (large <= small);
}

TEST_CASE ("A Dynamic Band reaches its full Dynamic Range 15 dB above Threshold, whatever its size")
{
    // 12 dB of overshoot plus the 3 dB knee (docs/dsp/filter-design.md, Dynamics).
    const double dynamicRange = GENERATE (-6.0, -12.0, 12.0);
    CAPTURE (dynamicRange);
    const auto run = play (1, 1.0, withBand (dynamicBell (0.0, dynamicRange, -30.0)), [] (int, int n) { return sine (1000.0, -15.0, n); });
    CHECK_THAT (last (run), WithinAbs (dynamicRange, 0.5));
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

TEST_CASE ("Auto Threshold starts afresh when a Band becomes active again, so it follows what plays now")
{
    // Quiet material for a second, Dynamics Bypass for a second, then material 30 dB louder: a
    // Threshold learned on the quiet material would duck the loud one at once.
    auto band = dynamicBell (0.0, -9.0, -30.0);
    band.thresholdAuto = true;
    const auto run = play (1, 2.5, withBand (band), [] (int, int n) { return sine (1000.0, n < 2.0 * sampleRate ? -40.0 : -10.0, n); },
                           [] (double seconds, Settings& s) { s.bands[0].dynamicsBypass = seconds >= 1.0 && seconds < 2.0; });
    for (size_t b = blockAt (2.0) / 4; b < run.liveGain.size(); ++b)
        REQUIRE (run.liveGain[b] > -1.0);
}

namespace
{
// White noise at levelDb, its level shaped by envelopeDb (seconds), drawn once so it can be replayed.
std::vector<double> shapedNoise (double seconds, double levelDb, const std::function<double (double)>& envelopeDb)
{
    std::mt19937 random (7);
    std::normal_distribution<double> gaussian;
    std::vector<double> material (static_cast<size_t> (seconds * sampleRate));
    for (size_t n = 0; n < material.size(); ++n)
        material[n] = amplitudeOf (levelDb + envelopeDb (n / sampleRate)) * gaussian (random);
    return material;
}

// A Dynamic Bell at 1 kHz, Q 1, Gain 0, in Auto Threshold, played over material in Engine runs.
Run playAuto (const std::vector<double>& material, double dynamicRange, const std::function<void (double, Settings&)>& change = {})
{
    auto band = test::bellBand (1000.0, 0.0, 1.0);
    band.dynamicRange = dynamicRange;
    REQUIRE (band.thresholdAuto);
    return play (1, material.size() / sampleRate, withBand (band), [&] (int, int n) { return material[static_cast<size_t> (n)]; },
                 change, timingBlock);
}

// The Live Gain furthest from Gain (0 dB), and the one nearest it, between from and to seconds.
double mostMoved (const Run& run, double from, double to)
{
    double most = 0.0;
    for (size_t b = blockAt (from); b < blockAt (to); ++b)
        most = std::max (most, std::abs (run.liveGain[b]));
    return most;
}

double leastMoved (const Run& run, double from, double to)
{
    double least = 1000.0;
    for (size_t b = blockAt (from); b < blockAt (to); ++b)
        least = std::min (least, std::abs (run.liveGain[b]));
    return least;
}
} // namespace

TEST_CASE ("Auto Threshold follows the material's spread, so a Band moves on ordinary swings at any level")
{
    // Noise whose level swings +/-6 dB at 4 Hz: loud around each quarter-period's peak, quiet around
    // each trough. Spec #1 "Dynamics response" (ADR 0005).
    const double levelDb = GENERATE (-36.0, -12.0);
    const double dynamicRange = GENERATE (-6.0, -12.0);
    CAPTURE (levelDb, dynamicRange);
    constexpr double seconds = 5.0, rate = 4.0, period = 1.0 / rate;
    const auto material = shapedNoise (seconds, levelDb, [] (double t) { return 6.0 * std::sin (2.0 * std::numbers::pi * rate * t); });
    const auto run = playAuto (material, dynamicRange);

    // After the first two seconds, each loud half-cycle moves at least 0.4 of the Dynamic Range, and
    // each quiet one comes back within a quarter of it: Auto Release limits how far it gets back.
    const double nearGain = 0.25 * std::abs (dynamicRange);
    for (double start = 2.0; start + period <= seconds; start += period)
    {
        CAPTURE (start);
        CHECK (mostMoved (run, start, start + 0.5 * period) >= 0.4 * std::abs (dynamicRange));
        CHECK (leastMoved (run, start + 0.5 * period, start + period) <= nearGain);
    }
}

TEST_CASE ("Auto Threshold learns a swell slowly, so a sustained swell moves the Band")
{
    // Steady noise for three seconds, then 6 dB louder for one.
    const auto material = shapedNoise (4.0, -24.0, [] (double t) { return t >= 3.0 ? 6.0 : 0.0; });
    const auto run = playAuto (material, -6.0);
    CHECK (std::abs (run.liveGain[blockAt (4.0) - 1]) >= 0.35 * 6.0);
}

TEST_CASE ("Auto Threshold holds the Band at Gain until it has heard the region for a moment")
{
    // Quiet noise, Dynamics Bypassed until 0.5 s; from 0.55 s a tone 20 dB louder stands out of it,
    // which would move the Band at once if Auto Threshold had already learned the noise.
    auto material = shapedNoise (1.5, -36.0, [] (double) { return 0.0; });
    for (size_t n = static_cast<size_t> (0.55 * sampleRate); n < material.size(); ++n)
        material[n] += sine (1000.0, -16.0, static_cast<int> (n));
    const auto run = playAuto (material, -6.0, [] (double seconds, Settings& s) { s.bands[0].dynamicsBypass = seconds < 0.5; });
    CHECK (mostMoved (run, 0.0, 0.7) == 0.0);
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

namespace
{
// A tone at frequency, levelDb loud, the same on every channel.
std::function<double (int, int)> tone (double frequency, double levelDb)
{
    return [=] (int, int n) { return sine (frequency, levelDb, n); };
}

const std::function<double (int, int)> silence = [] (int, int) { return 0.0; };

BandSettings externalBell()
{
    auto band = dynamicBell (0.0, -9.0, -30.0);
    band.detectionSource = DetectionSource::External;
    return band;
}

Run playWithSidechain (int numChannels, const BandSettings& band, const std::function<double (int, int)>& main, const Sidechain& sidechain)
{
    return play (numChannels, 1.0, withBand (band), main, {}, 64, &sidechain);
}
} // namespace

TEST_CASE ("An External Dynamic Band reacts to the Sidechain, not the main input")
{
    const auto band = externalBell();
    // Loud main input, silent Sidechain: no movement.
    CHECK (last (playWithSidechain (2, band, tone (1000.0, -6.0), { 2, silence })) == 0.0);
    // Silent main input, loud Sidechain in the Band's region: the full Dynamic Range.
    CHECK_THAT (last (playWithSidechain (2, band, silence, { 2, tone (1000.0, -6.0) })), WithinAbs (-9.0, 0.05));

    // An Internal Band ignores the Sidechain.
    auto internal = band;
    internal.detectionSource = DetectionSource::Internal;
    CHECK (last (playWithSidechain (2, internal, silence, { 2, tone (1000.0, -6.0) })) == 0.0);
}

TEST_CASE ("With a stereo Sidechain, a Mid or Side External Band listens to the Sidechain's Mid or Side")
{
    const auto placement = GENERATE (StereoPlacement::Mid, StereoPlacement::Side);
    CAPTURE (static_cast<int> (placement));
    auto band = externalBell();
    band.placement = placement;
    const auto toneIn = [] (StereoPlacement part) {
        return [part] (int ch, int n) { return (ch == 1 && part == StereoPlacement::Side ? -1.0 : 1.0) * sine (1000.0, -6.0, n); };
    };
    const auto other = placement == StereoPlacement::Mid ? StereoPlacement::Side : StereoPlacement::Mid;

    CHECK (last (playWithSidechain (2, band, silence, { 2, toneIn (other) })) == 0.0);
    CHECK_THAT (last (playWithSidechain (2, band, silence, { 2, toneIn (placement) })), WithinAbs (-9.0, 0.05));
}

TEST_CASE ("With a mono Sidechain, an External Band moves with it whatever its Stereo Placement, Side included")
{
    const auto placement = GENERATE (StereoPlacement::Stereo, StereoPlacement::Left, StereoPlacement::Right, StereoPlacement::Mid,
                                     StereoPlacement::Side);
    CAPTURE (static_cast<int> (placement));
    // A kick: a 60 Hz thump on the Sidechain, ducking a Low Shelf on the bass.
    auto band = externalBell();
    band.shape = Shape::LowShelf;
    band.frequency = 150.0;
    band.placement = placement;
    const auto run = playWithSidechain (2, band, tone (80.0, -20.0), { 1, tone (60.0, -6.0) });
    CHECK_THAT (last (run), WithinAbs (-9.0, 0.05));
}

TEST_CASE ("With no Sidechain connected, an External Band doesn't move")
{
    const auto numChannels = GENERATE (1, 2);
    CAPTURE (numChannels);
    const auto run = play (numChannels, 1.0, withBand (externalBell()), tone (1000.0, -3.0));
    for (double g : run.liveGain)
        REQUIRE (g == 0.0);

    // Nor with Auto Threshold, which has nothing to hear.
    auto automatic = externalBell();
    automatic.thresholdAuto = true;
    for (double g : play (numChannels, 1.0, withBand (automatic), tone (1000.0, -3.0)).liveGain)
        REQUIRE (g == 0.0);
}

TEST_CASE ("An External Band moved by the Sidechain returns to Gain when the Sidechain is disconnected")
{
    const auto band = externalBell();
    Engine engine;
    engine.prepare (sampleRate, 64, 2);
    engine.setSettings (withBand (band));
    std::vector<float> left (64), right (64), sidechainSamples (64);
    float* main[] = { left.data(), right.data() };
    const float* sidechainChannelPointers[] = { sidechainSamples.data() };
    const ConstAudioBlock sidechainBlock { sidechainChannelPointers, 1, 64 };
    for (int b = 0, n = 0; b < 750; ++b)
    {
        std::fill (left.begin(), left.end(), 0.0f);
        std::fill (right.begin(), right.end(), 0.0f);
        for (int i = 0; i < 64; ++i, ++n)
            sidechainSamples[static_cast<size_t> (i)] = static_cast<float> (sine (1000.0, -6.0, n));
        engine.process ({ main, 2, 64 }, &sidechainBlock);
    }
    CHECK_THAT (engine.liveGainDb (1), WithinAbs (-9.0, 0.05));
    for (int b = 0; b < 3000; ++b)
        engine.process ({ main, 2, 64 });
    CHECK_THAT (engine.liveGainDb (1), WithinAbs (0.0, 0.01));
}

TEST_CASE ("A Free Detection Range limits detection to its low and high limits, wherever the Band is")
{
    // A Bell at 5 kHz that listens only between 50 and 200 Hz.
    auto band = dynamicBell (0.0, -9.0, -30.0);
    band.frequency = 5000.0;
    band.detectionRange = DetectionRange::Free;
    band.detectionLow = 50.0;
    band.detectionHigh = 200.0;
    const auto liveGainOn = [&] (double frequency) { return last (play (1, 1.0, withBand (band), tone (frequency, -6.0))); };
    CHECK_THAT (liveGainOn (100.0), WithinAbs (-9.0, 0.05));
    // Not at the Band's own Frequency, nor two octaves beyond either limit.
    CHECK (liveGainOn (5000.0) == 0.0);
    CHECK_THAT (liveGainOn (800.0), WithinAbs (0.0, 0.01));
    CHECK_THAT (liveGainOn (12.5), WithinAbs (0.0, 0.01));

    // Back to a Band Detection Range: the Band hears its own region again.
    band.detectionRange = DetectionRange::Band;
    CHECK_THAT (liveGainOn (5000.0), WithinAbs (-9.0, 0.05));
    CHECK (liveGainOn (100.0) == 0.0);
}

TEST_CASE ("A Free Detection Range applies to the Sidechain too")
{
    auto band = externalBell();
    band.detectionRange = DetectionRange::Free;
    band.detectionLow = 40.0;
    band.detectionHigh = 120.0;
    CHECK_THAT (last (playWithSidechain (2, band, silence, { 1, tone (60.0, -6.0) })), WithinAbs (-9.0, 0.05));
    CHECK (last (playWithSidechain (2, band, silence, { 1, tone (1000.0, -6.0) })) == 0.0);
}

namespace
{
// The level in dB of frequency in the last half second of samples, where a full-scale sine reads 0 dB.
double levelOf (const std::vector<float>& samples, double frequency)
{
    const size_t length = static_cast<size_t> (0.5 * sampleRate), from = samples.size() - length;
    double re = 0.0, im = 0.0;
    for (size_t i = from; i < samples.size(); ++i)
    {
        const double w = 2.0 * std::numbers::pi * frequency * static_cast<double> (i) / sampleRate;
        re += samples[i] * std::cos (w);
        im -= samples[i] * std::sin (w);
    }
    return 20.0 * std::log10 (2.0 * std::hypot (re, im) / static_cast<double> (length) + 1.0e-30);
}
} // namespace

TEST_CASE ("An External Band whose Sidechain drops out for exactly one run start rejoins on its last design, not a stale glide")
{
    // While the Band's Frequency glides, the Sidechain drops out from sample 24 (or 23) until 40, over
    // the start of the grid's third run at 32. Silence until then, so the Detection Range filter's
    // history is the same whether its last piece ended at position 8 (rejoining where it left off, a
    // run later) or 7 (rejoining visibly late): either way it missed a run's start and must play what
    // it last designed.
    const auto auditionAfterDropout = [] (int lastConnectedBlock) {
        auto band = externalBell();
        auto settings = withBand (band);
        settings.auditionSlot = 1;
        Engine engine;
        engine.prepare (sampleRate, 64, 1);
        std::vector<float> output, mainSamples (64), sidechainSamples (64);
        float* main[] = { mainSamples.data() };
        const float* sidechainChannels[] = { sidechainSamples.data() };
        int n = 0;
        const auto playBlock = [&] (int numSamples, bool connected) {
            std::fill (mainSamples.begin(), mainSamples.end(), 0.0f);
            for (int i = 0; i < numSamples; ++i)
                sidechainSamples[static_cast<size_t> (i)] = n + i < 40 ? 0.0f : static_cast<float> (sine (1000.0, -6.0, n + i));
            const ConstAudioBlock sidechain { sidechainChannels, 1, numSamples };
            engine.process ({ main, 1, numSamples }, connected ? &sidechain : nullptr);
            output.insert (output.end(), mainSamples.begin(), mainSamples.begin() + numSamples);
            n += numSamples;
        };
        engine.setSettings (settings);
        playBlock (16, true);
        settings.bands[0].frequency = 4000.0;
        engine.setSettings (settings);
        playBlock (lastConnectedBlock, true);
        playBlock (40 - n, false);
        for (int b = 0; b < 8; ++b)
            playBlock (16, true);
        return output;
    };
    const auto rejoinedWhereItLeftOff = auditionAfterDropout (8), rejoinedLate = auditionAfterDropout (7);
    REQUIRE (rejoinedWhereItLeftOff.size() == rejoinedLate.size());
    CHECK (std::any_of (rejoinedLate.begin(), rejoinedLate.end(), [] (float x) { return x != 0.0f; }));
    CHECK (rejoinedWhereItLeftOff == rejoinedLate);
}

TEST_CASE ("Detection Audition plays what a Dynamic Band's detector hears instead of the output")
{
    // An External Band listening between 50 and 200 Hz; the Sidechain has a 100 Hz kick tone and a
    // 5 kHz hat, the main input a 1 kHz tone.
    auto band = externalBell();
    band.detectionRange = DetectionRange::Free;
    band.detectionLow = 50.0;
    band.detectionHigh = 200.0;
    auto settings = withBand (band);
    settings.auditionSlot = 1;
    const Sidechain sidechainSamples { 1, [] (int, int n) { return sine (100.0, -12.0, n) + sine (5000.0, -12.0, n); } };
    const auto run = play (2, 1.0, settings, tone (1000.0, -12.0), {}, 64, &sidechainSamples);
    for (const auto& channel : run.output)
    {
        CHECK_THAT (levelOf (channel, 100.0), WithinAbs (-12.0, 0.5));
        CHECK (levelOf (channel, 5000.0) < -60.0);
        CHECK (levelOf (channel, 1000.0) < -60.0);
    }

    // Letting go fades back to the EQ's output: after the fade, exactly what it plays unauditioned.
    const auto released = play (2, 1.0, settings, tone (1000.0, -12.0), [] (double seconds, Settings& s) { s.auditionSlot = seconds < 0.25 ? 1 : 0; },
                                64, &sidechainSamples);
    const auto unauditioned = play (2, 1.0, withBand (band), tone (1000.0, -12.0), {}, 64, &sidechainSamples);
    for (size_t ch = 0; ch < 2; ++ch)
        CHECK (std::equal (released.output[ch].begin() + static_cast<long> (0.5 * sampleRate), released.output[ch].end(),
                           unauditioned.output[ch].begin() + static_cast<long> (0.5 * sampleRate)));
}

TEST_CASE ("Detection Audition of a Stereo Band on a stereo source plays each channel's detection signal")
{
    auto band = dynamicBell (0.0, -9.0, -30.0);
    auto settings = withBand (band);
    settings.auditionSlot = 1;
    const auto run = play (2, 1.0, settings, [] (int ch, int n) { return sine (1000.0, ch == 0 ? -6.0 : -20.0, n); });
    CHECK_THAT (levelOf (run.output[0], 1000.0), WithinAbs (-6.0, 0.2));
    CHECK_THAT (levelOf (run.output[1], 1000.0), WithinAbs (-20.0, 0.2));
}

TEST_CASE ("Detection Audition on a mono track plays the mean of a stereo Sidechain's two detection channels")
{
    auto settings = withBand (externalBell());
    settings.auditionSlot = 1;
    const Sidechain stereo { 2, [] (int ch, int n) { return ch == 0 ? sine (1000.0, -6.0, n) : 0.0; } };
    const auto run = play (1, 1.0, settings, silence, {}, 64, &stereo);
    CHECK_THAT (levelOf (run.output[0], 1000.0), WithinAbs (-12.0, 0.2));
}

TEST_CASE ("Detection Audition needs a Band in use with a Shape that has dynamics")
{
    auto band = dynamicBell (0.0, -9.0, -30.0);
    band.shape = GENERATE (Shape::Notch, Shape::LowCut);
    const bool inUse = GENERATE (true, false);
    band.inUse = inUse;
    CAPTURE (static_cast<int> (band.shape), inUse);
    auto auditioned = withBand (band);
    auditioned.auditionSlot = 1;
    const auto signal = [] (int, int n) { return sine (300.0, -12.0, n) + sine (1000.0, -12.0, n); };
    CHECK (play (2, 0.5, auditioned, signal).output == play (2, 0.5, withBand (band), signal).output);
}

TEST_CASE ("With a mono Sidechain, an External Side Band ducks on every kick and recovers between them")
{
    // A kick at 120 bpm: a 60 Hz thump decaying over about 80 ms, on each beat.
    constexpr double beat = 0.5;
    const Sidechain kick { 1, [] (int, int n) {
                              const double t = std::fmod (n / sampleRate, beat);
                              return std::exp (-t / 0.08) * sine (60.0, -3.0, n);
                          } };
    auto band = externalBell();
    band.shape = Shape::LowShelf;
    band.frequency = 150.0;
    band.placement = StereoPlacement::Side;
    // The bass, wide in the stereo field.
    const auto bass = [] (int ch, int n) { return (ch == 0 ? 1.0 : -1.0) * sine (80.0, -20.0, n); };
    const auto run = play (2, 4.0, withBand (band), bass, {}, timingBlock, &kick);

    for (double hit = 1.0; hit + beat <= 4.0; hit += beat)
    {
        CAPTURE (hit);
        double deepest = 0.0;
        for (size_t b = blockAt (hit); b < blockAt (hit + 0.1); ++b)
            deepest = std::min (deepest, run.liveGain[b]);
        CHECK (deepest < -6.0);
        CHECK (run.liveGain[blockAt (hit + beat) - 1] > -3.0);
    }
}

TEST_CASE ("A metered Band reads a full-scale sine in its region at 0 dB, whatever its dynamics state")
{
    const int state = GENERATE (0, 1, 2); // active, Dynamics Bypass on, Dynamic Range 0
    const bool thresholdAuto = GENERATE (false, true);
    CAPTURE (state, thresholdAuto);
    auto band = dynamicBell (0.0, -9.0, -30.0);
    band.thresholdAuto = thresholdAuto;
    band.dynamicsBypass = state == 1;
    band.dynamicRange = state == 2 ? 0.0 : -9.0;
    auto settings = withBand (band);
    settings.meteredSlot = 1;
    const auto run = play (1, 0.5, settings, tone (1000.0, 0.0));
    CHECK_THAT (run.detectionLevel.back(), WithinAbs (0.0, 0.1));
}

TEST_CASE ("An active Dynamic Band meters what its detector compares with Threshold, as an idle one does")
{
    auto band = dynamicBell (0.0, -9.0, -30.0);
    auto settings = withBand (band);
    settings.meteredSlot = 1;
    // A level that falls by 2 dB every 0.1 s, from above Threshold to below it, so the Band moves.
    const auto falling = [] (int, int n) { return sine (1000.0, -20.0 - 2.0 * std::floor (n / (0.1 * sampleRate)), n); };
    const auto active = play (1, 1.0, settings, falling);
    settings.bands[0].dynamicsBypass = true;
    const auto idle = play (1, 1.0, settings, falling);
    CHECK (*std::min_element (active.liveGain.begin(), active.liveGain.end()) < -1.0); // it moved
    CHECK_THAT (active.detectionLevel[static_cast<size_t> (0.05 * sampleRate / 64)], WithinAbs (-20.0, 0.1));
    CHECK_THAT (active.detectionLevel.back(), WithinAbs (-38.0, 0.1));
    CHECK (active.detectionLevel == idle.detectionLevel);
}

TEST_CASE ("A Stereo Band meters the louder detection channel")
{
    auto settings = withBand (dynamicBell (0.0, -9.0, -30.0));
    settings.meteredSlot = 1;
    const auto run = play (2, 0.5, settings, [] (int ch, int n) { return sine (1000.0, ch == 0 ? -20.0 : -6.0, n); });
    CHECK_THAT (run.detectionLevel.back(), WithinAbs (-6.0, 0.1));
}

TEST_CASE ("A short burst between two reads is reported by the next read, and not by the one after")
{
    auto settings = withBand (dynamicBell (0.0, 0.0, -30.0));
    settings.meteredSlot = 1;
    // Reads every 0.1 s; a 20 ms burst in the second block.
    const auto run = play (1, 0.3, settings, burst (0.12, 0.02, -6.0), {}, 4800);
    REQUIRE (run.detectionLevel.size() == 3);
    CHECK (run.detectionLevel[0] == levelFloorDb);
    CHECK_THAT (run.detectionLevel[1], WithinAbs (-6.0, 0.2));
    CHECK (run.detectionLevel[2] < -40.0); // only what's left of its 5 ms smoothing
}

TEST_CASE ("The Detection Level follows the Detection Range and Detection Source")
{
    auto band = dynamicBell (0.0, 0.0, -30.0);
    const auto level = [&] (const std::function<double (int, int)>& main, const Sidechain* sidechain = nullptr) {
        auto settings = withBand (band);
        settings.meteredSlot = 1;
        return play (1, 0.5, settings, main, {}, 64, sidechain).detectionLevel.back();
    };

    SECTION ("Internal, Band: the Bell's region")
    {
        CHECK_THAT (level (tone (1000.0, -12.0)), WithinAbs (-12.0, 0.1));
        CHECK (level (tone (8000.0, -12.0)) < -30.0);
    }

    SECTION ("Free: between its limits, wherever the Band is")
    {
        band.detectionRange = DetectionRange::Free;
        band.detectionLow = 50.0;
        band.detectionHigh = 200.0;
        // A low tone ripples through the 5 ms smoothing, and the loudest level is the ripple's peak.
        CHECK_THAT (level (tone (100.0, -12.0)), WithinAbs (-12.0, 1.0));
        CHECK (level (tone (1000.0, -12.0)) < -30.0);
    }

    SECTION ("External: the Sidechain, not the main input")
    {
        band.detectionSource = DetectionSource::External;
        const Sidechain key { 1, tone (1000.0, -12.0) };
        const Sidechain quiet { 1, silence };
        CHECK_THAT (level (silence, &key), WithinAbs (-12.0, 0.1));
        CHECK (level (tone (1000.0, -6.0), &quiet) == levelFloorDb);
    }
}

TEST_CASE ("The Detection Level reads the floor when no Band with dynamics is metered, or it has nothing to listen to")
{
    auto band = dynamicBell (0.0, -9.0, -30.0);
    auto settings = withBand (band);
    settings.meteredSlot = 1;
    const auto loud = tone (1000.0, -6.0);
    const auto floorOf = [&] (int numChannels, const Settings& s) {
        for (double level : play (numChannels, 0.25, s, loud).detectionLevel)
            if (level != levelFloorDb)
                return false;
        return true;
    };

    SECTION ("no Band metered") { settings.meteredSlot = 0; CHECK (floorOf (1, settings)); }
    SECTION ("an unused slot") { settings.meteredSlot = 2; CHECK (floorOf (1, settings)); }
    SECTION ("a slot out of range") { settings.meteredSlot = 25; CHECK (floorOf (1, settings)); }
    SECTION ("a Shape without dynamics") { settings.bands[0].shape = Shape::Notch; CHECK (floorOf (1, settings)); }
    SECTION ("a Bypassed Band") { settings.bands[0].bypass = true; CHECK (floorOf (1, settings)); }
    SECTION ("External with no Sidechain") { settings.bands[0].detectionSource = DetectionSource::External; CHECK (floorOf (2, settings)); }
    SECTION ("a Side Band on mono") { settings.bands[0].placement = StereoPlacement::Side; CHECK (floorOf (1, settings)); }
}

TEST_CASE ("Metering never changes the output, even of a Band metered while idle that becomes active mid-render")
{
    const bool thresholdAuto = GENERATE (false, true);
    const int activation = GENERATE (0, 1); // Dynamics Bypass turned off, or Dynamic Range raised from 0
    // Idle from the start, or active first, so Auto Threshold has learned the region before it idles.
    const bool activeFirst = GENERATE (false, true);
    CAPTURE (thresholdAuto, activation, activeFirst);
    auto band = dynamicBell (3.0, -9.0, -30.0);
    band.thresholdAuto = thresholdAuto;
    const auto idle = [&] (Settings& s) {
        if (activation == 0)
            s.bands[0].dynamicsBypass = true;
        else
            s.bands[0].dynamicRange = 0.0;
    };
    // Louder while idle than before or after, so a detector that learned while metered would start
    // in a different place.
    const auto signal = [=] (int, int n) {
        const double seconds = n / sampleRate;
        const double db = activeFirst ? (seconds < 0.3 ? -24.0 : seconds < 0.7 ? -3.0 : -12.0) : (seconds < 0.6 ? -3.0 : -24.0);
        return sine (1000.0, db, n) + sine (1100.0, -30.0, n);
    };
    const auto change = [&] (int meteredSlot) {
        return [=] (double seconds, Settings& s) {
            s.bands[0] = band;
            if (seconds < 0.7 && (! activeFirst || seconds >= 0.3))
                idle (s);
            s.meteredSlot = meteredSlot;
        };
    };
    const auto unmetered = play (2, 1.5, withBand (band), signal, change (0));
    const auto metered = play (2, 1.5, withBand (band), signal, change (1));
    CHECK (metered.detectionLevel[static_cast<size_t> (0.5 * sampleRate / 64)] > -10.0); // it was metered while idle
    CHECK (metered.output == unmetered.output);
}

TEST_CASE ("Metering another Band forgets the last one's level, even before it is read")
{
    auto settings = withBand (dynamicBell (0.0, -9.0, -30.0));
    settings.bands[1] = dynamicBell (0.0, -9.0, -30.0);
    settings.bands[1].frequency = 8000.0;
    settings.meteredSlot = 1;
    Engine engine;
    engine.prepare (sampleRate, 512, 1);
    std::vector<float> block (512);
    float* channels[] = { block.data() };
    const auto playTone = [&] (int blocks) {
        for (int b = 0, n = 0; b < blocks; ++b)
        {
            for (int i = 0; i < 512; ++i, ++n)
                block[static_cast<size_t> (i)] = static_cast<float> (sine (1000.0, -6.0, n));
            engine.setSettings (settings);
            engine.process ({ channels, 1, 512 });
        }
    };
    playTone (20);
    settings.meteredSlot = 2; // a Bell at 8 kHz hears little of a 1 kHz tone
    playTone (1);
    CHECK (engine.readDetectionLevel() < -20.0);
}

TEST_CASE ("A Dynamic Band re-prepared mid-movement starts at its Gain, not where it was")
{
    const auto settings = withBand (dynamicBell (3.0, -9.0, -30.0));
    Engine engine;
    engine.prepare (sampleRate, 512, 1);
    engine.setSettings (settings);
    std::vector<float> block (512);
    float* channels[] = { block.data() };
    for (int b = 0, n = 0; b < 40; ++b)
    {
        for (int i = 0; i < 512; ++i, ++n)
            block[static_cast<size_t> (i)] = static_cast<float> (sine (1000.0, -3.0, n));
        engine.process ({ channels, 1, 512 });
    }
    REQUIRE (engine.liveGainDb (1) < 0.0); // moving well below its Gain of 3 dB

    // Re-prepared, its first run plays at Gain: the previous session's movement is gone.
    engine.prepare (sampleRate, 512, 1);
    engine.setSettings (settings);
    std::fill (block.begin(), block.end(), 0.0f);
    engine.process ({ channels, 1, timingBlock }); // one run of the grid
    CHECK_THAT (engine.liveGainDb (1), WithinAbs (3.0, 1.0e-9));
}
