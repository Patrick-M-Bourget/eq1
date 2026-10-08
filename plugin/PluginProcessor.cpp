#include "PluginProcessor.h"

#include "Parameters.h"

namespace eq1
{

PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "eq1", parameters::createLayout()),
      frequency (*parameters.getRawParameterValue (parameters::band1Frequency)),
      gain (*parameters.getRawParameterValue (parameters::band1Gain)),
      q (*parameters.getRawParameterValue (parameters::band1Q))
{
}

void PluginProcessor::prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock)
{
    engine.prepare (sampleRate, maximumExpectedSamplesPerBlock, getTotalNumOutputChannels());
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
           && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Settings are read from the host parameters on the audio thread, so the Engine needs no handoff yet.
    Settings settings;
    settings.bands[0] = { .inUse = true,
                          .shape = Shape::Bell,
                          .frequency = frequency.load(),
                          .gain = gain.load(),
                          .q = q.load() };
    engine.setSettings (settings);
    engine.process ({ buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples() });
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes); xml != nullptr && xml->hasTagName (parameters.state.getType()))
        parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

} // namespace eq1

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new eq1::PluginProcessor();
}
