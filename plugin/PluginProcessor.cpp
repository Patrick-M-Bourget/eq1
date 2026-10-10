#include "PluginProcessor.h"

#include "Parameters.h"
#include "PluginEditor.h"
#include "PresetSettings.h"

namespace eq1
{

PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "eq1", parameters::createLayout()),
      output (eq1::parameters::OutputValues::of (parameters))
{
    for (int slot = 1; slot <= numBandSlots; ++slot)
        slots[static_cast<size_t> (slot - 1)] = eq1::parameters::SlotValues::of (parameters, slot);
    seenGains = currentHeardGains();
}

void PluginProcessor::prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock)
{
    engine.prepare (sampleRate, maximumExpectedSamplesPerBlock, getTotalNumOutputChannels());
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Mono and stereo tracks, the same in and out, with a stereo, mono or no Sidechain.
    const auto& main = layouts.getMainOutputChannelSet();
    const auto sidechain = layouts.getChannelSet (true, 1);
    return layouts.getMainInputChannelSet() == main
           && (main == juce::AudioChannelSet::mono() || main == juce::AudioChannelSet::stereo())
           && (sidechain.isDisabled() || sidechain == juce::AudioChannelSet::mono() || sidechain == juce::AudioChannelSet::stereo());
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Host Automation arrives on the audio thread, so the snapshot is taken here, once per block.
    Settings settings;
    for (size_t slot = 0; slot < slots.size(); ++slot)
        settings.bands[slot] = slots[slot].read();
    settings.soloSlot = heldSoloSlot.load();
    settings.auditionSlot = heldAuditionSlot.load();
    settings.meteredSlot = heldMeteredSlot.load();
    output.readInto (settings);
    engine.setSettings (settings);

    auto main = getBusBuffer (buffer, false, 0);
    const auto sidechainBuffer = getBusBuffer (buffer, true, 1);
    const ConstAudioBlock sidechain { sidechainBuffer.getArrayOfReadPointers(), sidechainBuffer.getNumChannels(), sidechainBuffer.getNumSamples() };
    engine.process ({ main.getArrayOfWritePointers(), main.getNumChannels(), main.getNumSamples() },
                    sidechain.numChannels > 0 ? &sidechain : nullptr);
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

namespace
{
const juce::Identifier versionProperty { "version" }, displayRangeProperty { "displayRangeDb" }, outputMeterShownProperty { "outputMeterShown" },
    editorWidthProperty { "editorWidth" }, editorHeightProperty { "editorHeight" }, uiScaleProperty { "uiScalePercent" };

// Brings a saved state from an older version up to stateVersion, one version at a time.
void migrate (juce::ValueTree& state)
{
    const int version = state.getProperty (versionProperty, 0);
    // 0 to 1: the state gained its version and nothing else.
    if (version < 1)
        state.setProperty (versionProperty, 1, nullptr);
    // 1 to 2: A/B Compare. The settings are side A's, and B is a copy of them.
    if (version < 2)
    {
        state.appendChild (ABCompare::initialState(), nullptr);
        state.setProperty (versionProperty, 2, nullptr);
    }
    // 2 to 3: each side's Loaded Preset. Neither side has one.
    if (version < 3)
        state.setProperty (versionProperty, 3, nullptr);
}
const juce::Identifier analyzerType { "Analyzer" }, showPreEqProperty { "showPreEq" }, showPostEqProperty { "showPostEq" },
    showSidechainProperty { "showSidechain" },
    rangeProperty { "rangeDb" }, speedProperty { "speed" }, resolutionProperty { "resolution" }, analyzerTiltProperty { "tiltDbPerOctave" },
    peakHoldProperty { "peakHold" };

juce::ValueTree toTree (const AnalyzerSettings& a)
{
    return juce::ValueTree (analyzerType)
        .setProperty (showPreEqProperty, a.showPreEq, nullptr)
        .setProperty (showPostEqProperty, a.showPostEq, nullptr)
        .setProperty (showSidechainProperty, a.showSidechain, nullptr)
        .setProperty (rangeProperty, a.rangeDb, nullptr)
        .setProperty (speedProperty, static_cast<int> (a.speed), nullptr)
        .setProperty (resolutionProperty, static_cast<int> (a.resolution), nullptr)
        .setProperty (analyzerTiltProperty, a.tiltDbPerOctave, nullptr)
        .setProperty (peakHoldProperty, a.peakHold, nullptr);
}

AnalyzerSettings fromTree (const juce::ValueTree& tree)
{
    const AnalyzerSettings defaults;
    const int range = tree.getProperty (rangeProperty, defaults.rangeDb);
    return { .showPreEq = tree.getProperty (showPreEqProperty, defaults.showPreEq),
             .showPostEq = tree.getProperty (showPostEqProperty, defaults.showPostEq),
             .showSidechain = tree.getProperty (showSidechainProperty, defaults.showSidechain),
             .rangeDb = range == 60 || range == 120 ? range : 90,
             .speed = static_cast<AnalyzerSpeed> (juce::jlimit (static_cast<int> (AnalyzerSpeed::verySlow), static_cast<int> (AnalyzerSpeed::veryFast),
                                                                static_cast<int> (tree.getProperty (speedProperty, static_cast<int> (defaults.speed))))),
             .resolution = static_cast<AnalyzerResolution> (
                 juce::jlimit (static_cast<int> (AnalyzerResolution::low), static_cast<int> (AnalyzerResolution::maximum),
                               static_cast<int> (tree.getProperty (resolutionProperty, static_cast<int> (defaults.resolution))))),
             .tiltDbPerOctave = juce::jlimit (0.0, 6.0, static_cast<double> (tree.getProperty (analyzerTiltProperty, defaults.tiltDbPerOctave))),
             .peakHold = tree.getProperty (peakHoldProperty, defaults.peakHold) };
}
} // namespace

AnalyzerSettings PluginProcessor::analyzerSettings() const
{
    const juce::SpinLock::ScopedLockType lock (analyzerLock);
    return analyzer;
}

void PluginProcessor::setAnalyzerSettings (const AnalyzerSettings& settings)
{
    const juce::SpinLock::ScopedLockType lock (analyzerLock);
    analyzer = settings;
}

OutputLevel PluginProcessor::readOutputLevel (int channel)
{
    const auto level = engine.readOutputLevel (channel);
    // Above 0 dBFS: a sample beyond full scale. Full scale itself is not an over, nor the few float steps
    // past it that rounding leaves on a full-scale input (Output Gain's default reads 1.4e-6 dB).
    if (level.peakDb > clipThresholdDb && channel >= 0 && channel < static_cast<int> (clipLit.size()))
        clipLit[static_cast<size_t> (channel)] = true;
    return level;
}

bool PluginProcessor::isClipLit (int channel) const
{
    return channel >= 0 && channel < static_cast<int> (clipLit.size()) && clipLit[static_cast<size_t> (channel)].load();
}

void PluginProcessor::clearClipLights()
{
    for (auto& lit : clipLit)
        lit = false;
}

void PluginProcessor::setDisplayRangeDb (int rangeDb)
{
    displayRange = rangeDb == 6 || rangeDb == 30 ? rangeDb : 12;
}

void PluginProcessor::setEditorSize (juce::Point<int> logical)
{
    if (logical.x > 0 && logical.y > 0)
    {
        editorWidth = logical.x;
        editorHeight = logical.y;
    }
}

void PluginProcessor::setUiScalePercent (int percent)
{
    if (uiScale::isOffered (percent))
        uiScale = percent;
}

HeardGains PluginProcessor::currentHeardGains() const
{
    Settings settings;
    for (size_t slot = 0; slot < slots.size(); ++slot)
        settings.bands[slot] = slots[slot].read();
    output.readInto (settings);
    return heardGains (settings);
}

void PluginProcessor::fitDisplayRangeToHeardGains()
{
    if (history.isEditing())
        return;
    const auto now = currentHeardGains();
    const juce::SpinLock::ScopedLockType lock (seenGainsLock);
    setDisplayRangeDb (fittedDisplayRangeDb (displayRangeDb(), seenGains, now));
    seenGains = now;
}

juce::ValueTree PluginProcessor::presetState()
{
    return capturePresetSettings (parameters, parameters.state.getType()).setProperty (versionProperty, stateVersion, nullptr);
}

bool PluginProcessor::loadPreset (const juce::ValueTree& preset, const juce::String& name)
{
    if (! preset.hasType (parameters.state.getType()))
        return false;
    auto settings = preset.createCopy();
    migrate (settings);
    compare.loadPreset (settings, name);
    return true;
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = compare.savedState();
    state.setProperty (versionProperty, stateVersion, nullptr);
    state.setProperty (displayRangeProperty, displayRangeDb(), nullptr);
    state.setProperty (outputMeterShownProperty, isOutputMeterShown(), nullptr);
    state.setProperty (editorWidthProperty, editorWidth.load(), nullptr);
    state.setProperty (editorHeightProperty, editorHeight.load(), nullptr);
    state.setProperty (uiScaleProperty, uiScalePercent(), nullptr);
    state.appendChild (toTree (analyzerSettings()), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes); xml != nullptr && xml->hasTagName (parameters.state.getType()))
    {
        setSolo (0);
        setDetectionAudition (0);
        setMeteredBand (0);
        clearClipLights();
        auto state = juce::ValueTree::fromXml (*xml);
        migrate (state);
        state.removeProperty (versionProperty, nullptr);
        setDisplayRangeDb (state.getProperty (displayRangeProperty, 12));
        state.removeProperty (displayRangeProperty, nullptr);
        // Shown in a session saved before the Output Meter.
        setOutputMeterShown (state.getProperty (outputMeterShownProperty, true));
        state.removeProperty (outputMeterShownProperty, nullptr);
        // A session saved before them opens like a new instance.
        setEditorSize ({ state.getProperty (editorWidthProperty, newEditorWidth), state.getProperty (editorHeightProperty, newEditorHeight) });
        setUiScalePercent (state.getProperty (uiScaleProperty, uiScale::defaultPercent));
        for (const auto& property : { editorWidthProperty, editorHeightProperty, uiScaleProperty })
            state.removeProperty (property, nullptr);
        if (auto saved = state.getChildWithName (analyzerType); saved.isValid())
        {
            setAnalyzerSettings (fromTree (saved));
            state.removeChild (saved, nullptr);
        }
        // Without one (from a newer version that saves it elsewhere), the settings are side A's.
        const auto savedCompare = state.getChildWithName (ABCompare::stateType);
        compare.restore (savedCompare);
        state.removeChild (savedCompare, nullptr);
        parameters.replaceState (state);
        history.sessionRestored();
        const auto restored = currentHeardGains();
        const juce::SpinLock::ScopedLockType lock (seenGainsLock);
        seenGains = restored;
    }
}

} // namespace eq1

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new eq1::PluginProcessor();
}
