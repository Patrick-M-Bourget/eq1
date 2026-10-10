#include "BandEditing.h"
#include "BandSettingsByName.h"
#include "PluginProcessor.h"
#include "eq1/Engine.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <functional>
#include <map>
#include <numbers>
#include <random>
#include <string>

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
    eq1::BandEditing editing { processor.parameterState(), processor.editHistory() };

    float value (int slot, const char* control)
    {
        return processor.parameterState().getRawParameterValue ("band" + juce::String (slot) + "_" + control)->load();
    }

    void set (int slot, const char* control, float plain)
    {
        auto* parameter = processor.parameterState().getParameter ("band" + juce::String (slot) + "_" + control);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
    }

    // Every control of a Band Slot, by name.
    std::map<std::string, float> controlsOf (int slot)
    {
        std::map<std::string, float> controls;
        for (const char* control : { "in_use", "bypass", "shape", "frequency", "gain", "q", "slope", "brickwall", "placement",
                                     "dynamic_range", "threshold", "threshold_auto", "attack", "release", "dynamics_bypass",
                                     "detection_source", "detection_range", "detection_low", "detection_high" })
            controls[control] = value (slot, control);
        return controls;
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
    host.set (3, "dynamic_range", -12.0f);
    host.set (3, "threshold", -50.0f);
    host.set (3, "threshold_auto", 0.0f);
    host.set (3, "attack", 10.0f);
    host.set (3, "release", 90.0f);
    host.set (3, "dynamics_bypass", 1.0f);

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
    CHECK (host.value (3, "dynamic_range") == 0.0f);
    CHECK_THAT (host.value (3, "threshold"), WithinAbs (-30.0, 1.0e-4));
    CHECK (host.value (3, "threshold_auto") == 1.0f);
    CHECK_THAT (host.value (3, "attack"), WithinAbs (50.0, 1.0e-4));
    CHECK_THAT (host.value (3, "release"), WithinAbs (50.0, 1.0e-4));
    CHECK (host.value (3, "dynamics_bypass") == 0.0f);

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

TEST_CASE ("Under Gain Scale, Gains added and dragged on the display are the Gains heard")
{
    Host host;
    auto* gainScale = host.processor.parameterState().getParameter ("gain_scale");
    gainScale->setValueNotifyingHost (gainScale->convertTo0to1 (50.0f));
    CHECK (host.editing.settings().gainScale == 0.5);

    // Added where the display shows +6 dB: heard at +6, so stored at +12.
    const auto slot = host.editing.add (1000.0, 6.0);
    REQUIRE (slot == 1);
    CHECK_THAT (host.value (1, "gain"), WithinAbs (12.0, 1.0e-4));

    // Dragged down 3 dB on the display: stored 6 dB lower.
    host.editing.beginDrag ({ 1 });
    host.editing.dragBy (1.0, -3.0);
    host.editing.endDrag();
    CHECK_THAT (host.value (1, "gain"), WithinAbs (6.0, 1.0e-4));

    // At 0% every Gain is heard as 0 dB, so a vertical drag leaves the stored Gain alone.
    gainScale->setValueNotifyingHost (0.0f);
    host.editing.beginDrag ({ 1 });
    host.editing.dragBy (1.0, 5.0);
    host.editing.endDrag();
    CHECK_THAT (host.value (1, "gain"), WithinAbs (6.0, 1.0e-4));
    REQUIRE (host.editing.add (1000.0, 6.0) == 2);
    CHECK_THAT (host.value (2, "gain"), WithinAbs (0.0, 1.0e-4));
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
        eq1::BandEditing editing { host.processor.parameterState(), host.processor.editHistory() };
        editing.beginDrag ({ 1 });
        editing.dragBy (2.0, 1.0);
    }
    CHECK (log.begins[indexOf (host.processor, "band1_frequency")] == 1);
    CHECK (log.ends[indexOf (host.processor, "band1_frequency")] == 1);
    host.processor.removeListener (&log);
}

