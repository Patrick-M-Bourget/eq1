#include "EditorHarness.h"
#include "FooterBar.h"
#include "OutputMeter.h"
#include "OutputPopover.h"

#include "eq1/Response.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;

namespace
{

const juce::KeyPress up (juce::KeyPress::upKey), down (juce::KeyPress::downKey), left (juce::KeyPress::leftKey),
    right (juce::KeyPress::rightKey), escape (juce::KeyPress::escapeKey);

struct Footer : harness::OpenEditor
{
    eq1::FooterBar& footer = *harness::findChild<eq1::FooterBar> (*editor);
    eq1::GainScaleReadout& gainScale = *harness::findChild<eq1::GainScaleReadout> (*editor);
    eq1::OutputReadout& output = *harness::findChild<eq1::OutputReadout> (*editor);
    eq1::OutputPopover& popover = footer.getOutputPopover();

    template <typename T>
    T& titled (const juce::String& title)
    {
        auto found = findAll<T> ([&title] (T& c) { return c.getTitle() == title; });
        if (found.empty())
            FAIL ("Nothing is titled " << title);
        return *found.front();
    }

    // A press on component, a drag up by pixels and a release, with Shift held or not.
    void dragUp (juce::Component& component, float pixels, bool shift)
    {
        const juce::Point<float> from { 10.0f, 10.0f }, to { 10.0f, 10.0f - pixels };
        const auto mods = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier | (shift ? juce::ModifierKeys::shiftModifier : 0));
        component.mouseDown (harness::mouseEvent (component, from, mods, from));
        component.mouseDrag (harness::mouseEvent (component, to, mods, from));
        component.mouseUp (harness::mouseEvent (component, to, mods.withoutMouseButtons(), from));
    }

    void doubleClick (juce::Component& component, juce::Point<float> at = { 10.0f, 10.0f })
    {
        const juce::ModifierKeys leftButton (juce::ModifierKeys::leftButtonModifier);
        component.mouseDown (harness::mouseEvent (component, at, leftButton, at, 2));
        component.mouseUp (harness::mouseEvent (component, at, {}, at, 2));
        component.mouseDoubleClick (harness::mouseEvent (component, at, {}, at, 2));
    }

    void openPopover()
    {
        output.onClick();
        REQUIRE (popover.isOpen());
    }

    int undoSteps() { return processor.editHistory().undoSteps(); }
};

} // namespace

TEST_CASE ("Cmd/Ctrl+B toggles Global Bypass as a click does, leaving every Band's Bypass and Dynamics Bypass alone")
{
    Footer host;
    host.addBand (1, 1000.0f, 3.0f);
    host.addBand (2, 200.0f, -3.0f);
    host.set (2, "bypass", 1.0f);
    host.set (1, "dynamics_bypass", 1.0f);
    host.settle();
    host.display.grabKeyboardFocus();
    const int steps = host.undoSteps();
    CHECK_FALSE (host.footer.isBypassedLabelShown());
    CHECK_FALSE (host.gainScale.isNotApplied());
    CHECK_FALSE (host.output.isNotApplied());

    CHECK (host.press (juce::KeyPress ('b', juce::ModifierKeys::commandModifier, 0)));
    CHECK (host.value ("global_bypass") == 1.0f);
    CHECK (host.undoSteps() == steps + 1);
    CHECK (host.titled<staple::IconButton> ("Global Bypass").getToggleState());
    CHECK (host.value (1, "bypass") == 0.0f);
    CHECK (host.value (2, "bypass") == 1.0f);
    CHECK (host.value (1, "dynamics_bypass") == 1.0f);
    host.settle();
    CHECK (host.footer.isBypassedLabelShown());
    CHECK (host.gainScale.isNotApplied());
    CHECK (host.output.isNotApplied());

    CHECK (host.press (juce::KeyPress ('b', juce::ModifierKeys::commandModifier, 0)));
    CHECK (host.value ("global_bypass") == 0.0f);
    CHECK (host.undoSteps() == steps + 2);
    host.settle();
    CHECK_FALSE (host.footer.isBypassedLabelShown());
    CHECK_FALSE (host.output.isNotApplied());

    // And a click does the same.
    host.titled<staple::IconButton> ("Global Bypass").setToggleState (true, juce::sendNotificationSync);
    CHECK (host.value ("global_bypass") == 1.0f);
    CHECK (host.undoSteps() == steps + 3);
}

