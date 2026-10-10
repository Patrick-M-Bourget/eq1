#include "Parameters.h"
#include "PluginProcessor.h"
#include "staple/Fonts.h"
#include "staple/LookAndFeel.h"
#include "staple/controls/EdgeSelector.h"
#include "staple/controls/IconButton.h"
#include "staple/controls/Knob.h"
#include "staple/controls/KnobTooltip.h"
#include "staple/controls/ParseValue.h"
#include "staple/controls/Popover.h"
#include "staple/controls/TextChip.h"

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

namespace
{
// The Shape Edge selector, its items in the parameter's order, each with its icon.
void addShapes (staple::EdgeSelector& selector)
{
    const staple::Icon icons[] = { staple::Icon::bell,     staple::Icon::lowShelf, staple::Icon::lowCut,    staple::Icon::highShelf,
                                   staple::Icon::highCut,  staple::Icon::notch,    staple::Icon::bandPass,  staple::Icon::tiltShelf,
                                   staple::Icon::flatTilt, staple::Icon::allPass };
    const auto& names = eq1::parameters::shapeNames();
    for (int i = 0; i < names.size(); ++i)
        selector.addItem (names[i], i + 1, icons[i]);
}
} // namespace

TEST_CASE ("An EdgeSelector attached with ComboBoxAttachment sets its parameter when an item is picked, and steps with up and down")
{
    Kit kit;
    staple::EdgeSelector shape ("Shape", staple::EdgeSelector::Side::left);
    addShapes (shape);
    Attachment::ComboBoxAttachment attachment (kit.state(), eq1::parameters::shapeId (1), shape);
    kit.add (shape, { 0, 10, staple::tokens::layout::edgeSelectorWidth, staple::tokens::layout::edgeSelectorHeight });
    CHECK (shape.getText() == "Bell");

    shape.setSelectedId (5, juce::sendNotificationSync); // as a pick from its list does
    CHECK (kit.value (eq1::parameters::shapeId (1)) == 4.0f);
    CHECK (kit.history.undoSteps() == 1);

    CHECK (shape.keyPressed (juce::KeyPress (juce::KeyPress::downKey)));
    CHECK (kit.value (eq1::parameters::shapeId (1)) == 5.0f);
    CHECK (shape.getText() == "Notch");
    CHECK (shape.keyPressed (juce::KeyPress (juce::KeyPress::upKey)));
    CHECK (shape.keyPressed (juce::KeyPress (juce::KeyPress::upKey)));
    CHECK (kit.value (eq1::parameters::shapeId (1)) == 3.0f);
    CHECK (kit.history.undoSteps() == 4);

    // Its list carries the same icons.
    CHECK (shape.getRootMenu()->getNumItems() == 10);
    juce::PopupMenu::MenuItemIterator items (*shape.getRootMenu());
    REQUIRE (items.next());
    CHECK (items.getItem().image != nullptr);
}

TEST_CASE ("Knob and EdgeSelector keep JUCE's accessibility and the arrow-key steps")
{
    Kit kit;
    staple::Knob gain (staple::tokens::knob::gain);
    Attachment::SliderAttachment gainAttachment (kit.state(), "band2_gain", gain);
    gain.setTitle ("Band 2 Gain");
    gain.setTextValueSuffix (" dB");
    staple::EdgeSelector placement ("Stereo Placement", staple::EdgeSelector::Side::right);
    const staple::Icon icons[] = { staple::Icon::placementStereo, staple::Icon::placementLeft, staple::Icon::placementRight,
                                   staple::Icon::placementMid, staple::Icon::placementSide };
    for (int i = 0; i < eq1::parameters::placementNames().size(); ++i)
        placement.addItem (eq1::parameters::placementNames()[i], i + 1, icons[i]);
    Attachment::ComboBoxAttachment placementAttachment (kit.state(), eq1::parameters::placementId (2), placement);
    placement.setTitle ("Band 2 Stereo Placement");
    kit.add (gain, { 10, 10, gain.getIdealSize(), gain.getIdealSize() });
    kit.add (placement, { 200, 10, 104, 34 });
    gain.setValue (3.0, juce::sendNotificationSync);

    auto* knobHandler = gain.getAccessibilityHandler();
    REQUIRE (knobHandler != nullptr);
    CHECK (knobHandler->getTitle() == "Band 2 Gain");
    REQUIRE (knobHandler->getValueInterface() != nullptr);
    CHECK (knobHandler->getValueInterface()->getCurrentValueAsString() == "3.00 dB");
    auto* selectorHandler = placement.getAccessibilityHandler();
    REQUIRE (selectorHandler != nullptr);
    CHECK (selectorHandler->getTitle() == "Band 2 Stereo Placement");
    REQUIRE (selectorHandler->getValueInterface() != nullptr);
    CHECK (selectorHandler->getValueInterface()->getCurrentValueAsString() == "Stereo");

    CHECK (gain.getWantsKeyboardFocus());
    const double before = position (gain);
    CHECK (gain.keyPressed (juce::KeyPress (juce::KeyPress::upKey)));
    CHECK_THAT (position (gain), WithinAbs (before + 0.01, 1.0e-4));
    CHECK (placement.getWantsKeyboardFocus());
    CHECK (placement.keyPressed (juce::KeyPress (juce::KeyPress::downKey)));
    CHECK (selectorHandler->getValueInterface()->getCurrentValueAsString() == "Left");
}

