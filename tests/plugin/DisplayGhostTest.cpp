#include "display/GhostLayer.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using eq1::display::DisplayGeometry;

TEST_CASE ("The ghost Bell sits at the pointer's Frequency and Gain, at least 12 % of the range from 0 dB and with its peak 60 px inside the top and bottom")
{
    const DisplayGeometry geometry { .width = 1134, .height = 612, .rangeDb = 12 };
    const float x = geometry.xOf (500.0);
    auto ghost = eq1::display::ghostBell (geometry, { x, geometry.yOf (5.0) });
    CHECK_THAT (ghost.frequency, WithinRel (500.0, 1.0e-4));
    CHECK_THAT (ghost.gain, WithinAbs (5.0, 1.0e-4));
    // Near 0 dB: pushed out to 12 % of the range, on the pointer's side.
    CHECK_THAT (eq1::display::ghostBell (geometry, { x, geometry.yOf (-0.5) }).gain, WithinAbs (-1.44, 1.0e-4));
    CHECK_THAT (eq1::display::ghostBell (geometry, { x, geometry.yOf (0.0) }).gain, WithinAbs (1.44, 1.0e-4));
    // Near the top and bottom: the peak stays 60 px in.
    CHECK_THAT (geometry.yOf (eq1::display::ghostBell (geometry, { x, 5.0f }).gain), WithinAbs (60.0, 1.0e-3));
    CHECK_THAT (geometry.yOf (eq1::display::ghostBell (geometry, { x, 610.0f }).gain), WithinAbs (552.0, 1.0e-3));
}

TEST_CASE ("The ghost's readout: \"850.0 Hz\" below 1 kHz, \"1.25 kHz\", and \"12.5 kHz\" from 10 kHz")
{
    const DisplayGeometry geometry { .width = 1134, .height = 612, .rangeDb = 12 };
    const auto readout = [&] (double frequency) { return eq1::display::ghostBell (geometry, { geometry.xOf (frequency), 100.0f }).readout; };
    CHECK (readout (850.0) == "850.0 Hz");
    CHECK (readout (1250.0) == "1.25 kHz");
    CHECK (readout (12500.0) == "12.5 kHz");
}

TEST_CASE ("With no Bands the ghost rests at 1 kHz at half the Display Range")
{
    const DisplayGeometry geometry { .width = 1134, .height = 612, .rangeDb = 30 };
    const auto ghost = eq1::display::restingGhost (geometry);
    CHECK_THAT (ghost.frequency, WithinRel (1000.0, 1.0e-4));
    CHECK_THAT (ghost.gain, WithinAbs (15.0, 1.0e-4));
}

TEST_CASE ("Frequency labels within 46 px of the ghost's readout fade to 12 %")
{
    const DisplayGeometry geometry { .width = 1134, .height = 612, .rangeDb = 12 };
    const auto labels = eq1::display::gridLabels (geometry);
    const auto ghost = eq1::display::ghostBell (geometry, { geometry.xOf (1100.0), 100.0f });
    const auto faded = eq1::display::fadedForGhost (labels, geometry, ghost);
    REQUIRE (faded.size() == labels.size());
    for (size_t i = 0; i < labels.size(); ++i)
    {
        CAPTURE (labels[i].text);
        const bool near = labels[i].text == "1k";
        CHECK_THAT (faded[i].colour.getFloatAlpha(), WithinAbs (labels[i].colour.getFloatAlpha() * (near ? 0.12f : 1.0f), 0.01));
    }
}
