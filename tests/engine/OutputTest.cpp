#include "Measure.h"

#include "eq1/Response.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <functional>
#include <limits>
#include <numbers>
#include <random>
#include <vector>

using namespace eq1;
using Catch::Matchers::WithinAbs;

namespace
{
constexpr double sampleRate = 48000.0;

double bellGainDb (const Settings& settings)
{
    Engine engine;
    engine.prepare (sampleRate, 4096, 1);
    engine.setSettings (settings);
    return test::magnitudeDb (test::impulseResponse (engine), 1000.0, sampleRate);
}

Settings scaled (Settings settings, double gainScale)
{
    settings.gainScale = gainScale;
    return settings;
}

// The Live Gain slot 1 settles at, playing a 1 kHz tone at -6 dB for a second.
double settledLiveGain (const Settings& settings)
{
    Engine engine;
    constexpr int blockSize = 64;
    engine.prepare (sampleRate, blockSize, 1);
    engine.setSettings (settings);
    std::vector<float> block (blockSize);
    float* channels[] = { block.data() };
    for (int n = 0; n < static_cast<int> (sampleRate);)
    {
        for (auto& s : block)
            s = static_cast<float> (0.5 * std::sin (2.0 * std::numbers::pi * 1000.0 * n++ / sampleRate));
        engine.process ({ channels, 1, blockSize });
    }
    return engine.liveGainDb (1);
}

BandSettings dynamicBell (double gain, double dynamicRange)
{
    auto band = test::bellBand (1000.0, gain, 1.0);
    band.dynamicRange = dynamicRange;
    band.thresholdAuto = false;
    band.threshold = -40.0;
    return band;
}
} // namespace

TEST_CASE ("Gain Scale scales a Band's Gain in dB, and 100% leaves it alone")
{
    const auto settings = test::bell (1000.0, 12.0, 1.0);
    CHECK_THAT (bellGainDb (scaled (settings, 1.0)), WithinAbs (12.0, 0.01));
    CHECK_THAT (bellGainDb (scaled (settings, 0.5)), WithinAbs (6.0, 0.01));
    CHECK_THAT (bellGainDb (scaled (settings, 2.0)), WithinAbs (24.0, 0.01));
    CHECK_THAT (bellGainDb (scaled (settings, 0.0)), WithinAbs (0.0, 0.01));
    CHECK_THAT (bellGainDb (scaled (test::bell (1000.0, -9.0, 1.0), 2.0)), WithinAbs (-18.0, 0.01));
}

TEST_CASE ("Gain Scale above 100% holds Live Gain to +/-30 dB")
{
    CHECK_THAT (bellGainDb (scaled (test::bell (1000.0, 20.0, 1.0), 2.0)), WithinAbs (30.0, 0.01));
    CHECK_THAT (bellGainDb (scaled (test::bell (1000.0, -25.0, 1.0), 2.0)), WithinAbs (-30.0, 0.01));
}

TEST_CASE ("Gain Scale scales a Dynamic Band's Dynamic Range")
{
    Settings settings;
    settings.bands[0] = dynamicBell (0.0, -8.0);
    CHECK_THAT (settledLiveGain (settings), WithinAbs (-8.0, 0.05));
    CHECK_THAT (settledLiveGain (scaled (settings, 0.5)), WithinAbs (-4.0, 0.05));
    CHECK_THAT (settledLiveGain (scaled (settings, 2.0)), WithinAbs (-16.0, 0.05));

    settings.bands[0] = dynamicBell (6.0, -8.0);
    CHECK_THAT (settledLiveGain (scaled (settings, 2.0)), WithinAbs (-4.0, 0.05));
    settings.bands[0] = dynamicBell (-20.0, -15.0);
    CHECK_THAT (settledLiveGain (scaled (settings, 2.0)), WithinAbs (-30.0, 0.05));
}