TEST_CASE ("Spectrum Grab makes a Bell at the peak, Gain 0, in the lowest free slot, and the drag sets its Gain")
{
    Host host;
    host.addBand (1, 100.0f, 3.0f);
    host.set (2, "shape", 6.0f); // what slot 2 held before must not come back

    REQUIRE (host.editing.grab (1234.0) == 2);
    CHECK (host.value (2, "in_use") == 1.0f);
    CHECK (host.value (2, "shape") == 0.0f); // Bell
    CHECK_THAT (host.value (2, "frequency"), WithinRel (1234.0f, 1.0e-4f));
    CHECK_THAT (host.value (2, "gain"), WithinAbs (0.0, 1.0e-4));

    host.editing.dragBy (1.0, -5.5);
    host.editing.endDrag();
    CHECK_THAT (host.value (2, "gain"), WithinAbs (-5.5, 1.0e-4));
    CHECK_THAT (host.value (2, "frequency"), WithinRel (1234.0f, 1.0e-4f));
}

TEST_CASE ("Spectrum Grab with all 24 Band Slots in use is refused")
{
    Host host;
    for (int slot = 1; slot <= 24; ++slot)
        host.addBand (slot, 1000.0f, 0.0f);
    CHECK_FALSE (host.editing.grab (500.0).has_value());
}

TEST_CASE ("Analyzer settings are saved with the session")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor saved;
    const eq1::AnalyzerSettings defaults;
    CHECK (saved.analyzerSettings() == defaults);
    CHECK (defaults.showPreEq);
    CHECK (defaults.showPostEq);
    CHECK_FALSE (defaults.showSidechain);
    CHECK (defaults.rangeDb == 90);
    CHECK (defaults.speed == eq1::AnalyzerSpeed::medium);
    CHECK (defaults.resolution == eq1::AnalyzerResolution::medium);
    CHECK_THAT (defaults.tiltDbPerOctave, WithinAbs (4.5, 1.0e-9));
    CHECK (defaults.peakHold);

    const eq1::AnalyzerSettings changed { .showPreEq = false,
                                          .showPostEq = true,
                                          .showSidechain = true,
                                          .rangeDb = 120,
                                          .speed = eq1::AnalyzerSpeed::veryFast,
                                          .resolution = eq1::AnalyzerResolution::maximum,
                                          .tiltDbPerOctave = 1.5,
                                          .peakHold = false };
    saved.setAnalyzerSettings (changed);

    juce::MemoryBlock state;
    saved.getStateInformation (state);
    eq1::PluginProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    CHECK (restored.analyzerSettings() == changed);
}

TEST_CASE ("Bypass on a selection bypasses every Band, as one undo step, and Remove Bypass restores them")
{
    Host host;
    for (int slot = 1; slot <= 3; ++slot)
        host.addBand (slot, 100.0f * static_cast<float> (slot), 3.0f);
    host.set (2, "bypass", 1.0f);
    auto& history = host.processor.editHistory();

    host.editing.setBypass ({ 1, 2, 3 }, true);
    for (int slot = 1; slot <= 3; ++slot)
        CHECK (host.value (slot, "bypass") == 1.0f);
    CHECK (history.undoSteps() == 1);

    host.editing.setBypass ({ 1, 2, 3 }, false);
    for (int slot = 1; slot <= 3; ++slot)
        CHECK (host.value (slot, "bypass") == 0.0f);
    history.undo();
    for (int slot = 1; slot <= 3; ++slot)
        CHECK (host.value (slot, "bypass") == 1.0f);
}

