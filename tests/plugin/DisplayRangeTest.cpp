#include "BandEditing.h"
#include "DisplayRange.h"
#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <functional>
#include <random>

using Catch::Matchers::WithinAbs;
using eq1::CompareSide;

namespace
{

// A plugin as a host has it, with the editing rules the editor uses on top. Each frame() is the
// editor's look at the Bands while it is open.
struct Host
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    eq1::BandEditing editing { processor.parameterState(), processor.editHistory() };

    juce::RangedAudioParameter& parameter (const juce::String& id) { return *processor.parameterState().getParameter (id); }

    // Host Automation: no gesture.
    void set (const juce::String& id, float plain) { parameter (id).setValueNotifyingHost (parameter (id).convertTo0to1 (plain)); }

    // An edit in the editor, such as typing a value: inside a gesture.
    void edit (const juce::String& id, float plain)
    {
        auto& p = parameter (id);
        p.beginChangeGesture();
        p.setValueNotifyingHost (p.convertTo0to1 (plain));
        p.endChangeGesture();
    }

    void addBell (float gain)
    {
        set ("band1_in_use", 1.0f);
        set ("band1_frequency", 1000.0f);
        set ("band1_gain", gain);
    }

    void dragTo (double gain)
    {
        editing.beginDrag ({ 1 });
        editing.dragBy (1.0, gain - editing.band (1).gain);
        editing.endDrag();
    }

    int frame()
    {
        processor.fitDisplayRangeToHeardGains();
        return processor.displayRangeDb();
    }
};

// A Preset made in another plugin instance.
juce::ValueTree presetWith (const std::initializer_list<std::pair<const char*, float>> values)
{
    Host maker;
    for (const auto& [id, v] : values)
        maker.edit (id, v);
    return maker.processor.presetState();
}

} // namespace

TEST_CASE ("Dragging a Band beyond the Display Range zooms out when the drag ends, not during it")
{
    Host host;
    host.addBell (0.0f);
    REQUIRE (host.frame() == 12);

    host.editing.beginDrag ({ 1 });
    host.editing.dragBy (1.0, 15.0);
    CHECK (host.frame() == 12);
    CHECK_THAT (host.editing.band (1).gain, WithinAbs (15.0, 1.0e-4));
    host.editing.endDrag();
    CHECK (host.frame() == 30);
}

TEST_CASE ("Raising Gain Scale so a Band is beyond the Display Range zooms out to the smallest range that fits")
{
    Host host;
    host.processor.setDisplayRangeDb (6);
    host.addBell (5.0f);
    REQUIRE (host.frame() == 6);

    host.set ("gain_scale", 200.0f);
    CHECK (host.frame() == 12);
}

TEST_CASE ("Typing, Host Automation, a Preset load and an A/B switch that put a Band beyond the Display Range zoom it out")
{
    Host host;
    host.addBell (3.0f);
    // B holds the Band at +9.
    host.processor.selectCompareSide (CompareSide::B);
    host.set ("band1_gain", 9.0f);
    host.processor.selectCompareSide (CompareSide::A);
    host.processor.setDisplayRangeDb (6);
    REQUIRE (host.frame() == 6);

    const auto source = GENERATE (as<std::string> {}, "typing", "Host Automation", "a Preset load", "an A/B switch");
    CAPTURE (source);
    if (source == "typing")
        host.edit ("band1_gain", 9.0f);
    else if (source == "Host Automation")
        host.set ("band1_gain", 9.0f);
    else if (source == "a Preset load")
        REQUIRE (host.processor.loadPreset (presetWith ({ { "band1_in_use", 1.0f }, { "band1_gain", 9.0f } }), "Loud"));
    else
        host.processor.selectCompareSide (CompareSide::B);
    CHECK (host.frame() == 12);
}

TEST_CASE ("A Heard Gain exactly at the Display Range's edge doesn't zoom it; beyond +/-30 dB it stays at +/-30")
{
    Host host;
    host.addBell (0.0f);
    REQUIRE (host.frame() == 12);

    host.set ("band1_gain", 12.0f);
    CHECK (host.frame() == 12);
    host.set ("band1_gain", -12.0f);
    CHECK (host.frame() == 12);

    host.set ("band1_gain", 30.0f);
    host.set ("gain_scale", 200.0f);
    CHECK (host.frame() == 30);
}

