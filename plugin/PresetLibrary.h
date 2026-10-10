#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <optional>
#include <vector>

namespace eq1
{

// Where Presets are kept: User Presets as files in a folder and its subfolders, one per Preset, named
// after it; Factory Presets bundled with eq1. A Preset file is plain XML in the saved state's format
// (PluginProcessor::presetState), so it moves between machines and opens in newer versions.
class PresetLibrary
{
public:
    explicit PresetLibrary (juce::File userFolder = defaultUserFolder());

    // Documents/eq1/Presets.
    static juce::File defaultUserFolder();
    // The User folder this library keeps its Presets in.
    const juce::File& folder() const { return userFolder; }
    static inline const juce::String fileExtension { ".eq1preset" };

    // The User Presets' files, in browser order: the User folder's own, then its subfolders' at any
    // depth, depth-first, each level by name. None when the folder doesn't exist yet.
    std::vector<juce::File> userPresets() const;

    // Saves a Preset as name in the User folder, creating the folder if need be and replacing a
    // Preset of the same name. A name a file can't hold is made safe. Returns the file, or nothing
    // when it can't be written.
    std::optional<juce::File> save (const juce::String& name, const juce::ValueTree& preset) const;

    // A Preset file's contents, or an invalid tree when it isn't one (not XML, or not eq1's state).
    static juce::ValueTree read (const juce::File& file);

    struct FactoryPreset
    {
        juce::String name;
        juce::ValueTree preset;
    };
    // The Factory Presets, by name.
    static std::vector<FactoryPreset> factoryPresets();

    // A Preset as the browser lists it.
    struct Entry
    {
        juce::String name;
        // "Factory", "User", or a User subfolder's path, such as "User/Drums/Acoustic".
        juce::String folder;
        // A User Preset's file; none for a Factory Preset.
        juce::File file;
        juce::ValueTree preset;
    };
    // Every Preset, read from the disk now, in browser order: Factory, then User (userPresets()). A
    // User Preset file that doesn't read as a Preset is left out.
    std::vector<Entry> listing() const;
    // The entries whose Preset names contain text, ignoring case, in their order. All of them for no text.
    static std::vector<Entry> search (const std::vector<Entry>& entries, const juce::String& text);
    // The Loaded Preset's entry: lastLoaded's place (folder and name) when it is listed and still named
    // loadedPreset, as a name can be listed more than once; otherwise the first entry named loadedPreset.
    // Nothing when none is.
    static std::optional<std::size_t> find (const std::vector<Entry>& entries, const juce::String& loadedPreset, const Entry* lastLoaded = nullptr);
    // The entry by places after (or before, when negative) the Loaded Preset's (find), wrapping at the
    // ends. With no such entry, forward is the first and back the last. Nothing when there are none.
    static std::optional<std::size_t>
        step (const std::vector<Entry>& entries, const juce::String& loadedPreset, int by, const Entry* lastLoaded = nullptr);

private:
    juce::File userFolder;
};

} // namespace eq1
