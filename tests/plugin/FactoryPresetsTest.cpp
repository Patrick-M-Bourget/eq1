// The gate every Factory Preset passes (spec #1, "Factory Presets"). Each rule is a function of one
// Preset, or of the library for unique names, giving what is wrong or nothing; the test runs each
// over the real library and shows it rejects a bad example.

#include "Parameters.h"
#include "PluginProcessor.h"
#include "PresetLibrary.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <set>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using eq1::PresetLibrary;

namespace
{

using FactoryPreset = PresetLibrary::FactoryPreset;

// The Factory library's names are "<Category> – <Name>", with an en dash (U+2013).
const juce::String dash = juce::String::fromUTF8 (" \xe2\x80\x93 ");

juce::String named (const char* utf8) { return juce::String::fromUTF8 (utf8); }

// A plugin as a host has it.
struct Host
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;

    float value (const juce::String& id) { return processor.parameterState().getRawParameterValue (id)->load(); }
};

// Rule: apart from Default, the name is "<Category> – <Name>", the Category one of the spec's.
juce::String namingProblem (const FactoryPreset& preset)
{
    static const juce::StringArray categories { "Utility", "Drums", "Bass",   "Vocals",  "Guitar", "Keys",
                                                "Synth",   "Orchestral", "Mix Bus", "Master", "Sends" };
    if (preset.name == "Default")
        return {};
    if (! preset.name.contains (dash))
        return "not named <Category>" + dash + "<Name>";
    const auto category = preset.name.upToFirstOccurrenceOf (dash, false, false);
    const auto name = preset.name.fromFirstOccurrenceOf (dash, false, false);
    if (! categories.contains (category))
        return "no such Category: " + category;
    if (name.trim().isEmpty() || name != name.trim() || name.contains (dash))
        return "no single Name after the Category";
    return {};
}

// Rule: no two Factory Presets share a name.
juce::String duplicateName (const std::vector<FactoryPreset>& library)
{
    std::set<juce::String> names;
    for (const auto& preset : library)
        if (! names.insert (preset.name).second)
            return "named twice: " + preset.name;
    return {};
}

// Rule: it loads, and every setting it holds is restored.
juce::String loadingProblem (const FactoryPreset& preset)
{
    if (! preset.preset.hasType ("eq1"))
        return "not an eq1 Preset";
    Host host;
    if (! host.processor.loadPreset (preset.preset, preset.name))
        return "doesn't load";
    for (const auto& setting : preset.preset)
    {
        const auto id = setting.getProperty ("id").toString();
        if (host.processor.parameterState().getParameter (id) == nullptr)
            return "no such setting: " + id;
        const auto expected = static_cast<float> (setting.getProperty ("value"));
        const auto restored = host.value (id);
        if (std::abs (restored - expected) > std::max (1.0e-4f, std::abs (expected) * 1.0e-6f))
            return id + " is " + juce::String (restored) + ", not " + juce::String (expected);
    }
    return {};
}

// Rule: it is written at the current state version, holding only the settings that differ from their
// defaults.
juce::String formatProblem (const FactoryPreset& preset)
{
    if (static_cast<int> (preset.preset.getProperty ("version", 0)) != eq1::PluginProcessor::stateVersion)
        return "not at state version " + juce::String (eq1::PluginProcessor::stateVersion);
    Host host;
    for (const auto& setting : preset.preset)
    {
        const auto id = setting.getProperty ("id").toString();
        if (const auto* parameter = host.processor.parameterState().getParameter (id);
            parameter != nullptr
            && juce::exactlyEqual (parameter->convertFrom0to1 (parameter->getDefaultValue()), static_cast<float> (setting.getProperty ("value"))))
            return id + " is at its default";
    }
    return {};
}

FactoryPreset preset (const char* nameUtf8, std::initializer_list<std::pair<const char*, float>> settings = {})
{
    juce::ValueTree tree ("eq1");
    tree.setProperty ("version", eq1::PluginProcessor::stateVersion, nullptr);
    for (const auto& [id, value] : settings)
        tree.appendChild (juce::ValueTree ("PARAM").setProperty ("id", id, nullptr).setProperty ("value", value, nullptr), nullptr);
    return { named (nameUtf8), tree };
}

} // namespace

