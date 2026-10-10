#include "Parameters.h"
#include "PluginProcessor.h"
#include "staple/LookAndFeel.h"
#include "staple/controls/IconButton.h"
#include "staple/controls/Knob.h"
#include "staple/controls/KnobTooltip.h"
#include "staple/controls/ParseValue.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <memory>

using Catch::Matchers::WithinAbs;

namespace
{

using Attachment = juce::AudioProcessorValueTreeState;

// The kit's controls in a window of their own, drawn with Staple's LookAndFeel and attached to eq1's
// parameters as the editor attaches them. Mouse events go straight to the control under test.
struct Kit
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    eq1::EditHistory& history = processor.editHistory();
    staple::LookAndFeel lookAndFeel;
    juce::Component window;

    Kit()
    {
        window.setLookAndFeel (&lookAndFeel);
        window.setBounds (0, 0, 400, 300);
        window.addToDesktop (0);
        window.setVisible (true);
    }

    ~Kit() { window.setLookAndFeel (nullptr); }

    Attachment& state() { return processor.parameterState(); }
    float value (const juce::String& id) { return state().getRawParameterValue (id)->load(); }

    template <typename T>
    T& add (T& component, juce::Rectangle<int> bounds)
    {
        window.addAndMakeVisible (component);
        component.setBounds (bounds);
        return component;
    }

    static juce::MouseEvent event (juce::Component& target, juce::Point<float> position, juce::ModifierKeys mods,
                                   juce::Point<float> downAt, int clicks = 1)
    {
        const auto now = juce::Time::getCurrentTime();
        return { juce::Desktop::getInstance().getMainMouseSource(),
                 position,
                 mods,
                 juce::MouseInputSource::defaultPressure,
                 juce::MouseInputSource::defaultOrientation,
                 juce::MouseInputSource::defaultRotation,
                 juce::MouseInputSource::defaultTiltX,
                 juce::MouseInputSource::defaultTiltY,
                 &target,
                 &target,
                 now,
                 downAt,
                 now,
                 clicks,
                 position != downAt };
    }

    // A press at from, a vertical drag by dy (negative is up) and a release, with mods held throughout.
    static void drag (juce::Component& target, juce::Point<float> from, float dy, juce::ModifierKeys mods = {})
    {
        const auto left = mods.withFlags (juce::ModifierKeys::leftButtonModifier);
        const auto to = from.translated (0.0f, dy);
        target.mouseDown (event (target, from, left, from));
        target.mouseDrag (event (target, from.translated (0.0f, dy / 2.0f), left, from));
        target.mouseDrag (event (target, to, left, from));
        target.mouseUp (event (target, to, mods, from));
    }

    // A press and a release at the target's centre, through Component's interface (Button's is protected).
    static void press (juce::Component& target)
    {
        const auto at = target.getLocalBounds().toFloat().getCentre();
        target.mouseDown (event (target, at, juce::ModifierKeys::leftButtonModifier, at));
    }
    static void release (juce::Component& target)
    {
        const auto at = target.getLocalBounds().toFloat().getCentre();
        target.mouseUp (event (target, at, {}, at));
    }
    static void click (juce::Component& target)
    {
        press (target);
        release (target);
    }

    static void doubleClick (juce::Component& target, juce::Point<float> at)
    {
        const juce::ModifierKeys left (juce::ModifierKeys::leftButtonModifier);
        target.mouseDown (event (target, at, left, at));
        target.mouseUp (event (target, at, {}, at));
        target.mouseDown (event (target, at, left, at, 2));
        target.mouseDoubleClick (event (target, at, left, at, 2));
        target.mouseUp (event (target, at, {}, at, 2));
    }
};

juce::Point<float> centreOf (juce::Component& c) { return c.getLocalBounds().toFloat().getCentre(); }

double position (juce::Slider& slider) { return slider.valueToProportionOfLength (slider.getValue()); }

} // namespace

TEST_CASE ("A Knob's full range takes 200 px of vertical drag, 800 px with Shift, and each drag is one undo step")
{
    Kit kit;
    staple::Knob knob (staple::tokens::knob::gain);
    Attachment::SliderAttachment attachment (kit.state(), "band1_gain", knob);
    kit.add (knob, { 10, 10, knob.getIdealSize(), knob.getIdealSize() });
    REQUIRE_THAT (position (knob), WithinAbs (0.5, 1.0e-6));

    kit.drag (knob, centreOf (knob), -50.0f);
    CHECK_THAT (position (knob), WithinAbs (0.75, 1.0e-4));
    CHECK (kit.history.undoSteps() == 1);

    kit.drag (knob, centreOf (knob), 80.0f, juce::ModifierKeys::shiftModifier);
    CHECK_THAT (position (knob), WithinAbs (0.65, 1.0e-4));
    CHECK (kit.history.undoSteps() == 2);

    // Past either end, it stops there.
    kit.drag (knob, centreOf (knob), -400.0f);
    CHECK_THAT (position (knob), WithinAbs (1.0, 1.0e-6));
    kit.history.undo();
    CHECK_THAT (position (knob), WithinAbs (0.65, 1.0e-4));
}

