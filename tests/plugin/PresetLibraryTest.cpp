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

    const auto saving = library.save ("Bright Vocal", saved.processor.presetState());
    REQUIRE (saving.has_value());
    const auto file = *saving;
    CHECK (file == user.folder.getChildFile ("Bright Vocal.eq1preset"));
    CHECK (file.existsAsFile());
    const auto presets = library.userPresets();
    REQUIRE (presets.size() == 1);
    CHECK (presets.front() == file);

    Host loaded;
    loaded.processor.loadPreset (PresetLibrary::read (file), file.getFileNameWithoutExtension());
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
    const auto saving = PresetLibrary (here.folder).save ("Warm", saved.processor.presetState());
    REQUIRE (saving.has_value());
    const auto file = *saving;
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
    loaded.processor.loadPreset (PresetLibrary::read (presets.front()), presets.front().getFileNameWithoutExtension());
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
    REQUIRE (odd.has_value());
    CHECK (odd->existsAsFile());
    CHECK (odd->getParentDirectory() == user.folder);
    Host loaded;
    loaded.processor.loadPreset (PresetLibrary::read (user.folder.getChildFile ("Kick.eq1preset")), "Kick");
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

TEST_CASE ("Saving where a file can't be written gives nothing")
{
    Folder user;
    const auto notAFolder = user.folder.getChildFile ("file");
    REQUIRE (notAFolder.replaceWithText ("in the way"));
    Host host;
    CHECK_FALSE (PresetLibrary (notAFolder).save ("Kick", host.processor.presetState()).has_value());
}

TEST_CASE ("A file that isn't a Preset reads as nothing")
{
    Folder user;
    const auto file = user.folder.getChildFile ("broken.eq1preset");
    file.replaceWithText ("not XML");
    CHECK_FALSE (PresetLibrary::read (file).isValid());
    CHECK_FALSE (PresetLibrary::read (user.folder.getChildFile ("missing.eq1preset")).isValid());
    const auto other = user.folder.getChildFile ("other.eq1preset");
    other.replaceWithText ("<other/>");
    CHECK_FALSE (PresetLibrary::read (other).isValid());
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
        host.processor.loadPreset (preset, name);
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

namespace
{

// A User Preset file written straight to disk, as a user copying one in would.
void writePreset (const juce::File& file)
{
    Host host;
    REQUIRE (file.getParentDirectory().createDirectory());
    REQUIRE (host.processor.presetState().createXml()->writeTo (file));
}

juce::StringArray namesAndFolders (const std::vector<PresetLibrary::Entry>& entries)
{
    juce::StringArray listed;
    for (const auto& entry : entries)
        listed.add (entry.folder + ": " + entry.name);
    return listed;
}

} // namespace

TEST_CASE ("The browser lists Factory, then the User folder's Presets, then its subfolders at any depth, each level by name")
{
    Folder user;
    for (const auto* path : { "b", "A", "Drums/kick", "Drums/Snare", "Drums/Acoustic/Room", "bass/Sub" })
        writePreset (user.folder.getChildFile (juce::String (path) + PresetLibrary::fileExtension));
    user.folder.getChildFile ("notes.txt").replaceWithText ("not a Preset");
    user.folder.getChildFile ("Drums/kick.wav").replaceWithText ("not a Preset");
    user.folder.getChildFile ("broken" + PresetLibrary::fileExtension).replaceWithText ("not XML");
    REQUIRE (user.folder.getChildFile ("Empty").createDirectory());

    const PresetLibrary library (user.folder);
    const auto listing = library.listing();
    const auto factory = PresetLibrary::factoryPresets();
    REQUIRE (listing.size() == factory.size() + 6);
    for (std::size_t i = 0; i < factory.size(); ++i)
    {
        CHECK (listing[i].name == factory[i].name);
        CHECK (listing[i].folder == "Factory");
        CHECK (listing[i].preset.isEquivalentTo (factory[i].preset));
        CHECK (listing[i].file == juce::File());
    }
    const std::vector userEntries (listing.begin() + static_cast<std::ptrdiff_t> (factory.size()), listing.end());
    CHECK (namesAndFolders (userEntries)
           == juce::StringArray ({ "User: A", "User: b", "User/bass: Sub", "User/Drums: kick", "User/Drums: Snare", "User/Drums/Acoustic: Room" }));
    for (const auto& entry : userEntries)
    {
        CHECK (entry.preset.hasType ("eq1"));
        CHECK (entry.file.getFileNameWithoutExtension() == entry.name);
    }

    // The User Presets' files in the same order, a file with a Preset's name that doesn't read as one included.
    juce::StringArray files;
    for (const auto& file : library.userPresets())
        files.add (file.getRelativePathFrom (user.folder).upToLastOccurrenceOf (PresetLibrary::fileExtension, false, false));
    CHECK (files == juce::StringArray ({ "A", "b", "broken", "bass/Sub", "Drums/kick", "Drums/Snare", "Drums/Acoustic/Room" }));
}

TEST_CASE ("A Preset added to the User folder on disk is listed the next time the library is read")
{
    Folder user;
    const PresetLibrary library (user.folder);
    const auto before = library.listing().size();
    writePreset (user.folder.getChildFile ("Vocals/Air" + PresetLibrary::fileExtension));
    const auto after = library.listing();
    REQUIRE (after.size() == before + 1);
    CHECK (after.back().name == "Air");
    CHECK (after.back().folder == "User/Vocals");
}

TEST_CASE ("Search keeps the Presets whose names contain the text, ignoring case and folder names, in browser order")
{
    const std::vector<PresetLibrary::Entry> listing {
        { "Kick Tight", "Factory", {}, {} },
        { "Snare", "User/Kicks", {}, {} },
        { "808 KICK", "User/Drums", {}, {} },
        { "Vocal", "User", {}, {} },
    };
    CHECK (namesAndFolders (PresetLibrary::search (listing, "kick")) == juce::StringArray ({ "Factory: Kick Tight", "User/Drums: 808 KICK" }));
    CHECK (PresetLibrary::search (listing, "").size() == listing.size());
    CHECK (PresetLibrary::search (listing, "nothing").empty());
}

TEST_CASE ("Stepping moves to the next or previous Preset in browser order, wrapping at the ends")
{
    const std::vector<PresetLibrary::Entry> listing {
        { "Bright", "Factory", {}, {} },
        { "Warm", "Factory", {}, {} },
        { "Bright", "User", {}, {} },
        { "Dark", "User/Mix", {}, {} },
    };
    CHECK (PresetLibrary::step (listing, "Warm", 1) == 2u);
    CHECK (PresetLibrary::step (listing, "Warm", -1) == 0u);
    CHECK (PresetLibrary::step (listing, "Dark", 1) == 0u);
    CHECK (PresetLibrary::step (listing, "Bright", -1) == 3u);

    // A name listed more than once steps from its first occurrence.
    CHECK (PresetLibrary::step (listing, "Bright", 1) == 1u);

    // No Loaded Preset, or one not in the library: forward to the first, back to the last.
    for (const juce::String loaded : { "", "Elsewhere" })
    {
        CAPTURE (loaded);
        CHECK (PresetLibrary::step (listing, loaded, 1) == 0u);
        CHECK (PresetLibrary::step (listing, loaded, -1) == 3u);
    }
    CHECK_FALSE (PresetLibrary::step ({}, "Warm", 1).has_value());
}