TEST_CASE ("Invert Gain negates Gain and Dynamic Range on Bands with Gain, as one undo step; a Cut keeps its stored Gain")
{
    Host host;
    host.addBand (1, 500.0f, 6.0f);
    host.set (1, "dynamic_range", -4.0f);
    host.addBand (2, 2000.0f, -3.0f);
    host.set (2, "shape", 7.0f); // Tilt Shelf
    host.addBand (3, 50.0f, 5.0f);
    host.set (3, "shape", 2.0f); // Low Cut: no Gain
    host.set (3, "dynamic_range", 8.0f);

    host.editing.invertGain ({ 1, 2, 3 });

    CHECK_THAT (host.value (1, "gain"), WithinAbs (-6.0, 1.0e-4));
    CHECK_THAT (host.value (1, "dynamic_range"), WithinAbs (4.0, 1.0e-4));
    CHECK_THAT (host.value (2, "gain"), WithinAbs (3.0, 1.0e-4));
    CHECK_THAT (host.value (3, "gain"), WithinAbs (5.0, 1.0e-4));
    CHECK_THAT (host.value (3, "dynamic_range"), WithinAbs (8.0, 1.0e-4));
    auto& history = host.processor.editHistory();
    CHECK (history.undoSteps() == 1);
    history.undo();
    CHECK_THAT (host.value (1, "gain"), WithinAbs (6.0, 1.0e-4));
    CHECK_THAT (host.value (2, "gain"), WithinAbs (-3.0, 1.0e-4));
}

TEST_CASE ("Clear Dynamics puts every dynamics setting back to its default, as one undo step, on Bands with dynamics")
{
    Host host;
    const auto makeDynamic = [&] (int slot) {
        host.set (slot, "dynamic_range", -12.0f);
        host.set (slot, "threshold", -45.0f);
        host.set (slot, "threshold_auto", 0.0f);
        host.set (slot, "attack", 10.0f);
        host.set (slot, "release", 90.0f);
        host.set (slot, "dynamics_bypass", 1.0f);
        host.set (slot, "detection_source", 1.0f); // External
        host.set (slot, "detection_range", 1.0f);  // Free
        host.set (slot, "detection_low", 200.0f);
        host.set (slot, "detection_high", 4000.0f);
    };
    host.addBand (1, 500.0f, 3.0f);
    makeDynamic (1);
    host.addBand (2, 100.0f, 0.0f);
    host.set (2, "shape", 5.0f); // Notch: no dynamics, so it keeps them
    makeDynamic (2);

    host.editing.clearDynamics ({ 1, 2 });

    const eq1::BandSettings defaults;
    const auto cleared = host.editing.band (1);
    CHECK_FALSE (eq1::isDynamic (cleared));
    CHECK_THAT (cleared.dynamicRange, WithinAbs (defaults.dynamicRange, 1.0e-9));
    CHECK_THAT (cleared.threshold, WithinAbs (defaults.threshold, 1.0e-4));
    CHECK (cleared.thresholdAuto == defaults.thresholdAuto);
    CHECK_THAT (cleared.attack, WithinAbs (defaults.attack, 1.0e-4));
    CHECK_THAT (cleared.release, WithinAbs (defaults.release, 1.0e-4));
    CHECK (cleared.dynamicsBypass == defaults.dynamicsBypass);
    CHECK (cleared.detectionSource == defaults.detectionSource);
    CHECK (cleared.detectionRange == defaults.detectionRange);
    CHECK_THAT (cleared.detectionLow, WithinRel (defaults.detectionLow, 1.0e-4));
    CHECK_THAT (cleared.detectionHigh, WithinRel (defaults.detectionHigh, 1.0e-4));
    CHECK_THAT (host.value (2, "dynamic_range"), WithinAbs (-12.0, 1.0e-4));
    CHECK_THAT (host.value (2, "detection_low"), WithinRel (200.0f, 1.0e-4f));

    auto& history = host.processor.editHistory();
    CHECK (history.undoSteps() == 1);
    history.undo();
    CHECK_THAT (host.value (1, "dynamic_range"), WithinAbs (-12.0, 1.0e-4));
    CHECK_THAT (host.value (1, "detection_high"), WithinRel (4000.0f, 1.0e-4f));
}

