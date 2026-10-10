#include "EditorHarness.h"
#include "DisplayRangeChip.h"
#include "Parameters.h"
#include "staple/Tokens.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;
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

TEST_CASE ("A click inside a Band's filled curve selects it, the smallest curve there winning, Bypassed Bands included; a click on empty space clears the selection")
{
    OpenEditor host;
    analyzerOff (host);
    host.addBand (1, 1000.0f, 9.0f);
    host.addBand (2, 1000.0f, 3.0f);
    host.set (2, "bypass", 1.0f);
    host.addBand (3, 100.0f, -6.0f);
    host.settle();
    const auto& selected = host.display.selection();

    // Inside both Bells: Band 2's, the smaller there, wins although it is Bypassed.
    host.click (atDb (host, 1000.0, 1.5));
    CHECK (selected == std::set<int> { 2 });
    // Inside Band 1's alone, above Band 2's peak.
    host.click (atDb (host, 1000.0, 6.0));
    CHECK (selected == std::set<int> { 1 });
    // Inside the cut, below 0 dB.
    host.click (atDb (host, 100.0, -3.0));
    CHECK (selected == std::set<int> { 3 });
    // Above Band 1's peak, and below 0 dB under the Bells: empty space.
    host.click (atDb (host, 1000.0, 11.0));
    CHECK (selected.empty());
    host.click (atDb (host, 100.0, -3.0));
    host.click (atDb (host, 1000.0, -4.0));
    CHECK (selected.empty());
    // Where every curve is under 0.4 dB, none counts, even within 0.3 dB of 0 dB.
    host.click (atDb (host, 15000.0, 0.1));
    CHECK (selected.empty());
}

TEST_CASE ("A drag from empty space draws a marquee that selects the Bands inside it")
{
    OpenEditor host;
    analyzerOff (host);
    host.addBand (1, 100.0f, 0.0f);
    host.addBand (2, 1000.0f, 0.0f);
    host.addBand (3, 10000.0f, 0.0f);
    host.settle();
    host.drag (atDb (host, 50.0, 6.0), atDb (host, 2000.0, -6.0), juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier));
    CHECK (host.display.selection() == std::set<int> { 1, 2 });
}

TEST_CASE ("Spectrum Grab: a drag from the spectrum's line adds a Band at its peak")
{
    OpenEditor host;
    auto& processor = host.processor;
    processor.setAnalyzerSettings ({ .showPreEq = false, .showPostEq = true });
    processor.prepareToPlay (48000.0, 512);
    // A loud 1 kHz sine, so the spectrum's line peaks there.
    double phase = 0.0;
    for (int frame = 0; frame < 30; ++frame)
    {
        juce::AudioBuffer<float> buffer (processor.getTotalNumInputChannels(), 512);
        juce::MidiBuffer midi;
        for (int block = 0; block < 4; ++block)
        {
            for (int n = 0; n < 512; ++n, phase += 2.0 * juce::MathConstants<double>::pi * 1000.0 / 48000.0)
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    buffer.setSample (ch, n, static_cast<float> (0.5 * std::sin (phase)));
            processor.processBlock (buffer, midi);
        }
        host.settle (20);
    }
    // Down the display at 1 kHz until a press there grabs: only the spectrum's line does.
    const auto x = host.at (1000.0).x;
    const juce::ModifierKeys left (juce::ModifierKeys::leftButtonModifier);
    for (float y = 20.0f; y < static_cast<float> (host.display.getHeight()) && host.value (1, "in_use") == 0.0f; y += 6.0f)
        host.drag ({ x, y }, { x, y + 20.0f }, left);
    REQUIRE (host.value (1, "in_use") == 1.0f);
    CHECK_THAT (host.value (1, "frequency"), WithinRel (1000.0f, 0.1f));
    CHECK (host.display.selection() == std::set<int> { 1 });
}

TEST_CASE ("Hovering inside a Band's filled curve lights it, as hovering its handle does")
{
    OpenEditor host;
    analyzerOff (host);
    host.addBand (1, 1000.0f, 6.0f);
    host.settle();
    const auto fill = atDb (host, 1000.0, 3.0);
    const auto blue = [&] { return colourAt (snapshot (host), fill).getFloatBlue(); };
    const float resting = blue();
    const auto inside = atDb (host, 1200.0, 2.0);
    host.display.mouseMove (host.mouseEvent (inside, {}, inside));
    host.settle (400);
    CHECK (blue() > resting + 0.04f);
}

namespace
{
// The focusable element for Band slot's Dynamic Range grip, if shown.
juce::Component* gripElement (OpenEditor& host, int slot)
{
    const auto name = "Band " + juce::String (slot) + " Dynamic Range Handle";
    return harness::findChild<juce::Component> (host.display, [&] (juce::Component& c) { return c.getName() == name && c.isVisible(); });
}
} // namespace

