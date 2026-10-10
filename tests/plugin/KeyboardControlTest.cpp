#include "EditorHarness.h"
#include "staple/LookAndFeel.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;
using harness::OpenEditor;

namespace
{

const juce::KeyPress left (juce::KeyPress::leftKey), right (juce::KeyPress::rightKey), up (juce::KeyPress::upKey),
    down (juce::KeyPress::downKey);
juce::KeyPress withShift (juce::KeyPress key) { return { key.getKeyCode(), juce::ModifierKeys::shiftModifier, 0 }; }

// The editor with a Low Shelf in Band 1 on a Free Detection Range, selected, so the Band panel shows
// every control but Brickwall.
struct EveryControl : OpenEditor
{
    EveryControl()
    {
        addBand (1, 1000.0f, 3.0f, 1.0f);
        set (1, "detection_range", 1.0f);
        settle();
        click (at (1000.0).translated (0.0f, -yOffsetOf (3.0)));
        settle();
    }

    // How far above the 0 dB line the display draws a Gain, at its default range of +/-12 dB.
    float yOffsetOf (double gain) const
    {
        return static_cast<float> (gain / 12.0 * (display.getHeight() * 0.5 - 9.0));
    }

    std::vector<juce::Slider*> sliders()
    {
        return findAll<juce::Slider> ([] (juce::Slider& s) { return s.isShowing() && s.isEnabled(); });
    }
};

} // namespace

TEST_CASE ("Arrow keys step every slider 1% of its range, 0.2% with Shift, within its range")
{
    EveryControl host;
    const auto sliders = host.sliders();
    // The Band panel's ten knobs, Gain Scale, Output Gain, Output Pan and Analyzer Tilt.
    CHECK (sliders.size() == 14);
    for (auto* slider : sliders)
    {
        if (slider->getName() == "Threshold" || slider->getName() == "Analyzer Tilt")
            continue; // Auto is Threshold's top position, and Analyzer Tilt has steps: each has its own test
        CAPTURE (slider->getName());
        slider->grabKeyboardFocus();
        REQUIRE (slider->hasKeyboardFocus (false));
        const auto position = [slider] { return slider->valueToProportionOfLength (slider->getValue()); };
        const double start = position();

        CHECK (host.press (down));
        CHECK_THAT (position(), WithinAbs (std::max (0.0, start - 0.01), 1.0e-4));
        const double lowered = position();
        CHECK (host.press (withShift (right)));
        CHECK_THAT (position(), WithinAbs (lowered + 0.002, 1.0e-4));
        CHECK (host.press (up));
        CHECK_THAT (position(), WithinAbs (std::min (1.0, lowered + 0.012), 1.0e-4));
        CHECK (host.press (withShift (left)));
        CHECK_THAT (position(), WithinAbs (std::min (1.0, lowered + 0.012) - 0.002, 1.0e-4));

        // At the top of its range, a step up stays there.
        slider->setValue (slider->getMaximum(), juce::sendNotificationSync);
        host.press (right);
        CHECK_THAT (position(), WithinAbs (1.0, 1.0e-4));
    }
}

TEST_CASE ("Arrow keys step Analyzer Tilt by its 0.5 dB/oct steps, with or without Shift, within 0 to 6")
{
    EveryControl host;
    auto* tilt = host.findAll<juce::Slider> ([] (juce::Slider& s) { return s.getName() == "Analyzer Tilt"; }).front();
    tilt->setValue (3.0, juce::sendNotificationSync);
    tilt->grabKeyboardFocus();
    REQUIRE (tilt->hasKeyboardFocus (false));

    CHECK (host.press (down));
    CHECK (tilt->getValue() == 2.5);
    CHECK (host.press (withShift (up)));
    CHECK (tilt->getValue() == 3.0);
    CHECK (host.press (right));
    CHECK (tilt->getValue() == 3.5);

    tilt->setValue (6.0, juce::sendNotificationSync);
    host.press (up);
    CHECK (tilt->getValue() == 6.0);
    tilt->setValue (0.0, juce::sendNotificationSync);
    host.press (down);
    CHECK (tilt->getValue() == 0.0);
}

