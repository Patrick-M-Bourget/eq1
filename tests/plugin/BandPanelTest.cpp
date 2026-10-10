#include "BandEditing.h"
#include "BandPanel.h"
#include "DetectionArc.h"
#include "LevelBallistics.h"
#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <memory>

using Catch::Matchers::WithinAbs;

namespace
{

// A plugin with the Band panel the editor shows, and Bands 3 and 4 Bells, 5 a Low Cut.
struct Host
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    eq1::BandEditing editing { processor.parameterState(), processor.editHistory() };
    std::unique_ptr<eq1::BandPanel> panel = std::make_unique<eq1::BandPanel> (processor, editing);

    Host()
    {
        for (int slot : { 3, 4, 5 })
            set (slot, "in_use", 1.0f);
        set (5, "shape", 2.0f);
    }

    void set (int slot, const char* control, float plain)
    {
        auto* parameter = processor.parameterState().getParameter ("band" + juce::String (slot) + "_" + control);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
    }
};

} // namespace

TEST_CASE ("The Band panel meters the Band it shows, when its Shape has dynamics")
{
    Host host;
    CHECK (host.processor.meteredSlot() == 0);

    host.panel->show (3);
    CHECK (host.processor.meteredSlot() == 3);

    // Selecting another Band moves the metering to it.
    host.panel->show (4);
    CHECK (host.processor.meteredSlot() == 4);

    SECTION ("showing no Band lets go of it")
    {
        host.panel->show (0);
        CHECK (host.processor.meteredSlot() == 0);
    }
    SECTION ("a Shape without dynamics isn't metered")
    {
        host.panel->show (5);
        CHECK (host.processor.meteredSlot() == 0);
    }
    SECTION ("closing the editor lets go of it")
    {
        host.panel.reset();
        CHECK (host.processor.meteredSlot() == 0);
    }
}

TEST_CASE ("A level meter rises at once to a louder level and falls at 20 dB/s")
{
    eq1::LevelBallistics meter;
    CHECK_THAT (meter.update (-10.0, 0.0), WithinAbs (-10.0, 1.0e-9));
    CHECK_THAT (meter.update (-100.0, 0.25), WithinAbs (-15.0, 1.0e-9));
    CHECK_THAT (meter.update (-100.0, 0.25), WithinAbs (-20.0, 1.0e-9));
    CHECK_THAT (meter.update (-6.0, 0.25), WithinAbs (-6.0, 1.0e-9));
    // It falls no lower than the level read.
    CHECK_THAT (meter.update (-8.0, 1.0), WithinAbs (-8.0, 1.0e-9));
    meter.reset();
    CHECK_THAT (meter.update (eq1::levelFloorDb, 0.0), WithinAbs (eq1::levelFloorDb, 1.0e-9));
}

TEST_CASE ("The Detection Level arc reaches the Threshold knob's position for the Threshold it equals, and never Auto")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    eq1::BandEditing editing { processor.parameterState(), processor.editHistory() };
    eq1::BandPanel panel (processor, editing);
    // The Band panel's Threshold knob, with Auto as its top position.
    juce::Slider* threshold = nullptr;
    for (auto* child : panel.getChildren())
        if (auto* slider = dynamic_cast<juce::Slider*> (child); slider != nullptr && slider->getMinimum() < -59.0)
            threshold = slider;
    REQUIRE (threshold != nullptr);
    const auto knobAt = [&] (double thresholdDb) { return threshold->valueToProportionOfLength (thresholdDb); };

    for (double level : { -60.0, -42.5, -12.0, 0.0 })
    {
        CAPTURE (level);
        CHECK_THAT (eq1::DetectionArc::sweepProportion (*threshold, level), WithinAbs (knobAt (level), 1.0e-9));
    }
    // Above 0 dB it stops at 0 dB, short of Auto; below the knob's bottom it's empty.
    CHECK (knobAt (0.0) < 1.0);
    CHECK_THAT (eq1::DetectionArc::sweepProportion (*threshold, 6.0), WithinAbs (knobAt (0.0), 1.0e-9));
    CHECK_THAT (eq1::DetectionArc::sweepProportion (*threshold, -70.0), WithinAbs (0.0, 1.0e-9));
    CHECK_THAT (eq1::DetectionArc::sweepProportion (*threshold, eq1::levelFloorDb), WithinAbs (0.0, 1.0e-9));
}
