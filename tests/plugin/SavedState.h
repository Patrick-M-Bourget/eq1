#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace eq1::test
{

// A saved session decoded as setStateInformation reads it. The saved block starts with a binary
// header, so its raw bytes (MemoryBlock::toString) never show the XML.
inline std::unique_ptr<juce::XmlElement> savedXml (const juce::MemoryBlock& saved)
{
    return juce::AudioProcessor::getXmlFromBinary (saved.getData(), static_cast<int> (saved.getSize()));
}

inline juce::ValueTree savedState (const juce::MemoryBlock& saved)
{
    const auto xml = savedXml (saved);
    return xml != nullptr ? juce::ValueTree::fromXml (*xml) : juce::ValueTree();
}

// The session state a processor saves, decoded. A test that also restores the session saves the block
// once with getStateInformation and decodes that instead.
inline juce::MemoryBlock savedBlock (juce::AudioProcessor& processor)
{
    juce::MemoryBlock saved;
    processor.getStateInformation (saved);
    return saved;
}

inline std::unique_ptr<juce::XmlElement> savedXml (juce::AudioProcessor& processor) { return savedXml (savedBlock (processor)); }

inline juce::ValueTree savedState (juce::AudioProcessor& processor) { return savedState (savedBlock (processor)); }

// Every name a setting could be saved under, at any depth: each child's type, each property's name,
// and the value of each "id" property (host parameters are saved as children with an id).
inline juce::StringArray savedNames (const juce::ValueTree& tree)
{
    juce::StringArray names { tree.getType().toString() };
    for (int i = 0; i < tree.getNumProperties(); ++i)
    {
        const auto name = tree.getPropertyName (i);
        names.add (name.toString());
        if (name.toString() == "id")
            names.add (tree.getProperty (name).toString());
    }
    for (const auto& child : tree)
        names.addArray (savedNames (child));
    return names;
}

// The saved names containing word, ignoring case: empty when nothing is saved under it.
inline juce::StringArray savedNamesContaining (const juce::ValueTree& tree, const juce::String& word)
{
    juce::StringArray found;
    for (const auto& name : savedNames (tree))
        if (name.containsIgnoreCase (word))
            found.add (name);
    return found;
}

} // namespace eq1::test