TEST_CASE ("Every Factory Preset passes the gate")
{
    const auto factory = PresetLibrary::factoryPresets();
    REQUIRE (factory.size() >= 3);
    CHECK (duplicateName (factory).isEmpty());
    for (const auto& preset : factory)
    {
        CAPTURE (preset.name);
        CHECK (loadingProblem (preset) == "");
        CHECK (namingProblem (preset) == "");
        CHECK (formatProblem (preset) == "");
    }
}

TEST_CASE ("Every file in the Factory folder is bundled as a Factory Preset")
{
    juce::StringArray files;
    for (const auto& file : juce::File (juce::CharPointer_UTF8 (EQ1_FACTORY_PRESETS)).findChildFiles (juce::File::findFiles, false, "*" + PresetLibrary::fileExtension))
        files.add (file.getFileNameWithoutExtension());
    juce::StringArray bundled;
    for (const auto& preset : PresetLibrary::factoryPresets())
        bundled.add (preset.name);
    files.sort (false);
    bundled.sort (false);
    REQUIRE (files.size() >= 3);
    CHECK (bundled == files);
}

TEST_CASE ("The Factory Presets are renamed into Categories, Default kept")
{
    juce::StringArray names;
    for (const auto& preset : PresetLibrary::factoryPresets())
        names.add (preset.name);
    for (const auto* name : { "Default", "Drums \xe2\x80\x93 Kick In Punch", "Vocals \xe2\x80\x93 De-Esser", "Vocals \xe2\x80\x93 Lead Presence",
                              "Master \xe2\x80\x93 Polish" })
        CHECK (names.contains (named (name)));
    for (const auto* old : { "Kick Punch", "De-Esser", "Vocal Presence", "Master Polish" })
        CHECK_FALSE (names.contains (old));
}

TEST_CASE ("The Factory gate rejects a name outside the scheme")
{
    CHECK (namingProblem (preset ("Default")) == "");
    CHECK (namingProblem (preset ("Mix Bus \xe2\x80\x93 Glue")) == "");
    CHECK (namingProblem (preset ("Kick Punch")).isNotEmpty());
    CHECK (namingProblem (preset ("Drums - Kick")).isNotEmpty()); // a hyphen, not an en dash
    CHECK (namingProblem (preset ("Drums\xe2\x80\x93Kick")).isNotEmpty());
    CHECK (namingProblem (preset ("Drum \xe2\x80\x93 Kick")).isNotEmpty());
    CHECK (namingProblem (preset ("Drums \xe2\x80\x93  Kick")).isNotEmpty());
    CHECK (namingProblem (preset ("Drums \xe2\x80\x93 ")).isNotEmpty());
}

TEST_CASE ("The Factory gate rejects two Presets of one name")
{
    CHECK (duplicateName ({ preset ("Drums \xe2\x80\x93 Kick"), preset ("Drums \xe2\x80\x93 Snare") }) == "");
    CHECK (duplicateName ({ preset ("Drums \xe2\x80\x93 Kick"), preset ("Drums \xe2\x80\x93 Kick") }).isNotEmpty());
}

TEST_CASE ("The Factory gate rejects a Preset that doesn't load as written")
{
    CHECK (loadingProblem (preset ("Drums \xe2\x80\x93 Kick", { { "band1_in_use", 1.0f }, { "band1_frequency", 60.0f } })) == "");
    CHECK (loadingProblem (preset ("Drums \xe2\x80\x93 Kick", { { "band1_frequency", 50000.0f } })).isNotEmpty()); // beyond its range
    CHECK (loadingProblem (preset ("Drums \xe2\x80\x93 Kick", { { "band1_nonsense", 1.0f } })).isNotEmpty());
    CHECK (loadingProblem ({ named ("Drums \xe2\x80\x93 Kick"), juce::ValueTree ("other") }).isNotEmpty());
}

TEST_CASE ("The Factory gate rejects a Preset at an older version, or holding a default")
{
    CHECK (formatProblem (preset ("Drums \xe2\x80\x93 Kick", { { "band1_in_use", 1.0f }, { "output_gain", 3.0f } })) == "");
    CHECK (formatProblem (preset ("Drums \xe2\x80\x93 Kick", { { "output_gain", 0.0f } })).isNotEmpty());
    auto older = preset ("Drums \xe2\x80\x93 Kick", { { "band1_in_use", 1.0f } });
    older.preset.setProperty ("version", eq1::PluginProcessor::stateVersion - 1, nullptr);
    CHECK (formatProblem (older).isNotEmpty());
}
