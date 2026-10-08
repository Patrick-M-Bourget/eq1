#include "BandEditing.h"
#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <map>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using eq1::Shape;

namespace
{

// A plugin as a host has it, with the editing rules the editor uses on top.
struct Host
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    eq1::BandEditing editing { processor.parameterState() };

    float value (int slot, const char* control)
    {
        return processor.parameterState().getRawParameterValue ("band" + juce::String (slot) + "_" + control)->load();
    }

    void set (int slot, const char* control, float plain)
    {
        auto* parameter = processor.parameterState().getParameter ("band" + juce::String (slot) + "_" + control);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
    }

    void addBand (int slot, float frequency, float gain)
    {
        set (slot, "in_use", 1.0f);
        set (slot, "frequency", frequency);
        set (slot, "gain", gain);
    }
};

// Counts the gestures a host sees, per parameter index.
struct GestureLog final : juce::AudioProcessorListener
{
    std::map<int, int> begins, ends;
    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}
    void audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int index) override { ++begins[index]; }
    void audioProcessorParameterChangeGestureEnd (juce::AudioProcessor*, int index) override { ++ends[index]; }
};

int indexOf (eq1::PluginProcessor& processor, const juce::String& id)
{
    return processor.parameterState().getParameter (id)->getParameterIndex();
}

} // namespace

TEST_CASE ("A new Band takes the lowest free Band Slot, at the given Frequency and Gain, with defaults for the rest")
{
    Host host;
    for (int slot = 1; slot <= 5; ++slot)
        host.addBand (slot, 100.0f * static_cast<float> (slot), 3.0f);
    // What slot 3 held before must not come back.
    host.set (3, "shape", 4.0f);
    host.set (3, "q", 7.0f);
    host.set (3, "slope", 48.0f);
    host.set (3, "brickwall", 1.0f);
    host.set (3, "bypass", 1.0f);
    host.set (3, "placement", 4.0f);

    host.editing.deleteBand (3);
    REQUIRE (host.editing.add (250.0, -4.0) == 3);

    CHECK (host.value (3, "in_use") == 1.0f);
    CHECK_THAT (host.value (3, "frequency"), WithinRel (250.0f, 1.0e-4f));
    CHECK_THAT (host.value (3, "gain"), WithinAbs (-4.0, 1.0e-4));
    CHECK (host.value (3, "shape") == 0.0f); // Bell
    CHECK_THAT (host.value (3, "q"), WithinRel (1.0f, 1.0e-4f));
    CHECK_THAT (host.value (3, "slope"), WithinAbs (12.0, 1.0e-4));
    CHECK (host.value (3, "brickwall") == 0.0f);
    CHECK (host.value (3, "bypass") == 0.0f);
    CHECK (host.value (3, "placement") == 0.0f); // Stereo

    REQUIRE (host.editing.add (1000.0, 0.0) == 6);
}

TEST_CASE ("With all 24 Band Slots in use, adding a Band is refused")
{
    Host host;
    for (int slot = 1; slot <= 24; ++slot)
        host.addBand (slot, 1000.0f, 0.0f);

    CHECK (host.editing.isFull());
    CHECK_FALSE (host.editing.add (500.0, 6.0).has_value());
    CHECK_THAT (host.value (1, "frequency"), WithinRel (1000.0f, 1.0e-4f));
}

TEST_CASE ("Deleting a Band frees its slot and leaves the other Bands' numbers alone")
{
    Host host;
    for (int slot = 1; slot <= 5; ++slot)
        host.addBand (slot, 100.0f * static_cast<float> (slot), 1.0f);

    host.editing.deleteBand (3);

    CHECK (host.value (3, "in_use") == 0.0f);
    for (int slot : { 1, 2, 4, 5 })
    {
        CAPTURE (slot);
        CHECK (host.value (slot, "in_use") == 1.0f);
        CHECK_THAT (host.value (slot, "frequency"), WithinRel (100.0f * static_cast<float> (slot), 1.0e-4f));
    }
    CHECK_FALSE (host.editing.isFull());
}