TEST_CASE ("The Gain Scale readout steps 5% on up and down, 1% with Shift, each press or held key one undo step")
{
    Footer host;
    CHECK (host.gainScale.getTitle() == "Gain Scale");
    CHECK (host.gainScale.getTooltip().contains ("Gain Scale"));
    host.gainScale.grabKeyboardFocus();
    REQUIRE (host.gainScale.hasKeyboardFocus (false));
    const int steps = host.undoSteps();
    CHECK (host.press (up));
    CHECK_THAT (host.value ("gain_scale"), WithinAbs (105.0, 1.0e-3));
    CHECK (host.press (harness::withShift (down)));
    CHECK_THAT (host.value ("gain_scale"), WithinAbs (104.0, 1.0e-3));
    CHECK (host.undoSteps() == steps + 2);
    host.hold (down);
    host.hold (down);
    host.hold (down);
    host.release();
    CHECK_THAT (host.value ("gain_scale"), WithinAbs (89.0, 1.0e-3));
    CHECK (host.undoSteps() == steps + 3);
}

TEST_CASE ("The Gain Scale readout drags 1% per pixel, 0.25% with Shift, and a double-click sets 100%, each one undo step")
{
    Footer host;
    const int steps = host.undoSteps();
    host.dragUp (host.gainScale, 30.0f, false);
    CHECK_THAT (host.value ("gain_scale"), WithinAbs (130.0, 1.0e-3));
    CHECK (host.undoSteps() == steps + 1);
    host.dragUp (host.gainScale, -40.0f, true);
    CHECK_THAT (host.value ("gain_scale"), WithinAbs (120.0, 1.0e-3));
    CHECK (host.undoSteps() == steps + 2);
    host.settle();
    CHECK (host.gainScale.text() == "120%");

    host.doubleClick (host.gainScale);
    CHECK_THAT (host.value ("gain_scale"), WithinAbs (100.0, 1.0e-3));
    CHECK (host.undoSteps() == steps + 3);
}

TEST_CASE ("A double-click's second press resets the Gain Scale readout and the Output Pan slider and drags on from there, one undo step")
{
    Footer host;
    host.openPopover();
    auto& pan = host.titled<juce::Slider> ("Output Pan");
    const bool onPan = GENERATE (false, true);
    juce::Component& target = onPan ? static_cast<juce::Component&> (pan) : host.gainScale;
    const juce::String id = onPan ? "output_pan" : "gain_scale";
    host.set (id, 50.0f);
    // On the slider, 10% of its width is 20%; on the readout, a vertical drag is 1% per pixel.
    const juce::Point<float> at { onPan ? pan.getWidth() * 0.75f : 10.0f, 7.0f };
    const auto to = onPan ? at.translated (pan.getWidth() * 0.1f, 0.0f) : at.translated (0.0f, -10.0f);
    const juce::ModifierKeys leftButton (juce::ModifierKeys::leftButtonModifier);

    target.mouseDown (harness::mouseEvent (target, at, leftButton, at));
    target.mouseUp (harness::mouseEvent (target, at, {}, at));
    const int steps = host.undoSteps();
    // As JUCE sends them: the double-click comes after the second release.
    target.mouseDown (harness::mouseEvent (target, at, leftButton, at, 2));
    target.mouseDrag (harness::mouseEvent (target, to, leftButton, at, 2));
    target.mouseUp (harness::mouseEvent (target, to, {}, at, 2));
    target.mouseDoubleClick (harness::mouseEvent (target, to, {}, at, 2));
    CHECK_THAT (host.value (id), WithinAbs (onPan ? 20.0 : 110.0, 1.0e-3));
    CHECK (host.undoSteps() == steps + 1);
}

TEST_CASE ("The footer's Global Bypass and UI Scale buttons draw 14 px icons")
{
    Footer host;
    for (const juce::String title : { "Global Bypass", "UI Scale" })
    {
        CAPTURE (title);
        CHECK (host.titled<staple::IconButton> (title).getIconSide() == 14.0f);
    }
}

