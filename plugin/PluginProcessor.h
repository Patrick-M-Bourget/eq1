#pragma once

#include "ABCompare.h"
#include "AnalyzerSettings.h"
#include "DisplayRange.h"
#include "EditHistory.h"
#include "Parameters.h"
#include "UiScale.h"
#include "UserSettings.h"
#include "eq1/Engine.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

namespace eq1
{

class PluginProcessor final : public juce::AudioProcessor
{
public:
    // userSettingsFile keeps the UI Scale last picked, across instances (UserSettings). Without one, as in
    // tests, nothing is kept and a new instance opens at 100%.
    explicit PluginProcessor (juce::File userSettingsFile = {});

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

    // A/B Compare: which side the host parameters hold, from any thread; selecting a side and copying
    // the side you're on to the other side, each one undo step, from the message thread only.
    CompareSide compareSide() const { return compare.side(); }
    void selectCompareSide (CompareSide side) { compare.select (side); }
    void copyToOther() { compare.copyToOther(); }

    // The settings a Preset holds, from the side you're on, in the saved state's format.
    juce::ValueTree presetState();
    // Puts a Preset's settings on the side you're on and makes it the side's Loaded Preset, named
    // name, as one undo step, bringing an older version of the format up to date first. Anything that
    // isn't a Preset changes nothing and returns false. Message thread only.
    bool loadPreset (const juce::ValueTree& preset, const juce::String& name);
    // The side you're on was saved as a Preset (presetState()) named name: it becomes the side's
    // Loaded Preset, as one undo step. Message thread only.
    void presetSaved (const juce::ValueTree& preset, const juce::String& name) { compare.presetSaved (preset, name); }
    // The Loaded Preset of the side you're on, or an empty name for none, and whether the side is
    // Modified. Message thread only.
    juce::String loadedPresetName() const { return compare.loadedPresetName(); }
    bool isLoadedPresetModified() const { return compare.isModified(); }

    // The Display Range, +/- this many dB: 6, 12 or 30. Saved with the plugin, whether picked by hand
    // or zoomed out by fitDisplayRangeToHeardGains().
    int displayRangeDb() const { return displayRange.load(); }
    void setDisplayRangeDb (int rangeDb);
    // The editor's look at the Bands, each frame while it is open and when a drag ends: when a Band's
    // heard Gain has changed since the last look to beyond the Display Range, the range zooms out to
    // fit it (fittedDisplayRangeDb). Nothing while an edit is in progress, so a drag zooms when it
    // ends. Restoring a session takes its Bands as seen. Message thread only.
    void fitDisplayRangeToHeardGains();

    // Solo, while the editor holds a Band: its Band Slot (1 to 24), or 0. Not a host parameter, not
    // saved, not undoable; restoring a session lets go of it.
    void setSolo (int slot) { heldSoloSlot = slot; }
    int soloSlot() const { return heldSoloSlot.load(); }

    // Detection Audition, while the editor holds it: the Band Slot (1 to 24) whose detection signal
    // plays instead of the output, or 0. Like Solo: not a host parameter, not saved, let go on restore.
    void setDetectionAudition (int slot) { heldAuditionSlot = slot; }
    int detectionAuditionSlot() const { return heldAuditionSlot.load(); }

    // The Metered Band, while the editor shows it: its Band Slot (1 to 24), or 0. Like Solo: not a host
    // parameter, not saved, let go on restore.
    void setMeteredBand (int slot) { heldMeteredSlot = slot; }
    int meteredSlot() const { return heldMeteredSlot.load(); }
    // The Metered Band's Detection Level, in dB on Threshold's scale: the loudest since the last read,
    // or levelFloorDb. From one reader thread, the message thread.
    double readDetectionLevel() { return engine.readDetectionLevel(); }

    // A Band Slot's Live Gain in dB, as Gain Scale plays it, for the display: from any thread. Only
    // meaningful once audio has been processed since the host last prepared the plugin.
    double liveGainDb (int slot) const { return engine.liveGainDb (slot); }
    bool hasProcessedAudio() const { return processedAudio.load (std::memory_order_relaxed); }

    AnalyzerSettings analyzerSettings() const;
    void setAnalyzerSettings (const AnalyzerSettings& settings);

    // The Engine's analysis taps, for the Analyzer: from one reader thread, the message thread.
    int readAnalysis (AnalysisTap tap, float* destination, int maxSamples) { return engine.readAnalysis (tap, destination, maxSamples); }

    // The Output Meter. The Engine's Output Level of a channel (0 to outputLevelChannels() - 1), from
    // one reader thread, the message thread. A peak read above 0 dBFS lights that channel's Clip Light,
    // which stays lit, editor or not, until clearClipLights() or a restored session puts both out.
    int outputLevelChannels() const { return engine.outputLevelChannels(); }
    OutputLevel readOutputLevel (int channel);
    bool isClipLit (int channel) const;
    void clearClipLights();
    // Whether the editor shows the Output Meter. Saved with the session, not in Presets, not undoable.
    bool isOutputMeterShown() const { return outputMeterShown.load(); }
    void setOutputMeterShown (bool shown) { outputMeterShown = shown; }

    // The editor's window: its size in logical pixels (as at 100%) and its UI Scale in percent (one of
    // uiScale::percents). Saved with the session, not in Presets, not undoable. Message thread only.
    juce::Point<int> editorSize() const { return { editorWidth.load(), editorHeight.load() }; }
    void setEditorSize (juce::Point<int> logical);
    // An instance that has never had a UI Scale, new or from a session saved before it, takes the one
    // last picked in any instance here, and keeps it from then on.
    int uiScalePercent();
    // The UI Scale picked by hand: this instance's, and the default for new ones.
    void pickUiScale (int percent);

    // The version of the saved state's format. setStateInformation() brings older states up to it one
    // version at a time, and loads what it knows of newer ones. 0 is the state from before it had a
    // version. Bump it, and add a step to the migration, whenever the format changes.
    static constexpr int stateVersion = 3;

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
    std::atomic<bool> outputMeterShown { true };
    static constexpr int newEditorWidth = 1200, newEditorHeight = 760;
    std::atomic<int> editorWidth { newEditorWidth }, editorHeight { newEditorHeight };
    std::atomic<int> uiScale { 0 }; // 0 until the instance has one
    UserSettings userSettings;
    // The heard Gains at the editor's last look, or as a session restored them: not saved, and kept
    // while the editor is closed.
    HeardGains seenGains;
    juce::SpinLock seenGainsLock; // a host may restore a session from another thread
    HeardGains currentHeardGains() const;
    std::atomic<int> heldSoloSlot { 0 };
    std::atomic<bool> processedAudio { false };
    std::atomic<int> heldAuditionSlot { 0 };
    std::atomic<int> heldMeteredSlot { 0 };
    std::array<std::atomic<bool>, 2> clipLit {}; // per channel, not saved; mono and stereo only
    static constexpr double clipThresholdDb = 1.0e-5;
    AnalyzerSettings analyzer;
    mutable juce::SpinLock analyzerLock; // the editor sets it while a host may be saving

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};

} // namespace eq1
