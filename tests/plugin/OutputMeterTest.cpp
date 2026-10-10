#include "EditorHarness.h"
#include "EqDisplay.h"
#include "FooterBar.h"
#include "OutputMeter.h"
#include "PluginProcessor.h"
#include "staple/Tokens.h"

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
    harness::useLayout (processor, layout);
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
    harness::useLayout (processor, juce::AudioChannelSet::stereo());

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
    harness::useLayout (processor, juce::AudioChannelSet::stereo());
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
    harness::useLayout (processor, juce::AudioChannelSet::stereo());
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
    harness::useLayout (processor, juce::AudioChannelSet::stereo());
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

TEST_CASE ("The output popover's Output Meter toggle hides the Output Meter, the EQ display taking its rail, and a reopened editor follows the choice")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    const auto* constrainer = editor->getConstrainer();
    REQUIRE (constrainer != nullptr);
    editor->setSize (constrainer->getMinimumWidth(), constrainer->getMinimumHeight());

    auto* display = harness::findChild<eq1::EqDisplay> (*editor);
    auto* meter = harness::findChild<eq1::OutputMeter> (*editor);
    harness::findChild<eq1::OutputReadout> (*editor)->onClick();
    auto* button = harness::findChild<juce::Button> (*editor, [] (juce::Button& b) { return b.getTitle() == "Output Meter"; });
    REQUIRE (display != nullptr);
    REQUIRE (meter != nullptr);
    REQUIRE (button != nullptr);
    CHECK (button->getToggleState());
    CHECK (editor->getLocalBounds().contains (editor->getLocalArea (button->getParentComponent(), button->getBounds())));
    // Beside the EQ display, 12 px from it, its full height.
    CHECK (meter->isVisible());
    CHECK (meter->getX() == display->getRight() + 12);
    CHECK (meter->getY() == display->getY());
    CHECK (meter->getHeight() == display->getHeight());
    CHECK (meter->getWidth() == 40);
    const int shownWidth = display->getWidth();

    button->setToggleState (false, juce::sendNotificationSync);
    CHECK_FALSE (processor.isOutputMeterShown());
    CHECK_FALSE (meter->isVisible());
    CHECK (display->getWidth() == shownWidth + 52);

    editor.reset (processor.createEditor());
    editor->setSize (constrainer->getMinimumWidth(), constrainer->getMinimumHeight());
    CHECK_FALSE (harness::findChild<eq1::OutputMeter> (*editor)->isVisible());
    CHECK_FALSE (harness::findChild<juce::Button> (*editor, [] (juce::Button& b) { return b.getTitle() == "Output Meter"; })->getToggleState());
}

namespace
{

// The Output Meter, height px tall in its 40 px rail, as it paints after reading the Output Level once.
juce::Image paintMeter (eq1::OutputMeter& meter, int height)
{
    meter.setBounds (0, 0, 40, height);
    return harness::paintImage (40, height, [&] (juce::Graphics& g) { meter.paintEntireComponent (g, false); });
}

// The columns of row y painted in colour, give or take tolerance on each channel and alpha.
std::vector<int> columnsIn (const juce::Image& image, int y, juce::Colour colour, int tolerance = 4)
{
    std::vector<int> columns;
    for (int x = 0; x < image.getWidth(); ++x)
    {
        const auto pixel = image.getPixelAt (x, y);
        if (std::abs (pixel.getAlpha() - colour.getAlpha()) <= tolerance && std::abs (pixel.getRed() - colour.getRed()) <= tolerance
            && std::abs (pixel.getGreen() - colour.getGreen()) <= tolerance && std::abs (pixel.getBlue() - colour.getBlue()) <= tolerance)
            columns.push_back (x);
    }
    return columns;
}

bool near (juce::Colour actual, juce::Colour expected, int tolerance = 4)
{
    return std::abs (actual.getAlpha() - expected.getAlpha()) <= tolerance && std::abs (actual.getRed() - expected.getRed()) <= tolerance
           && std::abs (actual.getGreen() - expected.getGreen()) <= tolerance && std::abs (actual.getBlue() - expected.getBlue()) <= tolerance;
}

// Where the rail draws db: its bars run from 22 px down (10 padding, the 4 px Clip Lights, an 8 px
// gap) to 12 px above the bottom.
float yOf (double db, int height)
{
    const float top = 22.0f, bottom = static_cast<float> (height - 12);
    return bottom - static_cast<float> (eq1::OutputMeter::position (db)) * (bottom - top);
}

} // namespace

