#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <functional>
#include <vector>

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

    host.processor.loadPreset (preset, "Preset");
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

    host.processor.loadPreset (preset, "Preset");
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

    host.processor.loadPreset (preset, "Preset");
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

    host.processor.loadPreset (preset, "Preset");
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
    host.processor.loadPreset (juce::ValueTree::fromXml (*fixture ("state-v1.xml")), "State v1");
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
    CHECK_FALSE (host.processor.loadPreset (juce::ValueTree ("SomethingElse"), "Something Else"));
    CHECK_FALSE (host.processor.loadPreset ({}, "Nothing"));
    CHECK (host.value ("band1_in_use") == 1.0f);
    CHECK (host.processor.editHistory().undoSteps() == 1);
}

TEST_CASE ("Loading a Preset makes it the side's Loaded Preset, unmodified; the other side's is unchanged")
{
    const auto vocal = presetWith ({ { "band2_in_use", 1.0f }, { "band2_gain", -7.0f } });
    const auto kick = presetWith ({ { "band1_in_use", 1.0f }, { "band1_gain", 4.0f } });
    Host host;
    CHECK (host.processor.loadedPresetName().isEmpty());
    CHECK_FALSE (host.processor.isLoadedPresetModified());

    host.processor.loadPreset (vocal, "Vocal");
    CHECK (host.processor.loadedPresetName() == "Vocal");
    CHECK_FALSE (host.processor.isLoadedPresetModified());

    host.processor.selectCompareSide (CompareSide::B);
    host.processor.loadPreset (kick, "Kick");
    CHECK (host.processor.loadedPresetName() == "Kick");
    CHECK_FALSE (host.processor.isLoadedPresetModified());
    host.processor.selectCompareSide (CompareSide::A);
    CHECK (host.processor.loadedPresetName() == "Vocal");
    CHECK_FALSE (host.processor.isLoadedPresetModified());
}

TEST_CASE ("Changing a setting a Preset holds marks the side Modified; changing it back, or undoing, clears the mark")
{
    const auto preset = presetWith ({ { "band2_in_use", 1.0f }, { "band2_gain", -7.0f } });
    Host host;
    host.processor.loadPreset (preset, "Vocal");

    host.edit ("band2_gain", 3.0f);
    CHECK (host.processor.isLoadedPresetModified());
    host.edit ("band2_gain", -7.0f);
    CHECK_FALSE (host.processor.isLoadedPresetModified());

    // A setting the Preset left out counts as its default.
    host.edit ("output_gain", 2.0f);
    CHECK (host.processor.isLoadedPresetModified());
    host.processor.editHistory().undo();
    CHECK_FALSE (host.processor.isLoadedPresetModified());
    CHECK (host.processor.loadedPresetName() == "Vocal");
}

TEST_CASE ("Global Bypass, the Analyzer and the display range never make a side Modified")
{
    const auto preset = presetWith ({ { "band2_in_use", 1.0f } });
    Host host;
    host.processor.loadPreset (preset, "Vocal");
    host.edit ("global_bypass", 1.0f);
    auto analyzer = host.processor.analyzerSettings();
    analyzer.showPreEq = false;
    analyzer.rangeDb = 120;
    host.processor.setAnalyzerSettings (analyzer);
    host.processor.setDisplayRangeDb (30);
    CHECK_FALSE (host.processor.isLoadedPresetModified());
}

TEST_CASE ("A side with no Loaded Preset is never Modified")
{
    Host host;
    host.edit ("band1_in_use", 1.0f);
    CHECK (host.processor.loadedPresetName().isEmpty());
    CHECK_FALSE (host.processor.isLoadedPresetModified());
}

TEST_CASE ("A side switched away from and back to stays unmodified")
{
    // Switching puts the side's settings back on the parameters from their plain values.
    const auto preset = presetWith ({ { "band1_in_use", 1.0f }, { "band1_frequency", 1234.5f }, { "band1_q", 3.3f }, { "output_pan", 17.0f } });
    Host host;
    host.processor.loadPreset (preset, "Odd");
    host.processor.selectCompareSide (CompareSide::B);
    host.edit ("band1_gain", 5.0f);
    host.processor.selectCompareSide (CompareSide::A);
    CHECK (host.processor.loadedPresetName() == "Odd");
    CHECK_FALSE (host.processor.isLoadedPresetModified());
}

