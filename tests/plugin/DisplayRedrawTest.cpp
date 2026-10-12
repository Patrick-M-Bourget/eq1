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
}