TEST_CASE ("A Popover closes on an outside click and on Esc, returns focus to its opener, and its contents leave the focus order")
{
    Kit kit;
    juce::TextButton opener ("Analyzer"), elsewhere ("Elsewhere");
    kit.add (opener, { 20, 20, 80, 24 });
    kit.add (elsewhere, { 200, 200, 80, 24 });
    staple::Popover popover;
    juce::TextButton inside ("Pre + Post");
    popover.addAndMakeVisible (inside);
    popover.setCardSize (160, 80);
    inside.setBounds (popover.getCardBounds().reduced (8).withHeight (24));
    int closes = 0;
    popover.onClose = [&] { ++closes; };

    // Tab reaches c: it is showing, and among the focus stops of its focus container.
    const auto inFocusOrder = [&] (juce::Component& c) {
        auto* container = c.findKeyboardFocusContainer();
        const auto all = juce::KeyboardFocusTraverser().getAllComponents (container != nullptr ? container : &kit.window);
        return c.isShowing() && std::find (all.begin(), all.end(), &c) != all.end();
    };
    CHECK_FALSE (inFocusOrder (inside));

    opener.grabKeyboardFocus();
    popover.open (opener);
    REQUIRE (popover.isOpen());
    CHECK (popover.getParentComponent() == &kit.window); // inside the editor, not a window of its own
    CHECK (popover.getCardBounds().translated (popover.getX(), popover.getY()).getY() >= opener.getBottom()); // below its opener
    CHECK (inside.hasKeyboardFocus (false));
    CHECK (inFocusOrder (inside));

    // A click inside, or on its opener, leaves it open; a click anywhere else closes it.
    popover.mouseDown (Kit::event (inside, centreOf (inside), juce::ModifierKeys::leftButtonModifier, centreOf (inside)));
    CHECK (popover.isOpen());
    popover.mouseDown (Kit::event (elsewhere, centreOf (elsewhere), juce::ModifierKeys::leftButtonModifier, centreOf (elsewhere)));
    CHECK_FALSE (popover.isOpen());
    CHECK (closes == 1);
    CHECK (opener.hasKeyboardFocus (false));
    CHECK_FALSE (inFocusOrder (inside));

    popover.open (opener);
    REQUIRE (inside.hasKeyboardFocus (false));
    CHECK (kit.window.getPeer()->handleKeyPress (juce::KeyPress (juce::KeyPress::escapeKey)));
    CHECK_FALSE (popover.isOpen());
    CHECK (closes == 2);
    CHECK (opener.hasKeyboardFocus (false));
    CHECK_FALSE (inFocusOrder (inside));
}

TEST_CASE ("A Popover opens above its opener where there is no room below")
{
    Kit kit;
    juce::TextButton opener ("Display Range");
    kit.add (opener, { 20, 260, 80, 24 });
    staple::Popover popover;
    popover.setCardSize (160, 120);
    popover.open (opener);
    REQUIRE (popover.isOpen());
    CHECK (popover.getCardBounds().translated (popover.getX(), popover.getY()).getBottom() <= opener.getY());
}