TEST_CASE ("parseValue reads a typed value in its parameter's units, with or without a unit or a k")
{
    struct Row
    {
        const char* text;
        const char* unit;
        double expected;
    };
    const Row rows[] = {
        { "1.2k", "Hz", 1200.0 },   { "1200", "Hz", 1200.0 },  { "1.2 kHz", "Hz", 1200.0 }, { "1.2KHZ", "Hz", 1200.0 },
        { "450 Hz", "Hz", 450.0 },  { "+3", "dB", 3.0 },       { "-2.5 dB", "dB", -2.5 },   { "0.7", "", 0.7 },
        { ".5", "", 0.5 },          { "15 ms", "ms", 15.0 },   { "1.2 s", "ms", 1200.0 },   { "1.2 s", "s", 1.2 },
        { "15 ms", "s", 0.015 },    { " 2,5 dB ", "dB", 2.5 }, { "40 %", "%", 40.0 },
    };
    for (const auto& row : rows)
    {
        CAPTURE (row.text, row.unit);
        const auto value = staple::parseValue (row.text, row.unit);
        REQUIRE (value.has_value());
        CHECK_THAT (*value, WithinAbs (row.expected, 1.0e-9));
    }
}

TEST_CASE ("parseValue reads nothing from text that isn't a number with a unit it knows")
{
    for (const char* text : { "", "abc", "Auto", "1.2 parsecs", "3 dB dB", "--3", "k" })
    {
        CAPTURE (text);
        CHECK_FALSE (staple::parseValue (text, "dB").has_value());
    }
}

TEST_CASE ("A double-click on a Knob resets its parameter to the default as one undo step")
{
    Kit kit;
    staple::Knob knob (staple::tokens::knob::frequency);
    Attachment::SliderAttachment attachment (kit.state(), "band1_frequency", knob);
    kit.add (knob, { 10, 10, knob.getIdealSize(), knob.getIdealSize() });
    const float defaultFrequency = kit.value ("band1_frequency");
    kit.drag (knob, centreOf (knob), -60.0f);
    REQUIRE (kit.value ("band1_frequency") > defaultFrequency * 2.0f);
    const int steps = kit.history.undoSteps();

    kit.doubleClick (knob, centreOf (knob));
    CHECK_THAT (kit.value ("band1_frequency"), WithinAbs (defaultFrequency, 1.0e-2));
    CHECK (kit.history.undoSteps() == steps + 1);
    kit.history.undo();
    CHECK (kit.value ("band1_frequency") > defaultFrequency * 2.0f);
}

TEST_CASE ("A disabled Knob ignores drags, double-clicks and arrow keys, and shows no tooltip")
{
    Kit kit;
    staple::Knob knob (staple::tokens::knob::gain);
    Attachment::SliderAttachment attachment (kit.state(), "band1_gain", knob);
    kit.add (knob, { 10, 10, knob.getIdealSize(), knob.getIdealSize() });
    kit.processor.parameterState().getParameter ("band1_gain")->setValueNotifyingHost (0.6f);
    knob.setEnabled (false);

    kit.drag (knob, centreOf (knob), -50.0f);
    kit.doubleClick (knob, centreOf (knob));
    CHECK (knob.keyPressed (juce::KeyPress (juce::KeyPress::upKey)) == false);
    knob.mouseEnter (Kit::event (knob, centreOf (knob), {}, centreOf (knob)));
    CHECK_FALSE (knob.isTooltipShown());
    CHECK_THAT (position (knob), WithinAbs (0.6, 1.0e-4));
    CHECK (kit.history.undoSteps() == 0);
}

TEST_CASE ("A Knob's tooltip shows its title and its value text with the unit while hovered or dragged")
{
    Kit kit;
    staple::Knob knob (staple::tokens::knob::gain);
    Attachment::SliderAttachment attachment (kit.state(), "band4_gain", knob);
    knob.setTitle ("Band 4 Gain"); // as #48 names it
    knob.setTextValueSuffix (" dB");
    kit.add (knob, { 150, 150, knob.getIdealSize(), knob.getIdealSize() });
    knob.setValue (-11.42, juce::sendNotificationSync);
    CHECK_FALSE (knob.isTooltipShown());

    knob.mouseEnter (Kit::event (knob, centreOf (knob), {}, centreOf (knob)));
    REQUIRE (knob.isTooltipShown());
    auto& tooltip = *knob.getKnobTooltip();
    CHECK (tooltip.getParentComponent() == &kit.window); // a child of the editor, not a window of its own
    CHECK (tooltip.titleText() == "Band 4 Gain");
    CHECK (tooltip.valueText() == "-11.42 dB");
    CHECK (tooltip.getBottom() <= kit.window.getLocalArea (&knob, knob.getLocalBounds()).getCentreY() - 33); // above the face

    // It follows a drag, and stays while the drag goes on outside the knob.
    const auto left = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier);
    const auto from = centreOf (knob);
    knob.mouseDown (Kit::event (knob, from, left, from));
    knob.mouseExit (Kit::event (knob, from.translated (0.0f, -200.0f), left, from));
    knob.mouseDrag (Kit::event (knob, from.translated (0.0f, -200.0f), left, from));
    CHECK (knob.isTooltipShown());
    CHECK (tooltip.valueText() == "30.00 dB");
    knob.mouseUp (Kit::event (knob, from.translated (0.0f, -200.0f), {}, from));
    CHECK_FALSE (knob.isTooltipShown());
}

