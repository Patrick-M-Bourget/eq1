#include "EqDisplay.h"
#include "OutputMeter.h"
#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <functional>
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

// The first child of the editor, at any depth, that is a T and passes test.
template <typename T>
T* findChild (juce::Component& parent, std::function<bool (T&)> test = [] (T&) { return true; })
{
    for (auto* child : parent.getChildren())
    {
        if (auto* found = dynamic_cast<T*> (child); found != nullptr && test (*found))
            return found;
        if (auto* found = findChild<T> (*child, test))
            return found;
    }
    return nullptr;
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

    // A sample at full scale is not an over; one a float step beyond it is.
    juce::AudioBuffer<float> buffer (processor.getTotalNumInputChannels(), blockSize);
    juce::MidiBuffer midi;
    buffer.clear();
    buffer.setSample (0, 10, 1.0f);
    buffer.setSample (1, 10, -1.0f);
    processor.processBlock (buffer, midi);
    readAll (processor);
    CHECK_FALSE (processor.isClipLit (0));
    CHECK_FALSE (processor.isClipLit (1));
    buffer.setSample (0, 10, std::nextafter (1.0f, 2.0f));
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

TEST_CASE ("An Output Meter bar's peak rises at once and falls at 20 dB/s; its RMS is the RMS read")
{
    eq1::OutputMeterChannel channel;
    channel.update ({ .peakDb = -6.0, .rmsDb = -9.0 }, 1.0 / 60.0);
    CHECK_THAT (channel.peakDb(), WithinAbs (-6.0, 1.0e-9));
    CHECK_THAT (channel.rmsDb(), WithinAbs (-9.0, 1.0e-9));
    channel.update ({ .peakDb = eq1::levelFloorDb, .rmsDb = -20.0 }, 0.5);
    CHECK_THAT (channel.peakDb(), WithinAbs (-16.0, 1.0e-9));
    CHECK_THAT (channel.rmsDb(), WithinAbs (-20.0, 1.0e-9));
}

TEST_CASE ("An Output Meter bar's held-peak tick holds the highest peak for 1 s, then falls at 20 dB/s, never below the peak")
{
    eq1::OutputMeterChannel channel;
    const auto quiet = eq1::OutputLevel {};
    channel.update ({ .peakDb = -3.0, .rmsDb = -6.0 }, 0.1);
    for (int frame = 0; frame < 9; ++frame)
        channel.update (quiet, 0.1);
    CHECK_THAT (channel.heldPeakDb(), WithinAbs (-3.0, 1.0e-9)); // 0.9 s on
    CHECK_THAT (channel.peakDb(), WithinAbs (-21.0, 1.0e-9));
    for (int frame = 0; frame < 6; ++frame)
        channel.update (quiet, 0.1);
    CHECK_THAT (channel.heldPeakDb(), WithinAbs (-13.0, 1.0e-6)); // 1.5 s on: falling for 0.5 s

    // A louder peak holds again from where it is.
    channel.update ({ .peakDb = -1.0, .rmsDb = -6.0 }, 0.1);
    channel.update (quiet, 0.9);
    CHECK_THAT (channel.heldPeakDb(), WithinAbs (-1.0, 1.0e-9));
    // Falling, it stays on the peak once it meets it.
    channel.update ({ .peakDb = -2.0, .rmsDb = -6.0 }, 0.5);
    channel.update ({ .peakDb = -2.0, .rmsDb = -6.0 }, 0.5);
    CHECK_THAT (channel.heldPeakDb(), WithinAbs (-2.0, 1.0e-9));
}

TEST_CASE ("The Output Meter's scale runs linearly in dB from -60 dBFS at the bottom to +6 dBFS at the top")
{
    CHECK_THAT (eq1::OutputMeter::position (-60.0), WithinAbs (0.0, 1.0e-9));
    CHECK_THAT (eq1::OutputMeter::position (6.0), WithinAbs (1.0, 1.0e-9));
    CHECK_THAT (eq1::OutputMeter::position (0.0), WithinAbs (60.0 / 66.0, 1.0e-9));
    CHECK_THAT (eq1::OutputMeter::position (-27.0), WithinAbs (0.5, 1.0e-9));
    CHECK_THAT (eq1::OutputMeter::position (-90.0), WithinAbs (0.0, 1.0e-9));
    CHECK_THAT (eq1::OutputMeter::position (eq1::levelFloorDb), WithinAbs (0.0, 1.0e-9));
    CHECK_THAT (eq1::OutputMeter::position (12.0), WithinAbs (1.0, 1.0e-9));
}

TEST_CASE ("The toolbar's Meter button hides the Output Meter, widening the EQ display, and a reopened editor follows the choice")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    const auto* constrainer = editor->getConstrainer();
    REQUIRE (constrainer != nullptr);
    editor->setSize (constrainer->getMinimumWidth(), constrainer->getMinimumHeight());

    auto* display = findChild<eq1::EqDisplay> (*editor);
    auto* meter = findChild<eq1::OutputMeter> (*editor);
    auto* button = findChild<juce::Button> (*editor, [] (juce::Button& b) { return b.getButtonText() == "Meter"; });
    REQUIRE (display != nullptr);
    REQUIRE (meter != nullptr);
    REQUIRE (button != nullptr);
    CHECK (button->getToggleState());
    CHECK (editor->getLocalBounds().contains (editor->getLocalArea (button->getParentComponent(), button->getBounds())));
    // Beside the EQ display, its full height.
    CHECK (meter->isVisible());
    CHECK (meter->getX() == display->getRight());
    CHECK (meter->getY() == display->getY());
    CHECK (meter->getHeight() == display->getHeight());
    CHECK (meter->getWidth() == 40);
    const int shownWidth = display->getWidth();

    button->setToggleState (false, juce::sendNotificationSync);
    CHECK_FALSE (processor.isOutputMeterShown());
    CHECK_FALSE (meter->isVisible());
    CHECK (display->getWidth() == shownWidth + 40);

    editor.reset (processor.createEditor());
    editor->setSize (constrainer->getMinimumWidth(), constrainer->getMinimumHeight());
    CHECK_FALSE (findChild<eq1::OutputMeter> (*editor)->isVisible());
    CHECK_FALSE (findChild<juce::Button> (*editor, [] (juce::Button& b) { return b.getButtonText() == "Meter"; })->getToggleState());
}
