#include "PluginProcessor.h"
#include "staple/LookAndFeel.h"
#include "staple/controls/Knob.h"
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
