#include "PluginProcessor.h"

#include "Parameters.h"
#include "PluginEditor.h"

namespace eq1
{

PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "eq1", parameters::createLayout())
{
    for (int slot = 1; slot <= numBandSlots; ++slot)
        slots[static_cast<size_t> (slot - 1)] = eq1::parameters::SlotValues::of (parameters, slot);
}

void PluginProcessor::prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock)
{
    engine.prepare (sampleRate, maximumExpectedSamplesPerBlock, getTotalNumOutputChannels());
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Mono and stereo tracks, the same in and out.
    const auto& main = layouts.getMainOutputChannelSet();
    return layouts.getMainInputChannelSet() == main
           && (main == juce::AudioChannelSet::mono() || main == juce::AudioChannelSet::stereo());
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Host Automation arrives on the audio thread, so the snapshot is taken here, once per block.
    Settings settings;
    for (size_t slot = 0; slot < slots.size(); ++slot)
        settings.bands[slot] = slots[slot].read();
    engine.setSettings (settings);
    engine.process ({ buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples() });
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

namespace
{
const juce::Identifier displayRangeProperty { "displayRangeDb" };
}

void PluginProcessor::setDisplayRangeDb (int rangeDb)
{
    displayRange = rangeDb == 6 || rangeDb == 30 ? rangeDb : 12;
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.setProperty (displayRangeProperty, displayRangeDb(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes); xml != nullptr && xml->hasTagName (parameters.state.getType()))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        setDisplayRangeDb (state.getProperty (displayRangeProperty, 12));
        state.removeProperty (displayRangeProperty, nullptr);
        parameters.replaceState (state);
    }
}

} // namespace eq1

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new eq1::PluginProcessor();
}