TEST_CASE ("The Output readout shows Output Gain, plus Auto Gain's estimate while Auto Gain is on")
{
    Footer host;
    host.settle();
    CHECK (host.output.getButtonText() == "+0.0 dB");
    CHECK (host.output.getTitle() == "Output");
    host.set ("output_gain", -6.0f);
    host.settle();
    CHECK (host.output.getButtonText() == "-6.0 dB");
    host.set ("output_gain", eq1::parameters::outputGainSilentDb);
    host.settle();
    CHECK (host.output.getButtonText() == "-inf dB");

    // A Bell at 1 kHz, +12 dB: Auto Gain turns it down.
    host.set ("output_gain", 3.0f);
    host.addBand (1, 1000.0f, 12.0f);
    host.settle();
    CHECK (host.output.getButtonText() == "+3.0 dB");
    host.set ("auto_gain", 1.0f);
    host.settle();
    eq1::Settings settings;
    settings.bands[0] = { .inUse = true, .frequency = 1000.0, .gain = 12.0 };
    const double estimate = eq1::autoGainDb (settings, 48000.0);
    REQUIRE (estimate < -1.0);
    CHECK (host.output.getButtonText() == eq1::outputReadoutText (3.0 + estimate));
    CHECK (eq1::outputReadoutText (-1.84) == "-1.8 dB");
    CHECK (eq1::outputReadoutText (0.0) == "+0.0 dB");
    CHECK (eq1::outputReadoutText (2.25) == "+2.3 dB");
}

TEST_CASE ("The output popover opens above the Output readout, right-aligned to it, and closes on a second click, Escape or a click outside")
{
    Footer host;
    CHECK_FALSE (host.popover.isOpen());
    host.openPopover();
    host.settle (300); // popped in
    CHECK (host.popover.isVisible());
    CHECK (host.popover.getCardBounds().getWidth() == 176);
    const auto card = host.editor->getLocalArea (&host.popover, host.popover.getCardBounds());
    const auto readout = host.editor->getLocalArea (&host.output, host.output.getLocalBounds());
    CHECK (card.getBottom() < readout.getY());
    CHECK (card.getRight() == readout.getRight());

    host.output.onClick();
    CHECK_FALSE (host.popover.isOpen());

    host.openPopover();
    host.titled<juce::Slider> ("Output Gain").grabKeyboardFocus();
    CHECK (host.press (escape));
    CHECK_FALSE (host.popover.isOpen());

    host.openPopover();
    host.popover.mouseDown (harness::mouseEvent (host.display, { 20.0f, 20.0f }, juce::ModifierKeys::leftButtonModifier));
    CHECK_FALSE (host.popover.isOpen());
    // And it opens again.
    host.openPopover();
}

TEST_CASE ("The output popover's card is the prototype's 176 x 188.5 px, within 1 px")
{
    Footer host;
    const auto card = host.popover.getCardBounds();
    CHECK (card.getWidth() == 176);
    CHECK (card.getHeight() >= 188);
    CHECK (card.getHeight() <= 189);
}

TEST_CASE ("The output popover's controls set their parameters")
{
    Footer host;
    harness::useLayout (host.processor, juce::AudioChannelSet::stereo());
    host.openPopover();
    host.settle();

    auto& gain = host.titled<staple::Knob> ("Output Gain");
    CHECK (gain.getDiameter() == 64.0f);
    gain.setValue (-4.5, juce::sendNotificationSync);
    CHECK_THAT (host.value ("output_gain"), WithinAbs (-4.5, 1.0e-3));
    host.doubleClick (gain, gain.getFaceCentre());
    CHECK_THAT (host.value ("output_gain"), WithinAbs (0.0, 1.0e-3));

    auto& panMode = host.titled<juce::Button> ("Pan Mode");
    CHECK (panMode.getButtonText() == "L/R");
    const int steps = host.undoSteps();
    panMode.setToggleState (true, juce::sendNotificationSync);
    CHECK (host.value ("pan_mode") == 1.0f);
    CHECK (host.undoSteps() == steps + 1);
    host.settle();
    CHECK (panMode.getButtonText() == "M/S");
    CHECK (panMode.getTooltip().contains ("Pan Mode"));

    auto& pan = host.titled<juce::Slider> ("Output Pan");
    CHECK (host.popover.panReadout() == "Centre");
    // A click at 70% across sets 40 towards Side.
    const juce::Point<float> at { pan.getWidth() * 0.7f, 7.0f };
    pan.mouseDown (harness::mouseEvent (pan, at, juce::ModifierKeys::leftButtonModifier, at));
    pan.mouseUp (harness::mouseEvent (pan, at, {}, at));
    CHECK_THAT (host.value ("output_pan"), WithinAbs (40.0, 1.0e-3));
    CHECK (host.popover.panReadout() == "40 S");
    pan.grabKeyboardFocus();
    CHECK (host.press (left));
    CHECK_THAT (host.value ("output_pan"), WithinAbs (35.0, 1.0e-3));
    host.doubleClick (pan, at);
    CHECK_THAT (host.value ("output_pan"), WithinAbs (0.0, 1.0e-3));
    CHECK (host.popover.panReadout() == "Centre");
    host.set ("output_pan", -40.0f);
    host.set ("pan_mode", 0.0f);
    CHECK (host.popover.panReadout() == "40 L");
    CHECK (eq1::outputPanText (20.0, true) == "20 S");
    CHECK (eq1::outputPanText (-20.0, true) == "20 M");
    CHECK (eq1::outputPanText (0.0, false) == "Centre");

    for (const auto& [title, id] : { std::pair { "Phase Invert", "phase_invert" }, std::pair { "Auto Gain", "auto_gain" } })
    {
        CAPTURE (title);
        auto& toggle = host.titled<juce::Button> (title);
        toggle.setToggleState (true, juce::sendNotificationSync);
        CHECK (host.value (id) == 1.0f);
        toggle.setToggleState (false, juce::sendNotificationSync);
        CHECK (host.value (id) == 0.0f);
    }
}