TEST_CASE ("Threshold steps from 0 dB into Auto, its top position, and from Auto back to 0 dB")
{
    EveryControl host;
    auto* threshold = host.findAll<juce::Slider> ([] (juce::Slider& s) { return s.getName() == "Threshold"; }).front();
    threshold->grabKeyboardFocus();
    REQUIRE (threshold->hasKeyboardFocus (false));
    REQUIRE (host.value (1, "threshold_auto") == 1.0f);

    CHECK (host.press (down));
    CHECK (host.value (1, "threshold_auto") == 0.0f);
    CHECK_THAT (host.value (1, "threshold"), WithinAbs (0.0, 1.0e-4));
    host.press (down);
    CHECK_THAT (host.value (1, "threshold"), WithinAbs (-0.6, 0.05)); // 1% of the knob's turn
    host.press (up);
    CHECK_THAT (host.value (1, "threshold"), WithinAbs (0.0, 1.0e-4));
    CHECK (host.value (1, "threshold_auto") == 0.0f);
    host.press (withShift (up));
    CHECK (host.value (1, "threshold_auto") == 1.0f);
}

TEST_CASE ("Each arrow press on a slider is one undo step, and so is a held key with its repeats")
{
    EveryControl host;
    auto& history = host.processor.editHistory();
    for (auto* slider : host.sliders())
    {
        if (slider->getName() == "Analyzer Tilt")
            continue; // display only, never undone
        CAPTURE (slider->getName());
        slider->grabKeyboardFocus();
        const double start = slider->getValue();
        const int steps = history.undoSteps();

        host.press (down);
        host.press (down);
        CHECK (history.undoSteps() == steps + 2);
        for (int repeat = 0; repeat < 5; ++repeat)
            host.hold (up);
        host.release();
        CHECK (history.undoSteps() == steps + 3);

        history.undo();
        history.undo();
        history.undo();
        if (slider->getName() == "Threshold")
            CHECK (host.value (1, "threshold_auto") == 1.0f); // where it started
        else
            CHECK_THAT (slider->getValue(), WithinAbs (start, 1.0e-3 * (slider->getMaximum() - slider->getMinimum())));
    }
}

TEST_CASE ("A held arrow key's undo step ends when the slider loses focus")
{
    EveryControl host;
    auto& history = host.processor.editHistory();
    auto* gainScale = host.findAll<juce::Slider> ([] (juce::Slider& s) { return s.getName() == "Gain Scale"; }).front();
    gainScale->grabKeyboardFocus();
    const int steps = history.undoSteps();
    host.hold (down);
    host.hold (down);
    host.display.grabKeyboardFocus();
    CHECK (history.undoSteps() == steps + 1);
}

TEST_CASE ("The focus ring is hidden until an arrow key steps a slider, and hides again when told to")
{
    EveryControl host;
    auto* staple = dynamic_cast<staple::LookAndFeel*> (&host.editor->getLookAndFeel());
    REQUIRE (staple != nullptr);
    CHECK_FALSE (staple->isFocusRingShown());

    auto* slider = host.sliders().front();
    slider->grabKeyboardFocus();
    REQUIRE (slider->hasKeyboardFocus (false));
    CHECK_FALSE (staple->isFocusRingShown()); // focus alone, as after a click, doesn't show it
    host.press (up);
    CHECK (staple->isFocusRingShown());

    staple->showFocusRing (false);
    CHECK_FALSE (staple->isFocusRingShown());
}