TEST_CASE ("Gain Scale has no effect on Cut, Notch, Band Pass or All Pass")
{
    const auto shape = GENERATE (Shape::LowCut, Shape::HighCut, Shape::Notch, Shape::BandPass, Shape::AllPass);
    CAPTURE (static_cast<int> (shape));
    Settings settings = test::bell (1000.0, 12.0, 2.0);
    settings.bands[0].shape = shape;
    for (double frequency : { 100.0, 700.0, 1000.0, 1500.0, 8000.0 })
    {
        CAPTURE (frequency);
        Engine full, half, doubled;
        for (auto* engine : { &full, &half, &doubled })
            engine->prepare (sampleRate, 4096, 1);
        full.setSettings (settings);
        half.setSettings (scaled (settings, 0.5));
        doubled.setSettings (scaled (settings, 2.0));
        const double reference = test::magnitudeDb (test::impulseResponse (full), frequency, sampleRate);
        CHECK_THAT (test::magnitudeDb (test::impulseResponse (half), frequency, sampleRate), WithinAbs (reference, 1.0e-6));
        CHECK_THAT (test::magnitudeDb (test::impulseResponse (doubled), frequency, sampleRate), WithinAbs (reference, 1.0e-6));
    }
}

TEST_CASE ("The display's response follows Gain Scale, held to +/-30 dB like the Engine")
{
    auto settings = test::bell (1000.0, 12.0, 1.0);
    settings.bands[1] = test::bellBand (4000.0, 20.0, 4.0);
    for (double gainScale : { 0.0, 0.5, 1.0, 2.0 })
    {
        CAPTURE (gainScale);
        const auto scaledSettings = scaled (settings, gainScale);
        Engine engine;
        engine.prepare (sampleRate, 4096, 1);
        engine.setSettings (scaledSettings);
        const auto response = test::impulseResponse (engine);
        for (double frequency : { 1000.0, 4000.0 })
            CHECK_THAT (responseDb (scaledSettings, frequency, sampleRate), WithinAbs (test::magnitudeDb (response, frequency, sampleRate), 0.01));
    }
    CHECK_THAT (bandResponseDb (scaledByGainScale (settings.bands[1], 2.0), 4000.0, sampleRate), WithinAbs (30.0, 0.01));
}

namespace
{
constexpr int outputLength = 4096;

// Two channels of different noise, so the signal has both Mid and Side.
std::vector<std::vector<float>> stereoNoise()
{
    std::mt19937 random (11);
    std::uniform_real_distribution<float> unit (-0.5f, 0.5f);
    std::vector<std::vector<float>> signal (2, std::vector<float> (outputLength));
    for (auto& channel : signal)
        for (auto& s : channel)
            s = unit (random);
    return signal;
}

// What the Engine makes of signal with settings, from the start (settings apply at once after prepare).
std::vector<std::vector<float>> processed (const Settings& settings, std::vector<std::vector<float>> signal)
{
    Engine engine;
    engine.prepare (sampleRate, outputLength, static_cast<int> (signal.size()));
    engine.setSettings (settings);
    std::vector<float*> channels;
    for (auto& channel : signal)
        channels.push_back (channel.data());
    engine.process ({ channels.data(), static_cast<int> (channels.size()), outputLength });
    return signal;
}

// Checks output channel ch is left * l + right * r of the input, sample by sample.
void checkMix (const std::vector<std::vector<float>>& input, const std::vector<std::vector<float>>& output, size_t ch, double l, double r)
{
    CAPTURE (ch, l, r);
    for (size_t i = 0; i < input[0].size(); ++i)
    {
        const double right = input.size() > 1 ? input[1][i] : 0.0;
        REQUIRE_THAT (output[ch][i], WithinAbs (l * input[0][i] + r * right, 1.0e-6));
    }
}

Settings output (const std::function<void (Settings&)>& edit)
{
    Settings settings;
    edit (settings);
    return settings;
}
} // namespace

TEST_CASE ("Output Gain sets the level of the whole output, from silence to +36 dB")
{
    const auto input = stereoNoise();
    for (double db : { -12.0, 0.0, 6.0, 36.0 })
    {
        const double gain = std::pow (10.0, db / 20.0);
        const auto out = processed (output ([&] (Settings& s) { s.outputGainDb = db; }), input);
        checkMix (input, out, 0, gain, 0.0);
        checkMix (input, out, 1, 0.0, gain);
    }
    const auto silent = processed (output ([] (Settings& s) { s.outputGainDb = -std::numeric_limits<double>::infinity(); }), input);
    checkMix (input, silent, 0, 0.0, 0.0);
    checkMix (input, silent, 1, 0.0, 0.0);
}