TEST_CASE ("Dragging a Dynamic Range grip sets the stored Dynamic Range so its heard end follows the mouse, as one undo step, and never Solos or selects")
{
    OpenEditor host;
    analyzerOff (host);
    host.addBand (1, 1000.0f, 3.0f);
    host.set (1, "dynamic_range", 6.0f);
    host.addBand (2, 1000.0f, 0.0f, 1.0f); // a Low Shelf, never Dynamic, sharing the Frequency
    host.settle();
    const juce::ModifierKeys left (juce::ModifierKeys::leftButtonModifier);
    auto& history = host.processor.editHistory();

    SECTION ("at Gain Scale 100 %")
    {
        const auto grip = atDb (host, 1000.0, 9.0);
        host.display.mouseDown (host.mouseEvent (grip, left, grip));
        host.settle (500);
        CHECK (host.processor.soloSlot() == 0);
        host.display.mouseDrag (host.mouseEvent (atDb (host, 1000.0, 7.0), left, grip));
        host.display.mouseDrag (host.mouseEvent (atDb (host, 1000.0, 11.0), left, grip));
        host.display.mouseUp (host.mouseEvent (atDb (host, 1000.0, 11.0), {}, grip));
        CHECK_THAT (host.value (1, "dynamic_range"), WithinAbs (8.0, 1.0e-4));
        CHECK_THAT (host.value (1, "gain"), WithinAbs (3.0, 1.0e-4));
        CHECK_THAT (host.value (1, "frequency"), WithinRel (1000.0f, 1.0e-4f));
    }
    SECTION ("at Gain Scale 50 %: heard at +1.5 dB with +3 dB of range, dragged to -2 dB heard")
    {
        host.set (eq1::parameters::gainScaleId, 50.0f);
        host.settle();
        host.drag (atDb (host, 1000.0, 4.5), atDb (host, 1000.0, -2.0), left);
        CHECK_THAT (host.value (1, "dynamic_range"), WithinAbs (-7.0, 1.0e-4));
    }
    CHECK (history.undoSteps() == 1);
    // Pressing the grip selected its Band, and nothing else: no marquee, no Band moved.
    CHECK (host.display.selection() == std::set<int> { 1 });
    history.undo();
    CHECK_THAT (host.value (1, "dynamic_range"), WithinAbs (6.0, 1.0e-4));
}

TEST_CASE ("A selected Band with Gain and no Dynamic Range has a grip 26 px below its handle, which drags a range out")
{
    OpenEditor host;
    analyzerOff (host);
    host.addBand (1, 1000.0f, 0.0f);
    host.settle();
    CHECK (gripElement (host, 1) == nullptr);
    host.click (atDb (host, 1000.0, 0.0));
    host.settle();
    REQUIRE (gripElement (host, 1) != nullptr);
    const auto grip = atDb (host, 1000.0, 0.0).translated (0.0f, 26.0f);
    // Dragged 3 dB down from where it sits.
    host.drag (grip, grip.translated (0.0f, atDb (host, 1000.0, -3.0).y - atDb (host, 1000.0, 0.0).y), juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier));
    CHECK_THAT (host.value (1, "dynamic_range"), WithinAbs (-3.0, 1.0e-4));
}

TEST_CASE ("A double-click on a Dynamic Range grip sets the Dynamic Range to 0, as one undo step")
{
    OpenEditor host;
    analyzerOff (host);
    host.addBand (1, 1000.0f, 3.0f);
    host.set (1, "dynamic_range", 6.0f);
    host.settle();
    const auto grip = atDb (host, 1000.0, 9.0);
    host.click (grip);
    host.click (grip);
    host.display.mouseDoubleClick (host.mouseEvent (grip, {}, grip));
    CHECK_THAT (host.value (1, "dynamic_range"), WithinAbs (0.0, 1.0e-4));
    CHECK (host.value (2, "in_use") == 0.0f);
    CHECK (host.processor.editHistory().undoSteps() == 1);
}

