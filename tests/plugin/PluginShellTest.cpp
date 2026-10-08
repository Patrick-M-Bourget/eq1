#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <map>
#include <utility>
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

    setParameter (processor, "band1_in_use", 1.0f);
    setParameter (processor, "band1_frequency", 1000.0f);
    setParameter (processor, "band1_q", 1.0f);

    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (0.0, 0.05));

    setParameter (processor, "band1_gain", 12.0f);
    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (12.0, 0.1));

    setParameter (processor, "band1_gain", -9.0f);
    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (-9.0, 0.1));
}

TEST_CASE ("All 24 Band slots are exposed to the host with stable IDs and readable names")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;

    // These IDs are stored in hosts' sessions and automation: they must never change.
    const std::pair<const char*, const char*> controls[] = {
        { "frequency", "Frequency" }, { "gain", "Gain" }, { "q", "Q" },          { "in_use", "In Use" },
        { "bypass", "Bypass" },       { "shape", "Shape" }, { "slope", "Slope" },
    };

    std::map<juce::String, juce::String> names;
    for (auto* parameter : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
            names[ranged->getParameterID()] = ranged->getName (100);

    CHECK (names.size() == 24 * std::size (controls));
    for (int slot = 1; slot <= 24; ++slot)
        for (const auto& [suffix, name] : controls)
        {
            const auto id = "band" + juce::String (slot) + "_" + suffix;
            CAPTURE (id);
            REQUIRE (names.contains (id));
            CHECK (names[id] == "Band " + juce::String (slot) + " " + name);
        }
}

TEST_CASE ("A Band slot shapes the sound only when in use and not Bypassed")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    setParameter (processor, "band24_frequency", 1000.0f);
    setParameter (processor, "band24_gain", 12.0f);
    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (0.0, 0.05));

    setParameter (processor, "band24_in_use", 1.0f);
    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (12.0, 0.1));

    setParameter (processor, "band24_bypass", 1.0f);
    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (0.0, 0.05));

    setParameter (processor, "band24_bypass", 0.0f);
    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (12.0, 0.1));
}

TEST_CASE ("A producer can pick any Shape for a Band")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioParameterChoice* shape = nullptr;
    for (auto* parameter : processor.getParameters())
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameter); choice != nullptr && choice->getParameterID() == "band5_shape")
            shape = choice;
    REQUIRE (shape != nullptr);
    CHECK (shape->choices == juce::StringArray { "Bell", "Low Shelf", "High Shelf", "Tilt Shelf", "Flat Tilt" });

    setParameter (processor, "band5_in_use", 1.0f);
    setParameter (processor, "band5_frequency", 1000.0f);
    setParameter (processor, "band5_gain", 12.0f);
    setParameter (processor, "band5_slope", 24.0f);

    const auto pick = [&] (const char* name) { *shape = shape->choices.indexOf (name); };

    pick ("Low Shelf");
    CHECK_THAT (sineGainDb (processor, 100.0), WithinAbs (12.0, 0.1));
    CHECK_THAT (sineGainDb (processor, 10000.0), WithinAbs (0.0, 0.1));

    pick ("High Shelf");
    CHECK_THAT (sineGainDb (processor, 100.0), WithinAbs (0.0, 0.1));
    CHECK_THAT (sineGainDb (processor, 10000.0), WithinAbs (12.0, 0.1));

    pick ("Tilt Shelf");
    CHECK_THAT (sineGainDb (processor, 100.0), WithinAbs (-6.0, 0.1));
    CHECK_THAT (sineGainDb (processor, 10000.0), WithinAbs (6.0, 0.1));

    pick ("Flat Tilt");
    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (0.0, 0.1));
    CHECK_THAT (sineGainDb (processor, 2000.0), WithinAbs (1.2, 0.1));
}