TEST_CASE ("Phase Invert flips the polarity of every channel")
{
    const auto input = stereoNoise();
    const auto out = processed (output ([] (Settings& s) { s.phaseInvert = true; }), input);
    checkMix (input, out, 0, -1.0, 0.0);
    checkMix (input, out, 1, 0.0, -1.0);

    const std::vector<std::vector<float>> mono { input[0] };
    checkMix (mono, processed (output ([] (Settings& s) { s.phaseInvert = true; }), mono), 0, -1.0, 0.0);
}

TEST_CASE ("L/R Pan balances Left against Right: the centre leaves both alone, the other side turns down")
{
    const auto input = stereoNoise();
    const auto panned = [&] (double pan) { return processed (output ([=] (Settings& s) { s.outputPan = pan; }), input); };

    auto out = panned (0.0);
    checkMix (input, out, 0, 1.0, 0.0);
    checkMix (input, out, 1, 0.0, 1.0);
    out = panned (-0.5);
    checkMix (input, out, 0, 1.0, 0.0);
    checkMix (input, out, 1, 0.0, 0.5);
    out = panned (1.0);
    checkMix (input, out, 0, 0.0, 0.0);
    checkMix (input, out, 1, 0.0, 1.0);
}

TEST_CASE ("M/S Pan Mode balances Mid against Side: left keeps the Mid, right the Side")
{
    const auto input = stereoNoise();
    const auto panned = [&] (double pan) {
        return processed (output ([=] (Settings& s) {
                              s.panMode = PanMode::MidSide;
                              s.outputPan = pan;
                          }),
                          input);
    };

    auto out = panned (0.0);
    checkMix (input, out, 0, 1.0, 0.0);
    checkMix (input, out, 1, 0.0, 1.0);
    // All Mid: both channels carry (L + R) / 2.
    out = panned (-1.0);
    checkMix (input, out, 0, 0.5, 0.5);
    checkMix (input, out, 1, 0.5, 0.5);
    // All Side: left carries (L - R) / 2, right its opposite.
    out = panned (1.0);
    checkMix (input, out, 0, 0.5, -0.5);
    checkMix (input, out, 1, -0.5, 0.5);
    // Mid at half: L = M / 2 + S, R = M / 2 - S.
    out = panned (0.5);
    checkMix (input, out, 0, 0.75, -0.25);
    checkMix (input, out, 1, -0.25, 0.75);
}

TEST_CASE ("On a mono track, Pan and Pan Mode have no effect")
{
    const std::vector<std::vector<float>> mono { stereoNoise()[0] };
    for (auto mode : { PanMode::LeftRight, PanMode::MidSide })
        for (double pan : { -1.0, 0.5, 1.0 })
        {
            CAPTURE (static_cast<int> (mode), pan);
            checkMix (mono, processed (output ([=] (Settings& s) {
                                           s.panMode = mode;
                                           s.outputPan = pan;
                                       }),
                                       mono),
                      0, 1.0, 0.0);
        }
}

TEST_CASE ("Output Gain, Pan and Phase Invert come after the Bands")
{
    const auto input = stereoNoise();
    auto settings = test::bell (1000.0, 9.0, 1.0);
    const auto bandOnly = processed (settings, input);
    settings.outputGainDb = 6.0;
    settings.outputPan = 0.5;
    settings.phaseInvert = true;
    const double gain = std::pow (10.0, 6.0 / 20.0);
    const auto out = processed (settings, input);
    checkMix (bandOnly, out, 0, -0.5 * gain, 0.0);
    checkMix (bandOnly, out, 1, 0.0, -gain);
}

