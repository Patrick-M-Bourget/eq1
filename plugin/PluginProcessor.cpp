#include "PluginProcessor.h"

#include "Parameters.h"

namespace eq1
{

PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "eq1", parameters::createLayout())
{
    for (int slot = 1; slot <= numBandSlots; ++slot)
        slots[static_cast<size_t> (slot - 1)] = { parameters.getRawParameterValue (parameters::frequencyId (slot)),
                                                  parameters.getRawParameterValue (parameters::gainId (slot)),
                                                  parameters.getRawParameterValue (parameters::qId (slot)),
                                                  parameters.getRawParameterValue (parameters::inUseId (slot)),
                                                  parameters.getRawParameterValue (parameters::bypassId (slot)),
                                                  parameters.getRawParameterValue (parameters::shapeId (slot)),
                                                  parameters.getRawParameterValue (parameters::slopeId (slot)),
                                                  parameters.getRawParameterValue (parameters::brickwallId (slot)),
                                                  parameters.getRawParameterValue (parameters::placementId (slot)) };
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
    {
        const auto& p = slots[slot];
        settings.bands[slot] = { .inUse = p.inUse->load() >= 0.5f,
                                 .bypass = p.bypass->load() >= 0.5f,
                                 .shape = static_cast<Shape> (juce::roundToInt (p.shape->load())),
                                 .frequency = p.frequency->load(),
                                 .gain = p.gain->load(),
                                 .q = p.q->load(),
                                 .slope = p.slope->load(),
                                 .brickwall = p.brickwall->load() >= 0.5f,
                                 .placement = static_cast<StereoPlacement> (juce::roundToInt (p.placement->load())) };
    }
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