TEST_CASE ("On mono, Pan Mode and Output Pan are disabled")
{
    Footer host;
    const bool mono = GENERATE (true, false);
    harness::useLayout (host.processor, mono ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
    host.openPopover();
    host.settle (300);
    CHECK (host.titled<juce::Button> ("Pan Mode").isEnabled() != mono);
    CHECK (host.titled<juce::Slider> ("Output Pan").isEnabled() != mono);
    CHECK (host.titled<juce::Slider> ("Output Gain").isEnabled());
}

TEST_CASE ("The popover's Output Meter toggle hides and shows the Output Meter, isn't undoable, and the session keeps it")
{
    Footer host;
    host.openPopover();
    auto& toggle = host.titled<juce::Button> ("Output Meter");
    auto* meter = harness::findChild<eq1::OutputMeter> (*host.editor);
    REQUIRE (meter != nullptr);
    CHECK (toggle.getToggleState());
    CHECK (meter->isVisible());
    const int steps = host.undoSteps();
    toggle.setToggleState (false, juce::sendNotificationSync);
    CHECK_FALSE (host.processor.isOutputMeterShown());
    CHECK_FALSE (meter->isVisible());
    CHECK (host.undoSteps() == steps);

    juce::MemoryBlock state;
    host.processor.getStateInformation (state);
    eq1::PluginProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    CHECK_FALSE (restored.isOutputMeterShown());

    toggle.setToggleState (true, juce::sendNotificationSync);
    CHECK (meter->isVisible());
}

TEST_CASE ("The UI Scale menu, titled UI Scale, offers the five UI Scales, the current one ticked, and sets the one picked")
{
    Footer host;
    auto& button = host.titled<eq1::UiScaleMenu> ("UI Scale");
    CHECK (button.getIcon() == staple::Icon::uiScale);
    const auto items = [&button] {
        std::vector<std::pair<juce::String, bool>> found;
        const auto menu = button.menu();
        for (juce::PopupMenu::MenuItemIterator it (menu); it.next();)
            found.emplace_back (it.getItem().text, it.getItem().isTicked);
        return found;
    };
    const auto items100 = items();
    REQUIRE (items100.size() == 6);
    CHECK (items100[0].first == "UI Scale");
    CHECK (items100[1] == std::pair<juce::String, bool> { "75%", false });
    CHECK (items100[2] == std::pair<juce::String, bool> { "100%", true });
    CHECK (items100[5] == std::pair<juce::String, bool> { "200%", false });

    button.pick (150);
    CHECK (host.processor.uiScalePercent() == 150);
    CHECK (host.editor->getWidth() == 1800);
    CHECK (items()[4] == std::pair<juce::String, bool> { "150%", true });
}

// Renders the footer for checking by hand against the prototype (harness::writeSnapshot):
// footer-normal, footer-bypassed and footer-popover, each the window's bottom at 2x.
TEST_CASE ("Footer snapshots: normal, under Global Bypass and with the output popover open", "[.screens]")
{
    Footer host;
    harness::useLayout (host.processor, juce::AudioChannelSet::stereo());
    host.addBand (1, 1000.0f, 6.0f);
    host.set ("auto_gain", 1.0f);
    host.set ("output_pan", 40.0f);
    host.settle (300);
    const auto write = [&] (const char* name) {
        harness::writeSnapshot (*host.editor, juce::String ("footer-") + name, host.editor->getLocalBounds().removeFromBottom (300));
    };
    write ("normal");
    host.footer.toggleGlobalBypass();
    host.settle (300);
    write ("bypassed");
    host.footer.toggleGlobalBypass();
    host.openPopover();
    host.settle (300);
    write ("popover");
}
