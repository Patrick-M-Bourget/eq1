#pragma once

#include "ABCompare.h"
#include "AnalyzerSettings.h"
#include "EditHistory.h"
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

    // False on a mono track, where the editor offers no Output Pan or Pan Mode; they keep their
    // values and have no effect there.
    bool isOutputPanAvailable() const { return getMainBusNumOutputChannels() > 1; }

    // The host parameters and the editor's settings, saved with the plugin.
    juce::AudioProcessorValueTreeState& parameterState() { return parameters; }

    // The undo history of the editor's edits. It outlives the editor, and restoring a session empties it.
    EditHistory& editHistory() { return history; }

    // A/B Compare: which side the host parameters hold, and selecting the other, as one undo step.
    // Message thread only.
    CompareSide compareSide() const { return compare.side(); }
    void selectCompareSide (CompareSide side) { compare.select (side); }
    void copyAToB() { compare.copyAToB(); }

    // The settings a Preset holds, from the side you're on, in the saved state's format.
    juce::ValueTree presetState();
    // Puts a Preset's settings on the side you're on, as one undo step, bringing an older version of
    // the format up to date first. Anything that isn't a Preset changes nothing. Message thread only.
    void loadPreset (const juce::ValueTree& preset);

    // The EQ display's Gain range, +/- this many dB: 6, 12 or 30. Saved with the plugin.
    int displayRangeDb() const { return displayRange.load(); }
    void setDisplayRangeDb (int rangeDb);

    // Solo, while the editor holds a Band: its Band Slot (1 to 24), or 0. Not a host parameter, not
    // saved, not undoable; restoring a session lets go of it.
    void setSolo (int slot) { heldSoloSlot = slot; }
    int soloSlot() const { return heldSoloSlot.load(); }

    // Detection Audition, while the editor holds it: the Band Slot (1 to 24) whose detection signal
    // plays instead of the output, or 0. Like Solo: not a host parameter, not saved, let go on restore.
    void setDetectionAudition (int slot) { heldAuditionSlot = slot; }
    int detectionAuditionSlot() const { return heldAuditionSlot.load(); }

    // A Band Slot's Live Gain in dB, for the display: from any thread.
    double liveGainDb (int slot) const { return engine.liveGainDb (slot); }

    AnalyzerSettings analyzerSettings() const;
    void setAnalyzerSettings (const AnalyzerSettings& settings);

    // The Engine's analysis taps, for the Analyzer: from one reader thread, the message thread.
    int readAnalysis (AnalysisTap tap, float* destination, int maxSamples) { return engine.readAnalysis (tap, destination, maxSamples); }

    // The version of the saved state's format. setStateInformation() brings older states up to it one
    // version at a time, and loads what it knows of newer ones. 0 is the state from before it had a
    // version. Bump it, and add a step to the migration, whenever the format changes.
    static constexpr int stateVersion = 2;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    juce::AudioProcessorValueTreeState parameters;
    std::array<eq1::parameters::SlotValues, numBandSlots> slots; // read on the audio thread
    eq1::parameters::OutputValues output;
    EditHistory history { *this };
    ABCompare compare { parameters, history };

    Engine engine;
    // Kept out of the parameter state, which a host may save from another thread, and written into
    // a copy of it when saving.
    std::atomic<int> displayRange { 12 };
    std::atomic<int> heldSoloSlot { 0 };
    std::atomic<int> heldAuditionSlot { 0 };
    AnalyzerSettings analyzer;
    mutable juce::SpinLock analyzerLock; // the editor sets it while a host may be saving

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};

} // namespace eq1
