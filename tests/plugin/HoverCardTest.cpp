#include "EditorHarness.h"
#include "HeaderBar.h"
#include "HoverCard.h"
#include "Parameters.h"
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

// Rests the pointer on a handle until the card shows slot's, and checks it does. JUCE's Desktop timer
// can send the machine's real mouse to the display meanwhile (CODING_STANDARDS.md, "Editor behaviour"),
// which ends the rest, so the pointer is sent again until the card is up, for up to 3 s.
void showCard (OpenEditor& host, juce::Point<float> handle, int slot)
{
    for (int tries = 0; tries < 30 && cardOf (host).shownSlot() != slot; ++tries)
    {
        rest (host, handle);
        host.settle (100);
    }
    REQUIRE (cardOf (host).shownSlot() == slot);
}
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
    showCard (host, host.at (1000.0, 0.0), 4);

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
    showCard (host, host.at (200.0, 0.0), 2);
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
    showCard (host, host.at (1000.0, 0.0), 4);

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
    showCard (host, handle, 4);

    // Over empty space within the hide delay: no ghost Bell while the card is up.
    rest (host, host.at (5000.0, -6.0));
    CHECK_FALSE (host.display.ghost().has_value());

    showCard (host, handle, 4);
    host.click (host.at (200.0, 0.0));
    CHECK (card.shownSlot() == 0);
    CHECK (host.display.selection() == std::set<int> { 2 });

    showCard (host, handle, 4);
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
    const auto showAt = [&] (juce::Point<float> handle, int slot) { showCard (host, handle, slot); };

    const auto middle = host.at (1000.0, 0.0);
    showAt (middle, 4);
    CHECK (body().getWidth() == 172);
    CHECK (body().getHeight() == 70);
    CHECK (body().getCentreX() == juce::roundToInt (middle.x));
    CHECK (body().getBottom() == juce::roundToInt (middle.y - 18.0f));

    const auto top = host.at (1000.0, 11.5);
    showAt (top, 5);
    CHECK (body().getY() == juce::roundToInt (top.y + 18.0f));

    showAt (host.at (10.0, 0.0), 6);
    CHECK (body().getX() == 6);
}