TEST_CASE ("Shape and Stereo Placement on a selection change every Band, as one undo step each")
{
    Host host;
    host.addBand (1, 100.0f, 3.0f);
    host.addBand (2, 1000.0f, -3.0f);
    host.set (2, "shape", 2.0f);

    host.editing.setShape ({ 1, 2 }, Shape::HighShelf);
    host.editing.setPlacement ({ 1, 2 }, eq1::StereoPlacement::Side);

    for (int slot : { 1, 2 })
    {
        CAPTURE (slot);
        CHECK (host.value (slot, "shape") == 3.0f);
        CHECK (host.value (slot, "placement") == 4.0f);
    }
    auto& history = host.processor.editHistory();
    CHECK (history.undoSteps() == 2);
    history.undo();
    CHECK (host.value (1, "placement") == 0.0f);
    CHECK (host.value (2, "placement") == 0.0f);
    history.undo();
    CHECK (host.value (1, "shape") == 0.0f);
    CHECK (host.value (2, "shape") == 2.0f);
}

TEST_CASE ("Slope on a selection sets the Bands that have a Slope, turning a Cut's Brickwall off; Brickwall turns it on for Cuts only")
{
    Host host;
    host.addBand (1, 50.0f, 0.0f);
    host.set (1, "shape", 2.0f); // Low Cut, Brickwall
    host.set (1, "brickwall", 1.0f);
    host.addBand (2, 500.0f, 0.0f); // Bell: no Slope
    host.set (2, "slope", 30.0f);
    host.addBand (3, 5000.0f, 2.0f);
    host.set (3, "shape", 3.0f); // High Shelf
    host.addBand (4, 800.0f, 2.0f);
    host.set (4, "shape", 8.0f); // Flat Tilt: no Slope
    host.set (4, "slope", 30.0f);
    auto& history = host.processor.editHistory();

    host.editing.setSlope ({ 1, 2, 3, 4 }, 36.0);
    CHECK (host.value (1, "slope") == 36.0f);
    CHECK (host.value (1, "brickwall") == 0.0f);
    CHECK (host.value (3, "slope") == 36.0f);
    CHECK (host.value (2, "slope") == 30.0f);
    CHECK (host.value (4, "slope") == 30.0f);
    CHECK (history.undoSteps() == 1);

    host.editing.setBrickwall ({ 1, 2, 3, 4 });
    CHECK (host.value (1, "brickwall") == 1.0f);
    for (int slot : { 2, 3, 4 })
        CHECK (host.value (slot, "brickwall") == 0.0f);
    CHECK (history.undoSteps() == 2);

    history.undo();
    history.undo();
    CHECK (host.value (1, "brickwall") == 1.0f);
    CHECK (host.value (1, "slope") == 12.0f);
    CHECK (host.value (3, "slope") == 12.0f);
}

TEST_CASE ("Split leaves a Left Band in each Stereo Band's slot and a Right Band in the lowest free slot, as one undo step")
{
    Host host;
    host.addBand (1, 500.0f, 6.0f);
    host.set (1, "shape", 3.0f); // High Shelf
    host.set (1, "q", 2.5f);
    host.set (1, "slope", 24.0f);
    host.set (1, "bypass", 1.0f);
    host.set (1, "dynamic_range", -6.0f);
    host.set (1, "threshold_auto", 0.0f);
    host.set (1, "threshold", -40.0f);
    host.set (1, "attack", 20.0f);
    host.set (1, "release", 80.0f);
    host.set (1, "detection_source", 1.0f); // External
    host.set (1, "detection_range", 1.0f);  // Free
    host.set (1, "detection_low", 300.0f);
    host.set (1, "detection_high", 3000.0f);
    host.addBand (2, 1000.0f, 3.0f);
    host.set (2, "placement", 3.0f); // Mid: ignored
    host.addBand (3, 2000.0f, -3.0f);
    // What slot 4 held before must not come back.
    host.set (4, "q", 9.0f);
    const auto stereo = host.controlsOf (1);

    const auto result = host.editing.split ({ 1, 2, 3 });

    CHECK (result == std::vector<int> { 1, 3, 4, 5 });
    auto left = stereo, right = stereo;
    left["placement"] = 1.0f;
    right["placement"] = 2.0f;
    CHECK (host.controlsOf (1) == left);
    CHECK (host.controlsOf (4) == right);
    CHECK (host.value (2, "placement") == 3.0f);
    CHECK (host.value (3, "placement") == 1.0f);
    CHECK (host.value (5, "placement") == 2.0f);
    CHECK_THAT (host.value (5, "frequency"), WithinRel (2000.0f, 1.0e-4f));

    auto& history = host.processor.editHistory();
    CHECK (history.undoSteps() == 1);
    history.undo();
    CHECK (host.controlsOf (1) == stereo);
    CHECK (host.value (3, "placement") == 0.0f);
    CHECK (host.value (4, "in_use") == 0.0f);
    CHECK (host.value (5, "in_use") == 0.0f);
}

