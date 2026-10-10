#include "display/CurvesLayer.h"
#include "display/EdgeFadeLayer.h"
#include "display/GridLayer.h"
#include "staple/Tokens.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <vector>

using Catch::Matchers::WithinAbs;
using eq1::display::DisplayGeometry;

namespace
{
const juce::String minus = juce::String::charToString (0x2212);

std::vector<juce::String> texts (const std::vector<eq1::display::Label>& labels)
{
    std::vector<juce::String> out;
    for (const auto& label : labels)
        out.push_back (label.text);
    return out;
}

std::vector<eq1::display::Label> withColour (const std::vector<eq1::display::Label>& labels, juce::Colour colour)
{
    std::vector<eq1::display::Label> out;
    std::copy_if (labels.begin(), labels.end(), std::back_inserter (out), [&] (const auto& l) { return l.colour == colour; });
    return out;
}
} // namespace

TEST_CASE ("The grid's Gain lines are a third of the Display Range apart, with the 0 dB line apart from them")
{
    const auto [range, step] = GENERATE (std::pair { 6, 2.0 }, std::pair { 12, 4.0 }, std::pair { 30, 10.0 });
    const DisplayGeometry geometry { .width = 1134, .height = 612, .rangeDb = range };
    const auto lines = eq1::display::gridLines (geometry);
    std::vector<float> expected;
    for (double db : { -3.0, -2.0, -1.0, 1.0, 2.0, 3.0 })
        expected.push_back (geometry.yOf (db * step));
    REQUIRE (lines.gainY.size() == expected.size());
    for (size_t i = 0; i < expected.size(); ++i)
        CHECK_THAT (lines.gainY[i], WithinAbs (expected[i], 0.01));
    CHECK_THAT (lines.zeroY, WithinAbs (geometry.yOf (0.0), 0.01));
}

TEST_CASE ("The grid's Frequency lines run 20 Hz to 20 kHz: majors at 20, 50, 100 ..., minors between, hidden below 800 px")
{
    const DisplayGeometry wide { .width = 1134, .height = 612 };
    const auto lines = eq1::display::gridLines (wide);
    REQUIRE (lines.majorX.size() == 10);
    CHECK_THAT (lines.majorX.front(), WithinAbs (wide.xOf (20.0), 0.01));
    CHECK_THAT (lines.majorX.back(), WithinAbs (wide.xOf (20000.0), 0.01));
    REQUIRE (lines.minorX.size() == 18);
    CHECK_THAT (lines.minorX.front(), WithinAbs (wide.xOf (30.0), 0.01));
    CHECK_THAT (lines.minorX.back(), WithinAbs (wide.xOf (9000.0), 0.01));

    const DisplayGeometry narrow { .width = 799, .height = 612 };
    CHECK (eq1::display::gridLines (narrow).minorX.empty());
    CHECK (eq1::display::gridLines (narrow).majorX.size() == 10);
}

