#include "../engine/Measure.h"
#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>
#include <vector>

using Catch::Matchers::WithinAbs;
using eq1::CompareSide;

namespace
{

// A plugin as a host has it.
struct Host
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;

    juce::RangedAudioParameter& parameter (const juce::String& id) { return *processor.parameterState().getParameter (id); }
    float value (const juce::String& id) { return processor.parameterState().getRawParameterValue (id)->load(); }

    // An edit in the editor: inside a gesture.
    void edit (const juce::String& id, float plain)
    {
        auto& p = parameter (id);
        p.beginChangeGesture();
        p.setValueNotifyingHost (p.convertTo0to1 (plain));
        p.endChangeGesture();
    }
};

} // namespace

TEST_CASE ("A/B Compare starts on A, and B starts as a copy of A")
{
    Host host;
    CHECK (host.processor.compareSide() == CompareSide::A);
    host.edit ("band1_in_use", 1.0f);
    host.edit ("band1_gain", 6.0f);

    host.processor.selectCompareSide (CompareSide::B);
    CHECK (host.processor.compareSide() == CompareSide::B);
    CHECK (host.value ("band1_in_use") == 1.0f);
    CHECK_THAT (host.value ("band1_gain"), WithinAbs (6.0, 1.0e-4));
}

TEST_CASE ("Each side keeps its own settings")
{
    Host host;
    host.edit ("band1_in_use", 1.0f);
    host.edit ("band1_gain", 6.0f);
    host.processor.selectCompareSide (CompareSide::B);
    host.edit ("band1_gain", -3.0f);
    host.edit ("output_gain", 4.0f);

    host.processor.selectCompareSide (CompareSide::A);
    CHECK_THAT (host.value ("band1_gain"), WithinAbs (6.0, 1.0e-4));
    CHECK_THAT (host.value ("output_gain"), WithinAbs (0.0, 1.0e-4));

    host.processor.selectCompareSide (CompareSide::B);
    CHECK_THAT (host.value ("band1_gain"), WithinAbs (-3.0, 1.0e-4));
    CHECK_THAT (host.value ("output_gain"), WithinAbs (4.0, 1.0e-4));
}

TEST_CASE ("Global Bypass belongs to neither side: switching sides leaves it alone")
{
    Host host;
    host.edit ("global_bypass", 1.0f);
    host.processor.selectCompareSide (CompareSide::B);
    CHECK (host.value ("global_bypass") == 1.0f);
    host.edit ("global_bypass", 0.0f);
    host.processor.selectCompareSide (CompareSide::A);
    CHECK (host.value ("global_bypass") == 0.0f);
}

TEST_CASE ("Copy A to B, from either side, overwrites B with A")
{
    Host host;
    const bool fromB = GENERATE (false, true);
    CAPTURE (fromB);
    host.edit ("band2_in_use", 1.0f);
    host.processor.selectCompareSide (CompareSide::B);
    host.edit ("band2_in_use", 0.0f);
    host.edit ("gain_scale", 50.0f);
    host.processor.selectCompareSide (CompareSide::A);
    host.edit ("band2_frequency", 120.0f);
    if (fromB)
        host.processor.selectCompareSide (CompareSide::B);

    host.processor.copyAToB();
    CHECK (host.processor.compareSide() == (fromB ? CompareSide::B : CompareSide::A));
    host.processor.selectCompareSide (CompareSide::B);
    CHECK (host.value ("band2_in_use") == 1.0f);
    CHECK_THAT (host.value ("band2_frequency"), WithinAbs (120.0, 1.0e-3));
    CHECK_THAT (host.value ("gain_scale"), WithinAbs (100.0, 1.0e-3));

    // A is unchanged.
    host.processor.selectCompareSide (CompareSide::A);
    CHECK (host.value ("band2_in_use") == 1.0f);
    CHECK_THAT (host.value ("band2_frequency"), WithinAbs (120.0, 1.0e-3));
}

TEST_CASE ("Undo crosses an A/B switch: one history for both sides, a switch is one step")
{
    Host host;
    auto& history = host.processor.editHistory();
    host.edit ("band1_gain", 6.0f);
    host.processor.selectCompareSide (CompareSide::B);
    host.edit ("band1_gain", -3.0f);
    REQUIRE (history.undoSteps() == 3);

    history.undo(); // the edit on B
    CHECK (host.processor.compareSide() == CompareSide::B);
    CHECK_THAT (host.value ("band1_gain"), WithinAbs (6.0, 1.0e-4));
    history.undo(); // the switch
    CHECK (host.processor.compareSide() == CompareSide::A);
    CHECK_THAT (host.value ("band1_gain"), WithinAbs (6.0, 1.0e-4));
    history.undo(); // the edit on A
    CHECK_THAT (host.value ("band1_gain"), WithinAbs (0.0, 1.0e-4));
    CHECK_FALSE (history.canUndo());

    history.redo();
    history.redo();
    CHECK (host.processor.compareSide() == CompareSide::B);
    history.redo();
    CHECK_THAT (host.value ("band1_gain"), WithinAbs (-3.0, 1.0e-4));

    // B kept its own settings through it all.
    host.processor.selectCompareSide (CompareSide::A);
    CHECK_THAT (host.value ("band1_gain"), WithinAbs (6.0, 1.0e-4));
}

