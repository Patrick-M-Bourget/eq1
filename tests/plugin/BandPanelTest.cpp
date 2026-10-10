#include "BandEditing.h"
#include "BandPanel.h"
#include "EditorHarness.h"
#include "staple/controls/EdgeSelector.h"
#include "staple/controls/IconButton.h"
#include "DetectionRangeBar.h"
#include "DynamicRangeRing.h"
#include "DynamicsSection.h"
#include "LevelBallistics.h"
#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <memory>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace
{

// A plugin with the Band panel the editor shows, and Bands 3 and 4 Bells, 5 a Low Cut.
struct Host
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    eq1::BandEditing editing { processor.parameterState(), processor.editHistory() };
    std::unique_ptr<eq1::BandPanel> panel = std::make_unique<eq1::BandPanel> (processor, editing);

    Host()
    {
        for (int slot : { 3, 4, 5 })
            set (slot, "in_use", 1.0f);
        set (5, "shape", 2.0f);
    }

    void set (int slot, const char* control, float plain)
    {
        auto* parameter = processor.parameterState().getParameter ("band" + juce::String (slot) + "_" + control);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
    }
};

} // namespace

TEST_CASE ("The Band panel meters the Band it shows, when its Shape has dynamics")
{
    Host host;
    CHECK (host.processor.meteredSlot() == 0);

    host.panel->show (3);
    CHECK (host.processor.meteredSlot() == 3);

    // Selecting another Band moves the metering to it.
    host.panel->show (4);
    CHECK (host.processor.meteredSlot() == 4);

    SECTION ("showing no Band lets go of it")
    {
        host.panel->show (0);
        CHECK (host.processor.meteredSlot() == 0);
    }
    SECTION ("a Shape without dynamics isn't metered")
    {
        host.panel->show (5);
        CHECK (host.processor.meteredSlot() == 0);
    }
    SECTION ("closing the editor lets go of it")
    {
        host.panel.reset();
        CHECK (host.processor.meteredSlot() == 0);
    }
}

TEST_CASE ("A level meter rises at once to a louder level and falls at 20 dB/s")
{
    eq1::LevelBallistics meter;
    CHECK_THAT (meter.update (-10.0, 0.0), WithinAbs (-10.0, 1.0e-9));
    CHECK_THAT (meter.update (-100.0, 0.25), WithinAbs (-15.0, 1.0e-9));
    CHECK_THAT (meter.update (-100.0, 0.25), WithinAbs (-20.0, 1.0e-9));
    CHECK_THAT (meter.update (-6.0, 0.25), WithinAbs (-6.0, 1.0e-9));
    // It falls no lower than the level read.
    CHECK_THAT (meter.update (-8.0, 1.0), WithinAbs (-8.0, 1.0e-9));
    meter.reset();
    CHECK_THAT (meter.update (eq1::levelFloorDb, 0.0), WithinAbs (eq1::levelFloorDb, 1.0e-9));
}

namespace
{

// The editor with Bands 1 (Bell, 1 kHz), 2 (Low Cut, 100 Hz) and 3 (High Shelf, 5 kHz), and Band 1
// selected, on stereo unless told otherwise.
struct PanelEditor : harness::OpenEditor
{
    explicit PanelEditor (bool mono = false)
    {
        juce::AudioProcessor::BusesLayout layout;
        const auto channels = mono ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
        layout.inputBuses.add (channels);
        layout.inputBuses.add (juce::AudioChannelSet::disabled());
        layout.outputBuses.add (channels);
        REQUIRE (processor.setBusesLayout (layout));
        processor.prepareToPlay (48000.0, 512);
        addBand (1, 1000.0f, 0.0f);
        addBand (2, 100.0f, 0.0f, 2.0f);
        addBand (3, 5000.0f, 0.0f, 3.0f);
        settle();
        click (at (1000.0));
        settle();
    }

    eq1::BandPanel& panel() { return *harness::findChild<eq1::BandPanel> (*editor); }

    // The panel's control with this accessible title.
    template <typename T = juce::Component>
    T& control (const juce::String& title)
    {
        auto found = findAll<T> ([&title] (T& c) { return c.getTitle() == title; });
        INFO ("No control is titled " << title);
        REQUIRE (found.size() == 1);
        return *found.front();
    }

    bool tabReaches (juce::Component& c)
    {
        const auto order = juce::KeyboardFocusTraverser().getAllComponents (editor.get());
        return std::find (order.begin(), order.end(), &c) != order.end();
    }
};

} // namespace

TEST_CASE ("The Band panel keeps one width for every Shape, Band and Bypass state, on stereo and mono")
{
    const bool mono = GENERATE (false, true);
    PanelEditor host (mono);
    auto& panel = host.panel();
    REQUIRE (panel.isVisible());
    const auto bounds = panel.getBounds();
    CHECK (bounds.getWidth() == eq1::BandPanel::width);
    for (int shape = 0; shape < 10; ++shape)
        for (float bypass : { 0.0f, 1.0f })
        {
            CAPTURE (shape, bypass);
            host.set (1, "shape", static_cast<float> (shape));
            host.set (1, "bypass", bypass);
            host.settle (120);
            CHECK (panel.getBounds() == bounds);
        }
    host.click (host.at (100.0));
    host.settle();
    CHECK (panel.getBounds() == bounds);
}

TEST_CASE ("The Band panel is hidden while no Band is selected")
{
    PanelEditor host;
    CHECK (host.panel().isVisible());
    host.click (host.at (20000.0).translated (0.0f, 100.0f));
    host.settle();
    CHECK_FALSE (host.panel().isVisible());
}

