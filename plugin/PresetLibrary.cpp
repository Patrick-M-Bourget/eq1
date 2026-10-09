#include "PresetLibrary.h"

#include "FactoryPresets.h"

#include <algorithm>

namespace eq1
{

namespace
{
juce::ValueTree fromXml (const std::unique_ptr<juce::XmlElement>& xml)
{
    return xml != nullptr ? juce::ValueTree::fromXml (*xml) : juce::ValueTree();
}

bool byName (const juce::String& a, const juce::String& b) { return a.compareNatural (b) < 0; }
} // namespace

PresetLibrary::PresetLibrary (juce::File folder) : userFolder (std::move (folder)) {}

juce::File PresetLibrary::defaultUserFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("eq1").getChildFile ("Presets");
}

std::vector<juce::File> PresetLibrary::userPresets() const
{
    auto found = userFolder.findChildFiles (juce::File::findFiles, false, "*" + fileExtension);
    std::vector<juce::File> files (found.begin(), found.end());
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b) {
        return byName (a.getFileNameWithoutExtension(), b.getFileNameWithoutExtension());
    });
    return files;
}

juce::File PresetLibrary::save (const juce::String& name, const juce::ValueTree& preset) const
{
    const auto file = userFolder.getChildFile (juce::File::createLegalFileName (name.trim()) + fileExtension);
    const auto xml = preset.createXml();
    if (xml == nullptr || ! userFolder.createDirectory() || ! xml->writeTo (file))
        return {};
    return file;
}

juce::ValueTree PresetLibrary::read (const juce::File& file)
{
    return file.existsAsFile() ? fromXml (juce::XmlDocument::parse (file)) : juce::ValueTree();
}

std::vector<PresetLibrary::Factory> PresetLibrary::factoryPresets()
{
    std::vector<Factory> presets;
    for (int i = 0; i < FactoryPresets::namedResourceListSize; ++i)
    {
        int size = 0;
        const auto* data = FactoryPresets::getNamedResource (FactoryPresets::namedResourceList[i], size);
        const auto name = juce::String (FactoryPresets::originalFilenames[i]).upToLastOccurrenceOf (fileExtension, false, false);
        presets.push_back ({ name, fromXml (juce::XmlDocument::parse (juce::String::fromUTF8 (data, size))) });
    }
    std::sort (presets.begin(), presets.end(), [] (const Factory& a, const Factory& b) { return byName (a.name, b.name); });
    return presets;
}

} // namespace eq1
