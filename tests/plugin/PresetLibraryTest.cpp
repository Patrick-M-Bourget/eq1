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

// A plugin as a host has it.
struct Host
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;

    juce::RangedAudioParameter& parameter (const juce::String& id) { return *processor.parameterState().getParameter (id); }
    float value (const juce::String& id) { return processor.parameterState().getRawParameterValue (id)->load(); }

    void edit (const juce::String& id, float plain)
    {
        auto& p = parameter (id);
        p.beginChangeGesture();
        p.setValueNotifyingHost (p.convertTo0to1 (plain));
        p.endChangeGesture();
    }
};

// An empty folder, deleted afterwards.
struct Folder
{
    juce::TemporaryFile temporary;
    juce::File folder = temporary.getFile();
    Folder() { REQUIRE (folder.createDirectory()); }
    ~Folder() { folder.deleteRecursively(); }
};

} // namespace

TEST_CASE ("User Presets are kept in Documents/eq1/Presets by default")
{
    CHECK (PresetLibrary::defaultUserFolder()
           == juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("eq1").getChildFile ("Presets"));
}

TEST_CASE ("A User Preset saves as a file in the User folder and loads back")
{
    Folder user;
    const PresetLibrary library (user.folder);
    Host saved;
    saved.edit ("band3_in_use", 1.0f);
    saved.edit ("band3_frequency", 4200.0f);
    saved.edit ("band3_dynamic_range", -9.0f);
    saved.edit ("output_pan", 30.0f);

    const auto file = library.save ("Bright Vocal", saved.processor.presetState());
    CHECK (file == user.folder.getChildFile ("Bright Vocal.eq1preset"));
    CHECK (file.existsAsFile());
    const auto presets = library.userPresets();
    REQUIRE (presets.size() == 1);
    CHECK (presets.front() == file);

    Host loaded;
    loaded.processor.loadPreset (PresetLibrary::read (file));
    CHECK (loaded.value ("band3_in_use") == 1.0f);
    CHECK_THAT (loaded.value ("band3_frequency"), WithinAbs (4200.0, 1.0e-2));
    CHECK_THAT (loaded.value ("band3_dynamic_range"), WithinAbs (-9.0, 1.0e-4));
    CHECK_THAT (loaded.value ("output_pan"), WithinAbs (30.0, 1.0e-4));
}

TEST_CASE ("A User Preset file is portable: copied into another machine's folder, it loads the same")
{
    Folder here, there;
    Host saved;
    saved.edit ("band1_in_use", 1.0f);
    saved.edit ("band1_gain", 3.5f);
    const auto file = PresetLibrary (here.folder).save ("Warm", saved.processor.presetState());
    const auto copy = there.folder.getChildFile (file.getFileName());
    REQUIRE (file.copyFileTo (copy));
    REQUIRE (file.deleteFile());

    // Plain XML, holding parameter ids and plain values: nothing tied to this machine or build.
    const auto xml = juce::XmlDocument::parse (copy);
    REQUIRE (xml != nullptr);
    CHECK (xml->hasTagName ("eq1"));
    CHECK_FALSE (xml->toString().contains (here.folder.getFullPathName()));

    const auto presets = PresetLibrary (there.folder).userPresets();
    REQUIRE (presets.size() == 1);
    Host loaded;
    loaded.processor.loadPreset (PresetLibrary::read (presets.front()));
    CHECK (loaded.value ("band1_in_use") == 1.0f);
    CHECK_THAT (loaded.value ("band1_gain"), WithinAbs (3.5, 1.0e-4));
}

TEST_CASE ("Saving under an existing name replaces that Preset; a name a file can't hold is made safe")
{
    Folder user;
    const PresetLibrary library (user.folder);
    Host host;
    library.save ("Kick", host.processor.presetState());
    host.edit ("band1_in_use", 1.0f);
    library.save ("Kick", host.processor.presetState());
    const auto odd = library.save ("Bass: Low/High", host.processor.presetState());

    const auto presets = library.userPresets();
    REQUIRE (presets.size() == 2);
    CHECK (odd.existsAsFile());
    CHECK (odd.getParentDirectory() == user.folder);
    Host loaded;
    loaded.processor.loadPreset (PresetLibrary::read (user.folder.getChildFile ("Kick.eq1preset")));
    CHECK (loaded.value ("band1_in_use") == 1.0f);
}

TEST_CASE ("The User folder lists only Presets, by name; a missing folder lists none")
{
    Folder user;
    user.folder.getChildFile ("notes.txt").replaceWithText ("not a Preset");
    const PresetLibrary library (user.folder);
    Host host;
    library.save ("b", host.processor.presetState());
    library.save ("A", host.processor.presetState());
    const auto presets = library.userPresets();
    REQUIRE (presets.size() == 2);
    CHECK (presets[0].getFileNameWithoutExtension() == "A");
    CHECK (presets[1].getFileNameWithoutExtension() == "b");

    CHECK (PresetLibrary (user.folder.getChildFile ("missing")).userPresets().empty());
}

TEST_CASE ("A file that isn't a Preset reads as nothing")
{
    Folder user;
    const auto file = user.folder.getChildFile ("broken.eq1preset");
    file.replaceWithText ("not XML");
    CHECK_FALSE (PresetLibrary::read (file).isValid());
    CHECK_FALSE (PresetLibrary::read (user.folder.getChildFile ("missing.eq1preset")).isValid());
}

TEST_CASE ("Factory Presets are bundled, and each loads its settings")
{
    const auto factory = PresetLibrary::factoryPresets();
    REQUIRE (factory.size() >= 3);
    std::set<juce::String> names;
    for (const auto& [name, preset] : factory)
    {
        CAPTURE (name);
        CHECK (name.isNotEmpty());
        names.insert (name);
        REQUIRE (preset.hasType ("eq1"));

        Host host;
        host.processor.loadPreset (preset);
        for (const auto& setting : preset)
        {
            const auto id = setting.getProperty ("id").toString();
            CAPTURE (id);
            const auto expected = static_cast<float> (setting.getProperty ("value"));
            CHECK_THAT (host.value (id), WithinAbs (expected, 1.0e-4) || WithinRel (expected, 1.0e-6f));
        }
    }
    CHECK (names.size() == factory.size());
}