namespace
{
constexpr double settleSeconds = 1.0, probeSeconds = 9.0;
// At 96 kHz the Brickwall High Cut at 20 kHz is flat to within 1 dB up to 19 kHz; at 48 kHz it rolls
// off early, from about 10 kHz, as High Cuts near Nyquist do (docs/dsp/filter-design.md).
constexpr double probeRate = 96000.0;

// Pink noise from 20 Hz to 20 kHz, at probeRate: white noise through Paul Kellet's pink filter
// (within 0.05 dB of -3 dB/oct from 20 Hz to 20 kHz at 96 kHz), then Butterworth Brickwall Cuts at
// 20 Hz and 20 kHz.
const std::vector<float>& pinkProbe()
{
    static const std::vector<float> probe = [] {
        std::vector<float> samples (static_cast<size_t> (probeSeconds * probeRate));
        std::mt19937 random (11);
        std::normal_distribution<double> white (0.0, 0.05);
        double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
        for (auto& s : samples)
        {
            const double w = white (random);
            b0 = 0.99886 * b0 + w * 0.0555179;
            b1 = 0.99332 * b1 + w * 0.0750759;
            b2 = 0.96900 * b2 + w * 0.1538520;
            b3 = 0.86650 * b3 + w * 0.3104856;
            b4 = 0.55000 * b4 + w * 0.5329522;
            b5 = -0.7616 * b5 - w * 0.0168980;
            s = static_cast<float> (b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362);
            b6 = w * 0.115926;
        }
        Settings band;
        band.bands[0] = { .inUse = true, .shape = Shape::LowCut, .frequency = 20.0, .q = 0.7071, .brickwall = true };
        band.bands[1] = { .inUse = true, .shape = Shape::HighCut, .frequency = 20000.0, .q = 0.7071, .brickwall = true };
        Engine engine;
        engine.prepare (probeRate, static_cast<int> (samples.size()), 1);
        engine.setSettings (band);
        float* channels[] = { samples.data() };
        engine.process ({ channels, 1, static_cast<int> (samples.size()) });
        return samples;
    }();
    return probe;
}

// Mean power in dB of signal after the settling time.
double powerDb (const std::vector<float>& signal)
{
    double sum = 0.0;
    const auto start = static_cast<size_t> (settleSeconds * probeRate);
    for (size_t i = start; i < signal.size(); ++i)
        sum += static_cast<double> (signal[i]) * signal[i];
    return 10.0 * std::log10 (sum / static_cast<double> (signal.size() - start));
}

// The level change, in dB, of the pink probe through an Engine with settings, in blocks of 512.
double pinkGainDb (const Settings& settings)
{
    auto signal = pinkProbe();
    Engine engine;
    constexpr int blockSize = 512;
    engine.prepare (probeRate, blockSize, 1);
    engine.setSettings (settings);
    for (size_t start = 0; start + blockSize <= signal.size(); start += blockSize)
    {
        float* channels[] = { signal.data() + start };
        engine.process ({ channels, 1, blockSize });
    }
    return powerDb (signal) - powerDb (pinkProbe());
}

BandSettings band (Shape shape, double frequency, double gain, double q, double slope = 12.0)
{
    return { .inUse = true, .shape = shape, .frequency = frequency, .gain = gain, .q = q, .slope = slope };
}

// Reference settings for Auto Gain: single Bands of each kind with a Gain, wide and narrow, cuts,
// and several Bands together.
Settings referenceSettings (int which)
{
    Settings s;
    switch (which)
    {
        case 0: s.bands[0] = band (Shape::Bell, 1000.0, 12.0, 1.0); break;
        case 1: s.bands[0] = band (Shape::Bell, 3000.0, 20.0, 20.0); break;
        case 2: s.bands[0] = band (Shape::Bell, 250.0, -18.0, 0.5); break;
        case 3: s.bands[0] = band (Shape::LowShelf, 200.0, 6.0, 0.7); break;
        case 4: s.bands[0] = band (Shape::HighShelf, 6000.0, -9.0, 0.7, 24.0); break;
        case 5: s.bands[0] = band (Shape::TiltShelf, 1000.0, 6.0, 0.7); break;
        case 6: s.bands[0] = band (Shape::FlatTilt, 1000.0, -4.5, 0.7); break;
        case 7:
            s.bands[0] = band (Shape::HighCut, 5000.0, 0.0, 0.7, 24.0);
            s.bands[1] = band (Shape::Bell, 300.0, -9.0, 2.0);
            break;
        case 8:
            s.bands[0] = band (Shape::LowCut, 80.0, 0.0, 0.7, 48.0);
            s.bands[1] = band (Shape::LowShelf, 120.0, 4.0, 0.7);
            s.bands[2] = band (Shape::Bell, 500.0, -6.0, 3.0);
            s.bands[3] = band (Shape::Bell, 2500.0, 8.0, 1.5);
            s.bands[4] = band (Shape::Notch, 6000.0, 0.0, 8.0);
            s.bands[5] = band (Shape::HighShelf, 10000.0, 5.0, 0.7);
            break;
        case 9:
            s.bands[0] = band (Shape::LowShelf, 150.0, 9.0, 0.7);
            s.bands[1] = band (Shape::Bell, 2000.0, 10.0, 1.0);
            s.gainScale = 2.0;
            break;
    }
    return s;
}
} // namespace