TEST_CASE ("Split with fewer free Band Slots than Stereo Bands splits the lowest-Frequency ones, lower slots first on a tie")
{
    Host host;
    for (int slot = 1; slot <= 23; ++slot)
        host.addBand (slot, 100.0f, 0.0f);
    host.set (1, "frequency", 5000.0f);
    host.set (2, "frequency", 200.0f);
    host.set (3, "frequency", 200.0f);

    CHECK (host.editing.split ({ 3, 1, 2 }) == std::vector<int> { 2, 24 });
    CHECK (host.value (1, "placement") == 0.0f);
    CHECK (host.value (2, "placement") == 1.0f);
    CHECK (host.value (3, "placement") == 0.0f);
    CHECK (host.editing.split ({ 1, 3 }).empty());
}

namespace
{
constexpr double splitSampleRate = 48000.0;
constexpr int splitBlockSize = 64;

struct Played
{
    std::vector<std::vector<float>> output; // per channel
    std::vector<double> liveGains;          // each slot's Live Gain at the end, slot 1 first
};

// Plays signal (channel, sample index) through a stereo Engine with settings for seconds.
Played play (const eq1::Settings& settings, double seconds, const std::function<float (int, int)>& signal)
{
    eq1::Engine engine;
    engine.prepare (splitSampleRate, splitBlockSize, 2);
    engine.setSettings (settings);
    Played played;
    played.output.resize (2);
    std::vector<std::vector<float>> block (2, std::vector<float> (splitBlockSize));
    std::vector<float*> channels { block[0].data(), block[1].data() };
    for (int n = 0; n < static_cast<int> (seconds * splitSampleRate); n += splitBlockSize)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < splitBlockSize; ++i)
                block[static_cast<size_t> (ch)][static_cast<size_t> (i)] = signal (ch, n + i);
        engine.process ({ channels.data(), 2, splitBlockSize });
        for (int ch = 0; ch < 2; ++ch)
            played.output[static_cast<size_t> (ch)].insert (played.output[static_cast<size_t> (ch)].end(), block[static_cast<size_t> (ch)].begin(),
                                                            block[static_cast<size_t> (ch)].end());
    }
    for (int slot = 1; slot <= eq1::numBandSlots; ++slot)
        played.liveGains.push_back (engine.liveGainDb (slot));
    return played;
}
} // namespace

TEST_CASE ("Split doesn't change the output of a Band that isn't dynamic")
{
    Host host;
    host.addBand (1, 1000.0f, 9.0f);
    host.set (1, "q", 2.0f);
    host.addBand (2, 4000.0f, -6.0f);
    host.set (2, "shape", 3.0f); // High Shelf, not selected
    std::mt19937 random (7);
    std::uniform_real_distribution<float> unit (-0.5f, 0.5f);
    std::vector<std::vector<float>> noise (2, std::vector<float> (8192));
    for (auto& channel : noise)
        for (auto& sample : channel)
            sample = unit (random);
    const auto signal = [&] (int ch, int n) { return noise[static_cast<size_t> (ch)][static_cast<size_t> (n)]; };
    const auto before = play (host.editing.settings(), 8192 / splitSampleRate, signal);

    REQUIRE (host.editing.split ({ 1 }) == std::vector<int> { 1, 3 });
    const auto after = play (host.editing.settings(), 8192 / splitSampleRate, signal);

    for (size_t ch = 0; ch < 2; ++ch)
        for (size_t i = 0; i < noise[ch].size(); ++i)
        {
            CAPTURE (ch, i);
            REQUIRE_THAT (after.output[ch][i], WithinAbs (before.output[ch][i], 1.0e-5));
        }
}

