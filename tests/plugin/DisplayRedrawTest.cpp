#include "EditorHarness.h"

#include <catch2/catch_test_macros.hpp>

#include <array>

using harness::OpenEditor;

namespace
{
// What the display has built so far: each Band's curve, the sum, and its handle placements.
struct Built
{
    std::array<int, eq1::numBandSlots> curves {};
    int sum = 0, handles = 0;
    bool operator== (const Built&) const = default;
};

Built built (const OpenEditor& host)
{
    Built b;
    for (int slot = 1; slot <= eq1::numBandSlots; ++slot)
        b.curves[static_cast<size_t> (slot - 1)] = host.display.curveCache().builds (slot);
    b.sum = host.display.curveCache().sumBuilds();
    b.handles = host.display.handlePlacements();
    return b;
}

// Paints the display, as a window does on a frame: a test window never paints by itself.
void paintFrame (OpenEditor& host)
{
    REQUIRE (host.display.createComponentSnapshot (host.display.getLocalBounds(), true, 1.0f).isValid());
}
} // namespace

TEST_CASE ("An Analyzer frame with the Bands unchanged designs no Band curve, builds no sum and places no handle")
{
    OpenEditor host;
    host.addBand (1, 100.0f, 3.0f);
    host.addBand (2, 1000.0f, -4.0f);
    host.settle();
    paintFrame (host);
    // The Analyzer shows Pre and Post from the start: the tick runs at 60 Hz and every frame repaints
    // the display.
    auto& tick = host.editorTick();
    REQUIRE (tick.rateHz() == eq1::EditorTick::activeHz);
    const auto before = built (host);
    REQUIRE (before.curves[0] > 0);
    REQUIRE (before.sum > 0);

    const int ticked = tick.count();
    for (int frame = 0; frame < 10; ++frame)
    {
        host.settle (16);
        paintFrame (host);
    }
    REQUIRE (tick.count() > ticked + 5);
    CHECK (built (host) == before);
}

TEST_CASE ("Changing a Band's Gain designs its curve and the sum again on the next frame, and no other Band's")
{
    OpenEditor host;
    host.addBand (1, 100.0f, 3.0f);
    host.addBand (2, 1000.0f, -4.0f);
    host.settle();
    paintFrame (host);
    const auto before = built (host);

    host.set (1, "gain", 6.0f);
    host.settle();
    paintFrame (host);
    const auto after = built (host);
    CHECK (after.curves[0] == before.curves[0] + 1);
    CHECK (after.curves[1] == before.curves[1]);
    CHECK (after.sum == before.sum + 1);
    // The handle moved with the Gain.
    CHECK (after.handles == before.handles + 1);
}
