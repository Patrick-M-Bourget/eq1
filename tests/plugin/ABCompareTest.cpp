#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

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
