#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <optional>
#include <vector>

namespace eq1
{

// Where Presets are kept: User Presets as files in a folder, one per Preset, named after it; Factory
// Presets bundled with eq1. A Preset file is plain XML in the saved state's format
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

    // The User Presets' files, by name. None when the folder doesn't exist yet.
    std::vector<juce::File> userPresets() const;

    // Saves a Preset as name in the User folder, creating the folder if need be and replacing a
    // Preset of the same name. A name a file can't hold is made safe. Returns the file, or nothing
    // when it can't be written.
    std::optional<juce::File> save (const juce::String& name, const juce::ValueTree& preset) const;

    // A Preset file's contents, or an invalid tree when it isn't one.
    static juce::ValueTree read (const juce::File& file);

    struct FactoryPreset
    {
        juce::String name;
        juce::ValueTree preset;
    };
    // The Factory Presets, by name.
    static std::vector<FactoryPreset> factoryPresets();

private:
    juce::File userFolder;
};

} // namespace eq1