TEST_CASE ("Controls a Band's Shape or the track doesn't offer are shown dimmed, disabled, skipped by Tab and still named")
{
    const auto check = [] (PanelEditor& host, const juce::String& title, bool available) {
        CAPTURE (title);
        auto& c = host.control (title);
        CHECK (c.isShowing());
        CHECK (c.isEnabled() == available);
        CHECK (host.tabReaches (c) == available);
    };
    SECTION ("Gain on a Cut, Notch, Band Pass and All Pass")
    {
        PanelEditor host;
        for (float shape : { 2.0f, 4.0f, 5.0f, 6.0f, 9.0f })
        {
            host.set (1, "shape", shape);
            host.settle (120);
            check (host, "Band 1 Gain", false);
        }
        host.set (1, "shape", 1.0f);
        host.settle (120);
        check (host, "Band 1 Gain", true);
    }
    SECTION ("Slope on a Bell and Flat Tilt, Q on Flat Tilt")
    {
        PanelEditor host;
        check (host, "Band 1 Slope", false);
        check (host, "Band 1 Q", true);
        host.set (1, "shape", 8.0f);
        host.settle (120);
        check (host, "Band 1 Slope", false);
        check (host, "Band 1 Q", false);
        host.set (1, "shape", 2.0f);
        host.settle (120);
        check (host, "Band 1 Slope", true);
        check (host, "Band 1 Q", true);
    }
    SECTION ("Stereo Placement on mono")
    {
        PanelEditor mono (true);
        check (mono, "Band 1 Stereo Placement", false);
        PanelEditor stereo;
        check (stereo, "Band 1 Stereo Placement", true);
    }
}

TEST_CASE ("A Bypassed Band's panel fades to 38 % but Bypass and Delete, and stays editable")
{
    PanelEditor host;
    host.set (1, "dynamic_range", -6.0f);
    host.set (1, "bypass", 1.0f);
    host.settle (300);
    for (const juce::String title : { "Band 1 Solo", "Band 1 Shape", "Band 1 Slope", "Band 1 Frequency", "Band 1 Gain", "Band 1 Q",
                                      "Band 1 Stereo Placement", "Previous Band", "Next Band", "Band 1 Clear Dynamics",
                                      "Band 1 Dynamics Bypass" })
    {
        CAPTURE (title);
        CHECK_THAT (host.control (title).getAlpha(), WithinAbs (0.38, 0.01));
    }
    for (const juce::String title : { "Band 1 Bypass", "Band 1 Delete" })
        CHECK (host.control (title).getAlpha() == 1.0f);
    auto& gain = host.control<juce::Slider> ("Band 1 Gain");
    CHECK (gain.isEnabled());
    gain.setValue (4.0, juce::sendNotificationSync);
    CHECK_THAT (host.value (1, "gain"), WithinAbs (4.0, 1.0e-4));

    host.set (1, "bypass", 0.0f);
    host.settle (300);
    CHECK (host.control ("Band 1 Gain").getAlpha() == 1.0f);
}

TEST_CASE ("Holding the Band panel's Solo button Solos its Band until it is let go, the Band changes or is deleted")
{
    PanelEditor host;
    auto& solo = host.control<staple::IconButton> ("Band 1 Solo");
    const auto hold = [&solo] { solo.setState (juce::Button::buttonDown); };
    const auto letGo = [&solo] { solo.setState (juce::Button::buttonNormal); };

    hold();
    CHECK (host.processor.soloSlot() == 1);
    CHECK (solo.isLit());
    letGo();
    CHECK (host.processor.soloSlot() == 0);
    CHECK_FALSE (solo.isLit());

    SECTION ("changing Band lets go")
    {
        hold();
        host.click (host.at (100.0));
        host.settle();
        CHECK (host.processor.soloSlot() == 0);
    }
    SECTION ("deleting the Band lets go")
    {
        hold();
        host.control<juce::Button> ("Band 1 Delete").onClick();
        host.settle();
        CHECK (host.processor.soloSlot() == 0);
    }
    SECTION ("a held handle on the display still Solos")
    {
        const juce::ModifierKeys left (juce::ModifierKeys::leftButtonModifier);
        host.display.mouseDown (host.mouseEvent (host.at (1000.0), left, host.at (1000.0)));
        host.settle (450);
        CHECK (host.processor.soloSlot() == 1);
        host.display.mouseUp (host.mouseEvent (host.at (1000.0), {}, host.at (1000.0)));
        CHECK (host.processor.soloSlot() == 0);
    }
}

TEST_CASE ("The Band panel's Solo and the display's held handle share one Solo: neither lets go of the other's, and both draw the cue")
{
    PanelEditor host;
    auto& solo = host.control<staple::IconButton> ("Band 1 Solo");
    const juce::ModifierKeys left (juce::ModifierKeys::leftButtonModifier);
    const auto handle = host.at (1000.0);
    const auto holdHandle = [&] {
        host.display.mouseDown (host.mouseEvent (handle, left, handle));
        host.settle (450);
    };
    const auto letGoOfHandle = [&] { host.display.mouseUp (host.mouseEvent (handle, {}, handle)); };

    solo.setState (juce::Button::buttonDown);
    host.settle (60);
    CHECK (host.display.soloCueSlot() == 1);

    SECTION ("the display takes over: letting go of the button leaves the display's Solo")
    {
        holdHandle();
        solo.setState (juce::Button::buttonNormal);
        CHECK (host.processor.soloSlot() == 1);
        letGoOfHandle();
        CHECK (host.processor.soloSlot() == 0);
    }
    SECTION ("the button takes over: letting go of the handle leaves the button's Solo")
    {
        solo.setState (juce::Button::buttonNormal);
        holdHandle();
        solo.setState (juce::Button::buttonDown);
        letGoOfHandle();
        CHECK (host.processor.soloSlot() == 1);
        solo.setState (juce::Button::buttonNormal);
        CHECK (host.processor.soloSlot() == 0);
    }
    host.settle (60);
    CHECK (host.display.soloCueSlot() == 0);
}