TEST_CASE ("A TextChip clicks as a Button, and is as wide as its text, padding and chevron")
{
    Kit kit;
    staple::TextChip range ("12 dB", staple::TextChip::Look::filled);
    staple::TextChip plain ("Copy", staple::TextChip::Look::plain);
    range.setChevron (true);
    kit.add (range, { 10, 10, range.getIdealWidth(), 22 });
    int clicks = 0;
    range.onClick = [&] { ++clicks; };
    Kit::click (range);
    CHECK (clicks == 1);

    const float text = juce::GlyphArrangement::getStringWidth (staple::font (staple::tokens::size::fs3, staple::Weight::medium), "Copy");
    CHECK (plain.getIdealWidth() >= static_cast<int> (text) + 12);
    CHECK (range.getIdealWidth() > plain.getIdealWidth());
    range.setChevron (false);
    const int withoutChevron = range.getIdealWidth();
    range.setChevron (true);
    CHECK (range.getIdealWidth() >= withoutChevron + 8);
}

TEST_CASE ("A Knob's outer ring lane reports presses, drags, double-clicks and hovers to its handler, and doesn't turn the knob")
{
    struct Ring final : staple::Knob::RingHandler
    {
        int downs = 0, drags = 0, ups = 0, doubleClicks = 0, hoversOn = 0, hoversOff = 0, paints = 0;
        float inner = 0.0f, outer = 0.0f;
        void paintRing (juce::Graphics&, staple::Knob&, juce::Point<float>, float in, float out) override
        {
            ++paints;
            inner = in;
            outer = out;
        }
        void ringMouseDown (staple::Knob&, const juce::MouseEvent&) override { ++downs; }
        void ringMouseDrag (staple::Knob&, const juce::MouseEvent&) override { ++drags; }
        void ringMouseUp (staple::Knob&, const juce::MouseEvent&) override { ++ups; }
        void ringDoubleClick (staple::Knob&, const juce::MouseEvent&) override { ++doubleClicks; }
        void ringHover (staple::Knob&, bool over) override { ++(over ? hoversOn : hoversOff); }
    } ring;

    Kit kit;
    staple::Knob gain (staple::tokens::knob::gain);
    Attachment::SliderAttachment attachment (kit.state(), "band1_gain", gain);
    gain.setRing (&ring); // the Gain knob's 12 px lane at r + 10
    kit.add (gain, { 10, 10, gain.getIdealSize(), gain.getIdealSize() });
    const auto centre = gain.getFaceCentre();
    const auto onRing = centre.translated (0.0f, -(33.0f + 10.0f));
    const auto beyond = centre.translated (0.0f, -(33.0f + 17.0f));
    CHECK (gain.getIdealSize() >= 66 + 2 * 16);
    CHECK (gain.isOnRing (onRing));
    CHECK_FALSE (gain.isOnRing (centre));
    CHECK_FALSE (gain.isOnRing (beyond));
    CHECK (gain.hitTest (juce::roundToInt (onRing.x), juce::roundToInt (onRing.y)));
    CHECK_FALSE (gain.hitTest (juce::roundToInt (beyond.x), juce::roundToInt (beyond.y)));

    gain.mouseMove (Kit::event (gain, onRing, {}, onRing));
    CHECK (ring.hoversOn == 1);
    gain.mouseMove (Kit::event (gain, centre, {}, centre));
    CHECK (ring.hoversOff == 1);

    Kit::drag (gain, onRing, -50.0f);
    CHECK (ring.downs == 1);
    CHECK (ring.drags == 2);
    CHECK (ring.ups == 1);
    Kit::doubleClick (gain, onRing);
    CHECK (ring.doubleClicks == 1);
    CHECK_THAT (position (gain), WithinAbs (0.5, 1.0e-6));
    CHECK (kit.history.undoSteps() == 0);

    juce::Image image (juce::Image::ARGB, gain.getWidth(), gain.getHeight(), true);
    juce::Graphics g (image);
    gain.paintEntireComponent (g, false);
    CHECK (ring.paints == 1);
    CHECK_THAT (ring.inner, WithinAbs (33.0 + 4.0, 1.0e-4));
    CHECK_THAT (ring.outer, WithinAbs (33.0 + 16.0, 1.0e-4));
}

TEST_CASE ("Tab skips a disabled Knob")
{
    Kit kit;
    staple::Knob knob (staple::tokens::knob::q);
    kit.add (knob, { 10, 10, knob.getIdealSize(), knob.getIdealSize() });
    const auto focusStops = [&] { return juce::KeyboardFocusTraverser().getAllComponents (&kit.window); };
    CHECK (focusStops().size() == 1);
    knob.setEnabled (false);
    CHECK (focusStops().empty());
}