namespace
{
// What a focusable component is called in the focus order: its name, or a button's text.
juce::String describe (juce::Component& component)
{
    if (component.getName().isNotEmpty())
        return component.getName();
    if (auto* button = dynamic_cast<juce::Button*> (&component))
        return button->getButtonText();
    return "?";
}

std::vector<juce::String> focusOrder (juce::Component& editor)
{
    std::vector<juce::String> order;
    for (auto* component : juce::KeyboardFocusTraverser().getAllComponents (&editor))
        order.push_back (describe (*component));
    return order;
}

// Two edits, one undone, so Undo and Redo are both enabled.
void undoAndRedoEnabled (EveryControl& host)
{
    auto& gain = host.parameter ("band1_gain");
    for (float value : { 0.4f, 0.6f })
    {
        gain.beginChangeGesture();
        gain.setValueNotifyingHost (value);
        gain.endChangeGesture();
    }
    host.processor.editHistory().undo();
    host.settle (300);
}
} // namespace

TEST_CASE ("Tab reaches every visible, enabled control and every Band in use, in on-screen order")
{
    EveryControl host;
    host.addBand (2, 200.0f, 0.0f);
    host.addBand (3, 1000.0f, -3.0f); // ties with Band 1: the lower Band Slot first
    undoAndRedoEnabled (host);

    const std::vector<juce::String> expected {
        // The header.
        "Presets", "Previous Preset", "Next Preset", "A", "B", "Copy A to B", "Undo", "Redo",
        // The Analyzer and Display Range.
        "Pre", "Post", "Sidechain", "Peak Hold", "Analyzer Range", "Analyzer Speed", "Analyzer Resolution", "Analyzer Tilt",
        "Meter", "Display Range",
        // The EQ display and its Bands, by Frequency, then the Output Meter's Clip Lights.
        "EQ Display", "Band 2", "Band 1", "Band 3", "Output Meter",
        // The Band panel: its top row, its left column, then its knobs.
        "Dynamics Bypass", "Bypass", "Detection Audition", "Delete", "Shape", "Stereo Placement", "Detection Source",
        "Detection Range", "Frequency", "Gain", "Q", "Slope", "Dynamic Range", "Threshold", "Attack", "Release",
        "Detection Low", "Detection High",
        // The output panel.
        "Gain Scale", "Auto Gain", "Output Gain", "Output Pan", "Pan Mode", "Phase Invert", "Global Bypass"
    };
    CHECK (focusOrder (*host.editor) == expected);
}

namespace
{
// The element Tab reaches for a Band in use.
juce::Component& bandElement (OpenEditor& host, int slot)
{
    const auto name = "Band " + juce::String (slot);
    return *host.findAll<juce::Component> ([&name] (juce::Component& c) { return c.getName() == name; }).front();
}

constexpr double semitone = 1.0594630943592953; // 2^(1/12)
} // namespace

TEST_CASE ("Focusing a Band selects it alone, and the Band panel shows it")
{
    EveryControl host;
    host.addBand (2, 200.0f, 0.0f);
    host.settle();
    auto& band2 = bandElement (host, 2);
    band2.grabKeyboardFocus();
    REQUIRE (band2.hasKeyboardFocus (false));
    host.press (up);
    CHECK_THAT (host.value (2, "gain"), WithinAbs (0.5, 1.0e-4));
    CHECK_THAT (host.value (1, "gain"), WithinAbs (3.0, 1.0e-4)); // no longer selected
    const auto titles = host.findAll<juce::Label> ([] (juce::Label& l) { return l.getText() == "Band 2"; });
    CHECK (titles.size() == 1);
}

TEST_CASE ("A focused Band moves a semitone or 0.5 dB per arrow, 0.1 semitone or 0.05 dB with Shift")
{
    EveryControl host;
    auto& band1 = bandElement (host, 1);
    band1.grabKeyboardFocus();
    REQUIRE (band1.hasKeyboardFocus (false));

    CHECK (host.press (right));
    CHECK_THAT (host.value (1, "frequency"), WithinAbs (1000.0 * semitone, 0.01));
    CHECK (host.press (left));
    CHECK (host.press (left));
    CHECK_THAT (host.value (1, "frequency"), WithinAbs (1000.0 / semitone, 0.01));
    CHECK (host.press (withShift (right)));
    CHECK_THAT (host.value (1, "frequency"), WithinAbs (1000.0 / semitone * std::pow (2.0, 0.1 / 12.0), 0.01));

    CHECK (host.press (up));
    CHECK_THAT (host.value (1, "gain"), WithinAbs (3.5, 1.0e-4));
    CHECK (host.press (down));
    CHECK (host.press (withShift (down)));
    CHECK_THAT (host.value (1, "gain"), WithinAbs (2.95, 1.0e-4));
}

