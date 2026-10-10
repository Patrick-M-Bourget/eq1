#include "EditorHarness.h"
#include "DisplayRangeChip.h"
#include "Parameters.h"
#include "staple/Tokens.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinRel;
using harness::OpenEditor;

TEST_CASE ("A right-click on the display never Solos, drags or starts a marquee")
{
    OpenEditor host;
    host.addBand (1, 100.0f, 0.0f);
    host.addBand (2, 1000.0f, 0.0f);
    host.settle();
    // A right-click, or on macOS a Ctrl-click.
    const auto mods = GENERATE (juce::ModifierKeys (juce::ModifierKeys::rightButtonModifier),
                                juce::ModifierKeys (juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::leftButtonModifier));
    if (! mods.isPopupMenu())
        SKIP ("Ctrl-click is a right-click on macOS only");

    // Held on Band 2's handle past the time a held handle Solos, then dragged.
    const auto handle = host.at (1000.0);
    host.display.mouseDown (host.mouseEvent (handle, mods, handle));
    host.settle (500);
    CHECK (host.processor.soloSlot() == 0);
    host.display.mouseDrag (host.mouseEvent (host.at (4000.0), mods, handle));
    CHECK_THAT (host.value (2, "frequency"), WithinRel (1000.0f, 1.0e-4f));
    host.display.mouseUp (host.mouseEvent (host.at (4000.0), mods.withoutMouseButtons(), handle));
    juce::PopupMenu::dismissAllActiveMenus();

    // Dragged from empty space across Band 1's handle: no marquee selects it, so Band 2, which the
    // right-click selected, is still the selection.
    host.drag (host.at (30.0).translated (0.0f, -40.0f), host.at (300.0).translated (0.0f, 40.0f), mods);
    juce::PopupMenu::dismissAllActiveMenus();
    host.display.grabKeyboardFocus();
    host.press (juce::KeyPress (juce::KeyPress::deleteKey));
    CHECK (host.value (1, "in_use") == 1.0f);
    CHECK (host.value (2, "in_use") == 0.0f);
}

TEST_CASE ("Cmd/Ctrl+A on the display selects every Band in use")
{
    OpenEditor host;
    for (int slot : { 2, 5, 9 })
        host.addBand (slot, 100.0f * static_cast<float> (slot), 0.0f);
    host.settle();
    host.display.grabKeyboardFocus();

    CHECK (host.press (juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 0)));
    host.press (juce::KeyPress (juce::KeyPress::deleteKey));
    for (int slot : { 2, 5, 9 })
        CHECK (host.value (slot, "in_use") == 0.0f);
}

namespace
{
// The display drawn at 2x, as on a Retina screen.
juce::Image snapshot (OpenEditor& host)
{
    return host.display.createComponentSnapshot (host.display.getLocalBounds(), true, 2.0f);
}

// The brightest pixel in area (in the display's own pixels) of an image drawn at 2x.
float brightest (const juce::Image& image, juce::Rectangle<float> area)
{
    float most = 0.0f;
    const auto pixels = (area * 2.0f).getSmallestIntegerContainer().getIntersection (image.getBounds());
    for (int y = pixels.getY(); y < pixels.getBottom(); ++y)
        for (int x = pixels.getX(); x < pixels.getRight(); ++x)
            most = std::max (most, image.getPixelAt (x, y).getBrightness());
    return most;
}
} // namespace

TEST_CASE ("The display's edges fade, but not a handle or a label there")
{
    OpenEditor host;
    host.processor.setAnalyzerSettings ({ .showPreEq = false, .showPostEq = false });
    // Its handle inside the left edge's fade.
    host.addBand (1, 12.0f, 0.0f);
    host.settle();
    const auto image = snapshot (host);
    const auto handle = host.at (12.0);
    REQUIRE (handle.x < staple::tokens::layout::fadeLeft);
    CHECK (brightest (image, juce::Rectangle<float> (4.0f, 4.0f).withCentre (handle.translated (0.0f, -6.0f))) > 0.8f);
    // "20k" inside the bottom's fade, at the bottom right.
    const auto width = static_cast<float> (host.display.getWidth()), height = static_cast<float> (host.display.getHeight());
    const auto x20k = host.at (20000.0).x;
    CHECK (brightest (image, { x20k - 40.0f, height - 24.0f, 30.0f, 14.0f }) > 0.5f);
    // The 0 dB line is faded at the very right edge, but not inside it.
    CHECK (brightest (image, { width - 2.0f, height / 2.0f - 1.0f, 2.0f, 2.0f }) < brightest (image, { width / 2.0f + 3.0f, height / 2.0f - 1.0f, 2.0f, 2.0f }));
}

namespace
{
// Where the display draws db at frequency, at a Display Range of +/-12 dB.
juce::Point<float> atDb (OpenEditor& host, double frequency, double db)
{
    const auto half = static_cast<float> (host.display.getHeight()) * 0.5f;
    return { host.at (frequency).x, half - static_cast<float> (db) / 12.0f * (half - 9.0f) };
}

// The display's colour at a point, averaged over a 2 x 2 px square at 2x.
juce::Colour colourAt (const juce::Image& image, juce::Point<float> point)
{
    const auto p = (point * 2.0f).toInt();
    float r = 0, g = 0, b = 0;
    for (int dy = 0; dy < 2; ++dy)
        for (int dx = 0; dx < 2; ++dx)
        {
            const auto c = image.getPixelAt (p.x + dx, p.y + dy);
            r += c.getFloatRed();
            g += c.getFloatGreen();
            b += c.getFloatBlue();
        }
    return juce::Colour::fromFloatRGBA (r / 4, g / 4, b / 4, 1.0f);
}

void analyzerOff (OpenEditor& host)
{
    host.processor.setAnalyzerSettings ({ .showPreEq = false, .showPostEq = false });
}
} // namespace