TEST_CASE ("Undo of a Preset load puts back the side's previous Loaded Preset, or none; redo restores it")
{
    const auto vocal = presetWith ({ { "band2_in_use", 1.0f } });
    const auto kick = presetWith ({ { "band1_in_use", 1.0f } });
    Host host;
    auto& history = host.processor.editHistory();
    host.processor.loadPreset (vocal, "Vocal");
    host.edit ("output_gain", 3.0f);
    host.processor.loadPreset (kick, "Kick");

    history.undo();
    CHECK (host.processor.loadedPresetName() == "Vocal");
    CHECK (host.processor.isLoadedPresetModified());
    history.undo();
    history.undo();
    CHECK (host.processor.loadedPresetName().isEmpty());

    history.redo();
    CHECK (host.processor.loadedPresetName() == "Vocal");
    CHECK_FALSE (host.processor.isLoadedPresetModified());
    history.redo();
    history.redo();
    CHECK (host.processor.loadedPresetName() == "Kick");
    CHECK_FALSE (host.processor.isLoadedPresetModified());
}

TEST_CASE ("Copy to the other side, and B's first selection, give the other side the Loaded Preset and Modified state")
{
    const auto vocal = presetWith ({ { "band2_in_use", 1.0f } });
    const auto kick = presetWith ({ { "band1_in_use", 1.0f } });
    const bool modified = GENERATE (false, true);
    const auto how = GENERATE (Catch::Generators::as<std::string> {}, "first selection", "copy from A", "copy from B");
    CAPTURE (modified, how);
    Host host;
    // The side copied from holds Vocal; the other side Kick, until the copy.
    const auto from = how == "copy from B" ? CompareSide::B : CompareSide::A;
    const auto to = from == CompareSide::A ? CompareSide::B : CompareSide::A;
    if (how != "first selection")
    {
        host.processor.selectCompareSide (to);
        host.processor.loadPreset (kick, "Kick");
    }
    host.processor.selectCompareSide (from);
    host.processor.loadPreset (vocal, "Vocal");
    if (modified)
        host.edit ("band2_gain", 4.0f);

    if (how != "first selection")
        host.processor.copyToOther();
    host.processor.selectCompareSide (to);
    CHECK (host.processor.loadedPresetName() == "Vocal");
    CHECK (host.processor.isLoadedPresetModified() == modified);
}

TEST_CASE ("Settings saved as a Preset make it the side's Loaded Preset, unmodified, as one undo step")
{
    const auto vocal = presetWith ({ { "band2_in_use", 1.0f } });
    Host host;
    host.processor.loadPreset (vocal, "Vocal");
    host.edit ("band2_gain", 4.0f);
    REQUIRE (host.processor.isLoadedPresetModified());
    const int steps = host.processor.editHistory().undoSteps();

    host.processor.presetSaved (host.processor.presetState(), "Vocal Louder");
    CHECK (host.processor.loadedPresetName() == "Vocal Louder");
    CHECK_FALSE (host.processor.isLoadedPresetModified());
    host.edit ("band2_gain", 5.0f);
    CHECK (host.processor.isLoadedPresetModified());

    CHECK (host.processor.editHistory().undoSteps() == steps + 2);
    host.processor.editHistory().undo();
    host.processor.editHistory().undo();
    CHECK (host.processor.loadedPresetName() == "Vocal");
    CHECK (host.processor.isLoadedPresetModified());
}


TEST_CASE ("Each side remembers the folder its Loaded Preset came from, while it keeps that name, without saving it")
{
    const auto x = presetWith ({ { "band2_in_use", 1.0f } });
    Host host;
    host.processor.loadPreset (x, "X", "Factory");
    host.processor.selectCompareSide (CompareSide::B);
    // B, selected first, takes A's Loaded Preset and where it came from.
    CHECK (host.processor.loadedPresetFolder() == "Factory");
    host.processor.loadPreset (x, "X", "User");
    CHECK (host.processor.loadedPresetFolder() == "User");
    host.processor.selectCompareSide (CompareSide::A);
    CHECK (host.processor.loadedPresetFolder() == "Factory");

    // Copy gives the other side where it came from too.
    host.processor.copyToOther();
    host.processor.selectCompareSide (CompareSide::B);
    CHECK (host.processor.loadedPresetFolder() == "Factory");

    // Saving names the folder saved to; a load from a file, none.
    host.processor.presetSaved (host.processor.presetState(), "Y", "User/Drums");
    CHECK (host.processor.loadedPresetFolder() == "User/Drums");
    host.processor.loadPreset (x, "Z");
    CHECK (host.processor.loadedPresetFolder().isEmpty());

    // An undo to another name leaves none; nothing of it is in the saved state.
    host.processor.editHistory().undo();
    CHECK (host.processor.loadedPresetName() == "Y");
    CHECK (host.processor.loadedPresetFolder().isEmpty());

    Host plain;
    plain.processor.loadPreset (x, "X");
    Host remembering;
    remembering.processor.loadPreset (x, "X", "Factory");
    juce::MemoryBlock withFolder, without;
    remembering.processor.getStateInformation (withFolder);
    plain.processor.getStateInformation (without);
    CHECK (withFolder == without);
}