TEST_CASE ("A focused Dynamic Range grip steps the range 1 dB per arrow, 0.5 dB with Shift, a press or a held key being one undo step")
{
    OpenEditor host;
    host.addBand (1, 1000.0f, 3.0f);
    host.set (1, "dynamic_range", 6.0f);
    host.settle();
    auto* grip = gripElement (host, 1);
    REQUIRE (grip != nullptr);
    grip->grabKeyboardFocus();
    auto& history = host.processor.editHistory();

    host.press (juce::KeyPress (juce::KeyPress::upKey));
    CHECK_THAT (host.value (1, "dynamic_range"), WithinAbs (7.0, 1.0e-4));
    host.press (juce::KeyPress (juce::KeyPress::downKey, juce::ModifierKeys::shiftModifier, 0));
    CHECK_THAT (host.value (1, "dynamic_range"), WithinAbs (6.5, 1.0e-4));
    CHECK (history.undoSteps() == 2);
    host.hold (juce::KeyPress (juce::KeyPress::downKey));
    host.hold (juce::KeyPress (juce::KeyPress::downKey));
    host.hold (juce::KeyPress (juce::KeyPress::downKey));
    host.release();
    CHECK_THAT (host.value (1, "dynamic_range"), WithinAbs (3.5, 1.0e-4));
    CHECK (history.undoSteps() == 3);
    // The Band itself didn't move.
    CHECK_THAT (host.value (1, "gain"), WithinAbs (3.0, 1.0e-4));
    // A screen reader reads it as the Band's Dynamic Range Handle.
    auto* handler = grip->getAccessibilityHandler();
    REQUIRE (handler != nullptr);
    CHECK (handler->getTitle() == "Band 1 Dynamic Range Handle");
    CHECK (handler->getValueInterface()->getCurrentValueAsString() == "+3.50 dB");
}

TEST_CASE ("A handle takes a press within 9 px of its centre, and a Dynamic Band's handle has no Dynamic Range ring around it")
{
    OpenEditor host;
    analyzerOff (host);
    host.addBand (1, 1000.0f, 0.0f);
    host.set (1, "dynamic_range", 24.0f);
    host.settle();
    const auto centre = atDb (host, 1000.0, 0.0);
    // Where the ring went: 14 px up and to the right of the handle, which the ring reached at +24 dB,
    // clear of the sum curve along 0 dB.
    const auto ring = colourAt (snapshot (host), centre.translated (10.0f, -10.0f));
    CHECK (ring.getFloatRed() < 0.2f);

    // 8.5 px off to the left: a press on the handle, which selects it and drags it.
    host.drag (centre.translated (-8.5f, 0.0f), host.at (2000.0).translated (-8.5f, 0.0f), juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier));
    CHECK_THAT (host.value (1, "frequency"), WithinRel (2000.0f, 0.01f));
}

TEST_CASE ("The ghost Bell follows the mouse over empty space, rests at 1 kHz with no Bands, and hides over a handle, a grip or a curve, under a menu and with every Band Slot in use")
{
    OpenEditor host;
    analyzerOff (host);
    host.settle();
    const auto move = [&] (juce::Point<float> to) { host.display.mouseMove (host.mouseEvent (to, {}, to)); };

    // No Bands, and the mouse elsewhere: resting at 1 kHz, half the range up.
    REQUIRE (host.display.ghost().has_value());
    CHECK_THAT (host.display.ghost()->frequency, WithinRel (1000.0, 1.0e-3));
    CHECK_THAT (host.display.ghost()->gain, WithinAbs (6.0, 1.0e-3));

    host.addBand (1, 1000.0f, 6.0f);
    host.settle();
    host.display.mouseExit (host.mouseEvent ({ 1.0f, 1.0f }, {}, { 1.0f, 1.0f }));
    CHECK_FALSE (host.display.ghost().has_value());
    // Over empty space it follows the mouse.
    move (atDb (host, 100.0, -5.0));
    REQUIRE (host.display.ghost().has_value());
    CHECK_THAT (host.display.ghost()->frequency, WithinRel (100.0, 1.0e-3));
    CHECK_THAT (host.display.ghost()->gain, WithinAbs (-5.0, 1.0e-3));
    // Over the handle, the curve and the selected Band's grip, none.
    move (atDb (host, 1000.0, 6.0));
    CHECK_FALSE (host.display.ghost().has_value());
    move (atDb (host, 1000.0, 3.0));
    CHECK_FALSE (host.display.ghost().has_value());
    host.click (atDb (host, 1000.0, 6.0));
    move (atDb (host, 1000.0, 6.0).translated (0.0f, 26.0f));
    CHECK_FALSE (host.display.ghost().has_value());

    // Under a menu, none.
    move (atDb (host, 100.0, -5.0));
    juce::PopupMenu menu;
    menu.addItem (1, "Item");
    menu.showMenuAsync ({});
    CHECK_FALSE (host.display.ghost().has_value());
    juce::PopupMenu::dismissAllActiveMenus();
    host.settle();
    // Desktop's timer may have moved the pointer to the machine's real mouse meanwhile: back to empty space.
    move (atDb (host, 100.0, -5.0));
    CHECK (host.display.ghost().has_value());

    // With all 24 Band Slots in use, none.
    for (int slot = 2; slot <= 24; ++slot)
        host.addBand (slot, 20.0f, 0.0f, 1.0f);
    host.settle();
    move (atDb (host, 100.0, -5.0));
    CHECK_FALSE (host.display.ghost().has_value());
}
