#include "display/HandlesLayer.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <optional>
#include <set>

using Catch::Matchers::WithinAbs;
using eq1::display::DisplayGeometry;

namespace
{
// A display frame over bands as heard, with selected selected.
struct Frame
{
    eq1::Settings bands;
    std::set<int> selected;
    eq1::display::DisplayFrame frame { .bands = bands, .selected = selected };

    eq1::BandSettings& band (int slot) { return bands.bands[static_cast<size_t> (slot - 1)]; }
    void add (int slot, double frequency, double gain, double dynamicRange = 0.0)
    {
        auto& b = band (slot);
        b.inUse = true;
        b.frequency = frequency;
        b.gain = gain;
        b.dynamicRange = dynamicRange;
    }
};

std::optional<eq1::display::Grip> gripOf (const std::vector<eq1::display::Grip>& grips, int slot)
{
    for (const auto& grip : grips)
        if (grip.slot == slot)
            return grip;
    return std::nullopt;
}
} // namespace

TEST_CASE ("A Dynamic Range grip sits at its Band's Frequency and heard Gain + Dynamic Range, 26 px below the handle with none, 14 px inside the edges")
{
    const DisplayGeometry geometry { .width = 1134, .height = 612, .rangeDb = 12 };
    Frame f;
    f.add (1, 1000.0, 3.0, 6.0);
    f.add (2, 200.0, -2.0);
    f.add (3, 5000.0, 10.0, 20.0);
    f.selected = { 2 };
    const auto grips = eq1::display::dynamicRangeGrips (geometry, f.frame);
    REQUIRE (grips.size() == 3);
    CHECK_THAT (gripOf (grips, 1)->centre.x, WithinAbs (geometry.xOf (1000.0), 0.01));
    CHECK_THAT (gripOf (grips, 1)->centre.y, WithinAbs (geometry.yOf (9.0), 0.01));
    CHECK_THAT (gripOf (grips, 2)->centre.y, WithinAbs (geometry.yOf (-2.0) + 26.0f, 0.01));
    CHECK_THAT (gripOf (grips, 3)->centre.y, WithinAbs (14.0, 0.01));
}

TEST_CASE ("Grips show for the selected Band with Gain unless Bypassed, and other Dynamic Bands not under Dynamics Bypass, at 55 % until hovered")
{
    const DisplayGeometry geometry { .width = 1134, .height = 612, .rangeDb = 12 };
    Frame f;
    f.add (1, 100.0, 0.0);        // selected, no dynamics: shown at full
    f.add (2, 200.0, 0.0, 6.0);   // Dynamic: at 55 %
    f.add (3, 400.0, 0.0, 6.0);   // Dynamic, hovered: full
    f.add (4, 800.0, 0.0, 6.0);   // under Dynamics Bypass: none
    f.band (4).dynamicsBypass = true;
    f.add (5, 1600.0, 0.0);       // not Dynamic: none
    f.selected = { 1 };
    f.frame.hover[2] = 1.0f;
    auto grips = eq1::display::dynamicRangeGrips (geometry, f.frame);
    CHECK (grips.size() == 3);
    CHECK_THAT (gripOf (grips, 1)->alpha, WithinAbs (1.0, 1.0e-6));
    CHECK_THAT (gripOf (grips, 2)->alpha, WithinAbs (0.55, 1.0e-6));
    CHECK_THAT (gripOf (grips, 3)->alpha, WithinAbs (1.0, 1.0e-6));

    // The selected Band Bypassed, or with a Shape without Gain, has none.
    f.band (1).bypass = true;
    CHECK_FALSE (gripOf (eq1::display::dynamicRangeGrips (geometry, f.frame), 1));
    f.band (1).bypass = false;
    f.band (1).shape = eq1::Shape::LowCut;
    CHECK_FALSE (gripOf (eq1::display::dynamicRangeGrips (geometry, f.frame), 1));
}

TEST_CASE ("Band handles: 16 px in the Band's colour with a dark 1 px ring; selected 22 px with a 2 px white ring and a 40 % glow; x1.15 on hover and while dragged")
{
    namespace tokens = staple::tokens;
    using eq1::display::handleStyle;
    const auto normal = handleStyle ({ .slot = 4 });
    CHECK (normal.diameter == 16.0f);
    CHECK (normal.fill == tokens::band[3]);
    CHECK (normal.ring == tokens::colour::handleRing);
    CHECK (normal.ringWidth == 1.0f);
    CHECK (normal.glowAlpha == 0.0f);

    const auto selected = handleStyle ({ .slot = 4, .selected = true });
    CHECK (selected.diameter == 22.0f);
    CHECK (selected.ring == juce::Colours::white);
    CHECK (selected.ringWidth == 2.0f);
    CHECK_THAT (selected.glowAlpha, WithinAbs (0.4, 1.0e-6));

    CHECK_THAT (handleStyle ({ .slot = 4, .hover = 1.0f }).diameter, WithinAbs (16.0 * 1.15, 1.0e-4));
    CHECK (handleStyle ({ .slot = 4, .selected = true, .hover = 1.0f }).diameter == 22.0f);
    CHECK_THAT (handleStyle ({ .slot = 4, .selected = true, .dragged = true }).diameter, WithinAbs (22.0 * 1.15, 1.0e-4));
}

TEST_CASE ("A Bypassed handle, and every handle under Global Bypass, is in the bypassed colour at 85 % with no glow; selected, its ring is 60 % white")
{
    namespace tokens = staple::tokens;
    using eq1::display::handleStyle;
    for (const auto state : { eq1::display::HandleState { .slot = 2, .selected = true, .bypassed = true },
                              eq1::display::HandleState { .slot = 2, .selected = true, .globalBypass = 1.0f } })
    {
        const auto style = handleStyle (state);
        CHECK (style.fill == tokens::bandBypassed[1].withAlpha (0.85f));
        CHECK (style.ring == juce::Colours::white.withAlpha (0.6f));
        CHECK (style.diameter == 22.0f);
        CHECK (style.glowAlpha == 0.0f);
    }
    CHECK (handleStyle ({ .slot = 2, .bypassed = true }).ring == tokens::colour::handleRing);
}

TEST_CASE ("The drag readout reads \"Band n\" over Frequency, Gain and Q, Gain left out on Shapes without it")
{
    eq1::BandSettings band;
    band.frequency = 1000.0;
    band.gain = 3.0;
    band.q = 1.0;
    auto readout = eq1::display::dragReadout (4, band);
    CHECK (readout.title == "Band 4");
    CHECK (readout.value == "1.00 kHz  +3.0 dB  Q 1.00");
    band.shape = eq1::Shape::HighCut;
    band.frequency = 250.0;
    band.q = 0.71;
    CHECK (eq1::display::dragReadout (12, band).value == "250 Hz  Q 0.71");
}