TEST_CASE ("A selection update that keeps the Band panel's Band keeps the panel's Solo")
{
    PanelEditor host; // Band 2 at 100 Hz, 1 at 1 kHz, 3 at 5 kHz
    host.control<juce::Button> ("Next Band").onClick();
    REQUIRE (host.panel().shownSlot() == 3);
    host.control<staple::IconButton> ("Band 3 Solo").setState (juce::Button::buttonDown);
    REQUIRE (host.processor.soloSlot() == 3);

    // Shift adds a marquee around Band 2 to the selection; Band 3 is still the highest selected.
    const juce::ModifierKeys shiftLeft (juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier);
    const auto band2 = host.at (100.0);
    host.drag (band2.translated (-30.0f, -30.0f), band2.translated (30.0f, 30.0f), shiftLeft);
    REQUIRE (host.display.selection() == std::set<int> { 2, 3 });
    CHECK (host.panel().shownSlot() == 3);
    CHECK (host.processor.soloSlot() == 3);
    CHECK (host.processor.holdsSolo (eq1::PluginProcessor::SoloHolder::panel));
}

TEST_CASE ("The Band selector steps through the Bands in Frequency order, wrapping, and the display's selection follows")
{
    PanelEditor host; // Band 2 at 100 Hz, 1 at 1 kHz, 3 at 5 kHz
    auto& next = host.control<juce::Button> ("Next Band");
    auto& previous = host.control<juce::Button> ("Previous Band");
    const auto shown = [&host] { return host.panel().shownSlot(); };
    REQUIRE (shown() == 1);
    next.onClick();
    CHECK (shown() == 3);
    CHECK (host.display.selection() == std::set<int> { 3 });
    next.onClick();
    CHECK (shown() == 2);
    previous.onClick();
    CHECK (shown() == 3);
    previous.onClick();
    CHECK (shown() == 1);
    CHECK (host.display.selection() == std::set<int> { 1 });
    CHECK (host.control ("Band 1 Gain").isShowing());
}

TEST_CASE ("The Slope button reads the Slope or Brickwall, and opens the Slope list on a click")
{
    PanelEditor host;
    host.click (host.at (100.0)); // the Low Cut
    host.settle();
    host.set (2, "slope", 24.0f);
    host.settle (120);
    auto& slope = host.control<eq1::BandPanel::SlopeButton> ("Band 2 Slope");
    CHECK (slope.text() == "24 dB/oct");
    host.set (2, "slope", 37.5f);
    host.settle (120);
    CHECK (slope.text() == "37.5 dB/oct");
    host.set (2, "brickwall", 1.0f);
    host.settle (120);
    CHECK (slope.text() == "Brickwall");

    juce::PopupMenu::dismissAllActiveMenus();
    const auto centre = slope.getLocalBounds().getCentre().toFloat();
    const juce::ModifierKeys left (juce::ModifierKeys::leftButtonModifier);
    const auto now = juce::Time::getCurrentTime();
    const auto event = [&] (juce::ModifierKeys mods) {
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), centre, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &slope, &slope, now,
                                 centre, now, 1, false);
    };
    slope.mouseDown (event (left));
    slope.mouseUp (event ({}));
    // After the double-click time, as a double-click types a value instead.
    CHECK_FALSE (juce::PopupMenu::dismissAllActiveMenus());
    host.settle (juce::MouseEvent::getDoubleClickTimeout() + 100);
    CHECK (juce::PopupMenu::dismissAllActiveMenus());
}

TEST_CASE ("Dragging or typing a Slope is one undo step, and on a Brickwall Cut clears Brickwall in it")
{
    PanelEditor host;
    host.click (host.at (100.0));
    host.settle();
    host.set (2, "brickwall", 1.0f);
    host.settle (120);
    auto& history = host.processor.editHistory();
    auto& slope = host.control<eq1::BandPanel::SlopeButton> ("Band 2 Slope");
    const int steps = history.undoSteps();

    SECTION ("a drag")
    {
        const auto at = [&slope] (float dy) { return slope.getLocalBounds().getCentre().toFloat().translated (0.0f, dy); };
        const juce::ModifierKeys left (juce::ModifierKeys::leftButtonModifier);
        const auto now = juce::Time::getCurrentTime();
        const auto event = [&] (juce::Point<float> p, juce::ModifierKeys mods) {
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), p, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &slope, &slope, now,
                                     at (0.0f), now, 1, p != at (0.0f));
        };
        slope.mouseDown (event (at (0.0f), left));
        slope.mouseDrag (event (at (-10.0f), left));
        slope.mouseDrag (event (at (-25.0f), left));
        slope.mouseUp (event (at (-25.0f), {}));
        // 25 px of 200 for the whole 0 to 96 range, from 12.
        CHECK_THAT (host.value (2, "slope"), WithinAbs (12.0 + 96.0 * 25.0 / 200.0, 0.01));
        CHECK_FALSE (juce::PopupMenu::dismissAllActiveMenus()); // a drag isn't a click
    }
    SECTION ("a typed value")
    {
        CHECK (slope.commitTypedText ("30"));
        CHECK_THAT (host.value (2, "slope"), WithinAbs (30.0, 1.0e-4));
    }
    CHECK (host.value (2, "brickwall") == 0.0f);
    CHECK (history.undoSteps() == steps + 1);
    history.undo();
    CHECK (host.value (2, "brickwall") == 1.0f);
    CHECK_THAT (host.value (2, "slope"), WithinAbs (12.0, 1.0e-4));
}

