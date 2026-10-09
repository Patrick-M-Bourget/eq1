#include "PluginProcessor.h"

#include "Parameters.h"
#include "PluginEditor.h"

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
const juce::Identifier displayRangeProperty { "displayRangeDb" };
const juce::Identifier analyzerType { "Analyzer" }, showPreEqProperty { "showPreEq" }, showPostEqProperty { "showPostEq" },
    showSidechainProperty { "showSidechain" },
    rangeProperty { "rangeDb" }, speedProperty { "speed" }, resolutionProperty { "resolution" }, analyzerTiltProperty { "tiltDbPerOctave" };

juce::ValueTree toTree (const AnalyzerSettings& a)
{
    return juce::ValueTree (analyzerType)
        .setProperty (showPreEqProperty, a.showPreEq, nullptr)
        .setProperty (showPostEqProperty, a.showPostEq, nullptr)
        .setProperty (showSidechainProperty, a.showSidechain, nullptr)
        .setProperty (rangeProperty, a.rangeDb, nullptr)
        .setProperty (speedProperty, static_cast<int> (a.speed), nullptr)
        .setProperty (resolutionProperty, static_cast<int> (a.resolution), nullptr)
        .setProperty (analyzerTiltProperty, a.tiltDbPerOctave, nullptr);
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
             .tiltDbPerOctave = juce::jlimit (0.0, 6.0, static_cast<double> (tree.getProperty (analyzerTiltProperty, defaults.tiltDbPerOctave))) };
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

void PluginProcessor::setDisplayRangeDb (int rangeDb)
{
    displayRange = rangeDb == 6 || rangeDb == 30 ? rangeDb : 12;
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.setProperty (displayRangeProperty, displayRangeDb(), nullptr);
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
        auto state = juce::ValueTree::fromXml (*xml);
        setDisplayRangeDb (state.getProperty (displayRangeProperty, 12));
        state.removeProperty (displayRangeProperty, nullptr);
        if (auto saved = state.getChildWithName (analyzerType); saved.isValid())
        {
            setAnalyzerSettings (fromTree (saved));
            state.removeChild (saved, nullptr);
        }
        parameters.replaceState (state);
    }
}

} // namespace eq1

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new eq1::PluginProcessor();
}
