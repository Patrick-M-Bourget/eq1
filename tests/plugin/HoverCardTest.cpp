#include "EditorHarness.h"
#include "HoverCard.h"

#include <catch2/catch_test_macros.hpp>

using harness::OpenEditor;

namespace
{
eq1::HoverCard& cardOf (OpenEditor& host)
{
    auto* card = harness::findChild<eq1::HoverCard> (*host.editor);
    REQUIRE (card != nullptr);
    return *card;
}

// The pointer comes to rest at position on the display.
void rest (OpenEditor& host, juce::Point<float> position) { host.display.mouseMove (host.mouseEvent (position, {}, position)); }
} // namespace

TEST_CASE ("Resting 300 ms on a Band's handle shows its Hover Card")
{
    OpenEditor host;
    host.addBand (4, 1000.0f, 0.0f);
    host.settle();
    auto& card = cardOf (host);
    CHECK (card.shownSlot() == 0);
    CHECK_FALSE (card.isVisible());

    rest (host, host.at (1000.0, 0.0));
    host.settle (150);
    CHECK (card.shownSlot() == 0);
    host.settle (250);
    CHECK (card.shownSlot() == 4);
    CHECK (card.isVisible());
}

TEST_CASE ("The Hover Card hides 220 ms after the pointer leaves its handle and the card, and switches straight to another handle's")
{
    OpenEditor host;
    host.addBand (4, 1000.0f, 0.0f);
    host.addBand (2, 200.0f, 0.0f);
    host.settle();
    auto& card = cardOf (host);
    rest (host, host.at (1000.0, 0.0));
    host.settle (400);
    REQUIRE (card.shownSlot() == 4);

    // Onto another handle: at once.
    rest (host, host.at (200.0, 0.0));
    CHECK (card.shownSlot() == 2);

    // Off the handle, onto empty space: it stays up a while, then hides.
    rest (host, host.at (5000.0, 8.0));
    host.settle (100);
    CHECK (card.shownSlot() == 2);
    host.settle (250);
    CHECK (card.shownSlot() == 0);
    CHECK_FALSE (card.isVisible());

    // Off the display altogether, too; and back on the handle within the time, it stays.
    rest (host, host.at (200.0, 0.0));
    host.settle (400);
    REQUIRE (card.shownSlot() == 2);
    host.display.mouseExit (host.mouseEvent ({ 1.0f, 1.0f }, {}, { 1.0f, 1.0f }));
    host.settle (100);
    rest (host, host.at (200.0, 0.0));
    host.settle (250);
    CHECK (card.shownSlot() == 2);
    host.display.mouseExit (host.mouseEvent ({ 1.0f, 1.0f }, {}, { 1.0f, 1.0f }));
    host.settle (300);
    CHECK (card.shownSlot() == 0);
}

TEST_CASE ("The Hover Card stays up while the pointer is on it, and hides 220 ms after it leaves")
{
    OpenEditor host;
    host.addBand (4, 1000.0f, 0.0f);
    host.settle();
    auto& card = cardOf (host);
    rest (host, host.at (1000.0, 0.0));
    host.settle (400);
    REQUIRE (card.shownSlot() == 4);

    const auto middle = card.getLocalBounds().getCentre().toFloat();
    host.display.mouseExit (host.mouseEvent ({ 1.0f, 1.0f }, {}, { 1.0f, 1.0f }));
    card.mouseEnter (harness::mouseEvent (card, middle));
    host.settle (400);
    CHECK (card.shownSlot() == 4);
    card.mouseExit (harness::mouseEvent (card, { -20.0f, -20.0f }));
    host.settle (100);
    CHECK (card.shownSlot() == 4);
    host.settle (250);
    CHECK (card.shownSlot() == 0);
}

TEST_CASE ("No Hover Card on a Band's curve or its Dynamic Range Handle, or while a handle is pressed or dragged")
{
    OpenEditor host;
    host.addBand (4, 1000.0f, 6.0f);
    host.settle();
    auto& card = cardOf (host);

    // Inside the curve's fill, under the handle.
    rest (host, host.at (1000.0, 3.0));
    host.settle (400);
    CHECK (card.shownSlot() == 0);

    // The selected Band's Dynamic Range Handle, 26 px below its handle.
    host.click (host.at (1000.0, 6.0));
    host.display.mouseExit (host.mouseEvent ({ 1.0f, 1.0f }, {}, { 1.0f, 1.0f }));
    host.settle();
    rest (host, host.at (1000.0, 6.0).translated (0.0f, 26.0f));
    host.settle (400);
    CHECK (card.shownSlot() == 0);

    // Held still on the handle, then dragged: never.
    const juce::ModifierKeys left (juce::ModifierKeys::leftButtonModifier);
    const auto handle = host.at (1000.0, 6.0);
    rest (host, handle);
    host.display.mouseDown (host.mouseEvent (handle, left, handle));
    host.settle (400);
    CHECK (card.shownSlot() == 0);
    host.display.mouseDrag (host.mouseEvent (handle.translated (40.0f, 0.0f), left, handle));
    host.settle (400);
    CHECK (card.shownSlot() == 0);
    host.display.mouseUp (host.mouseEvent (handle.translated (40.0f, 0.0f), {}, handle));
}

TEST_CASE ("Pressing a handle or a click elsewhere hides the Hover Card, and the ghost Bell stays hidden while it is up")
{
    OpenEditor host;
    host.addBand (4, 1000.0f, 0.0f);
    host.addBand (2, 200.0f, 0.0f);
    host.settle();
    auto& card = cardOf (host);
    const auto handle = host.at (1000.0, 0.0);
    rest (host, handle);
    host.settle (400);
    REQUIRE (card.shownSlot() == 4);

    // Over empty space within the hide delay: no ghost Bell while the card is up.
    rest (host, host.at (5000.0, -6.0));
    CHECK_FALSE (host.display.ghost().has_value());

    rest (host, handle);
    host.settle (400);
    REQUIRE (card.shownSlot() == 4);
    host.click (host.at (200.0, 0.0));
    CHECK (card.shownSlot() == 0);
    CHECK (host.display.selection() == std::set<int> { 2 });

    rest (host, handle);
    host.settle (400);
    REQUIRE (card.shownSlot() == 4);
    host.click (host.at (5000.0, -6.0));
    CHECK (card.shownSlot() == 0);
}