TEST_CASE ("The Edge selectors set Shape and Stereo Placement through their parameters")
{
    PanelEditor host;
    auto& shape = host.control<staple::EdgeSelector> ("Band 1 Shape");
    auto& placement = host.control<staple::EdgeSelector> ("Band 1 Stereo Placement");
    CHECK (shape.getText() == "Bell");
    CHECK (placement.getText() == "Stereo");
    shape.setSelectedId (5, juce::sendNotificationSync); // High Cut
    placement.setSelectedId (4, juce::sendNotificationSync); // Mid
    CHECK (host.value (1, "shape") == 4.0f);
    CHECK (host.value (1, "placement") == 3.0f);
    host.set (1, "placement", 2.0f);
    host.settle (120);
    CHECK (placement.getText() == "Right");
}

namespace
{

// A mouse event on c at position in its own coordinates, pressed at downAt.
juce::MouseEvent eventOn (juce::Component& c, juce::Point<float> position, juce::ModifierKeys mods, juce::Point<float> downAt, int clicks = 1)
{
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), position, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, now, downAt,
                             now, clicks, position != downAt);
}

const juce::ModifierKeys leftButton (juce::ModifierKeys::leftButtonModifier);

// A press at from on c, a drag by (dx, dy), halfway first unless c moves as it is dragged, and a release.
void dragOn (juce::Component& c, juce::Point<float> from, juce::Point<float> by, juce::ModifierKeys mods = {}, bool moves = false)
{
    const auto down = mods.withFlags (juce::ModifierKeys::leftButtonModifier);
    c.mouseDown (eventOn (c, from, down, from));
    if (! moves)
        c.mouseDrag (eventOn (c, from + by * 0.5f, down, from));
    c.mouseDrag (eventOn (c, from + by, down, from));
    c.mouseUp (eventOn (c, from + by, mods, from));
}

// A double-click at position on c: two presses, the second one's double-click, and its release.
void doubleClickOn (juce::Component& c, juce::Point<float> position)
{
    c.mouseDown (eventOn (c, position, leftButton, position));
    c.mouseUp (eventOn (c, position, {}, position));
    c.mouseDown (eventOn (c, position, leftButton, position, 2));
    c.mouseDoubleClick (eventOn (c, position, leftButton, position, 2));
    c.mouseUp (eventOn (c, position, {}, position, 2));
}

juce::KeyPress withMods (int key, int mods) { return { key, juce::ModifierKeys (mods), 0 }; }

// The panel editor with Band 1 a Dynamic Bell (Dynamic Range -6 dB).
struct DynamicEditor : PanelEditor
{
    DynamicEditor()
    {
        set (1, "dynamic_range", -6.0f);
        settle (300);
    }

    staple::Knob& gain() { return control<staple::Knob> ("Band 1 Gain"); }
    eq1::DynamicRangeRing& ring() { return control<eq1::DynamicRangeRing> ("Band 1 Dynamic Range"); }
    // A point on the Gain knob's ring, at 12 o'clock.
    juce::Point<float> onRing()
    {
        auto& knob = gain();
        return knob.getFaceCentre().translated (0.0f, -(knob.getFaceRadius() + staple::tokens::knob::ringOffset));
    }
    bool shows (const juce::String& title)
    {
        auto found = findAll<juce::Component> ([&title] (juce::Component& c) { return c.getTitle() == title && c.isShowing(); });
        return ! found.empty();
    }
};

} // namespace

TEST_CASE ("Dragging the Gain knob's ring sets Dynamic Range, 60 dB per 200 px, as one undo step; a double-click on it sets 0")
{
    DynamicEditor host;
    auto& history = host.processor.editHistory();
    auto& gain = host.gain();
    const int steps = history.undoSteps();

    dragOn (gain, host.onRing(), { 0.0f, -50.0f }); // up 50 px: +15 dB
    CHECK_THAT (host.value (1, "dynamic_range"), WithinAbs (9.0, 1.0e-4));
    CHECK_THAT (host.value (1, "gain"), WithinAbs (0.0, 1.0e-4)); // the face's Gain is untouched
    CHECK (history.undoSteps() == steps + 1);

    SECTION ("with Shift, 60 dB per 800 px, rounded to 0.5 dB")
    {
        dragOn (gain, host.onRing(), { 0.0f, 31.0f }, juce::ModifierKeys::shiftModifier); // down 31 px: -2.325 dB
        CHECK_THAT (host.value (1, "dynamic_range"), WithinAbs (6.5, 1.0e-4));
    }
    SECTION ("a double-click sets 0, as one undo step")
    {
        doubleClickOn (gain, host.onRing());
        CHECK_THAT (host.value (1, "dynamic_range"), WithinAbs (0.0, 1.0e-4));
        CHECK (history.undoSteps() == steps + 2);
        history.undo();
        CHECK_THAT (host.value (1, "dynamic_range"), WithinAbs (9.0, 1.0e-4));
    }
    SECTION ("hovering the ring shows Dynamic Range in the Gain knob's tooltip")
    {
        gain.mouseEnter (eventOn (gain, host.onRing(), {}, host.onRing()));
        CHECK (gain.tooltipTitle() == "Band 1 Dynamic Range");
        CHECK (gain.tooltipValue() == "+9.00 dB");
        const auto face = gain.getFaceCentre();
        gain.mouseMove (eventOn (gain, face, {}, face));
        CHECK (gain.tooltipTitle() == "Band 1 Gain");
    }
}