TEST_CASE ("Dragging several Bands moves them together, stopping together at the edges of the ranges")
{
    Host host;
    host.addBand (1, 100.0f, 3.0f);
    host.addBand (2, 1000.0f, -2.0f);
    host.addBand (3, 20000.0f, 28.0f);
    host.addBand (4, 50.0f, 5.0f);
    host.set (4, "shape", 2.0f); // Low Cut: no Gain

    host.editing.beginDrag ({ 1, 2, 3, 4 });
    host.editing.dragBy (1.5, 1.0);
    host.editing.dragBy (2.0, 4.0); // relative to where the drag began, not cumulative
    host.editing.endDrag();

    // Band 3 reaches 30 kHz and +30 dB first, so every Band moves by x1.5 and +2 dB: they keep their spacing.
    CHECK_THAT (host.value (1, "frequency"), WithinRel (150.0f, 1.0e-4f));
    CHECK_THAT (host.value (1, "gain"), WithinAbs (5.0, 1.0e-4));
    CHECK_THAT (host.value (2, "frequency"), WithinRel (1500.0f, 1.0e-4f));
    CHECK_THAT (host.value (2, "gain"), WithinAbs (0.0, 1.0e-4));
    CHECK_THAT (host.value (3, "frequency"), WithinRel (30000.0f, 1.0e-4f));
    CHECK_THAT (host.value (3, "gain"), WithinAbs (30.0, 1.0e-4));
    // A Shape without Gain keeps its stored Gain.
    CHECK_THAT (host.value (4, "frequency"), WithinRel (75.0f, 1.0e-4f));
    CHECK_THAT (host.value (4, "gain"), WithinAbs (5.0, 1.0e-4));
}

TEST_CASE ("The wheel scales a Band's Q, within its range")
{
    Host host;
    host.addBand (2, 1000.0f, 6.0f);

    host.editing.scaleQ (2, 1.5);
    CHECK_THAT (host.value (2, "q"), WithinRel (1.5f, 1.0e-4f));
    host.editing.scaleQ (2, 1000.0);
    CHECK_THAT (host.value (2, "q"), WithinRel (40.0f, 1.0e-4f));
}

TEST_CASE ("Changing a Band's Shape keeps its Frequency, Gain and Q")
{
    Host host;
    host.addBand (2, 700.0f, -5.0f);
    host.set (2, "q", 3.0f);

    host.editing.setShape (2, Shape::HighShelf);

    CHECK (host.value (2, "shape") == 3.0f);
    CHECK_THAT (host.value (2, "frequency"), WithinRel (700.0f, 1.0e-4f));
    CHECK_THAT (host.value (2, "gain"), WithinAbs (-5.0, 1.0e-4));
    CHECK_THAT (host.value (2, "q"), WithinRel (3.0f, 1.0e-4f));
}

TEST_CASE ("Every edit reaches the host as a gesture it can record as automation")
{
    Host host;
    host.addBand (1, 100.0f, 0.0f);
    GestureLog log;
    host.processor.addListener (&log);

    host.editing.beginDrag ({ 1 });
    for (int step = 1; step <= 10; ++step)
        host.editing.dragBy (1.0 + 0.1 * step, 0.5 * step);
    host.editing.endDrag();
    // One gesture for the whole drag, on each parameter it moves.
    for (const char* id : { "band1_frequency", "band1_gain" })
    {
        CAPTURE (id);
        CHECK (log.begins[indexOf (host.processor, id)] == 1);
        CHECK (log.ends[indexOf (host.processor, id)] == 1);
    }

    host.editing.scaleQ (1, 2.0);
    host.editing.setShape (1, Shape::Notch);
    host.editing.deleteBand (1);
    const auto slot = host.editing.add (300.0, 2.0);
    REQUIRE (slot == 1);
    for (const char* id : { "band1_q", "band1_shape", "band1_in_use" })
    {
        CAPTURE (id);
        CHECK (log.begins[indexOf (host.processor, id)] >= 1);
        CHECK (log.begins[indexOf (host.processor, id)] == log.ends[indexOf (host.processor, id)]);
    }
    host.processor.removeListener (&log);
}

TEST_CASE ("The display's Gain range is saved with the plugin")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor saved;
    CHECK (saved.displayRangeDb() == 12);
    saved.setDisplayRangeDb (30);

    juce::MemoryBlock state;
    saved.getStateInformation (state);
    eq1::PluginProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    CHECK (restored.displayRangeDb() == 30);
}

TEST_CASE ("An editor closed in the middle of a drag still ends its gestures")
{
    Host host;
    host.addBand (1, 100.0f, 0.0f);
    GestureLog log;
    host.processor.addListener (&log);
    {
        eq1::BandEditing editing { host.processor.parameterState() };
        editing.beginDrag ({ 1 });
        editing.dragBy (2.0, 1.0);
    }
    CHECK (log.begins[indexOf (host.processor, "band1_frequency")] == 1);
    CHECK (log.ends[indexOf (host.processor, "band1_frequency")] == 1);
    host.processor.removeListener (&log);
}
