#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <random>

using Catch::Matchers::WithinAbs;

namespace
{

constexpr double sampleRate = 48000.0;
constexpr int blockSize = 512;

juce::RangedAudioParameter& parameter (juce::AudioProcessor& processor, const juce::String& id)
{
    for (auto* p : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p); ranged != nullptr && ranged->getParameterID() == id)
            return *ranged;
    FAIL ("No parameter " << id);
    throw std::logic_error ("unreachable");
}

float value (juce::AudioProcessor& processor, const juce::String& id)
{
    auto& p = parameter (processor, id);
    return p.convertFrom0to1 (p.getValue());
}

void set (juce::AudioProcessor& processor, const juce::String& id, float plain)
{
    auto& p = parameter (processor, id);
    p.setValueNotifyingHost (p.convertTo0to1 (plain));
}

std::unique_ptr<juce::XmlElement> savedXml (juce::AudioProcessor& processor)
{
    juce::MemoryBlock state;
    processor.getStateInformation (state);
    return juce::AudioProcessor::getXmlFromBinary (state.getData(), static_cast<int> (state.getSize()));
}

void load (juce::AudioProcessor& processor, const juce::XmlElement& xml)
{
    juce::MemoryBlock state;
    juce::AudioProcessor::copyXmlToBinary (xml, state);
    processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
}

std::unique_ptr<juce::XmlElement> fixture (const char* name)
{
    auto xml = juce::XmlDocument::parse (juce::File (EQ1_TEST_FIXTURES).getChildFile (name));
    REQUIRE (xml != nullptr);
    return xml;
}

// Two seconds of the same stereo noise, with a Sidechain of other noise, through processor.
juce::AudioBuffer<float> play (juce::AudioProcessor& processor)
{
    processor.prepareToPlay (sampleRate, blockSize);
    std::mt19937 random (3);
    std::uniform_real_distribution<float> unit (-0.5f, 0.5f);
    const int blocks = static_cast<int> (2.0 * sampleRate / blockSize);
    juce::AudioBuffer<float> output (2, blocks * blockSize), buffer (processor.getTotalNumInputChannels(), blockSize);
    juce::MidiBuffer midi;
    for (int b = 0; b < blocks; ++b)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < blockSize; ++i)
                buffer.setSample (ch, i, unit (random));
        processor.processBlock (buffer, midi);
        for (int ch = 0; ch < 2; ++ch)
            output.copyFrom (ch, b * blockSize, buffer, ch, 0, blockSize);
    }
    return output;
}

} // namespace

TEST_CASE ("Saved state carries the state version")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    const auto xml = savedXml (processor);
    REQUIRE (xml != nullptr);
    CHECK (xml->getIntAttribute ("version", -1) == eq1::PluginProcessor::stateVersion);
    CHECK (eq1::PluginProcessor::stateVersion == 2);
}

TEST_CASE ("Save and reload sound exactly the same: Bands, dynamics, Sidechain detection and the output section")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor saved;
    const std::pair<const char*, float> values[] = {
        { "band1_in_use", 1.0f },         { "band1_frequency", 250.0f },     { "band1_gain", -4.5f },      { "band1_q", 2.0f },
        { "band3_in_use", 1.0f },         { "band3_shape", 4.0f },           { "band3_frequency", 9000.0f }, { "band3_slope", 36.0f },
        { "band3_placement", 3.0f },      { "band7_in_use", 1.0f },          { "band7_frequency", 3000.0f }, { "band7_gain", 2.0f },
        { "band7_dynamic_range", -6.0f }, { "band7_threshold_auto", 0.0f },  { "band7_threshold", -36.0f }, { "band7_detection_source", 1.0f },
        { "gain_scale", 150.0f },         { "auto_gain", 1.0f },             { "output_gain", -3.0f },     { "output_pan", 25.0f },
        { "pan_mode", 1.0f },             { "phase_invert", 1.0f } };
    for (const auto& [id, v] : values)
        set (saved, id, v);
    const auto xml = savedXml (saved);
    REQUIRE (xml != nullptr);

    eq1::PluginProcessor restored;
    load (restored, *xml);
    const auto expected = play (saved), actual = play (restored);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < expected.getNumSamples(); ++i)
            REQUIRE (juce::exactlyEqual (actual.getSample (ch, i), expected.getSample (ch, i)));
}