TEST_CASE ("Bringing a Band back within a smaller range leaves the Display Range as it is, and undo doesn't zoom back in")
{
    Host host;
    host.addBell (3.0f);
    REQUIRE (host.frame() == 12);

    host.dragTo (15.0);
    REQUIRE (host.frame() == 30);
    host.dragTo (2.0);
    CHECK (host.frame() == 30);
    host.processor.editHistory().undo();
    host.processor.editHistory().undo();
    REQUIRE_THAT (host.editing.band (1).gain, WithinAbs (3.0, 1.0e-4));
    CHECK (host.frame() == 30);
}

TEST_CASE ("A range picked by hand stays, across closing and reopening the editor, until a Gain changes beyond it")
{
    Host host;
    host.addBell (10.0f);
    REQUIRE (host.frame() == 12);

    host.processor.setDisplayRangeDb (6);
    CHECK (host.frame() == 6);
    // Closed for a while, then opened again: no frames in between.
    host.set ("band2_in_use", 1.0f);
    CHECK (host.frame() == 6);

    // A Gain that changes beyond it while the editor is closed zooms it on opening.
    host.set ("band1_gain", 11.0f);
    CHECK (host.frame() == 12);
}

TEST_CASE ("A Band whose Heard Gain is unchanged doesn't zoom the Display Range, but it is fitted when another one zooms it")
{
    eq1::HeardGains seen {}, now {};
    seen[0] = now[0] = 20.0;
    seen[1] = 3.0;
    CHECK (eq1::fittedDisplayRangeDb (6, seen, now) == 6);
    now[1] = 8.0;
    CHECK (eq1::fittedDisplayRangeDb (6, seen, now) == 30);
}

TEST_CASE ("A Band coming into use, or changing to a Shape with Gain, beyond the Display Range zooms it")
{
    Host host;
    host.set ("band1_gain", 10.0f);
    host.set ("band1_shape", 5.0f); // Notch
    host.set ("band1_in_use", 1.0f);
    host.processor.setDisplayRangeDb (6);
    REQUIRE (host.frame() == 6);

    host.set ("band1_shape", 0.0f); // Bell
    CHECK (host.frame() == 12);
}

TEST_CASE ("A Dynamic Band's Live Gain beyond the Display Range doesn't zoom it")
{
    Host host;
    host.addBell (10.0f);
    host.set ("band1_threshold_auto", 0.0f);
    host.set ("band1_threshold", -40.0f);
    host.set ("band1_dynamic_range", 30.0f);
    REQUIRE (host.frame() == 12);

    constexpr int blockSize = 512;
    host.processor.prepareToPlay (48000.0, blockSize);
    juce::AudioBuffer<float> buffer (host.processor.getTotalNumInputChannels(), blockSize);
    juce::MidiBuffer midi;
    std::mt19937 random (3);
    std::uniform_real_distribution<float> unit (-0.5f, 0.5f);
    for (int block = 0; block < 100; ++block)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < blockSize; ++i)
                buffer.setSample (ch, i, unit (random));
        host.processor.processBlock (buffer, midi);
    }
    REQUIRE (host.processor.liveGainDb (1) > 12.0);
    CHECK (host.frame() == 12);
}

TEST_CASE ("A zoomed Display Range is saved with the session, and a restored session's Bands count as seen")
{
    Host host;
    host.addBell (0.0f);
    REQUIRE (host.frame() == 12);
    host.dragTo (15.0);
    REQUIRE (host.frame() == 30);
    juce::MemoryBlock zoomed;
    host.processor.getStateInformation (zoomed);
    host.processor.setDisplayRangeDb (6);
    juce::MemoryBlock picked;
    host.processor.getStateInformation (picked);

    Host restored;
    REQUIRE (restored.frame() == 12);
    restored.processor.setStateInformation (picked.getData(), static_cast<int> (picked.getSize()));
    CHECK (restored.frame() == 6);
    restored.processor.setStateInformation (zoomed.getData(), static_cast<int> (zoomed.getSize()));
    CHECK (restored.frame() == 30);
}
