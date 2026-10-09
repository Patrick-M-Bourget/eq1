#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;
using eq1::CompareSide;

namespace
{

// A plugin as a host has it.
struct Host
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;

    juce::RangedAudioParameter& parameter (const juce::String& id) { return *processor.parameterState().getParameter (id); }
    float value (const juce::String& id) { return processor.parameterState().getRawParameterValue (id)->load(); }

    // An edit in the editor: inside a gesture.
    void edit (const juce::String& id, float plain)
    {
        auto& p = parameter (id);
        p.beginChangeGesture();
        p.setValueNotifyingHost (p.convertTo0to1 (plain));
        p.endChangeGesture();
    }
};

// A Preset made in another plugin instance.
juce::ValueTree presetWith (const std::initializer_list<std::pair<const char*, float>> values)
{
    Host maker;
    for (const auto& [id, v] : values)
        maker.edit (id, v);
    return maker.processor.presetState();
}

std::unique_ptr<juce::XmlElement> fixture (const char* name)
{
    auto xml = juce::XmlDocument::parse (juce::File (EQ1_TEST_FIXTURES).getChildFile (name));
    REQUIRE (xml != nullptr);
    return xml;
}

} // namespace

TEST_CASE ("Loading a Preset sets every sound setting, those it doesn't change back to their defaults")
{
    const auto preset = presetWith ({ { "band2_in_use", 1.0f }, { "band2_gain", -7.0f }, { "pan_mode", 1.0f } });
    Host host;
    host.edit ("band5_in_use", 1.0f);
    host.edit ("output_gain", 5.0f);

    host.processor.loadPreset (preset);
    CHECK (host.value ("band2_in_use") == 1.0f);
    CHECK_THAT (host.value ("band2_gain"), WithinAbs (-7.0, 1.0e-4));
    CHECK (host.value ("pan_mode") == 1.0f);
    CHECK (host.value ("band5_in_use") == 0.0f);
    CHECK_THAT (host.value ("output_gain"), WithinAbs (0.0, 1.0e-4));
}

TEST_CASE ("Loading a Preset is one undo step")
{
    const auto preset = presetWith ({ { "band2_in_use", 1.0f }, { "band2_gain", -7.0f } });
    Host host;
    host.edit ("output_gain", 5.0f);
    auto& history = host.processor.editHistory();
    const int steps = history.undoSteps();

    host.processor.loadPreset (preset);
    CHECK (history.undoSteps() == steps + 1);
    history.undo();
    CHECK (host.value ("band2_in_use") == 0.0f);
    CHECK_THAT (host.value ("output_gain"), WithinAbs (5.0, 1.0e-4));
}

TEST_CASE ("Loading a Preset doesn't change Global Bypass, the Analyzer settings or the display range")
{
    const auto preset = presetWith ({ { "band2_in_use", 1.0f }, { "global_bypass", 0.0f } });
    Host host;
    host.edit ("global_bypass", 1.0f);
    auto analyzer = host.processor.analyzerSettings();
    analyzer.showPreEq = false;
    analyzer.rangeDb = 120;
    analyzer.speed = eq1::AnalyzerSpeed::veryFast;
    analyzer.tiltDbPerOctave = 1.5;
    host.processor.setAnalyzerSettings (analyzer);
    host.processor.setDisplayRangeDb (30);

    host.processor.loadPreset (preset);
    CHECK (host.value ("band2_in_use") == 1.0f);
    CHECK (host.value ("global_bypass") == 1.0f);
    CHECK (host.processor.analyzerSettings() == analyzer);
    CHECK (host.processor.displayRangeDb() == 30);
}

TEST_CASE ("Loading a Preset on B leaves A unchanged")
{
    const auto preset = presetWith ({ { "band2_in_use", 1.0f }, { "band2_gain", -7.0f } });
    Host host;
    host.edit ("band1_in_use", 1.0f);
    host.processor.selectCompareSide (CompareSide::B);

    host.processor.loadPreset (preset);
    CHECK (host.processor.compareSide() == CompareSide::B);
    CHECK (host.value ("band1_in_use") == 0.0f);
    CHECK (host.value ("band2_in_use") == 1.0f);

    host.processor.selectCompareSide (CompareSide::A);
    CHECK (host.value ("band1_in_use") == 1.0f);
    CHECK (host.value ("band2_in_use") == 0.0f);
}

TEST_CASE ("A Preset holds the sound settings in the versioned state format, and nothing else")
{
    Host host;
    host.edit ("band1_in_use", 1.0f);
    host.edit ("global_bypass", 1.0f);
    const auto preset = host.processor.presetState();
    CHECK (preset.hasType ("eq1"));
    CHECK (static_cast<int> (preset.getProperty ("version")) == eq1::PluginProcessor::stateVersion);
    CHECK (preset.getChildWithProperty ("id", "band1_in_use").isValid());
    CHECK_FALSE (preset.getChildWithProperty ("id", "global_bypass").isValid());
    for (const auto& child : preset)
        CHECK (child.hasType ("PARAM"));
    CHECK_FALSE (preset.hasProperty ("displayRangeDb"));
}

TEST_CASE ("A Preset in an older version of the format loads")
{
    // A version 1 session's settings, as a version 1 Preset would hold them.
    Host host;
    host.processor.loadPreset (juce::ValueTree::fromXml (*fixture ("state-v1.xml")));
    CHECK_THAT (host.value ("band1_gain"), WithinAbs (-4.5, 1.0e-4));
    CHECK (host.value ("band3_shape") == 4.0f);
    CHECK (host.value ("band24_bypass") == 1.0f);
    // The session's Analyzer and display range are not a Preset's.
    CHECK (host.processor.displayRangeDb() == 12);
    CHECK (host.processor.analyzerSettings() == eq1::AnalyzerSettings {});
}

TEST_CASE ("Something that isn't a Preset changes nothing")
{
    Host host;
    host.edit ("band1_in_use", 1.0f);
    host.processor.loadPreset (juce::ValueTree ("SomethingElse"));
    host.processor.loadPreset ({});
    CHECK (host.value ("band1_in_use") == 1.0f);
    CHECK (host.processor.editHistory().undoSteps() == 1);
}
