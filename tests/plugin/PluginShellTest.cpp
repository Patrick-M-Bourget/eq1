#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>

using Catch::Matchers::WithinAbs;

namespace
{

constexpr double sampleRate = 48000.0;
constexpr int blockSize = 512;

void setParameter (juce::AudioProcessor& processor, const juce::String& id, float value)
{
    for (auto* parameter : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter); ranged != nullptr && ranged->getParameterID() == id)
            return ranged->setValueNotifyingHost (ranged->convertTo0to1 (value));
    FAIL ("No parameter " << id);
}

// Level change in dB of a steady sine through the processor.
double sineGainDb (juce::AudioProcessor& processor, double frequency)
{
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    double inputPower = 0.0, outputPower = 0.0;
    int n = 0;
    for (int block = 0; block < 64; ++block)
    {
        for (int i = 0; i < blockSize; ++i, ++n)
        {
            const auto s = static_cast<float> (std::sin (2.0 * std::numbers::pi * frequency * n / sampleRate));
            buffer.setSample (0, i, s);
            buffer.setSample (1, i, s);
            if (block >= 32)
                inputPower += s * s;
        }
        processor.processBlock (buffer, midi);
        if (block >= 32)
            for (int i = 0; i < blockSize; ++i)
                outputPower += buffer.getSample (0, i) * buffer.getSample (0, i);
    }
    return 10.0 * std::log10 (outputPower / inputPower);
}

} // namespace

TEST_CASE ("Plugin reports zero latency")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    CHECK (processor.getLatencySamples() == 0);
}

TEST_CASE ("Moving the Bell's Gain changes the sound at its Frequency")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    setParameter (processor, "band1_frequency", 1000.0f);
    setParameter (processor, "band1_q", 1.0f);

    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (0.0, 0.05));

    setParameter (processor, "band1_gain", 12.0f);
    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (12.0, 0.1));

    setParameter (processor, "band1_gain", -9.0f);
    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (-9.0, 0.1));
}
