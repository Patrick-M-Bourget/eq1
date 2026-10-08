#pragma once

#include "Parameters.h"
#include "eq1/Engine.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

namespace eq1
{

class PluginProcessor final : public juce::AudioProcessor
{
public:
    PluginProcessor();

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    // False on a mono track, where the editor offers no Stereo Placement. Each Band keeps its stored
    // Stereo Placement there, and the sound follows what a mono signal has: all Mid, Left and Right the same.
    bool isStereoPlacementAvailable() const { return getMainBusNumOutputChannels() > 1; }

    // The host parameters and the editor's settings, saved with the plugin.
    juce::AudioProcessorValueTreeState& parameterState() { return parameters; }

    // The EQ display's Gain range, +/- this many dB: 6, 12 or 30. Saved with the plugin.
    int displayRangeDb() const { return displayRange.load(); }
    void setDisplayRangeDb (int rangeDb);

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    juce::AudioProcessorValueTreeState parameters;
    std::array<eq1::parameters::SlotValues, numBandSlots> slots; // read on the audio thread

    Engine engine;
    // Kept out of the parameter state, which a host may save from another thread, and written into
    // a copy of it when saving.
    std::atomic<int> displayRange { 12 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};

} // namespace eq1
