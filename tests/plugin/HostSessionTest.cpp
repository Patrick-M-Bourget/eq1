#include "PluginProcessor.h"

#include "../engine/Measure.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <numbers>
#include <random>

namespace
{

void setParameter (juce::AudioProcessor& processor, const juce::String& id, float value)
{
    for (auto* parameter : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter); ranged != nullptr && ranged->getParameterID() == id)
            return ranged->setValueNotifyingHost (ranged->convertTo0to1 (value));
    FAIL ("No parameter " << id);
}

constexpr double toneFrequency = 1000.0;

// A +12 dB Bell at the tone's Frequency, and a High Cut at the top of the range, which sits above
// Nyquist at the lower sample rates.
void setUpBands (juce::AudioProcessor& processor)
{
    setParameter (processor, "band1_in_use", 1.0f);
    setParameter (processor, "band1_frequency", static_cast<float> (toneFrequency));
    setParameter (processor, "band1_gain", 12.0f);
    setParameter (processor, "band2_in_use", 1.0f);
    setParameter (processor, "band2_shape", 4.0f); // High Cut
    setParameter (processor, "band2_frequency", 30000.0f);
}

// Plays seconds of the tone through the processor as a host does, in blocks of blockAt(n) samples
// for the nth block, and returns the first channel's output. The tone carries on from sample `from`.
std::vector<float> playTone (juce::AudioProcessor& processor, double sampleRate, double seconds, const std::function<int (int)>& blockAt,
                             long from = 0)
{
    const auto length = static_cast<int> (seconds * sampleRate);
    juce::AudioBuffer<float> buffer (processor.getTotalNumInputChannels(), 0);
    juce::MidiBuffer midi;
    std::vector<float> output;
    for (int start = 0, n = 0; start < length; ++n)
    {
        const int count = std::min (blockAt (n), length - start);
        buffer.setSize (buffer.getNumChannels(), count, false, false, true);
        buffer.clear();
        for (int i = 0; i < count; ++i)
        {
            const auto s = static_cast<float> (0.25 * std::sin (2.0 * std::numbers::pi * toneFrequency * static_cast<double> (from + start + i) / sampleRate));
            buffer.setSample (0, i, s);
            buffer.setSample (1, i, s);
        }
        processor.processBlock (buffer, midi);
        for (int i = 0; i < count; ++i)
            output.push_back (buffer.getSample (0, i));
        start += count;
    }
    return output;
}

double levelDb (const std::vector<float>& samples, size_t from, size_t to)
{
    double power = 0.0;
    for (size_t i = from; i < to; ++i)
        power += static_cast<double> (samples[i]) * samples[i];
    return 10.0 * std::log10 (power / static_cast<double> (to - from));
}

} // namespace

TEST_CASE ("Changing sample rate and block size mid-session keeps the Bands sounding as set, from the first block")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    setUpBands (processor);

    struct Session
    {
        double sampleRate;
        int blockSize;
    };
    // Every sample rate, up and down, with block sizes from a sample at a time to large ones.
    constexpr Session sessions[] = { { 48000.0, 512 },  { 192000.0, 4096 }, { 44100.0, 1 },   { 96000.0, 64 },
                                     { 176400.0, 33 },  { 88200.0, 256 },   { 44100.0, 2048 }, { 192000.0, 7 },
                                     { 48000.0, 128 } };
    // A sine at -12 dBFS, raised 12 dB by the Bell: 0 dB in RMS terms is a full-scale sine's -3 dB.
    const double expectedDb = 20.0 * std::log10 (0.25) - 3.0103 + 12.0;
    for (const auto& session : sessions)
    {
        processor.releaseResources();
        processor.prepareToPlay (session.sampleRate, session.blockSize);
        const auto output = playTone (processor, session.sampleRate, 0.25, [&] (int) { return session.blockSize; });

        CAPTURE (session.sampleRate, session.blockSize);
        REQUIRE (std::all_of (output.begin(), output.end(), [] (float s) { return std::isfinite (s); }));
        // The Bell is in full effect from the first block: over the first 20 ms (two periods of the tone
        // in, once the tone's own abrupt start has rung out) as over the rest. A fade in would take about 50 ms.
        const auto ms = [&] (double milliseconds) { return static_cast<size_t> (milliseconds * session.sampleRate / 1000.0); };
        CHECK (std::abs (levelDb (output, ms (2.0), ms (20.0)) - expectedDb) < 0.2);
        REQUIRE (std::abs (levelDb (output, ms (20.0), output.size()) - expectedDb) < 0.1);
        // No blow-up as the filters start afresh: the tone raised 12 dB is 4 times its 0.25 peak.
        constexpr float raisedPeak = 0.25f * 4.0f;
        float peak = 0.0f;
        for (float s : output)
            peak = std::max (peak, std::abs (s));
        REQUIRE (peak < 1.5f * raisedPeak);
    }
}

TEST_CASE ("Blocks changing size from call to call within a session do not click")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    constexpr double sampleRate = 48000.0;
    processor.prepareToPlay (sampleRate, 512);
    setUpBands (processor);

    // Some hosts send whatever is left before a loop point or automation change: any size up to the maximum.
    std::mt19937 random (11);
    std::uniform_int_distribution<int> anySize (1, 512);
    const auto output = playTone (processor, sampleRate, 1.0, [&] (int) { return anySize (random); });

    REQUIRE (eq1::test::discontinuity (output, sampleRate, toneFrequency, static_cast<size_t> (0.1 * sampleRate)) < 2.0);
}
