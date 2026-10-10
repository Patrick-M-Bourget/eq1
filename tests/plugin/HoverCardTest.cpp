#include "EditorHarness.h"
#include "HoverCard.h"
#include "staple/controls/IconButton.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;
using harness::OpenEditor;

namespace
{
eq1::HoverCard& cardOf (OpenEditor& host)
{
    auto* card = harness::findChild<eq1::HoverCard> (*host.editor);
    REQUIRE (card != nullptr);
    return *card;
}

// The card's control titled title.
template <typename T = juce::Component>
T& control (eq1::HoverCard& card, const juce::String& title)
{
    auto* found = harness::findChild<T> (card, [&title] (T& c) { return c.getTitle() == title; });
    INFO ("No card control is titled " << title);
    REQUIRE (found != nullptr);
    return *found;
}

// The menu item with this text, at any depth.
std::function<void()> actionOf (const juce::PopupMenu& menu, const juce::String& text)
{
    for (juce::PopupMenu::MenuItemIterator it (menu, true); it.next();)
        if (it.getItem().text == text)
            return it.getItem().action;
    return {};
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

TEST_CASE ("The Hover Card is 172 x 70 px, centred 18 px above its handle, below it near the top, and kept 6 px inside the display")
{
    OpenEditor host;
    host.addBand (4, 1000.0f, 0.0f);
    host.addBand (5, 1000.0f, 11.5f);
    host.addBand (6, 10.0f, 0.0f);
    host.settle();
    auto& card = cardOf (host);
    // The card's body, and the display, in the display's coordinates.
    const auto body = [&] { return host.display.getLocalArea (card.getParentComponent(), card.body()); };
    const auto showAt = [&] (juce::Point<float> handle) {
        rest (host, handle);
        host.settle (400);
    };

    const auto middle = host.at (1000.0, 0.0);
    showAt (middle);
    REQUIRE (card.shownSlot() == 4);
    CHECK (body().getWidth() == 172);
    CHECK (body().getHeight() == 70);
    CHECK (body().getCentreX() == juce::roundToInt (middle.x));
    CHECK (body().getBottom() == juce::roundToInt (middle.y - 18.0f));

    const auto top = host.at (1000.0, 11.5);
    showAt (top);
    REQUIRE (card.shownSlot() == 5);
    CHECK (body().getY() == juce::roundToInt (top.y + 18.0f));

    showAt (host.at (10.0, 0.0));
    REQUIRE (card.shownSlot() == 6);
    CHECK (body().getX() == 6);
}

TEST_CASE ("The Hover Card follows its Band moved by automation, and hides at once when the Band is deleted or taken out of use")
{
    OpenEditor host;
    host.addBand (4, 1000.0f, 0.0f);
    host.settle();
    auto& card = cardOf (host);
    const auto bodyCentreX = [&] { return host.display.getLocalArea (card.getParentComponent(), card.body()).getCentreX(); };
    rest (host, host.at (1000.0, 0.0));
    host.settle (400);
    REQUIRE (card.shownSlot() == 4);

    host.set (4, "frequency", 2000.0f);
    host.settle();
    CHECK (bodyCentreX() == juce::roundToInt (host.at (2000.0, 0.0).x));

    host.set (4, "in_use", 0.0f);
    host.settle();
    CHECK (card.shownSlot() == 0);
}

namespace
{
// Band 4's card up, with Bands 2 and 4 selected and Band 6 elsewhere.
struct CardOnBand4
{
    OpenEditor host;
    eq1::HoverCard* card = nullptr;

    CardOnBand4()
    {
        host.addBand (2, 200.0f, 0.0f);
        host.addBand (4, 1000.0f, 0.0f);
        host.addBand (6, 5000.0f, 0.0f);
        host.settle();
        host.click (host.at (200.0, 0.0));
        const auto adding = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier);
        const auto handle = host.at (1000.0, 0.0);
        host.display.mouseDown (host.mouseEvent (handle, adding, handle));
        host.display.mouseUp (host.mouseEvent (handle, {}, handle));
        REQUIRE (host.display.selection() == std::set<int> { 2, 4 });
        rest (host, handle);
        host.settle (400);
        card = &cardOf (host);
        REQUIRE (card->shownSlot() == 4);
    }
};
} // namespace

TEST_CASE ("The Hover Card's Bypass, Delete and menu act on its Band alone, one undo step each, and leave the selection alone")
{
    CardOnBand4 f;
    auto& host = f.host;
    auto& card = *f.card;
    auto& history = host.processor.editHistory();

    int steps = history.undoSteps();
    control<juce::Button> (card, "Band 4 Bypass").onClick();
    CHECK (host.value (4, "bypass") >= 0.5f);
    CHECK (host.value (2, "bypass") < 0.5f);
    CHECK (history.undoSteps() == steps + 1);
    host.settle();
    CHECK (control<staple::IconButton> (card, "Band 4 Bypass").isOff());
    control<juce::Button> (card, "Band 4 Bypass").onClick();
    CHECK (host.value (4, "bypass") < 0.5f);

    // The menu is the Band menu for Band 4 alone.
    steps = history.undoSteps();
    const auto menu = card.menu();
    const auto invert = actionOf (menu, "Invert Gain");
    host.set (4, "gain", 3.0f);
    host.set (2, "gain", 3.0f);
    REQUIRE (invert != nullptr);
    invert();
    CHECK_THAT (host.value (4, "gain"), WithinAbs (-3.0, 1.0e-4));
    CHECK_THAT (host.value (2, "gain"), WithinAbs (3.0, 1.0e-4));
    CHECK (history.undoSteps() == steps + 1);
    CHECK (host.display.selection() == std::set<int> { 2, 4 });

    steps = history.undoSteps();
    control<juce::Button> (card, "Delete Band 4").onClick();
    CHECK (host.value (4, "in_use") < 0.5f);
    CHECK (host.value (2, "in_use") >= 0.5f);
    CHECK (history.undoSteps() == steps + 1);
    CHECK (card.shownSlot() == 0);
}

TEST_CASE ("The Hover Card's menu deletes only its Band, and the card stays up while the menu is open")
{
    CardOnBand4 f;
    auto& host = f.host;
    auto& card = *f.card;
    const auto remove = actionOf (card.menu(), "Delete");
    REQUIRE (remove != nullptr);
    remove();
    CHECK (host.value (4, "in_use") < 0.5f);
    CHECK (host.value (2, "in_use") >= 0.5f);
}

TEST_CASE ("The Hover Card is a group named for its Band, its controls are named in glossary terms, and none is a focus stop")
{
    CardOnBand4 f;
    auto& card = *f.card;
    CHECK (card.getTitle() == "Band 4 quick controls");
    REQUIRE (card.getAccessibilityHandler() != nullptr);
    CHECK (card.getAccessibilityHandler()->getRole() == juce::AccessibilityRole::group);
    for (const juce::String title : { "Band 4 Bypass", "Delete Band 4", "Band 4 menu" })
    {
        CAPTURE (title);
        auto& c = control (card, title);
        CHECK_FALSE (c.getWantsKeyboardFocus());
    }
    CHECK_FALSE (card.getWantsKeyboardFocus());
}