TEST_CASE ("A/B Compare survives save and reload: the side you're on and both sides' settings")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor saved;
    set (saved, "band1_in_use", 1.0f);
    set (saved, "band1_frequency", 250.0f);
    set (saved, "band1_gain", -4.5f);
    saved.selectCompareSide (eq1::CompareSide::B);
    set (saved, "band1_gain", 9.0f);
    set (saved, "band4_in_use", 1.0f);
    set (saved, "band4_shape", 2.0f);
    set (saved, "output_gain", -2.0f);
    const auto xml = savedXml (saved);
    REQUIRE (xml != nullptr);

    eq1::PluginProcessor restored;
    load (restored, *xml);
    CHECK (restored.compareSide() == eq1::CompareSide::B);
    const auto checkSameSound = [&] {
        const auto expected = play (saved), actual = play (restored);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < expected.getNumSamples(); ++i)
                REQUIRE (juce::exactlyEqual (actual.getSample (ch, i), expected.getSample (ch, i)));
    };
    checkSameSound();

    saved.selectCompareSide (eq1::CompareSide::A);
    restored.selectCompareSide (eq1::CompareSide::A);
    CHECK_THAT (value (restored, "band1_gain"), WithinAbs (-4.5, 1.0e-4));
    CHECK (value (restored, "band4_in_use") == 0.0f);
    checkSameSound();
}

TEST_CASE ("A version 1 session loads on side A, with B a copy of it")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    processor.selectCompareSide (eq1::CompareSide::B);
    load (processor, *fixture ("state-v1.xml"));
    CHECK (processor.compareSide() == eq1::CompareSide::A);
    CHECK_THAT (value (processor, "band1_gain"), WithinAbs (-4.5, 1.0e-4));

    processor.selectCompareSide (eq1::CompareSide::B);
    CHECK_THAT (value (processor, "band1_gain"), WithinAbs (-4.5, 1.0e-4));
    CHECK (value (processor, "band3_shape") == 4.0f);
    CHECK_THAT (value (processor, "gain_scale"), WithinAbs (150.0, 1.0e-3));
}

TEST_CASE ("A session saved before the state had a version (version 0) loads every setting")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    load (processor, *fixture ("state-v0.xml"));

    CHECK (value (processor, "band1_in_use") == 1.0f);
    CHECK_THAT (value (processor, "band1_frequency"), WithinAbs (250.0, 1.0e-3));
    CHECK_THAT (value (processor, "band1_gain"), WithinAbs (-4.5, 1.0e-4));
    CHECK (value (processor, "band3_shape") == 4.0f);
    CHECK (value (processor, "band3_placement") == 3.0f);
    CHECK_THAT (value (processor, "band7_dynamic_range"), WithinAbs (-6.0, 1.0e-4));
    CHECK_THAT (value (processor, "band7_threshold"), WithinAbs (-36.0, 1.0e-4));
    CHECK (value (processor, "band7_detection_source") == 1.0f);
    CHECK (value (processor, "band24_bypass") == 1.0f);
    CHECK_THAT (value (processor, "gain_scale"), WithinAbs (150.0, 1.0e-3));
    CHECK_THAT (value (processor, "output_gain"), WithinAbs (-3.0, 1.0e-3));
    CHECK (value (processor, "pan_mode") == 1.0f);
    CHECK (processor.displayRangeDb() == 30);
    const auto analyzer = processor.analyzerSettings();
    CHECK_FALSE (analyzer.showPreEq);
    CHECK (analyzer.rangeDb == 120);
    CHECK (analyzer.speed == eq1::AnalyzerSpeed::fast);

    // Saved again, it is the current version.
    CHECK (savedXml (processor)->getIntAttribute ("version", -1) == eq1::PluginProcessor::stateVersion);
}

TEST_CASE ("A session from a newer version loads the settings this version knows")
{
    juce::ScopedJuceInitialiser_GUI juce;
    auto xml = fixture ("state-v0.xml");
    xml->setAttribute ("version", eq1::PluginProcessor::stateVersion + 1);
    auto* unknown = xml->createNewChildElement ("PARAM");
    unknown->setAttribute ("id", "band1_from_the_future");
    unknown->setAttribute ("value", 7.0);
    xml->createNewChildElement ("SomethingNew")->setAttribute ("x", 1);

    eq1::PluginProcessor processor;
    processor.selectCompareSide (eq1::CompareSide::B);
    load (processor, *xml);
    CHECK_THAT (value (processor, "band1_gain"), WithinAbs (-4.5, 1.0e-4));
    CHECK (processor.displayRangeDb() == 30);
    CHECK (processor.analyzerSettings().rangeDb == 120);
    // It saves no A/B Compare this version knows: its settings are side A's.
    CHECK (processor.compareSide() == eq1::CompareSide::A);
}