TEST_CASE ("Arrow keys move the display's selection together, and it stops together at the edge of a range")
{
    EveryControl host;
    host.addBand (2, 20000.0f, 29.0f);
    host.settle();
    host.display.grabKeyboardFocus();
    host.press (juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 0));

    for (int i = 0; i < 12; ++i)
        host.press (right); // Band 2 reaches 30 kHz after seven
    CHECK_THAT (host.value (2, "frequency"), WithinAbs (30000.0, 0.5));
    CHECK_THAT (host.value (1, "frequency"), WithinAbs (30000.0 / 20000.0 * 1000.0, 0.5));
    for (int i = 0; i < 4; ++i)
        host.press (up); // Band 2 reaches +30 dB after two
    CHECK_THAT (host.value (2, "gain"), WithinAbs (30.0, 1.0e-4));
    CHECK_THAT (host.value (1, "gain"), WithinAbs (4.0, 1.0e-4));

    host.set (2, "frequency", 12.0f);
    host.set (2, "gain", -29.8f);
    host.settle();
    for (int i = 0; i < 6; ++i)
        host.press (left);
    CHECK_THAT (host.value (2, "frequency"), WithinAbs (10.0, 0.01));
    host.press (down);
    CHECK_THAT (host.value (2, "gain"), WithinAbs (-30.0, 1.0e-4));
    CHECK_THAT (host.value (1, "gain"), WithinAbs (3.8, 1.0e-4));
}

TEST_CASE ("Each arrow press on a Band is one undo step, and so is a held key with its repeats")
{
    EveryControl host;
    auto& history = host.processor.editHistory();
    bandElement (host, 1).grabKeyboardFocus();
    const int steps = history.undoSteps();

    host.press (right);
    host.press (up);
    CHECK (history.undoSteps() == steps + 2);
    for (int repeat = 0; repeat < 5; ++repeat)
        host.hold (withShift (left));
    host.release();
    CHECK (history.undoSteps() == steps + 3);

    history.undo();
    history.undo();
    history.undo();
    CHECK_THAT (host.value (1, "frequency"), WithinAbs (1000.0, 0.01));
    CHECK_THAT (host.value (1, "gain"), WithinAbs (3.0, 1.0e-4));
}

TEST_CASE ("A held arrow key's undo step on a Band ends when the Band loses focus")
{
    EveryControl host;
    auto& history = host.processor.editHistory();
    bandElement (host, 1).grabKeyboardFocus();
    const int steps = history.undoSteps();
    host.hold (up);
    host.hold (up);
    host.findAll<juce::Slider>().front()->grabKeyboardFocus();
    CHECK (history.undoSteps() == steps + 1);
}

TEST_CASE ("Delete on a focused Band removes it and moves focus to the next Band, else the one before, else the display")
{
    EveryControl host;
    host.addBand (2, 200.0f, 0.0f);
    host.addBand (3, 5000.0f, 0.0f);
    host.settle();
    // Tab's order: Band 2, Band 1, Band 3.
    bandElement (host, 1).grabKeyboardFocus();

    host.press (juce::KeyPress (juce::KeyPress::deleteKey));
    CHECK (host.value (1, "in_use") == 0.0f);
    CHECK (bandElement (host, 3).hasKeyboardFocus (false));

    host.press (juce::KeyPress (juce::KeyPress::deleteKey));
    CHECK (host.value (3, "in_use") == 0.0f);
    CHECK (bandElement (host, 2).hasKeyboardFocus (false));

    host.press (juce::KeyPress (juce::KeyPress::backspaceKey));
    CHECK (host.value (2, "in_use") == 0.0f);
    CHECK (host.display.hasKeyboardFocus (false));
}
