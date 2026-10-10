#include "BandEditing.h"
#include "BandPanel.h"
#include "EditorHarness.h"
#include "staple/controls/EdgeSelector.h"
#include "staple/controls/IconButton.h"
#include "DetectionArc.h"
#include "LevelBallistics.h"
#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <memory>

using Catch::Matchers::WithinAbs;

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

TEST_CASE ("The Detection Level arc reaches the Threshold knob's position for the Threshold it equals, and never Auto")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    eq1::BandEditing editing { processor.parameterState(), processor.editHistory() };
    eq1::BandPanel panel (processor, editing);
    // The Band panel's Threshold knob, with Auto as its top position.
    auto* threshold = harness::findChild<juce::Slider> (panel, [] (juce::Slider& s) { return s.getMinimum() < -59.0; });
    REQUIRE (threshold != nullptr);
    const auto knobAt = [&] (double thresholdDb) { return threshold->valueToProportionOfLength (thresholdDb); };

    for (double level : { -60.0, -42.5, -12.0, 0.0 })
    {
        CAPTURE (level);
        CHECK_THAT (eq1::DetectionArc::sweepProportion (*threshold, level), WithinAbs (knobAt (level), 1.0e-9));
    }
    // Above 0 dB it stops at 0 dB, short of Auto; below the knob's bottom it's empty.
    CHECK (knobAt (0.0) < 1.0);
    CHECK_THAT (eq1::DetectionArc::sweepProportion (*threshold, 6.0), WithinAbs (knobAt (0.0), 1.0e-9));
    CHECK_THAT (eq1::DetectionArc::sweepProportion (*threshold, -70.0), WithinAbs (0.0, 1.0e-9));
    CHECK_THAT (eq1::DetectionArc::sweepProportion (*threshold, eq1::levelFloorDb), WithinAbs (0.0, 1.0e-9));
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
    host.set (1, "bypass", 1.0f);
    host.settle (300);
    for (const juce::String title : { "Band 1 Solo", "Band 1 Shape", "Band 1 Slope", "Band 1 Frequency", "Band 1 Gain", "Band 1 Q",
                                      "Band 1 Stereo Placement", "Previous Band", "Next Band", "Band 1 Dynamics" })
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

TEST_CASE ("Today's dynamics controls work from the Band panel's temporary Dynamics button")
{
    PanelEditor host;
    auto& dynamics = host.control<juce::Button> ("Band 1 Dynamics");
    CHECK (dynamics.isEnabled());
    CHECK (host.findAll<juce::Slider> ([] (juce::Slider& s) { return s.getTitle() == "Band 1 Dynamic Range" && s.isShowing(); }).empty());
    dynamics.onClick();
    auto* box = harness::findChild<juce::CallOutBox> (*host.editor, [] (juce::CallOutBox& b) { return b.isVisible(); });
    REQUIRE (box != nullptr);
    auto& range = host.control<juce::Slider> ("Band 1 Dynamic Range");
    REQUIRE (range.isShowing());
    range.setValue (-6.0, juce::sendNotificationSync);
    CHECK_THAT (host.value (1, "dynamic_range"), WithinAbs (-6.0, 1.0e-4));
    for (const juce::String title : { "Band 1 Threshold", "Band 1 Attack", "Band 1 Release", "Band 1 Dynamics Bypass", "Band 1 Detection Source",
                                      "Band 1 Detection Range", "Band 1 Detection Audition" })
    {
        CAPTURE (title);
        CHECK (host.control (title).isShowing());
    }
    box->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));

    // Not on a Shape without dynamics.
    host.set (1, "shape", 2.0f);
    host.settle (120);
    CHECK_FALSE (dynamics.isEnabled());
}

// Renders the editor at 1200 x 760 and 2x with the Band panel on a Bell, a Low Cut with Brickwall, a
// Bypassed Band and on mono, for checking by hand against the prototype. Hidden; run with
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
}