TEST_CASE ("A split Dynamic Band's halves each move with their own channel only")
{
    Host host;
    host.addBand (1, 1000.0f, 0.0f);
    host.set (1, "dynamic_range", -9.0f);
    host.set (1, "threshold_auto", 0.0f);
    host.set (1, "threshold", -30.0f);
    REQUIRE (host.editing.split ({ 1 }) == std::vector<int> { 1, 2 });

    const auto toneOnLeft = [] (int ch, int n) {
        return ch == 0 ? static_cast<float> (0.5 * std::sin (2.0 * std::numbers::pi * 1000.0 * n / splitSampleRate)) : 0.0f;
    };
    const auto played = play (host.editing.settings(), 1.0, toneOnLeft);
    CHECK_THAT (played.liveGains[0], WithinAbs (-9.0, 0.05));
    CHECK (played.liveGains[1] == 0.0);
}

TEST_CASE ("Paste adds the Bands in the lowest free Band Slots with every stored setting, as one undo step, returning their slots")
{
    Host host;
    host.addBand (1, 100.0f, 3.0f);
    host.addBand (3, 300.0f, 3.0f);
    // What slot 2 held before must not come back.
    host.set (2, "q", 9.0f);
    host.set (2, "detection_low", 500.0f);
    host.set (eq1::numBandSlots, "q", 9.0f);
    eq1::BandSettings band;
    band.shape = Shape::HighShelf;
    band.frequency = 2500.0;
    band.gain = -7.5;
    band.slope = 36.0;
    band.placement = eq1::StereoPlacement::Side;
    band.dynamicRange = 12.5;
    band.thresholdAuto = false;
    band.threshold = -42.0;
    band.detectionRange = eq1::DetectionRange::Free;
    band.detectionHigh = 6000.0;

    CHECK (host.editing.paste ({ band, eq1::BandSettings {} }) == std::vector<int> { 2, 4 });
    band.inUse = true;
    CHECK (near (host.editing.band (2), band));
    CHECK (near (host.editing.band (4), eq1::BandSettings { .inUse = true }));

    auto& history = host.processor.editHistory();
    CHECK (history.undoSteps() == 1);
    history.undo();
    CHECK (host.value (2, "in_use") == 0.0f);
    CHECK (host.value (4, "in_use") == 0.0f);
}

TEST_CASE ("Paste keeps stored Gain and Dynamic Range whatever the Gain Scale")
{
    Host host;
    auto* gainScale = host.processor.parameterState().getParameter (eq1::parameters::gainScaleId);
    gainScale->setValueNotifyingHost (gainScale->convertTo0to1 (50.0f));
    const eq1::BandSettings band { .gain = 6.0, .dynamicRange = -4.0 };

    REQUIRE (host.editing.paste ({ band }) == std::vector<int> { 1 });
    CHECK_THAT (host.value (1, "gain"), WithinAbs (6.0, 1.0e-4));
    CHECK_THAT (host.value (1, "dynamic_range"), WithinAbs (-4.0, 1.0e-4));
}

TEST_CASE ("Paste with fewer free Band Slots than Bands pastes the lowest-Frequency ones, earlier first on a tie")
{
    Host host;
    for (int slot = 1; slot <= 22; ++slot)
        host.addBand (slot, 100.0f, 0.0f);
    const std::vector<eq1::BandSettings> bands { { .frequency = 5000.0 }, { .frequency = 200.0, .gain = 1.0 }, { .frequency = 200.0, .gain = 2.0 } };

    CHECK (host.editing.paste (bands) == std::vector<int> { 23, 24 });
    CHECK_THAT (host.value (23, "gain"), WithinAbs (1.0, 1.0e-4));
    CHECK_THAT (host.value (24, "gain"), WithinAbs (2.0, 1.0e-4));
    CHECK (host.editing.paste (bands).empty());
    CHECK (host.processor.editHistory().undoSteps() == 1);
}
