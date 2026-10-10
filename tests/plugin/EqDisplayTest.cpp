#include "EditorHarness.h"
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