TEST_CASE ("Alt with the arrows on the Gain knob, and the arrows on the ring, step Dynamic Range 1 dB, 0.5 dB with Shift, one undo step a press")
{
    DynamicEditor host;
    auto& history = host.processor.editHistory();
    const int alt = juce::ModifierKeys::altModifier, shift = juce::ModifierKeys::shiftModifier;
    auto& target = GENERATE (true, false) ? static_cast<juce::Component&> (host.gain()) : static_cast<juce::Component&> (host.ring());
    const int mods = &target == &host.gain() ? alt : 0;
    CAPTURE (target.getTitle());
    target.grabKeyboardFocus();
    REQUIRE (target.hasKeyboardFocus (false));
    const int steps = history.undoSteps();

    CHECK (host.press (withMods (juce::KeyPress::upKey, mods)));
    CHECK_THAT (host.value (1, "dynamic_range"), WithinAbs (-5.0, 1.0e-4));
    CHECK (host.press (withMods (juce::KeyPress::leftKey, mods | shift)));
    CHECK_THAT (host.value (1, "dynamic_range"), WithinAbs (-5.5, 1.0e-4));
    CHECK (history.undoSteps() == steps + 2);
    for (int repeat = 0; repeat < 4; ++repeat)
        host.hold (withMods (juce::KeyPress::downKey, mods));
    host.release();
    CHECK_THAT (host.value (1, "dynamic_range"), WithinAbs (-9.5, 1.0e-4));
    CHECK (history.undoSteps() == steps + 3);
    CHECK_THAT (host.value (1, "gain"), WithinAbs (0.0, 1.0e-4));
}

TEST_CASE ("The ring follows Gain in Tab's order, and is absent while Gain is unavailable")
{
    DynamicEditor host;
    const auto order = juce::KeyboardFocusTraverser().getAllComponents (host.editor.get());
    const auto gain = std::find (order.begin(), order.end(), &host.gain());
    REQUIRE (gain != order.end());
    REQUIRE (gain + 1 != order.end());
    CHECK (*(gain + 1) == &host.ring());
    CHECK (host.ring().getAccessibilityHandler()->getRole() == juce::AccessibilityRole::slider);

    host.set (1, "shape", 2.0f); // Low Cut
    host.settle (120);
    CHECK_FALSE (host.shows ("Band 1 Dynamic Range"));
    CHECK_FALSE (host.gain().isOnRing (host.onRing()));
}

TEST_CASE ("The ring's range is faint under Dynamics Bypass, and its Live Gain arc follows Live Gain but not under Dynamics Bypass, Bypass or Global Bypass")
{
    DynamicEditor host;
    auto& ring = host.ring();
    CHECK_THAT (ring.rangeAlpha(), WithinAbs (0.85, 1.0e-6));
    // Full-scale noise far above a -60 dB Threshold: Live Gain moves the whole range down.
    host.set (1, "threshold_auto", 0.0f);
    host.set (1, "threshold", -60.0f);
    juce::Random random (1);
    const auto play = [&] {
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        for (int block = 0; block < 40; ++block)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int n = 0; n < 512; ++n)
                    buffer.setSample (ch, n, random.nextFloat() - 0.5f);
            host.processor.processBlock (buffer, midi);
        }
        host.settle (60);
    };
    play();
    REQUIRE (host.processor.liveGainDb (1) < -1.0);
    REQUIRE (ring.liveGainShown().has_value());
    CHECK_THAT (*ring.liveGainShown(), WithinAbs (host.processor.liveGainDb (1), 0.05));

    const juce::String hides = GENERATE ("band1_dynamics_bypass", "band1_bypass", "global_bypass");
    CAPTURE (hides);
    host.set (hides, 1.0f);
    play();
    CHECK_FALSE (ring.liveGainShown().has_value());
    if (hides == "band1_dynamics_bypass")
        CHECK_THAT (ring.rangeAlpha(), WithinAbs (0.3, 1.0e-6));
}

TEST_CASE ("The ring's Live Gain arc measures Live Gain's movement as Gain Scale plays it, and is absent before any audio is processed")
{
    DynamicEditor host;
    auto& ring = host.ring();
    host.set (1, "gain", 10.0f);
    host.settle (120);
    // Nothing processed yet: no movement to show.
    CHECK_FALSE (ring.liveGainShown().has_value());

    host.set ("gain_scale", 50.0f);
    host.set (1, "threshold_auto", 0.0f);
    const auto play = [&] (float level) {
        juce::Random random (1);
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        for (int block = 0; block < 40; ++block)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int n = 0; n < 512; ++n)
                    buffer.setSample (ch, n, level * (random.nextFloat() - 0.5f));
            host.processor.processBlock (buffer, midi);
        }
        host.settle (60);
    };

    SECTION ("dynamics idle: Live Gain is the scaled Gain, so no arc")
    {
        host.set (1, "threshold", 0.0f);
        play (0.0f);
        REQUIRE_THAT (host.processor.liveGainDb (1), WithinAbs (5.0, 0.01));
        CHECK_FALSE (ring.liveGainShown().has_value());
    }
    SECTION ("dynamics moving: the arc ends where the movement, unscaled, takes Gain")
    {
        host.set (1, "threshold", -60.0f);
        play (1.0f);
        const double movement = host.processor.liveGainDb (1) - 5.0;
        REQUIRE (movement < -1.0);
        REQUIRE (ring.liveGainShown().has_value());
        CHECK_THAT (*ring.liveGainShown(), WithinAbs (10.0 + movement / 0.5, 0.1));
    }
}