TEST_CASE ("The Output Meter's rail: a 6 x 4 Clip Light at the top and a 6 px bar on a dark track per channel, 3 px apart and centred")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    const auto layout = GENERATE (juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo());
    harness::useLayout (processor, layout);
    eq1::OutputMeter meter (processor);
    const auto image = paintMeter (meter, 500);

    const std::vector<int> expected = layout.size() == 1 ? std::vector<int> { 17, 18, 19, 20, 21, 22 }
                                                         : std::vector<int> { 12, 13, 14, 15, 16, 17, 21, 22, 23, 24, 25, 26 };
    // Silent, the bars show only their track, and the Clip Lights are off.
    CHECK (columnsIn (image, 300, staple::tokens::colour::meterTrack) == expected);
    CHECK (columnsIn (image, 12, staple::tokens::colour::meterClipOff) == expected);
    CHECK (columnsIn (image, 8, staple::tokens::colour::meterClipOff).empty());
    CHECK (columnsIn (image, 15, staple::tokens::colour::meterClipOff).empty());
    // No background.
    CHECK (image.getPixelAt (2, 300).getAlpha() == 0);
}

TEST_CASE ("The Output Meter's colours are fixed to the scale: -10 dBFS RMS draws in the meter2-meter3 blend at any rail height")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    harness::useLayout (processor, juce::AudioChannelSet::stereo());
    // A sine with RMS -10 dBFS (peak -7 dBFS).
    playSine (processor, static_cast<float> (std::sqrt (2.0) * std::pow (10.0, -0.5)));
    eq1::OutputMeter meter (processor);
    // -10 dBFS is 60 % of the way from meter2 at -16 dBFS to meter3 at -6 dBFS.
    const auto expected = staple::tokens::colour::meter2.interpolatedWith (staple::tokens::colour::meter3, 0.6f);
    for (const int height : { 300, 612 })
    {
        CAPTURE (height);
        const auto image = paintMeter (meter, height);
        const auto pixel = image.getPixelAt (14, juce::roundToInt (yOf (-10.0, height)) + 1);
        CAPTURE (pixel.toString(), expected.toString());
        CHECK (near (pixel, expected, 6));
        // Lower down, the colour is the scale's, not the same as at -10 dBFS.
        CHECK_FALSE (near (image.getPixelAt (14, juce::roundToInt (yOf (-40.0, height))), expected, 20));
    }
}

TEST_CASE ("A lit Clip Light draws in meterClip, and clicking either Clip Light puts out both")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    harness::useLayout (processor, juce::AudioChannelSet::stereo());
    playSine (processor, 2.0f, { 0 });
    eq1::OutputMeter meter (processor);
    auto image = paintMeter (meter, 500);
    CHECK (columnsIn (image, 12, staple::tokens::colour::meterClip) == std::vector<int> { 21, 22, 23, 24, 25, 26 });
    CHECK (columnsIn (image, 12, staple::tokens::colour::meterClipOff) == std::vector<int> { 12, 13, 14, 15, 16, 17 });

    // A click a few px off the unlit left light, within its 10 px tall click area.
    meter.mouseDown (harness::mouseEvent (meter, { 14.0f, 16.0f }, juce::ModifierKeys::leftButtonModifier));
    CHECK_FALSE (processor.isClipLit (0));
    CHECK_FALSE (processor.isClipLit (1));
    image = paintMeter (meter, 500);
    CHECK (columnsIn (image, 12, staple::tokens::colour::meterClip).empty());
}

TEST_CASE ("Output Meter snapshots: a playing signal, and a lit Clip Light", "[.screens]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    harness::useLayout (processor, juce::AudioChannelSet::stereo());
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1200, 760);
    const auto write = [&] (const char* name) {
        harness::settle (51); // three frames
        harness::writeSnapshot (*editor, juce::String ("meter-") + name, editor->getLocalBounds().removeFromRight (160));
    };
    // Peaks at -4 dBFS on the left and -7 dBFS on the right.
    playSine (processor, 0.63f, { 1 });
    playSine (processor, 0.45f, { 0 });
    playSine (processor, 0.63f);
    write ("playing");
    playSine (processor, 1.5f, { 0 });
    playSine (processor, 0.5f);
    write ("clip");
}