TEST_CASE ("The Frequency labels sit 10 px above the bottom, centred on their lines, the first and last pulled 12 px in")
{
    namespace colour = staple::tokens::colour;
    const DisplayGeometry geometry { .width = 1134, .height = 612 };
    const auto labels = withColour (eq1::display::gridLabels (geometry), colour::text2);
    // The 0 dB label is text2 too: the Frequency labels are the ones at the bottom.
    std::vector<eq1::display::Label> frequency;
    std::copy_if (labels.begin(), labels.end(), std::back_inserter (frequency), [] (const auto& l) { return l.area.getBottom() > 590.0f; });
    CHECK (texts (frequency) == std::vector<juce::String> { "20", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k" });
    for (const auto& label : frequency)
        CHECK_THAT (label.area.getBottom(), WithinAbs (602.0, 0.01));
    CHECK_THAT (frequency.front().area.getX(), WithinAbs (geometry.xOf (20.0) + 12.0f, 0.01));
    CHECK (frequency.front().justification == juce::Justification::centredLeft);
    CHECK_THAT (frequency.back().area.getRight(), WithinAbs (geometry.xOf (20000.0) - 12.0f, 0.01));
    CHECK (frequency.back().justification == juce::Justification::centredRight);
    CHECK_THAT (frequency[5].area.getCentreX(), WithinAbs (geometry.xOf (1000.0), 0.01));
    CHECK (frequency[5].justification == juce::Justification::centred);

    SECTION ("below 800 px, every other one is hidden")
    {
        const DisplayGeometry narrow { .width = 799, .height = 612 };
        std::vector<eq1::display::Label> shown;
        for (const auto& l : eq1::display::gridLabels (narrow))
            if (l.area.getBottom() > 590.0f)
                shown.push_back (l);
        CHECK (texts (shown) == std::vector<juce::String> { "20", "100", "500", "2k", "10k" });
    }
}

TEST_CASE ("The Gain labels sit 42 px from the right, signed, 0 brighter than the rest, without +/- the Display Range")
{
    namespace colour = staple::tokens::colour;
    const auto [range, expected] = GENERATE_COPY (
        std::pair { 6, std::vector<juce::String> { "+4", "+2", "0", minus + "2", minus + "4" } },
        std::pair { 12, std::vector<juce::String> { "+8", "+4", "0", minus + "4", minus + "8" } },
        std::pair { 30, std::vector<juce::String> { "+20", "+10", "0", minus + "10", minus + "20" } });
    const DisplayGeometry geometry { .width = 1134, .height = 612, .rangeDb = range };
    std::vector<eq1::display::Label> gain;
    for (const auto& l : eq1::display::gridLabels (geometry))
        if (l.area.getBottom() <= 590.0f)
            gain.push_back (l);
    CHECK (texts (gain) == expected);
    for (const auto& label : gain)
    {
        CHECK_THAT (label.area.getRight(), WithinAbs (1134.0 - 42.0, 0.01));
        CHECK (label.justification == juce::Justification::centredRight);
        CHECK (label.colour == (label.text == "0" ? colour::text2 : colour::text3));
    }
    CHECK_THAT (gain[1].area.getCentreY(), WithinAbs (geometry.yOf (range / 3.0), 0.01));
}

TEST_CASE ("The Analyzer's dB scale runs in 10 dB steps at 60 and 90 dB, 20 dB at 120, clear of the top and bottom")
{
    const DisplayGeometry geometry { .width = 1134, .height = 612 };
    const auto [range, expected] = GENERATE_COPY (
        std::pair { 60, std::vector<juce::String> { minus + "10", minus + "20", minus + "30", minus + "40", minus + "50" } },
        std::pair { 90, std::vector<juce::String> { minus + "10", minus + "20", minus + "30", minus + "40", minus + "50", minus + "60", minus + "70", minus + "80" } },
        std::pair { 120, std::vector<juce::String> { minus + "20", minus + "40", minus + "60", minus + "80", minus + "100" } });
    eq1::AnalyzerSettings settings;
    settings.rangeDb = range;
    const auto labels = eq1::display::analyzerScaleLabels (geometry, settings);
    CHECK (texts (labels) == expected);
    for (const auto& label : labels)
    {
        CHECK_THAT (label.area.getRight(), WithinAbs (1134.0 - 10.0, 0.01));
        CHECK (label.colour == staple::tokens::colour::text4);
        CHECK (label.area.getCentreY() > 40.0f);
        CHECK (label.area.getCentreY() < 612.0f - 30.0f);
    }
    // The top of the display is 0 dB on the Analyzer's scale.
    CHECK_THAT (labels[0].area.getCentreY(), WithinAbs ((range > 90 ? 20.0 : 10.0) / range * 612.0, 0.01));

    SECTION ("hidden when the Analyzer shows nothing")
    {
        settings.showPreEq = settings.showPostEq = settings.showSidechain = false;
        CHECK (eq1::display::analyzerScaleLabels (geometry, settings).empty());
    }
}

TEST_CASE ("The display's edges fade to bg0 over 18 px at the top, 84 at the bottom, 36 on the left and 56 on the right")
{
    const auto background = staple::tokens::colour::bg0;
    const DisplayGeometry geometry { .width = 600, .height = 400 };
    juce::Image image (juce::Image::ARGB, geometry.width, geometry.height, true);
    {
        juce::Graphics g (image);
        g.fillAll (juce::Colours::white);
        eq1::display::paintEdgeFades (g, geometry);
    }
    // How much of the white is left: 1 untouched, 0 covered by bg0.
    const auto whiteLeft = [&] (int x, int y) {
        const auto c = image.getPixelAt (x, y);
        return (c.getFloatRed() - background.getFloatRed()) / (1.0f - background.getFloatRed());
    };
    CHECK (whiteLeft (300, 200) > 0.99f);
    // Opaque at each edge, half-way across each fade's distance, and untouched just past it.
    const int cx = 300, cy = 200;
    CHECK (whiteLeft (cx, 0) < 0.06f);
    CHECK_THAT (whiteLeft (cx, 9), WithinAbs (0.5, 0.08));
    CHECK (whiteLeft (cx, 19) > 0.97f);
    CHECK (whiteLeft (cx, 399) < 0.03f);
    CHECK_THAT (whiteLeft (cx, 400 - 42), WithinAbs (0.5, 0.05));
    CHECK (whiteLeft (cx, 400 - 85) > 0.97f);
    CHECK (whiteLeft (0, cy) < 0.04f);
    CHECK_THAT (whiteLeft (18, cy), WithinAbs (0.5, 0.06));
    CHECK (whiteLeft (37, cy) > 0.97f);
    CHECK (whiteLeft (599, cy) < 0.03f);
    CHECK_THAT (whiteLeft (600 - 28, cy), WithinAbs (0.5, 0.05));
    CHECK (whiteLeft (600 - 57, cy) > 0.97f);
}

TEST_CASE ("A Band's curve takes its colour from the 24-slot palette by Band Slot, the bypassed palette when Bypassed")
{
    using eq1::display::bandCurveStyle;
    const int slot = GENERATE (1, 7, 13, 24);
    CHECK (bandCurveStyle ({ .slot = slot }).colour == staple::tokens::band[slot - 1]);
    CHECK (bandCurveStyle ({ .slot = slot, .bypassed = true }).colour == staple::tokens::bandBypassed[slot - 1]);
    CHECK (bandCurveStyle ({ .slot = slot, .selected = true, .bypassed = true }).colour == staple::tokens::bandBypassed[slot - 1]);
}

TEST_CASE ("Band curves: others 1 px at 50 % with a 10 % fill, lit on hover; the selected one 1.5 px, full, a 30 % fill and a glow")
{
    using eq1::display::bandCurveStyle;
    const auto other = bandCurveStyle ({ .slot = 3 });
    CHECK_THAT (other.lineWidth, WithinAbs (1.0, 1e-6));
    CHECK_THAT (other.lineAlpha, WithinAbs (0.5, 1e-6));
    CHECK_THAT (other.fillAlpha, WithinAbs (0.1, 1e-6));
    CHECK_THAT (other.glowAlpha, WithinAbs (0.0, 1e-6));

    const auto hovered = bandCurveStyle ({ .slot = 3, .hover = 1.0f });
    CHECK_THAT (hovered.lineWidth, WithinAbs (1.5, 1e-6));
    CHECK_THAT (hovered.lineAlpha, WithinAbs (0.95, 1e-6));
    CHECK_THAT (hovered.fillAlpha, WithinAbs (0.26, 1e-6));
    // Part-way through its fade.
    CHECK_THAT (bandCurveStyle ({ .slot = 3, .hover = 0.5f }).fillAlpha, WithinAbs (0.18, 1e-6));

    const auto selected = bandCurveStyle ({ .slot = 3, .selected = true });
    CHECK_THAT (selected.lineWidth, WithinAbs (1.5, 1e-6));
    CHECK_THAT (selected.lineAlpha, WithinAbs (1.0, 1e-6));
    CHECK_THAT (selected.fillAlpha, WithinAbs (0.3, 1e-6));
    CHECK_THAT (selected.glowAlpha, WithinAbs (0.3, 1e-6));
}

TEST_CASE ("A Bypassed Band's curve: selected, a 50 % line, a 14 % fill and no glow; others at 60 % of their alphas")
{
    using eq1::display::bandCurveStyle;
    const auto selected = bandCurveStyle ({ .slot = 2, .selected = true, .bypassed = true });
    CHECK_THAT (selected.lineAlpha, WithinAbs (0.5, 1e-6));
    CHECK_THAT (selected.fillAlpha, WithinAbs (0.14, 1e-6));
    CHECK_THAT (selected.glowAlpha, WithinAbs (0.0, 1e-6));
    const auto other = bandCurveStyle ({ .slot = 2, .bypassed = true });
    CHECK_THAT (other.lineAlpha, WithinAbs (0.3, 1e-6));
    CHECK_THAT (other.fillAlpha, WithinAbs (0.06, 1e-6));
    CHECK_THAT (eq1::display::dynamicRangeWashAlpha (false, 0.0f), WithinAbs (0.2, 1e-6));
    CHECK_THAT (eq1::display::dynamicRangeWashAlpha (true, 0.0f), WithinAbs (0.1, 1e-6));
}

TEST_CASE ("Under Global Bypass every curve is in its bypassed style at 45 % of its alphas, and the sum at 30 %")
{
    using eq1::display::bandCurveStyle;
    const auto other = bandCurveStyle ({ .slot = 5, .globalBypass = 1.0f });
    CHECK (other.colour == staple::tokens::bandBypassed[4]);
    CHECK_THAT (other.lineAlpha, WithinAbs (0.5 * 0.45, 1e-6));
    CHECK_THAT (other.fillAlpha, WithinAbs (0.1 * 0.45, 1e-6));
    const auto selected = bandCurveStyle ({ .slot = 5, .selected = true, .globalBypass = 1.0f });
    CHECK (selected.colour == staple::tokens::bandBypassed[4]);
    CHECK_THAT (selected.lineAlpha, WithinAbs (0.45, 1e-6));
    CHECK_THAT (selected.fillAlpha, WithinAbs (0.3 * 0.45, 1e-6));
    CHECK_THAT (selected.glowAlpha, WithinAbs (0.0, 1e-6));
    CHECK_THAT (eq1::display::dynamicRangeWashAlpha (false, 1.0f), WithinAbs (0.2 * 0.45, 1e-6));
    CHECK_THAT (eq1::display::sumCurveAlpha (1.0f), WithinAbs (0.3, 1e-6));
    CHECK_THAT (eq1::display::sumCurveAlpha (0.0f), WithinAbs (1.0, 1e-6));

    SECTION ("half-way through its fade, half-way between")
    {
        CHECK_THAT (bandCurveStyle ({ .slot = 5, .globalBypass = 0.5f }).lineAlpha, WithinAbs ((0.5 + 0.225) / 2.0, 1e-6));
        CHECK_THAT (eq1::display::sumCurveAlpha (0.5f), WithinAbs (0.65, 1e-6));
    }
}