TEST_CASE ("The Auto Gain estimate is within 0.5 dB of what pink noise from 20 Hz to 20 kHz measures", "[response]")
{
    const int which = GENERATE (Catch::Generators::range (0, 10));
    CAPTURE (which);
    const auto settings = referenceSettings (which);
    CHECK_THAT (autoGainDb (settings, probeRate), WithinAbs (-pinkGainDb (settings), 0.5));
}

TEST_CASE ("With Auto Gain on, pink noise comes out at the level it went in", "[response]")
{
    const int which = GENERATE (0, 1, 4, 8, 9);
    CAPTURE (which);
    auto settings = referenceSettings (which);
    settings.autoGain = true;
    CHECK_THAT (pinkGainDb (settings), WithinAbs (0.0, 0.5));
}

TEST_CASE ("With no Band in use, or only Bands without effect on level, Auto Gain is 0 dB")
{
    CHECK (autoGainDb (Settings {}, sampleRate) == 0.0);
    auto settings = test::bell (1000.0, 12.0, 1.0);
    settings.bands[0].bypass = true;
    CHECK (autoGainDb (settings, sampleRate) == 0.0);
    settings.bands[0] = band (Shape::AllPass, 1000.0, 0.0, 1.0);
    CHECK_THAT (autoGainDb (settings, sampleRate), WithinAbs (0.0, 1.0e-6));
}

TEST_CASE ("Auto Gain is estimated from the settings and doesn't follow dynamic movement")
{
    Settings dynamic;
    dynamic.bands[0] = dynamicBell (-6.0, -12.0);
    Settings still = dynamic;
    still.bands[0].dynamicRange = 0.0;
    const double estimate = autoGainDb (still, sampleRate);
    REQUIRE (estimate > 0.5);
    CHECK (autoGainDb (dynamic, sampleRate) == estimate);

    // A burst drives the Band down by its Dynamic Range and lets it go; with Auto Gain on, the output
    // is the output with it off, raised by the static estimate throughout.
    const auto play = [&] (bool autoGain) {
        auto settings = dynamic;
        settings.autoGain = autoGain;
        Engine engine;
        constexpr int blockSize = 64;
        engine.prepare (sampleRate, blockSize, 1);
        engine.setSettings (settings);
        std::vector<float> out, block (blockSize);
        float* channels[] = { block.data() };
        for (int n = 0; n < static_cast<int> (2.0 * sampleRate);)
        {
            for (auto& s : block)
            {
                const double level = n > 0.5 * sampleRate && n < 1.0 * sampleRate ? 0.5 : 0.005;
                s = static_cast<float> (level * std::sin (2.0 * std::numbers::pi * 1000.0 * n++ / sampleRate));
            }
            engine.process ({ channels, 1, blockSize });
            out.insert (out.end(), block.begin(), block.end());
        }
        return out;
    };
    const auto off = play (false), on = play (true);
    const double gain = std::pow (10.0, estimate / 20.0);
    for (size_t i = static_cast<size_t> (0.2 * sampleRate); i < off.size(); ++i)
        REQUIRE_THAT (on[i], WithinAbs (gain * off[i], 1.0e-5));
}

TEST_CASE ("Global Bypass passes the input through, whatever the Bands and output controls")
{
    const auto input = stereoNoise();
    auto settings = test::bell (1000.0, 12.0, 1.0);
    settings.gainScale = 2.0;
    settings.autoGain = true;
    settings.outputGainDb = 9.0;
    settings.outputPan = -0.5;
    settings.panMode = PanMode::MidSide;
    settings.phaseInvert = true;
    settings.globalBypass = true;
    const auto out = processed (settings, input);
    checkMix (input, out, 0, 1.0, 0.0);
    checkMix (input, out, 1, 0.0, 1.0);

    const std::vector<std::vector<float>> mono { input[0] };
    checkMix (mono, processed (settings, mono), 0, 1.0, 0.0);
}