TEST_CASE ("The dynamics icons show only on a Dynamic Band: Clear Dynamics clears it in one undo step, Dynamics Bypass toggles")
{
    PanelEditor host;
    for (const juce::String title : { "Band 1 Clear Dynamics", "Band 1 Dynamics Bypass", "Hide Band 1 dynamics" })
        CHECK (host.findAll<juce::Component> ([&title] (juce::Component& c) { return c.getTitle() == title && c.isShowing(); }).empty());

    host.set (1, "dynamic_range", -6.0f);
    host.set (1, "threshold_auto", 0.0f);
    host.settle (300);
    auto& history = host.processor.editHistory();
    auto& bypass = host.control<staple::IconButton> ("Band 1 Dynamics Bypass");
    CHECK (bypass.isShowing());
    bypass.setToggleState (true, juce::sendNotificationSync); // as a click does
    host.settle (120);
    CHECK (host.value (1, "dynamics_bypass") == 1.0f);
    CHECK (bypass.isOff());

    const int steps = history.undoSteps();
    host.control<juce::Button> ("Band 1 Clear Dynamics").onClick();
    CHECK (host.value (1, "dynamic_range") == 0.0f);
    CHECK (host.value (1, "dynamics_bypass") == 0.0f);
    CHECK (host.value (1, "threshold_auto") == 1.0f);
    CHECK (history.undoSteps() == steps + 1);
    host.settle (120);
    CHECK_FALSE (bypass.isShowing());
}

TEST_CASE ("The dynamics section opens and closes, widening the panel alone; it is absent on a Band that isn't dynamic")
{
    PanelEditor host;
    auto& panel = host.panel();
    const auto closed = panel.getBounds();
    const auto displayBounds = host.display.getBounds();
    CHECK_FALSE (host.findAll<eq1::DynamicsSection> ().front()->isShowing());

    host.set (1, "dynamic_range", -6.0f);
    host.settle (400);
    auto& section = *host.findAll<eq1::DynamicsSection>().front();
    // Open by default: wider by the section, about the same centre.
    CHECK (section.isShowing());
    CHECK (panel.isDynamicsOpen());
    CHECK (panel.getWidth() == eq1::BandPanel::openWidth);
    CHECK (std::abs (panel.getBounds().getCentreX() - closed.getCentreX()) <= 1);
    CHECK (panel.getY() == closed.getY());
    CHECK (panel.getHeight() == closed.getHeight());
    CHECK (host.display.getBounds() == displayBounds);
    // Between Gain and Q.
    const auto inPanel = [&panel] (juce::Component& c) { return panel.getLocalArea (c.getParentComponent(), c.getBounds()); };
    CHECK (section.getX() > host.control ("Band 1 Gain").getX());
    auto& q = host.control<staple::Knob> ("Band 1 Q");
    CHECK (section.getRight() < inPanel (q).getCentreX() - q.getFaceRadius());

    auto& chevron = host.control<juce::Button> ("Hide Band 1 dynamics");
    chevron.onClick();
    host.settle (400);
    CHECK_FALSE (section.isShowing());
    CHECK (panel.getBounds() == closed);
    CHECK (chevron.getTitle() == "Show Band 1 dynamics");
    // Remembered from Band to Band, and opened again.
    host.click (host.at (100.0));
    host.settle();
    host.click (host.at (1000.0));
    host.settle (300);
    CHECK_FALSE (section.isShowing());
    chevron.onClick();
    host.settle (400);
    CHECK (section.isShowing());

    // Absent once the Band isn't dynamic.
    host.set (1, "dynamic_range", 0.0f);
    host.settle (400);
    CHECK_FALSE (section.isShowing());
    CHECK (panel.getBounds() == closed);
}

TEST_CASE ("The Threshold fader's top step is Auto: a double-click sets Auto, and a drag is one undo step across Threshold and Auto")
{
    DynamicEditor host;
    auto& history = host.processor.editHistory();
    auto& fader = host.control<eq1::ThresholdFader> ("Band 1 Threshold");
    REQUIRE (host.value (1, "threshold_auto") == 1.0f);
    const float thumbAuto = fader.thumbCentreY (eq1::ThresholdFader::autoPosition);
    CHECK_THAT (thumbAuto, WithinAbs (8.0, 1.0e-4)); // the top of the travel

    // A click doesn't move it.
    const juce::Point<float> middle { 13.0f, 40.0f };
    fader.mouseDown (eventOn (fader, middle, leftButton, middle));
    fader.mouseUp (eventOn (fader, middle, {}, middle));
    CHECK (host.value (1, "threshold_auto") == 1.0f);

    // Dragged down from Auto by half the travel: out of Auto, in one undo step.
    const int steps = history.undoSteps();
    dragOn (fader, { 13.0f, thumbAuto }, { 0.0f, 32.0f });
    CHECK (host.value (1, "threshold_auto") == 0.0f);
    CHECK_THAT (host.value (1, "threshold"), WithinAbs (3.0 - 63.0 / 2.0, 0.1));
    CHECK (history.undoSteps() == steps + 1);
    history.undo();
    CHECK (host.value (1, "threshold_auto") == 1.0f);
    history.redo();

    doubleClickOn (fader, middle);
    CHECK (host.value (1, "threshold_auto") == 1.0f);
    CHECK (history.undoSteps() == steps + 2);
}

TEST_CASE ("The Detection Level reaches the Threshold fader's thumb for the Threshold it equals, and never Auto")
{
    DynamicEditor host;
    auto& fader = host.control<eq1::ThresholdFader> ("Band 1 Threshold");
    for (double level : { -60.0, -42.5, -12.0, 0.0 })
    {
        CAPTURE (level);
        CHECK_THAT (fader.levelTopY (level), WithinAbs (fader.thumbCentreY (level), 1.0e-4));
    }
    // Above 0 dB it stops at 0 dB, short of Auto; below the fader's bottom the track is empty.
    CHECK (fader.thumbCentreY (0.0) > fader.thumbCentreY (eq1::ThresholdFader::autoPosition));
    CHECK_THAT (fader.levelTopY (6.0), WithinAbs (fader.thumbCentreY (0.0), 1.0e-4));
    CHECK_THAT (fader.levelTopY (-70.0), WithinAbs (fader.getHeight(), 1.0e-4));
    CHECK_THAT (fader.levelTopY (eq1::levelFloorDb), WithinAbs (fader.getHeight(), 1.0e-4));
}

