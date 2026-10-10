#include "EditorHarness.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <vector>

// How long the whole editor takes to draw a busy frame at 2x (docs/performance.md, "Paint time"). It
// measures and prints; it has no ceiling yet (#79). Hidden from the normal run, whose timings it
// would flake: run it with `build/tests/eq1_plugin_tests "[paint]"`. EQ1_PAINT_SNAPSHOT=<file.png>
// also saves the frame.
TEST_CASE ("Paint time: the editor at 1200 x 760 and 2x with 24 Dynamic Bells, every spectrum and the Output Meter", "[.paint]")
{
    harness::OpenEditor host;
    host.editor->setSize (1200, 760);
    auto& processor = host.processor;
    const auto stereo = juce::AudioChannelSet::stereo();
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (stereo);
    layout.inputBuses.add (stereo); // the Sidechain
    layout.outputBuses.add (stereo);
    REQUIRE (processor.setBusesLayout (layout));
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    processor.prepareToPlay (sampleRate, blockSize);

    // 24 Dynamic Bells, a third of an octave apart from 30 Hz, Gains alternating; Band 12 at 0 dB, selected.
    for (int slot = 1; slot <= 24; ++slot)
    {
        const auto frequency = static_cast<float> (30.0 * std::pow (2.0, (slot - 1) / 3.0));
        host.addBand (slot, frequency, slot == 12 ? 0.0f : (slot % 2 == 0 ? 4.0f : -4.0f));
        host.set (slot, "dynamic_range", -6.0f);
    }
    processor.setAnalyzerSettings ({ .showPreEq = true, .showPostEq = true, .showSidechain = true, .peakHold = true });
    processor.setOutputMeterShown (true);
    host.settle();
    host.click (host.at (30.0 * std::pow (2.0, 11.0 / 3.0)));
    host.settle();
    // EQ1_DISPLAY_SNAPSHOT=<file.png> saves the display alone before any audio, the same on every run.
    if (const auto path = juce::SystemStats::getEnvironmentVariable ("EQ1_DISPLAY_SNAPSHOT", {}); path.isNotEmpty())
    {
        juce::File file (path);
        file.deleteFile();
        juce::FileOutputStream stream (file);
        juce::PNGImageFormat().writeImageToStream (host.display.createComponentSnapshot (host.display.getLocalBounds(), true, 2.0f), stream);
    }

    // Noise on the main input and the Sidechain, so every spectrum, Peak Hold and the meter have content.
    std::mt19937 random (1);
    std::uniform_real_distribution<float> noise (-0.5f, 0.5f);
    const auto play = [&] {
        juce::AudioBuffer<float> buffer (processor.getTotalNumInputChannels(), blockSize);
        juce::MidiBuffer midi;
        for (int i = 0; i < 8; ++i)
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                for (int n = 0; n < blockSize; ++n)
                    buffer.setSample (ch, n, noise (random));
            processor.processBlock (buffer, midi);
        }
    };
    for (int warm = 0; warm < 10; ++warm)
    {
        play();
        host.settle (20);
    }

    std::vector<double> milliseconds;
    juce::Image frame;
    for (int i = 0; i < 20; ++i)
    {
        play();
        host.settle (20);
        const auto start = juce::Time::getMillisecondCounterHiRes();
        frame = host.editor->createComponentSnapshot (host.editor->getLocalBounds(), true, 2.0f);
        milliseconds.push_back (juce::Time::getMillisecondCounterHiRes() - start);
    }
    std::sort (milliseconds.begin(), milliseconds.end());
    const double median = milliseconds[milliseconds.size() / 2];
    std::cout << "Paint time: median " << median << " ms over " << milliseconds.size() << " frames ("
              << frame.getWidth() << " x " << frame.getHeight() << " px)" << std::endl;
    CHECK (frame.getWidth() == 2400);
    CHECK (median > 0.0);

    if (const auto path = juce::SystemStats::getEnvironmentVariable ("EQ1_PAINT_SNAPSHOT", {}); path.isNotEmpty())
    {
        juce::File file (path);
        file.deleteFile();
        juce::FileOutputStream stream (file);
        juce::PNGImageFormat().writeImageToStream (frame, stream);
    }
}