TEST_CASE ("A selected Dynamic Band shows a red wash between its curves at Gain and Gain + Dynamic Range, none under Dynamics Bypass")
{
    OpenEditor host;
    analyzerOff (host);
    // Band 1 is blue; its curve at Gain is flat, at Gain + Dynamic Range a +12 dB bell.
    host.addBand (1, 1000.0f, 0.0f);
    host.set (1, "dynamic_range", 12.0f);
    host.settle();
    const auto inside = atDb (host, 1000.0, 6.0);
    const auto unselected = colourAt (snapshot (host), inside);
    CHECK (unselected.getFloatRed() < 0.1f);

    host.click (host.at (1000.0));
    host.settle();
    const auto washed = colourAt (snapshot (host), inside);
    CHECK (washed.getFloatRed() > washed.getFloatBlue() + 0.05f);
    CHECK (washed.getFloatRed() > unselected.getFloatRed() + 0.1f);

    host.set (1, "dynamics_bypass", 1.0f);
    host.settle();
    CHECK (colourAt (snapshot (host), inside).getFloatRed() < 0.1f);
}

TEST_CASE ("Global Bypass fades the curves to their bypassed style and the sum to 30 %, and back when it is turned off")
{
    OpenEditor host;
    analyzerOff (host);
    host.addBand (1, 1000.0f, 6.0f);
    host.settle();
    // The sum curve on the Bell's skirt, an octave above it, away from the handle; the Band's fill under
    // the peak.
    const auto skirt = atDb (host, 2000.0, 5.5);
    const auto peak = juce::Rectangle<float> (skirt, atDb (host, 2000.0, 0.5)).expanded (1.0f, 0.0f);
    const auto fill = atDb (host, 1000.0, 3.0);
    const auto before = snapshot (host);
    const float sum = brightest (before, peak);
    const auto blueness = [] (juce::Colour c) { return c.getFloatBlue() - c.getFloatRed(); };
    const float fillBlue = blueness (colourAt (before, fill));
    REQUIRE (fillBlue > 0.03f);

    host.set (eq1::parameters::globalBypassId, 1.0f);
    host.settle (400);
    const auto bypassed = snapshot (host);
    CHECK (brightest (bypassed, peak) < sum * 0.5f);
    CHECK (blueness (colourAt (bypassed, fill)) < fillBlue * 0.5f);

    host.set (eq1::parameters::globalBypassId, 0.0f);
    host.settle (400);
    const auto back = snapshot (host);
    CHECK (brightest (back, peak) > sum * 0.95f);
    CHECK (blueness (colourAt (back, fill)) > fillBlue * 0.95f);
}

TEST_CASE ("Hovering a Band's handle lights its curve and fill, and they fade back when the mouse leaves")
{
    OpenEditor host;
    analyzerOff (host);
    host.addBand (1, 1000.0f, 6.0f);
    host.settle();
    const auto fill = atDb (host, 1000.0, 3.0);
    const auto blue = [&] { return colourAt (snapshot (host), fill).getFloatBlue(); };
    const float resting = blue();

    const auto handle = atDb (host, 1000.0, 6.0);
    host.display.mouseMove (host.mouseEvent (handle, {}, handle));
    host.settle (400);
    const float lit = blue();
    CHECK (lit > resting + 0.04f);

    host.display.mouseExit (host.mouseEvent ({ 1.0f, 1.0f }, {}, { 1.0f, 1.0f }));
    host.settle (400);
    CHECK_THAT (blue(), Catch::Matchers::WithinAbs (resting, 0.01));
}

TEST_CASE ("The Display Range chip reads the range, sets it from its menu, follows auto-zoom, and is named Display Range")
{
    OpenEditor host;
    const juce::String pm = juce::String::charToString (0x00B1);
    auto* chip = harness::findChild<eq1::DisplayRangeChip> (*host.editor);
    REQUIRE (chip != nullptr);
    CHECK (chip->getButtonText() == pm + "12 dB");
    auto* handler = chip->getAccessibilityHandler();
    REQUIRE (handler != nullptr);
    CHECK (handler->getTitle() == "Display Range");
    CHECK (handler->getValueInterface()->getCurrentValueAsString() == pm + "12 dB");
    // Top right of the display, 6 px in and 8 px down, 24 px tall.
    const auto display = host.display.getBoundsInParent();
    CHECK (chip->getRight() == display.getRight() - 6);
    CHECK (chip->getY() == display.getY() + 8);
    CHECK (chip->getHeight() == 24);

    // Its menu: the three ranges, the current one ticked; picking one sets it.
    const auto menu = chip->menu();
    std::vector<juce::String> items;
    for (juce::PopupMenu::MenuItemIterator it (menu); it.next();)
    {
        auto& item = it.getItem();
        items.push_back (item.text);
        CHECK (item.isTicked == (item.text == pm + "12 dB"));
        if (item.text == pm + "30 dB")
            item.action();
    }
    CHECK (items == std::vector<juce::String> { pm + "6 dB", pm + "12 dB", pm + "30 dB" });
    CHECK (host.processor.displayRangeDb() == 30);
    host.settle (300);
    CHECK (chip->getButtonText() == pm + "30 dB");

    // Auto-zoom shows on it too.
    host.processor.setDisplayRangeDb (6);
    host.settle (300);
    CHECK (chip->getButtonText() == pm + "6 dB");
    host.addBand (1, 1000.0f, 10.0f);
    host.settle (300);
    CHECK (chip->getButtonText() == pm + "12 dB");
}
