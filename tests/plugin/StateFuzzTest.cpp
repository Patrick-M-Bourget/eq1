#include "PluginProcessor.h"
#include "SavedState.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace
{

// A session worth corrupting: Bands in use on both A/B sides, a Loaded Preset, a Display Range, an
// editor size and Analyzer settings, so every part of the format is there to be damaged.
juce::MemoryBlock richSession()
{
    eq1::PluginProcessor maker;
    const auto set = [&maker] (const juce::String& id, float plain) {
        auto* p = maker.parameterState().getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
    };
    set ("band1_in_use", 1.0f);
    set ("band1_frequency", 250.0f);
    set ("band1_gain", -4.5f);
    set ("band3_in_use", 1.0f);
    set ("band3_shape", 4.0f);
    set ("band3_frequency", 9000.0f);
    set ("band7_in_use", 1.0f);
    set ("band7_dynamic_range", -6.0f);
    maker.loadPreset (maker.presetState(), "Odd");
    maker.selectCompareSide (eq1::CompareSide::B);
    set ("band1_gain", 9.0f);
    set ("band4_in_use", 1.0f);
    set ("output_gain", -2.0f);
    maker.setDisplayRangeDb (30);
    maker.setEditorSize ({ 1400, 900 });
    auto analyzer = maker.analyzerSettings();
    analyzer.rangeDb = 120;
    analyzer.tiltDbPerOctave = 2.5;
    maker.setAnalyzerSettings (analyzer);
    return eq1::test::savedBlock (maker);
}

// A value a file could hold where a number belongs.
juce::String junkValue (std::mt19937& random)
{
    static const char* const fixed[] = { "nan", "NaN", "inf", "-inf", "1e30", "-1e30", "1e-30", "-0", "", "text", "1,5", "0x10", "99999999999999999999" };
    std::uniform_int_distribution<int> kind (0, 2);
    switch (kind (random))
    {
        case 0: return fixed[std::uniform_int_distribution<std::size_t> (0, std::size (fixed) - 1) (random)];
        case 1: return juce::String (std::uniform_real_distribution<double> (-1.0e4, 1.0e4) (random));
        default: return juce::String (std::uniform_real_distribution<double> (-1.0, 1.0) (random) * std::pow (10.0, std::uniform_int_distribution<int> (-40, 40) (random)));
    }
}

// Every attribute of element and its children that reads as a number set to a junk value, about one in
// chance; children sometimes dropped or doubled.
void junkNumbers (juce::XmlElement& element, std::mt19937& random, int chance)
{
    std::uniform_int_distribution<int> roll (1, chance);
    for (int i = 0; i < element.getNumAttributes(); ++i)
        if (roll (random) == 1 && element.getAttributeName (i) != "id")
            element.setAttribute (element.getAttributeName (i), junkValue (random));
    std::vector<juce::XmlElement*> children;
    for (auto* child : element.getChildIterator())
        children.push_back (child);
    for (auto* child : children)
    {
        junkNumbers (*child, random, chance);
        if (roll (random) == 1)
            element.removeChildElement (child, true);
        else if (roll (random) == 1)
            element.addChildElement (new juce::XmlElement (*child));
    }
}

} // namespace

TEST_CASE ("Truncated, bit-flipped and random bytes, and random values for every setting, restore without a crash to finite, in-range parameters")
{
    juce::ScopedJuceInitialiser_GUI juce;
    const auto started = std::chrono::steady_clock::now();
    std::mt19937 random (182);
    const auto valid = richSession();
    const auto validXml = eq1::test::savedXml (valid);
    REQUIRE (validXml != nullptr);

    eq1::PluginProcessor processor;
    processor.prepareToPlay (48000.0, 256);
    juce::AudioBuffer<float> buffer (processor.getTotalNumInputChannels(), 256);
    juce::MidiBuffer midi;
    const auto checkRestored = [&] (const std::string& what) {
        CAPTURE (what);
        REQUIRE (eq1::test::firstParameterOutOfRange (processor).isEmpty());
        REQUIRE (std::isfinite (processor.analyzerSettings().tiltDbPerOctave));
        // Both A/B sides and the Loaded Preset, through what applies them.
        processor.selectCompareSide (processor.compareSide() == eq1::CompareSide::A ? eq1::CompareSide::B : eq1::CompareSide::A);
        REQUIRE (eq1::test::firstParameterOutOfRange (processor).isEmpty());
        processor.isLoadedPresetModified();
        // And a block of audio through the restored Settings.
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (ch, i, std::uniform_real_distribution<float> (-0.5f, 0.5f) (random));
        processor.processBlock (buffer, midi);
        for (int ch = 0; ch < processor.getMainBusNumOutputChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                REQUIRE (std::isfinite (buffer.getSample (ch, i)));
    };

    constexpr int casesOfEachKind = 150;
    int cases = 0;
    for (int n = 0; n < casesOfEachKind; ++n)
    {
        {
            // Truncated: the saved block cut short anywhere, header included.
            const auto length = std::uniform_int_distribution<std::size_t> (0, valid.getSize() - 1) (random);
            eq1::test::restoreState (processor, juce::MemoryBlock (valid.getData(), length));
            checkRestored ("truncated to " + std::to_string (length) + " bytes");
            ++cases;
        }
        {
            // Bit-flipped: a few bits anywhere in the block.
            juce::MemoryBlock flipped (valid);
            const int flips = std::uniform_int_distribution<int> (1, 24) (random);
            std::string where;
            for (int f = 0; f < flips; ++f)
            {
                const auto bit = std::uniform_int_distribution<std::size_t> (0, flipped.getSize() * 8 - 1) (random);
                flipped.setBitRange (bit, 1, flipped.getBitRange (bit, 1) ^ 1);
                where += std::to_string (bit) + " ";
            }
            eq1::test::restoreState (processor, flipped);
            checkRestored ("bits flipped at " + where);
            ++cases;
        }
        {
            // Random bytes: not a saved state at all, or one by chance.
            juce::MemoryBlock bytes (std::uniform_int_distribution<std::size_t> (0, 4096) (random), true);
            for (std::size_t i = 0; i < bytes.getSize(); ++i)
                bytes[static_cast<int> (i)] = static_cast<char> (std::uniform_int_distribution<int> (0, 255) (random));
            eq1::test::restoreState (processor, bytes);
            checkRestored (std::to_string (bytes.getSize()) + " random bytes");
            ++cases;
        }
        {
            // Random values: a well-formed state whose numbers, every setting's included, are junk.
            juce::XmlElement xml (*validXml);
            junkNumbers (xml, random, n % 2 == 0 ? 1 : 4);
            eq1::test::restoreState (processor, xml);
            checkRestored ("random values, case " + std::to_string (n));
            ++cases;
        }
    }
    const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - started).count();
    std::printf ("State restore fuzz: %d cases in %.2f s\n", cases, seconds);
    CHECK (cases == 4 * casesOfEachKind);
}