TEST_CASE ("A Knob's tooltip sits below the knob where there is no room above")
{
    Kit kit;
    staple::Knob knob (staple::tokens::knob::small);
    kit.add (knob, { 10, 0, knob.getIdealSize(), knob.getIdealSize() });
    knob.mouseEnter (Kit::event (knob, centreOf (knob), {}, centreOf (knob)));
    REQUIRE (knob.isTooltipShown());
    CHECK (knob.getKnobTooltip()->getY() >= knob.getBounds().getCentreY() + 15);
}

TEST_CASE ("A value typed into a Knob's tooltip commits as one undo step on Enter, and Esc cancels it")
{
    Kit kit;
    staple::Knob knob (staple::tokens::knob::frequency);
    Attachment::SliderAttachment attachment (kit.state(), "band1_frequency", knob);
    knob.setTitle ("Band 1 Frequency");
    knob.setTextValueSuffix (" Hz");
    kit.add (knob, { 150, 150, knob.getIdealSize(), knob.getIdealSize() });
    knob.mouseEnter (Kit::event (knob, centreOf (knob), {}, centreOf (knob)));
    auto& tooltip = *knob.getKnobTooltip();

    kit.doubleClick (tooltip, centreOf (tooltip));
    REQUIRE (tooltip.isEditing());
    auto* field = tooltip.getEditor();
    CHECK (field->getTitle() == "Band 1 Frequency value");
    CHECK (field->getWantsKeyboardFocus());
    field->setText ("1.2k");
    field->keyPressed (juce::KeyPress (juce::KeyPress::returnKey));
    CHECK_FALSE (tooltip.isEditing());
    CHECK_THAT (kit.value ("band1_frequency"), WithinAbs (1200.0, 0.5));
    CHECK (kit.history.undoSteps() == 1);

    tooltip.startEditing();
    tooltip.getEditor()->setText ("50 Hz");
    tooltip.getEditor()->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
    CHECK_FALSE (tooltip.isEditing());
    CHECK_THAT (kit.value ("band1_frequency"), WithinAbs (1200.0, 0.5));
    CHECK (kit.history.undoSteps() == 1);

    // Out of range, it stops at the end of the range.
    tooltip.startEditing();
    tooltip.getEditor()->setText ("90 kHz");
    tooltip.getEditor()->keyPressed (juce::KeyPress (juce::KeyPress::returnKey));
    CHECK_THAT (position (knob), WithinAbs (1.0, 1.0e-6));
    CHECK (kit.history.undoSteps() == 2);
}

TEST_CASE ("A momentary IconButton is lit only while held, and reports its press and release")
{
    Kit kit;
    staple::IconButton solo ("Solo", staple::Icon::headphones);
    solo.setMomentary (true);
    solo.setLitColour (staple::tokens::band[3]);
    int presses = 0, releases = 0;
    solo.onPress = [&] { ++presses; };
    solo.onRelease = [&] { ++releases; };
    kit.add (solo, { 10, 10, 24, 24 });
    CHECK_FALSE (solo.isLit());

    Kit::press (solo);
    CHECK (solo.isLit());
    CHECK (presses == 1);
    CHECK (releases == 0);
    Kit::release (solo);
    CHECK_FALSE (solo.isLit());
    CHECK (presses == 1);
    CHECK (releases == 1);
    CHECK_FALSE (solo.getToggleState()); // held, never toggled
}

TEST_CASE ("An IconButton attached as a toggle is lit while on, or shows Off for a Bypass-style power button")
{
    Kit kit;
    staple::IconButton phase ("Phase Invert", staple::Icon::phaseInvert);
    phase.setClickingTogglesState (true);
    Attachment::ButtonAttachment phaseAttachment (kit.state(), eq1::parameters::phaseInvertId, phase);
    staple::IconButton bypass ("Bypass", staple::Icon::power);
    bypass.setOffLook (true);
    bypass.setClickingTogglesState (true);
    Attachment::ButtonAttachment bypassAttachment (kit.state(), eq1::parameters::bypassId (1), bypass);
    kit.add (phase, { 10, 10, 24, 24 });
    kit.add (bypass, { 40, 10, 24, 24 });

    CHECK_FALSE (phase.isLit());
    Kit::click (phase);
    CHECK (kit.value (eq1::parameters::phaseInvertId) == 1.0f);
    CHECK (phase.isLit());
    CHECK (kit.history.undoSteps() == 1);

    CHECK_FALSE (bypass.isOff());
    kit.processor.parameterState().getParameter (eq1::parameters::bypassId (1))->setValueNotifyingHost (1.0f);
    CHECK (bypass.getToggleState());
    CHECK (bypass.isOff());
    CHECK_FALSE (bypass.isLit());
}