TEST_CASE ("Holding Detection Audition plays the detection signal and lights it; its release, a Band change and closing the editor let go")
{
    auto host = std::make_unique<DynamicEditor>();
    auto* audition = &host->control<staple::IconButton> ("Band 1 Detection Audition");
    const auto hold = [&] { audition->setState (juce::Button::buttonDown); };
    hold();
    CHECK (host->processor.detectionAuditionSlot() == 1);
    CHECK (audition->isLit());
    audition->setState (juce::Button::buttonNormal);
    CHECK (host->processor.detectionAuditionSlot() == 0);
    CHECK_FALSE (audition->isLit());

    SECTION ("a Band change")
    {
        hold();
        host->click (host->at (100.0));
        host->settle();
        CHECK (host->processor.detectionAuditionSlot() == 0);
    }
    SECTION ("closing the editor")
    {
        hold();
        auto& processor = host->processor;
        host->editor.reset();
        CHECK (processor.detectionAuditionSlot() == 0);
    }
}

TEST_CASE ("Detection Source and Detection Range switch with a click, each one undo step")
{
    DynamicEditor host;
    auto& history = host.processor.editHistory();
    const int steps = history.undoSteps();
    auto& source = host.control<staple::IconButton> ("Band 1 Detection Source");
    CHECK_FALSE (source.isLit());
    source.onClick();
    host.settle();
    CHECK (host.value (1, "detection_source") == 1.0f);
    CHECK (source.isLit());
    auto& range = host.control<juce::Button> ("Band 1 Detection Range");
    CHECK (range.getAccessibilityHandler()->getValueInterface()->getCurrentValueAsString() == "Band");
    range.onClick();
    host.settle();
    CHECK (host.value (1, "detection_range") == 1.0f);
    CHECK (range.getAccessibilityHandler()->getValueInterface()->getCurrentValueAsString() == "Free");
    CHECK (history.undoSteps() == steps + 2);
    // Attack and Release read Auto at their centres.
    CHECK (host.control<staple::Knob> ("Band 1 Attack").tooltipValue() == "Auto");
    CHECK (host.control<staple::Knob> ("Band 1 Release").tooltipValue() == "Auto");
}

TEST_CASE ("Hovering and pressing the Detection Range button light its translucent fill more opaque")
{
    DynamicEditor host;
    auto& range = host.control<juce::Button> ("Band 1 Detection Range");
    // The fill's alpha at the button's left edge, clear of its icon and text.
    const auto fillAlpha = [&range] (juce::Button::ButtonState state) {
        range.setState (state);
        const auto image = range.createComponentSnapshot (range.getLocalBounds(), true, 1.0f);
        return image.getPixelAt (3, image.getHeight() / 2).getAlpha();
    };
    const auto normal = fillAlpha (juce::Button::buttonNormal);
    const auto over = fillAlpha (juce::Button::buttonOver);
    const auto down = fillAlpha (juce::Button::buttonDown);
    CHECK (normal > 0);
    CHECK (over > normal);
    CHECK (down > over);
}

namespace
{
// A Dynamic Bell on a Free Detection Range from 120 Hz to 4.5 kHz.
struct FreeEditor : DynamicEditor
{
    FreeEditor()
    {
        set (1, "detection_range", 1.0f);
        set (1, "detection_low", 120.0f);
        set (1, "detection_high", 4500.0f);
        settle (120);
    }
    eq1::DetectionRangeBar& bar() { return *findAll<eq1::DetectionRangeBar>().front(); }
    juce::Slider& handle (const char* which) { return control<juce::Slider> (juce::String ("Band 1 Detection ") + which); }
};
} // namespace

TEST_CASE ("The Detection Range bar shows on a Free Dynamic Band that isn't Bypassed, above the Band panel")
{
    PanelEditor host;
    auto& bar = *host.findAll<eq1::DetectionRangeBar>().front();
    CHECK_FALSE (bar.isVisible());
    host.set (1, "detection_range", 1.0f);
    host.settle (120);
    CHECK_FALSE (bar.isVisible()); // not dynamic
    host.set (1, "dynamic_range", -6.0f);
    host.settle (120);
    CHECK (bar.isVisible());
    const auto& panel = host.panel();
    CHECK (juce::roundToInt (bar.getY() + bar.lineY()) == panel.getY() - 30);
    CHECK (bar.getX() == host.display.getX());
    CHECK (bar.getWidth() == host.display.getWidth());
    host.set (1, "bypass", 1.0f);
    host.settle (120);
    CHECK_FALSE (bar.isVisible());
    host.set (1, "bypass", 0.0f);
    host.settle (120);
    CHECK (bar.isVisible());
    host.click (host.at (100.0)); // a Low Cut
    host.settle (120);
    CHECK_FALSE (bar.isVisible());
}