TEST_CASE ("The Hover Card follows its Band moved by automation, and hides at once when the Band is deleted or taken out of use")
{
    OpenEditor host;
    host.addBand (4, 1000.0f, 0.0f);
    host.settle();
    auto& card = cardOf (host);
    const auto bodyCentreX = [&] { return host.display.getLocalArea (card.getParentComponent(), card.body()).getCentreX(); };
    showCard (host, host.at (1000.0, 0.0), 4);

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
        showCard (host, handle, 4);
        card = &cardOf (host);
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

TEST_CASE ("The Hover Card's menu has no Select All, deletes only its Band, and the card stays up while it is open")
{
    CardOnBand4 f;
    auto& host = f.host;
    auto& card = *f.card;
    control<juce::Button> (card, "Band 4 menu").onClick();
    CHECK (card.isHeld());
    host.display.mouseExit (host.mouseEvent ({ 1.0f, 1.0f }, {}, { 1.0f, 1.0f }));
    host.settle (400);
    CHECK (card.shownSlot() == 4);
    juce::PopupMenu::dismissAllActiveMenus();

    // It never changes the selection, so it has no Select All.
    CHECK (actionOf (card.menu(), "Select All") == nullptr);
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

TEST_CASE ("The Hover Card's values read \"1.00 kHz\", \"+3.00 dB\" and \"Q 0.707\", and are named for the Band")
{
    OpenEditor host;
    host.addBand (4, 1000.0f, 3.0f);
    host.set (4, "q", 0.707f);
    host.settle();
    showCard (host, host.at (1000.0, 3.0), 4);
    auto& card = cardOf (host);
    CHECK (control<eq1::HoverCard::Value> (card, "Band 4 Frequency").text() == "1.00 kHz");
    CHECK (control<eq1::HoverCard::Value> (card, "Band 4 Gain").text() == "+3.00 dB");
    CHECK (control<eq1::HoverCard::Value> (card, "Band 4 Q").text() == "Q 0.707");
    host.set (4, "frequency", 85.0f);
    host.set (4, "gain", -2.5f);
    host.settle();
    CHECK (control<eq1::HoverCard::Value> (card, "Band 4 Frequency").text() == "85.0 Hz");
    CHECK (control<eq1::HoverCard::Value> (card, "Band 4 Gain").text() == "-2.50 dB");
    for (const juce::String title : { "Band 4 Frequency", "Band 4 Gain", "Band 4 Q" })
        CHECK_FALSE (control (card, title).getWantsKeyboardFocus());
}

TEST_CASE ("Dragging a Hover Card value moves its Band alone as one undo step, over the Band panel's range and scaling, while the card stays put")
{
    CardOnBand4 f;
    auto& host = f.host;
    auto& card = *f.card;
    auto& history = host.processor.editHistory();
    auto& frequency = control<eq1::HoverCard::Value> (card, "Band 4 Frequency");
    const auto body = card.body();
    const juce::ModifierKeys left (juce::ModifierKeys::leftButtonModifier);
    const auto press = frequency.getLocalBounds().getCentre().toFloat();
    const int steps = history.undoSteps();

    // A knob's whole range over 200 px: 50 px up is a quarter of it.
    frequency.mouseDown (harness::mouseEvent (frequency, press, left));
    frequency.mouseDrag (harness::mouseEvent (frequency, press.translated (0.0f, -50.0f), left, press));
    host.settle();
    const auto& parameter = host.parameter ("band4_frequency");
    CHECK_THAT (parameter.getValue(), WithinAbs (parameter.convertTo0to1 (1000.0f) + 0.25f, 1.0e-3));
    CHECK (card.shownSlot() == 4);
    CHECK (card.body() == body);
    // Shift: 800 px for the range, so 40 px down is a twentieth of it.
    const auto fine = left.withFlags (juce::ModifierKeys::shiftModifier);
    frequency.mouseDrag (harness::mouseEvent (frequency, press.translated (0.0f, -50.0f), fine, press));
    frequency.mouseDrag (harness::mouseEvent (frequency, press.translated (0.0f, -10.0f), fine, press));
    CHECK_THAT (parameter.getValue(), WithinAbs (parameter.convertTo0to1 (1000.0f) + 0.25f - 0.05f, 1.0e-3));
    frequency.mouseUp (harness::mouseEvent (frequency, press.translated (0.0f, -10.0f), {}, press));
    CHECK (history.undoSteps() == steps + 1);
    CHECK (host.display.selection() == std::set<int> { 2, 4 });
    CHECK_THAT (host.value (2, "frequency"), WithinAbs (200.0, 1.0e-3));

    // Once the drag ends it goes back to its handle, which the drag moved.
    host.settle();
    CHECK (card.body() != body);
}

TEST_CASE ("Double-clicking a Hover Card value types it: Enter sets it as one undo step, Esc and a click away cancel")
{
    CardOnBand4 f;
    auto& host = f.host;
    auto& card = *f.card;
    auto& history = host.processor.editHistory();
    auto& gain = control<eq1::HoverCard::Value> (card, "Band 4 Gain");
    const auto at = gain.getLocalBounds().getCentre().toFloat();
    const auto body = card.body();

    const auto open = [&] {
        gain.mouseDoubleClick (harness::mouseEvent (gain, at, {}, at, 2));
        REQUIRE (gain.isTypingIn());
        REQUIRE (gain.getTypeInField() != nullptr);
    };
    open();
    CHECK (gain.getTypeInField()->getText() == "0.00");
    CHECK (card.isHeld());
    const int steps = history.undoSteps();
    gain.getTypeInField()->setText ("4.5");
    gain.getTypeInField()->keyPressed (juce::KeyPress (juce::KeyPress::returnKey));
    CHECK_THAT (host.value (4, "gain"), WithinAbs (4.5, 1.0e-4));
    CHECK_FALSE (gain.isTypingIn());
    CHECK (history.undoSteps() == steps + 1);
    CHECK (host.display.selection() == std::set<int> { 2, 4 });

    open();
    gain.getTypeInField()->setText ("-9");
    gain.getTypeInField()->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
    CHECK_THAT (host.value (4, "gain"), WithinAbs (4.5, 1.0e-4));
    CHECK_FALSE (gain.isTypingIn());

    open();
    gain.getTypeInField()->setText ("-9");
    gain.getTypeInField()->onFocusLost();
    CHECK_THAT (host.value (4, "gain"), WithinAbs (4.5, 1.0e-4));
    CHECK_FALSE (gain.isTypingIn());
    CHECK (card.body() == body);
}

TEST_CASE ("On a Cut the Hover Card shows its Slope in Gain's place, on other gainless Shapes \"No Gain\", read-only; a Bypassed Band's values fade and stay editable")
{
    OpenEditor host;
    host.addBand (4, 1000.0f, 0.0f, static_cast<float> (eq1::Shape::LowCut));
    host.set (4, "slope", 24.0f);
    host.settle();
    showCard (host, host.at (1000.0, 0.0), 4);
    auto& card = cardOf (host);
    auto& gain = control<eq1::HoverCard::Value> (card, "Band 4 Gain");
    CHECK (gain.text() == "24 dB/oct");
    CHECK_FALSE (gain.isEnabled());

    host.set (4, "shape", static_cast<float> (eq1::Shape::Notch));
    host.settle();
    CHECK (gain.text() == "No Gain");
    CHECK_FALSE (gain.isEnabled());

    host.set (4, "shape", static_cast<float> (eq1::Shape::Bell));
    host.set (4, "bypass", 1.0f);
    host.settle (300);
    CHECK (gain.isEnabled());
    auto& frequency = control<eq1::HoverCard::Value> (card, "Band 4 Frequency");
    CHECK_THAT (frequency.getAlpha(), WithinAbs (0.38, 1.0e-3));
    CHECK (frequency.isEnabled());
    CHECK (control (card, "Band 4 Bypass").getAlpha() == 1.0f);
}

TEST_CASE ("The Hover Card's Shape opens a strip of every Shape under the card, which sets its Band's Shape as one undo step and keeps the card up")
{
    CardOnBand4 f;
    auto& host = f.host;
    auto& card = *f.card;
    auto& history = host.processor.editHistory();
    auto& shape = control<juce::Button> (card, "Band 4 Shape");
    CHECK_FALSE (shape.getWantsKeyboardFocus());

    shape.onClick();
    auto* strip = card.shapeStrip();
    REQUIRE (strip != nullptr);
    REQUIRE (strip->isVisible());
    CHECK (card.isHeld());
    const auto inDisplay = [&] (juce::Rectangle<int> r) { return host.display.getLocalArea (card.getParentComponent(), r); };
    const auto stripArea = inDisplay (strip->getBoundsInParent());
    CHECK (stripArea.getY() == inDisplay (card.body()).getBottom() + 6);
    CHECK (stripArea.getHeight() == 28 + 2 * 4);
    std::vector<juce::String> names;
    for (auto* child : strip->getChildren())
    {
        CHECK (child->getWidth() == 32);
        CHECK (child->getHeight() == 28);
        CHECK_FALSE (child->getWantsKeyboardFocus());
        names.push_back (child->getTitle());
    }
    CHECK (names == std::vector<juce::String> (eq1::parameters::shapeNames().begin(), eq1::parameters::shapeNames().end()));

    const int steps = history.undoSteps();
    auto* notch = harness::findChild<juce::Button> (*strip, [] (juce::Button& b) { return b.getTitle() == "Notch"; });
    REQUIRE (notch != nullptr);
    notch->onClick();
    CHECK (host.value (4, "shape") == static_cast<float> (eq1::Shape::Notch));
    CHECK (host.value (2, "shape") == static_cast<float> (eq1::Shape::Bell));
    CHECK (history.undoSteps() == steps + 1);
    CHECK_FALSE (strip->isVisible());
    CHECK (host.display.selection() == std::set<int> { 2, 4 });
}

TEST_CASE ("The Hover Card's Shape strip opens above the card when there is no room under it")
{
    OpenEditor host;
    host.addBand (4, 1000.0f, -12.0f);
    host.settle();
    showCard (host, host.at (1000.0, -12.0), 4);
    auto& card = cardOf (host);
    control<juce::Button> (card, "Band 4 Shape").onClick();
    REQUIRE (card.shapeStrip() != nullptr);
    CHECK (card.shapeStrip()->getBottom() == card.body().getY() - 6);
}

TEST_CASE ("Hover Card screenshots", "[.screens]")
{
    OpenEditor host;
    host.editor->setSize (1200, 760);
    host.addBand (1, 120.0f, 4.0f);
    host.addBand (4, 1000.0f, 3.0f);
    host.addBand (6, 8000.0f, 0.0f, static_cast<float> (eq1::Shape::HighCut));
    host.set (6, "slope", 24.0f);
    host.settle (300);
    auto& card = cardOf (host);
    // The card and what is around it, in the editor's coordinates.
    const auto save = [&] (const juce::String& name) {
        host.settle (300);
        auto area = host.editor->getLocalArea (card.getParentComponent(), card.body()).expanded (60, 30);
        if (auto* strip = card.shapeStrip())
            area = area.getUnion (host.editor->getLocalArea (card.getParentComponent(), strip->getBoundsInParent()).expanded (20));
        harness::writeSnapshot (*host.editor, "hover-card-" + name, area);
    };
    const auto showOn = [&] (double frequency, double db, int slot) { showCard (host, host.at (frequency, db), slot); };

    showOn (1000.0, 3.0, 4);
    save ("bell");
    control<juce::Button> (card, "Band 4 Shape").onClick();
    save ("shape-strip");
    control<juce::Button> (card, "Band 4 Shape").onClick();
    host.set (4, "bypass", 1.0f);
    save ("bypassed");
    showOn (8000.0, 0.0, 6);
    save ("high-cut");
}

TEST_CASE ("A click anywhere else in the editor hides the Hover Card, even with its Shape strip open, but not one on the card or its strip")
{
    CardOnBand4 f;
    auto& host = f.host;
    auto& card = *f.card;
    const auto clickOn = [&] (juce::Component& target) {
        const auto at = target.getLocalBounds().getCentre().toFloat();
        host.editor->mouseDown (harness::mouseEvent (target, at, juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier)));
    };
    auto& shape = control<juce::Button> (card, "Band 4 Shape");
    shape.onClick();
    REQUIRE (card.shapeStrip() != nullptr);
    clickOn (shape);
    clickOn (*card.shapeStrip());
    CHECK (card.shownSlot() == 4);

    auto* header = harness::findChild<eq1::HeaderBar> (*host.editor);
    REQUIRE (header != nullptr);
    clickOn (*header);
    CHECK (card.shownSlot() == 0);
    CHECK (card.shapeStrip() == nullptr);
}
