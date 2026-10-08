#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <map>
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

TEST_CASE ("The host parameter layout is pinned: IDs, names, ranges, steps, defaults and choices")
{
    // Hosts store parameter IDs in sessions and automation as normalised (0 to 1) values, so after
    // release every field below is frozen: changing one remaps users' saved automation. A change
    // here must be deliberate and say so in its commit.
    struct Control
    {
        const char* suffix;
        const char* name;
        const char* label;
        float start, end, interval;
        float defaultValue;
        float valueAtHalfway; // pins the mapping between normalised and plain values
        juce::StringArray choices;
    };
    const Control controls[] = {
        { "frequency", "Frequency", "Hz", 10.0f, 30000.0f, 0.0f, 1000.0f, 547.7226f, {} },
        { "gain", "Gain", "dB", -30.0f, 30.0f, 0.0f, 0.0f, 0.0f, {} },
        { "q", "Q", "", 0.025f, 40.0f, 0.0f, 1.0f, 1.0f, {} },
        { "in_use", "In Use", "", 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, {} },
        { "bypass", "Bypass", "", 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, {} },
        { "shape", "Shape", "", 0.0f, 4.0f, 1.0f, 0.0f, 2.0f, { "Bell", "Low Shelf", "High Shelf", "Tilt Shelf", "Flat Tilt" } },
        { "slope", "Slope", "dB/oct", 6.0f, 96.0f, 6.0f, 12.0f, 54.0f, {} },
    };

    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;

    std::map<juce::String, juce::RangedAudioParameter*> parameters;
    for (auto* parameter : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
            parameters[ranged->getParameterID()] = ranged;

    CHECK (parameters.size() == 24 * std::size (controls));
    for (int slot = 1; slot <= 24; ++slot)
        for (const auto& control : controls)
        {
            const auto id = "band" + juce::String (slot) + "_" + control.suffix;
            CAPTURE (id);
            REQUIRE (parameters.contains (id));
            auto* parameter = parameters[id];
            const auto& range = parameter->getNormalisableRange();

            CHECK (parameter->getName (100) == "Band " + juce::String (slot) + " " + control.name);
            CHECK (parameter->getLabel() == control.label);
            CHECK (range.start == control.start);
            CHECK (range.end == control.end);
            CHECK (range.interval == control.interval);
            CHECK_THAT (parameter->convertFrom0to1 (parameter->getDefaultValue()), WithinAbs (control.defaultValue, 1.0e-4));
            CHECK_THAT (parameter->convertFrom0to1 (0.5f), WithinAbs (control.valueAtHalfway, 1.0e-3));

            auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameter);
            CHECK ((choice != nullptr) == ! control.choices.isEmpty());
            if (choice != nullptr)
                CHECK (choice->choices == control.choices);
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
