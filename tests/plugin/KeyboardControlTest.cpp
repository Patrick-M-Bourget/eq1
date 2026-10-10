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
        if (slider->getName() == "Threshold")
            continue; // Auto is its top position: it has its own test
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
