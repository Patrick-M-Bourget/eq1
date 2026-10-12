#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <cmath>

namespace eq1::test
{

// Restores a session into processor, as a host does: the block as saved, or a state as XML.
inline void restoreState (juce::AudioProcessor& processor, const juce::MemoryBlock& saved)
{
    processor.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
}

inline void restoreState (juce::AudioProcessor& processor, const juce::XmlElement& xml)
{
    juce::MemoryBlock saved;
    juce::AudioProcessor::copyXmlToBinary (xml, saved);
    restoreState (processor, saved);
}

// The first host parameter whose value isn't finite and in range, named with its value: its normalised
// value within 0 to 1 and its plain one within the parameter's range. Empty when every one is.
inline juce::String firstParameterOutOfRange (juce::AudioProcessor& processor)
{
    for (auto* p : processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (ranged == nullptr)
            return p->getName (64) + ": not a RangedAudioParameter";
        const float normalised = ranged->getValue();
        if (! std::isfinite (normalised) || normalised < 0.0f || normalised > 1.0f)
            return ranged->getParameterID() + ": normalised " + juce::String (normalised);
        const float plain = ranged->convertFrom0to1 (normalised);
        const auto& range = ranged->getNormalisableRange();
        if (! std::isfinite (plain) || plain < range.start || plain > range.end)
            return ranged->getParameterID() + ": " + juce::String (plain) + " outside " + juce::String (range.start) + " to " + juce::String (range.end);
    }
    return {};
}

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
