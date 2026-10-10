#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>

using Catch::Matchers::WithinAbs;

namespace
{

constexpr double sampleRate = 48000.0;
constexpr int blockSize = 512;

void setParameter (juce::AudioProcessor& processor, const juce::String& id, float value)
{
    for (auto* parameter : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter); ranged != nullptr && ranged->getParameterID() == id)
            return ranged->setValueNotifyingHost (ranged->convertTo0to1 (value));
    FAIL ("No parameter " << id);
}

// Switches the main input and output to channels, with no Sidechain, and prepares to play.
void useLayout (juce::AudioProcessor& processor, const juce::AudioChannelSet& channels)
{
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (channels);
    layout.inputBuses.add (juce::AudioChannelSet::disabled());
    layout.outputBuses.add (channels);
    REQUIRE (processor.setBusesLayout (layout));
    processor.prepareToPlay (sampleRate, blockSize);
}

// Half a second of a 1 kHz sine at amplitude on every main channel, but silence on the channels
// silent lists.
void playSine (juce::AudioProcessor& processor, float amplitude, std::initializer_list<int> silent = {})
{
    juce::AudioBuffer<float> buffer (processor.getTotalNumInputChannels(), blockSize);
    juce::MidiBuffer midi;
    int n = 0;
    for (int block = 0; block < static_cast<int> (0.5 * sampleRate / blockSize); ++block)
    {
        buffer.clear();
        for (int i = 0; i < blockSize; ++i, ++n)
            for (int ch = 0; ch < processor.getMainBusNumInputChannels(); ++ch)
                if (std::find (silent.begin(), silent.end(), ch) == silent.end())
                    buffer.setSample (ch, i, amplitude * static_cast<float> (std::sin (2.0 * std::numbers::pi * 1000.0 * n / sampleRate)));
        processor.processBlock (buffer, midi);
    }
}

// Reads every channel's Output Level, as the Output Meter does each frame.
void readAll (eq1::PluginProcessor& processor)
{
    for (int ch = 0; ch < processor.outputLevelChannels(); ++ch)
        processor.readOutputLevel (ch);
}

} // namespace

TEST_CASE ("The Output Meter reads one channel on mono and two on stereo; a full-scale sine peaks at 0 dBFS with RMS about -3 dBFS, the Clip Light off")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    const auto layout = GENERATE (juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo());
    useLayout (processor, layout);
    REQUIRE (processor.outputLevelChannels() == layout.size());

    playSine (processor, 1.0f);
    for (int ch = 0; ch < processor.outputLevelChannels(); ++ch)
    {
        CAPTURE (ch);
        const auto level = processor.readOutputLevel (ch);
        CHECK_THAT (level.peakDb, WithinAbs (0.0, 0.01));
        CHECK_THAT (level.rmsDb, WithinAbs (-3.01, 0.05));
        CHECK_FALSE (processor.isClipLit (ch));
    }
}

TEST_CASE ("An over lights its channel's Clip Light, which stays lit until cleared, and clearing puts out both")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    useLayout (processor, juce::AudioChannelSet::stereo());

    // A sample at full scale is not an over; one a thousandth of a dB beyond it is.
    juce::AudioBuffer<float> buffer (processor.getTotalNumInputChannels(), blockSize);
    juce::MidiBuffer midi;
    buffer.clear();
    buffer.setSample (0, 10, 1.0f);
    buffer.setSample (1, 10, -1.0f);
    processor.processBlock (buffer, midi);
    readAll (processor);
    CHECK_FALSE (processor.isClipLit (0));
    CHECK_FALSE (processor.isClipLit (1));
    buffer.setSample (0, 10, juce::Decibels::decibelsToGain (0.001f));
    processor.processBlock (buffer, midi);
    readAll (processor);
    CHECK (processor.isClipLit (0));
    processor.clearClipLights();

    playSine (processor, 1.5f, { 0 });
    readAll (processor);
    CHECK_FALSE (processor.isClipLit (0));
    CHECK (processor.isClipLit (1));

    // Quiet again, read again, prepared again at another sample rate: still lit.
    playSine (processor, 0.1f);
    readAll (processor);
    processor.prepareToPlay (96000.0, 256);
    readAll (processor);
    CHECK (processor.isClipLit (1));

    playSine (processor, 1.5f, { 1 });
    readAll (processor);
    REQUIRE (processor.isClipLit (0));
    processor.clearClipLights();
    CHECK_FALSE (processor.isClipLit (0));
    CHECK_FALSE (processor.isClipLit (1));
}

TEST_CASE ("An over while nothing reads the Output Level, as with the editor closed, lights the Clip Light on the next read")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    useLayout (processor, juce::AudioChannelSet::stereo());
    playSine (processor, 2.0f);
    playSine (processor, 0.1f);
    CHECK_FALSE (processor.isClipLit (0));
    readAll (processor);
    CHECK (processor.isClipLit (0));
    CHECK (processor.isClipLit (1));
}

TEST_CASE ("Restoring a session puts out the Clip Lights, and they are not saved")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    useLayout (processor, juce::AudioChannelSet::stereo());
    playSine (processor, 2.0f);
    readAll (processor);
    REQUIRE (processor.isClipLit (0));

    juce::MemoryBlock state;
    processor.getStateInformation (state);
    processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    CHECK_FALSE (processor.isClipLit (0));
    CHECK_FALSE (processor.isClipLit (1));
    readAll (processor);
    CHECK_FALSE (processor.isClipLit (0));
}

TEST_CASE ("During Global Bypass the Output Meter reads the input")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    useLayout (processor, juce::AudioChannelSet::stereo());
    setParameter (processor, "output_gain", -12.0f);
    playSine (processor, 1.0f);
    REQUIRE_THAT (processor.readOutputLevel (0).peakDb, WithinAbs (-12.0, 0.05));

    setParameter (processor, "global_bypass", 1.0f);
    playSine (processor, 1.0f);
    CHECK_THAT (processor.readOutputLevel (0).peakDb, WithinAbs (0.0, 0.01));
}