// Not run by ctest: draws every kit control in its states into the PNG at $EQ1_GALLERY_PNG, for
// checking by hand against the prototype (docs/staple-handoff/prototype/Main.dc.html).
//   build/tests/eq1_plugin_tests "[.gallery]"
TEST_CASE ("The Staple controls kit's gallery", "[.gallery]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("EQ1_GALLERY_PNG", {});
    if (path.isEmpty())
        SKIP ("EQ1_GALLERY_PNG names the file to write");

    Kit kit;
    auto& w = kit.window;
    w.setSize (760, 520);
    w.setOpaque (true);
    struct Background final : juce::Component
    {
        void paint (juce::Graphics& g) override { g.fillAll (staple::tokens::colour::bg0); }
    } background;
    kit.add (background, w.getLocalBounds());

    struct Lane final : staple::Knob::RingHandler
    {
        void paintRing (juce::Graphics& g, staple::Knob&, juce::Point<float> c, float inner, float outer) override
        {
            juce::Path lane;
            const float r = (inner + outer) / 2.0f;
            lane.addCentredArc (c.x, c.y, r, r, 0.0f, juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
            g.setColour (staple::tokens::colour::knobRingLane);
            g.strokePath (lane, juce::PathStrokeType (outer - inner));
        }
    } lane;

    namespace t = staple::tokens;
    std::vector<std::unique_ptr<juce::Component>> owned;
    const auto keep = [&] (auto* c) {
        owned.emplace_back (c);
        return c;
    };

    // Knobs: Frequency, Gain (bipolar, with its ring lane), Q, a 30 px dynamics knob, Output Gain from
    // 0 dB on its skewed range, and a disabled one.
    auto* frequency = keep (new staple::Knob (t::knob::frequency, "Frequency"));
    auto* gain = keep (new staple::Knob (t::knob::gain, "Gain"));
    auto* q = keep (new staple::Knob (t::knob::q, "Q"));
    auto* small = keep (new staple::Knob (t::knob::small, "Attack"));
    auto* output = keep (new staple::Knob (64.0f, "Output Gain"));
    auto* disabled = keep (new staple::Knob (t::knob::q, "Q"));
    Attachment::SliderAttachment a1 (kit.state(), "band4_frequency", *frequency), a2 (kit.state(), "band4_gain", *gain),
        a3 (kit.state(), "band4_q", *q), a4 (kit.state(), eq1::parameters::outputGainId, *output);
    for (auto* k : { frequency, gain, q, small, disabled })
        k->setArcColour (t::band[3]);
    output->setArcColour (t::colour::curveMain);
    output->setArcOrigin (0.0);
    gain->setBipolar (true);
    gain->setRing (&lane);
    gain->setTitle ("Band 4 Gain");
    gain->setTextValueSuffix (" dB");
    frequency->setTitle ("Band 4 Frequency");
    frequency->setTextValueSuffix (" Hz");
    frequency->setValue (2400.0, juce::sendNotificationSync);
    gain->setValue (-11.42, juce::sendNotificationSync);
    q->setValue (2.0, juce::sendNotificationSync);
    small->setRange (0.0, 100.0);
    small->setValue (30.0);
    output->setValue (3.0, juce::sendNotificationSync);
    disabled->setEnabled (false);
    int x = 20;
    for (auto* k : { frequency, gain, q, small, output, disabled })
    {
        const int s = k->getIdealSize();
        kit.add (*k, { x, 150 - s / 2, s, s });
        x += s + 16;
    }
    frequency->mouseEnter (Kit::event (*frequency, centreOf (*frequency), {}, centreOf (*frequency)));
    gain->mouseEnter (Kit::event (*gain, centreOf (*gain), {}, centreOf (*gain)));
    gain->getKnobTooltip()->startEditing();

    // Icon buttons: normal, hover, pressed, lit, Off, Off hovered, disabled, focused.
    x = 20;
    std::vector<staple::IconButton*> icons;
    for (int i = 0; i < 8; ++i)
    {
        auto* b = keep (new staple::IconButton ("icon", i == 4 || i == 5 ? staple::Icon::power : staple::Icon::headphones));
        b->setLitColour (t::band[3]);
        kit.add (*b, { x, 230, 24, 24 });
        icons.push_back (b);
        x += 40;
    }
    icons[1]->setState (juce::Button::buttonOver);
    icons[2]->setState (juce::Button::buttonDown);
    icons[3]->setToggleState (true, juce::dontSendNotification);
    for (int i : { 4, 5 })
    {
        icons[static_cast<size_t> (i)]->setOffLook (true);
        icons[static_cast<size_t> (i)]->setToggleState (true, juce::dontSendNotification);
    }
    icons[5]->setState (juce::Button::buttonOver);
    icons[6]->setEnabled (false);

    // Text chips: filled with a chevron (normal, hover, pressed), plain (normal, hover), disabled.
    x = 20;
    std::vector<staple::TextChip*> chips;
    for (int i = 0; i < 6; ++i)
    {
        const bool filled = i < 3 || i == 5;
        auto* c = keep (new staple::TextChip (filled ? "12 dB" : "A/B", filled ? staple::TextChip::Look::filled : staple::TextChip::Look::plain));
        c->setChevron (filled);
        kit.add (*c, { x, 290, c->getIdealWidth(), 22 });
        chips.push_back (c);
        x += c->getIdealWidth() + 16;
    }
    chips[1]->setState (juce::Button::buttonOver);
    chips[2]->setState (juce::Button::buttonDown);
    chips[4]->setState (juce::Button::buttonOver);
    chips[5]->setEnabled (false);

    // Edge selectors: Shape on a panel's left side, Stereo Placement on its right, and a disabled one.
    struct Panel final : juce::Component
    {
        void paint (juce::Graphics& g) override
        {
            g.setColour (staple::tokens::colour::raised);
            g.fillRoundedRectangle (getLocalBounds().toFloat(), staple::tokens::size::r3);
        }
    } panel;
    kit.add (panel, { 20, 340, 480, 60 });
    staple::EdgeSelector shape ("Shape", staple::EdgeSelector::Side::left);
    addShapes (shape);
    shape.setSelectedId (1);
    shape.setEdgeColour (t::band[3]);
    shape.setIconColour (t::band[3]);
    staple::EdgeSelector placement ("Stereo Placement", staple::EdgeSelector::Side::right);
    placement.addItem ("Stereo", 1, staple::Icon::placementStereo, t::colour::placeStereo);
    placement.setSelectedId (1);
    placement.setEdgeColour (t::band[3]);
    staple::EdgeSelector off ("Shape", staple::EdgeSelector::Side::left);
    addShapes (off);
    off.setSelectedId (3);
    off.setEdgeColour (t::band[3]);
    off.setIconColour (t::band[3]);
    off.setEnabled (false);
    panel.addAndMakeVisible (shape);
    panel.addAndMakeVisible (placement);
    shape.setBounds (0, 13, 104, 34);
    placement.setBounds (480 - 104, 13, 104, 34);
    kit.add (off, { 560, 353, 104, 34 });

    // A popover, opened under a chip.
    staple::TextChip opener ("Analyzer", staple::TextChip::Look::filled);
    opener.setChevron (true);
    kit.add (opener, { 540, 230, opener.getIdealWidth(), 22 });
    staple::Popover popover;
    staple::TextChip item1 ("Pre + Post", staple::TextChip::Look::plain), item2 ("Post", staple::TextChip::Look::plain);
    popover.setCardSize (150, 74);
    popover.addAndMakeVisible (item1);
    popover.addAndMakeVisible (item2);
    item1.setBounds (popover.getCardBounds().reduced (6).withHeight (30));
    item2.setBounds (item1.getBounds().translated (0, 32));
    item1.setState (juce::Button::buttonOver);
    popover.open (opener);
    for (int frame = 0; frame < 20 && popover.getAlpha() < 1.0f; ++frame) // its pop-in, played to the end
    {
        juce::Thread::sleep (20);
        juce::Timer::callPendingTimersSynchronously();
    }

    // The focus ring, as LookAndFeel's FocusOutline draws it after Tab (a window of its own, so drawn
    // here in its place).
    struct FocusRing final : juce::Component
    {
        void paint (juce::Graphics& g) override
        {
            namespace size = staple::tokens::size;
            g.setColour (staple::tokens::colour::focus);
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (size::focusWidth / 2.0f),
                                    size::r2 + size::focusOffset + size::focusWidth / 2.0f, size::focusWidth);
        }
    } focusRing;
    focusRing.setInterceptsMouseClicks (false, false);
    kit.add (focusRing, icons[7]->getBounds().expanded (4));
    juce::Desktop::getInstance().getAnimator().cancelAllAnimations (true);

    CHECK (popover.isOpen());
    CHECK (popover.isVisible());
    CHECK (popover.getAlpha() == 1.0f);
    const auto image = w.createComponentSnapshot (w.getLocalBounds(), true, 2.0f);
    juce::File file (path);
    file.deleteFile();
    juce::FileOutputStream out (file);
    REQUIRE (out.openedOk());
    CHECK (juce::PNGImageFormat().writeImageToStream (image, out));
}