TEST_CASE ("Dragging a Detection Range handle or the segment is one undo step, keeping low at most high / 1.25 within 20 Hz to 20 kHz")
{
    FreeEditor host;
    auto& bar = host.bar();
    auto& history = host.processor.editHistory();
    const int steps = history.undoSteps();
    auto& low = host.handle ("Low");
    auto& high = host.handle ("High");
    const auto centreOf = [] (juce::Component& c) { return c.getLocalBounds().getCentre().toFloat(); };
    const auto to = [&] (juce::Component& c, double frequency) { return juce::Point<float> (bar.xOf (frequency) - static_cast<float> (c.getX()), centreOf (c).y) - centreOf (c); };

    SECTION ("the low handle, stopped at high / 1.25")
    {
        dragOn (low, centreOf (low), to (low, 10000.0), {}, true);
        CHECK_THAT (host.value (1, "detection_low"), WithinAbs (4500.0 / 1.25, 1.0));
        CHECK (history.undoSteps() == steps + 1);
    }
    SECTION ("the high handle, stopped at 20 kHz")
    {
        dragOn (high, centreOf (high), to (high, 28000.0), {}, true);
        CHECK_THAT (host.value (1, "detection_high"), WithinAbs (20000.0, 1.0));
        CHECK (history.undoSteps() == steps + 1);
    }
    SECTION ("the segment moves both by one ratio, stopping at 20 Hz")
    {
        auto& segment = *host.findAll<eq1::DetectionRangeBar::Segment>().front();
        CHECK (segment.getTitle() == "Band 1 Detection Range");
        CHECK (segment.valueText() == juce::String::fromUTF8 ("120 Hz \xe2\x80\x93 4.50 kHz"));
        const auto from = centreOf (segment);
        const double startFrequency = bar.frequencyAt (static_cast<float> (segment.getX()) + from.x);
        dragOn (segment, from, { bar.xOf (startFrequency * 2.0) - bar.xOf (startFrequency), 0.0f }, {}, true);
        CHECK_THAT (host.value (1, "detection_low"), WithinRel (240.0, 0.01));
        CHECK_THAT (host.value (1, "detection_high"), WithinRel (9000.0, 0.01));
        CHECK (history.undoSteps() == steps + 1);
        history.undo();
        CHECK_THAT (host.value (1, "detection_low"), WithinRel (120.0, 0.001));
        CHECK_THAT (host.value (1, "detection_high"), WithinRel (4500.0, 0.001));
        dragOn (segment, centreOf (segment), { -400.0f, 0.0f }, {}, true);
        CHECK_THAT (host.value (1, "detection_low"), WithinRel (20.0, 0.001));
        CHECK_THAT (host.value (1, "detection_high"), WithinRel (4500.0 * 20.0 / 120.0, 0.01));
    }
}

TEST_CASE ("Left and right on a Detection Range handle nudge it 1/6 octave, one undo step a press, within the bar's limits")
{
    FreeEditor host;
    auto& history = host.processor.editHistory();
    auto& high = host.handle ("High");
    high.grabKeyboardFocus();
    REQUIRE (high.hasKeyboardFocus (false));
    const int steps = history.undoSteps();
    CHECK (host.press (juce::KeyPress (juce::KeyPress::rightKey)));
    CHECK_THAT (host.value (1, "detection_high"), WithinRel (4500.0 * std::pow (2.0, 1.0 / 6.0), 0.001));
    CHECK (host.press (juce::KeyPress (juce::KeyPress::leftKey)));
    CHECK_THAT (host.value (1, "detection_high"), WithinRel (4500.0, 0.001));
    CHECK (history.undoSteps() == steps + 2);
    for (int repeat = 0; repeat < 30; ++repeat)
        host.hold (juce::KeyPress (juce::KeyPress::leftKey));
    host.release();
    CHECK (history.undoSteps() == steps + 3);
    CHECK_THAT (host.value (1, "detection_high"), WithinRel (120.0 * 1.25, 0.001));
    CHECK_FALSE (host.press (juce::KeyPress (juce::KeyPress::upKey)));
}

// Renders the editor at 1200 x 760 and 2x with the Band panel on a Bell, a Low Cut with Brickwall, a
// Bypassed Band, on mono, and on a Dynamic Bell with its dynamics section open, its Free Detection Range
// bar, and under Dynamics Bypass, for checking by hand against the prototype. Hidden; run with
//   EQ1_PANEL_SCREENS=/some/dir/81 build/tests/eq1_plugin_tests "[.screens]"
TEST_CASE ("Band panel screenshots", "[.screens]")
{
    const auto prefix = juce::SystemStats::getEnvironmentVariable ("EQ1_PANEL_SCREENS", {});
    if (prefix.isEmpty())
        SKIP ("EQ1_PANEL_SCREENS is not set");
    const auto save = [&prefix] (PanelEditor& host, const juce::String& name) {
        host.editor->setSize (1200, 760);
        host.settle (300);
        juce::File file (prefix + "-" + name + ".png");
        file.deleteFile();
        juce::FileOutputStream stream (file);
        juce::PNGImageFormat().writeImageToStream (host.editor->createComponentSnapshot (host.editor->getLocalBounds(), true, 2.0f), stream);
    };
    {
        PanelEditor host;
        host.set (1, "gain", 4.0f);
        save (host, "bell");
        host.click (host.at (100.0));
        host.set (2, "brickwall", 1.0f);
        save (host, "low-cut-brickwall");
        host.set (2, "bypass", 1.0f);
        save (host, "bypassed");
    }
    PanelEditor mono (true);
    mono.set (1, "shape", 8.0f); // Flat Tilt: no Slope, no Q
    save (mono, "mono-flat-tilt");
    FreeEditor dynamic;
    dynamic.set (1, "gain", 4.0f);
    dynamic.set (1, "dynamic_range", -9.0f);
    dynamic.set (1, "threshold_auto", 0.0f);
    dynamic.set (1, "threshold", -24.0f);
    // Noise through the plugin, so the Live Gain arc and the Detection Level show.
    juce::Random random (1);
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    for (int block = 0; block < 40; ++block)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < 512; ++n)
                buffer.setSample (ch, n, 0.2f * (random.nextFloat() - 0.5f));
        dynamic.processor.processBlock (buffer, midi);
    }
    save (dynamic, "dynamic-free");
    dynamic.set (1, "dynamics_bypass", 1.0f);
    save (dynamic, "dynamics-bypass");
}
