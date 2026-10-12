#include "EditorHarness.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iostream>
#include <random>
#include <vector>

// How long the whole editor takes to draw a busy frame at 2x (docs/performance.md, "Paint time"): it
// prints the median and fails above its ceiling. Hidden from the normal run, whose timings it would
// flake: run it with `scripts/check.sh paint`, or `build/tests/eq1_plugin_tests "[paint]"` on a busy
// machine. With EQ1_SCREENS set, it also writes the frame as paint-frame and the display before any
// audio as paint-display (harness::writeSnapshot).
TEST_CASE ("Paint time: the editor at 1200 x 760 and 2x with 24 Dynamic Bells, every spectrum and the Output Meter", "[.paint]")
{
    harness::OpenEditor host;
    host.editor->setSize (1200, 760);
    auto& processor = host.processor;
    harness::useLayout (processor, juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo()); // with a stereo Sidechain
    constexpr int blockSize = 512;

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
    // The display alone before any audio, the same on every run, when EQ1_SCREENS asks for it.
    harness::writeSnapshot (host.display, "paint-display");

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
    // About 3x the median measured with the Staple display on an Apple M3 (docs/performance.md).
    constexpr double ceilingMs = 200.0;
    CHECK (median < ceilingMs);

    harness::writeSnapshot (frame, "paint-frame");
}

// What an idle frame costs with the Analyzer on (docs/performance.md, "Paint time"): the display at 2x
// with 24 Bells, one selected, the Analyzer showing Pre and Post on silence and nothing changing. It
// prints the median of 20 paints of the display alone, and the message thread's CPU per tick over 2 s
// of ticks (a repaint asked of a window that never paints in a test costs nothing, so that is the
// tick's own work). Hidden like the paint time, and run with it.
TEST_CASE ("Idle frame, Analyzer on: the display at 2x with 24 Bells, one selected, nothing changing", "[.paint]")
{
    harness::OpenEditor host;
    host.editor->setSize (1200, 760);
    harness::useLayout (host.processor, juce::AudioChannelSet::stereo());
    for (int slot = 1; slot <= 24; ++slot)
        host.addBand (slot, static_cast<float> (30.0 * std::pow (2.0, (slot - 1) / 3.0)), slot == 12 ? 0.0f : (slot % 2 == 0 ? 4.0f : -4.0f));
    host.settle();
    host.click (host.at (30.0 * std::pow (2.0, 11.0 / 3.0)));
    host.settle();
    REQUIRE (host.editorTick().rateHz() == eq1::EditorTick::activeHz); // the Analyzer keeps it at 60 Hz

    std::vector<double> milliseconds;
    for (int i = 0; i < 25; ++i)
    {
        host.settle (20);
        const auto start = juce::Time::getMillisecondCounterHiRes();
        const auto frame = host.display.createComponentSnapshot (host.display.getLocalBounds(), true, 2.0f);
        if (i >= 5) // warmed up
            milliseconds.push_back (juce::Time::getMillisecondCounterHiRes() - start);
        CHECK (frame.isValid());
    }
    std::sort (milliseconds.begin(), milliseconds.end());
    const double paint = milliseconds[milliseconds.size() / 2];

    const int ticksBefore = host.editorTick().count();
    const auto cpuBefore = std::clock();
    host.settle (2000);
    const double cpuMs = 1000.0 * static_cast<double> (std::clock() - cpuBefore) / CLOCKS_PER_SEC;
    const int ticks = host.editorTick().count() - ticksBefore;
    REQUIRE (ticks > 0);
    std::cout << "Idle frame, Analyzer on: display paint median " << paint << " ms over " << milliseconds.size() << " frames; tick "
              << cpuMs / ticks << " ms CPU over " << ticks << " ticks" << std::endl;
    CHECK (paint > 0.0);
}

// The display as the Staple handoff's screenshot shows it, for checking by hand against the
// prototype: 1200 x 760 at 2x, a few Bands, the Analyzer on noise, and a selected Dynamic Band, as
// screenshot (harness::writeSnapshot).
TEST_CASE ("Screenshot: the editor with a few Bands, the Analyzer and a selected Dynamic Band", "[.screens]")
{
    harness::OpenEditor host;
    host.editor->setSize (1200, 760);
    auto& processor = host.processor;
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    processor.prepareToPlay (sampleRate, blockSize);
    host.addBand (1, 40.0f, 0.0f, 2.0f); // Low Cut
    host.addBand (2, 120.0f, 3.5f);
    host.addBand (3, 450.0f, -4.0f);
    host.set (3, "dynamic_range", 5.0f);
    host.addBand (4, 2500.0f, 2.0f);
    host.addBand (5, 9000.0f, 4.0f, 3.0f); // High Shelf
    host.set (5, "bypass", 1.0f);
    host.settle();
    // Band 3's handle, 4 dB below the middle at +/-12 dB.
    host.click (host.at (450.0).translated (0.0f, (static_cast<float> (host.display.getHeight()) * 0.5f - 9.0f) * 4.0f / 12.0f));
    std::mt19937 random (1);
    std::uniform_real_distribution<float> noise (-0.3f, 0.3f);
    for (int frame = 0; frame < 40; ++frame)
    {
        juce::AudioBuffer<float> buffer (processor.getTotalNumInputChannels(), blockSize);
        juce::MidiBuffer midi;
        for (int i = 0; i < 6; ++i)
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                for (int n = 0; n < blockSize; ++n)
                    buffer.setSample (ch, n, noise (random));
            processor.processBlock (buffer, midi);
        }
        host.settle (20);
    }
    host.settleAnimations(); // the Band panel slid open
    harness::writeSnapshot (*host.editor, "screenshot");
}