TEST_CASE ("Copy A to B is one undo step, from either side")
{
    Host host;
    const bool fromB = GENERATE (false, true);
    CAPTURE (fromB);
    auto& history = host.processor.editHistory();
    host.processor.selectCompareSide (CompareSide::B);
    host.edit ("band1_gain", -3.0f);
    host.processor.selectCompareSide (CompareSide::A);
    host.edit ("band1_gain", 6.0f);
    if (fromB)
        host.processor.selectCompareSide (CompareSide::B);
    const int steps = history.undoSteps();

    host.processor.copyAToB();
    CHECK (history.undoSteps() == steps + 1);
    history.undo();
    CHECK (host.processor.compareSide() == (fromB ? CompareSide::B : CompareSide::A));
    host.processor.selectCompareSide (CompareSide::B);
    CHECK_THAT (host.value ("band1_gain"), WithinAbs (-3.0, 1.0e-4));
}

TEST_CASE ("Switching between two very different sides does not click")
{
    const auto [sampleRate, blockSize] = GENERATE (std::pair { 44100.0, 17 }, std::pair { 48000.0, 512 }, std::pair { 96000.0, 64 });
    CAPTURE (sampleRate, blockSize);
    Host host;
    // A: a big boost on the tone, turned down and inverted.
    host.edit ("band1_in_use", 1.0f);
    host.edit ("band1_frequency", 200.0f);
    host.edit ("band1_gain", 18.0f);
    host.edit ("output_gain", -6.0f);
    host.edit ("phase_invert", 1.0f);
    host.processor.selectCompareSide (CompareSide::B);
    // B: the boost a steep Low Cut above the tone instead, other Bands and the output section changed.
    host.edit ("band1_shape", 2.0f);
    host.edit ("band1_frequency", 400.0f);
    host.edit ("band1_slope", 48.0f);
    host.edit ("band2_in_use", 1.0f);
    host.edit ("band2_shape", 3.0f);
    host.edit ("band2_frequency", 150.0f);
    host.edit ("band2_gain", 12.0f);
    host.edit ("gain_scale", 200.0f);
    host.edit ("auto_gain", 1.0f);
    host.edit ("output_gain", 3.0f);
    host.edit ("phase_invert", 0.0f);
    host.edit ("output_pan", -60.0f);

    // A tone, switching side every 50 ms after it has settled, as the editor would between blocks.
    constexpr double toneFrequency = 200.0, onsetSeconds = 0.1;
    host.processor.prepareToPlay (sampleRate, blockSize);
    juce::AudioBuffer<float> buffer (host.processor.getTotalNumInputChannels(), blockSize);
    juce::MidiBuffer midi;
    std::vector<float> left, right;
    int n = 0, switches = 0;
    while (n < static_cast<int> (sampleRate))
    {
        const double seconds = n / sampleRate;
        const int due = seconds < onsetSeconds ? 0 : static_cast<int> ((seconds - onsetSeconds) / 0.05) + 1;
        for (; switches < due; ++switches)
            host.processor.selectCompareSide (host.processor.compareSide() == CompareSide::A ? CompareSide::B : CompareSide::A);
        buffer.clear();
        for (int i = 0; i < blockSize; ++i, ++n)
        {
            const double phase = 2.0 * std::numbers::pi * toneFrequency * n / sampleRate;
            buffer.setSample (0, i, static_cast<float> (0.25 * std::sin (phase)));
            buffer.setSample (1, i, static_cast<float> (0.15 * std::sin (phase + 1.0)));
        }
        host.processor.processBlock (buffer, midi);
        left.insert (left.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
        right.insert (right.end(), buffer.getReadPointer (1), buffer.getReadPointer (1) + blockSize);
    }
    REQUIRE (switches > 10);
    const auto from = static_cast<size_t> (onsetSeconds * sampleRate);
    CHECK (eq1::test::discontinuity (left, sampleRate, toneFrequency, from) < 2.0);
    CHECK (eq1::test::discontinuity (right, sampleRate, toneFrequency, from) < 2.0);
}
