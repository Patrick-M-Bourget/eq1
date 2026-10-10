#include "BandPanel.h"
#include "DetectionRangeBar.h"
#include "EditorHarness.h"
#include "FooterBar.h"
#include "HeaderBar.h"
#include "OutputMeter.h"
#include "staple/LookAndFeel.h"
#include "staple/controls/Popover.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>

using Catch::Matchers::WithinAbs;
using harness::OpenEditor;

namespace
{

const juce::KeyPress left (juce::KeyPress::leftKey), right (juce::KeyPress::rightKey), up (juce::KeyPress::upKey),
    down (juce::KeyPress::downKey);
juce::KeyPress withShift (juce::KeyPress key) { return { key.getKeyCode(), juce::ModifierKeys::shiftModifier, 0 }; }

// The editor with a dynamic Low Shelf in Band 1 on a Free Detection Range, selected, so the Band panel,
// its dynamics section and the Detection Range bar show every control.
struct EveryControl : OpenEditor
{
    EveryControl()
    {
        addBand (1, 1000.0f, 3.0f, 1.0f);
        set (1, "dynamic_range", -6.0f);
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

    // Opens what the button titled title shows from the keyboard, as Return on it does: the footer's
    // Analyzer or output popover.
    void openCallOut (const juce::String& title)
    {
        auto* button = findAll<juce::Button> ([&title] (juce::Button& b) { return b.getTitle() == title; }).front();
        button->grabKeyboardFocus();
        press (juce::KeyPress (juce::KeyPress::returnKey));
        REQUIRE ((harness::findChild<juce::CallOutBox> (*editor, [] (juce::CallOutBox& b) { return b.isVisible(); }) != nullptr
                  || harness::findChild<staple::Popover> (*editor, [] (staple::Popover& p) { return p.isOpen(); }) != nullptr));
    }

    // Closes the open popover or call-out, as Escape does.
    void closeCallOut()
    {
        if (auto* popover = harness::findChild<staple::Popover> (*editor, [] (staple::Popover& p) { return p.isOpen(); }))
        {
            popover->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
            return;
        }
        auto* box = harness::findChild<juce::CallOutBox> (*editor, [] (juce::CallOutBox& b) { return b.isVisible(); });
        REQUIRE (box != nullptr);
        box->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
    }
};

} // namespace

namespace
{
void checkArrowSteps (OpenEditor& host, const std::vector<juce::Slider*>& sliders)
{
    for (auto* slider : sliders)
    {
        if (slider->getName() == "Threshold" || slider->getName() == "Gain Scale" || slider->getName() == "Output Pan"
            || slider->getName() == "Dynamic Range" || slider->getName().startsWith ("Detection "))
            continue; // Auto is Threshold's top position; Gain Scale and Output Pan step by 5 (FooterTest), Dynamic
                      // Range by 1 dB and the Detection Range's limits by 1/6 octave (BandPanelTest): each has its own test
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
} // namespace

TEST_CASE ("Arrow keys step every slider 1% of its range, 0.2% with Shift, within its range")
{
    EveryControl host;
    // The Band panel's Frequency, Gain, Q and Slope, the Dynamic Range ring, Threshold, Attack and
    // Release, the Detection Range bar's two limits, and Gain Scale.
    const auto sliders = host.sliders();
    CHECK (sliders.size() == 11);
    checkArrowSteps (host, sliders);

    // Output Gain and Output Pan, in the output popover.
    host.openCallOut ("Output");
    auto inCallOut = host.sliders();
    std::erase_if (inCallOut, [&sliders] (juce::Slider* s) { return std::find (sliders.begin(), sliders.end(), s) != sliders.end(); });
    CHECK (inCallOut.size() == 2);
    checkArrowSteps (host, inCallOut);
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
    // The Band panel's, the Detection Range bar's and the footer's sliders.
    for (auto* slider : host.sliders())
    {
        if (slider->getName().startsWith ("Detection "))
            continue; // stepped by left and right only (BandPanelTest)
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

// The editor's focus order while a popover is open: the order without it, with the popover's controls
// in one run somewhere in it.
void checkWalkedWith (juce::Component& editor, juce::Component& popover, const std::vector<juce::String>& closed)
{
    const auto open = focusOrder (editor), inside = focusOrder (popover);
    REQUIRE_FALSE (inside.empty());
    const auto run = std::search (open.begin(), open.end(), inside.begin(), inside.end());
    CHECK (run != open.end());
    auto rest = open;
    if (run != open.end())
        rest.erase (rest.begin() + (run - open.begin()), rest.begin() + (run - open.begin()) + static_cast<std::ptrdiff_t> (inside.size()));
    CHECK (rest == closed);
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

namespace
{
template <typename T>
T& named (OpenEditor& host, const juce::String& name)
{
    return *host.findAll<T> ([&name] (T& c) { return c.getName() == name || c.getTitle() == name; }).front();
}

// The groups Tab walks through, in order: the header, Display Range, the display with its Bands and
// the Output Meter, the Band panel, the footer, and an open popover or call-out.
int groupOf (juce::Component& component)
{
    for (auto* c = &component; c != nullptr; c = c->getParentComponent())
    {
        if (dynamic_cast<juce::CallOutBox*> (c) != nullptr || dynamic_cast<staple::Popover*> (c) != nullptr)
            return 5;
        if (dynamic_cast<eq1::FooterBar*> (c) != nullptr)
            return 4;
        if (dynamic_cast<eq1::BandPanel*> (c) != nullptr || dynamic_cast<eq1::DetectionRangeBar*> (c) != nullptr)
            return 3;
        if (dynamic_cast<eq1::EqDisplay*> (c) != nullptr || dynamic_cast<eq1::OutputMeter*> (c) != nullptr)
            return 2;
        if (c->getName() == "Display Range")
            return 1;
        if (dynamic_cast<eq1::HeaderBar*> (c) != nullptr)
            return 0;
    }
    return -1;
}

// What is wrong with Tab's order: a visible, enabled control (a button, slider or menu, the display, a
// Band in use or the Output Meter) not reached exactly once, or a control reached out of its group's order.
std::vector<juce::String> focusProblems (OpenEditor& host)
{
    std::vector<juce::String> problems;
    const auto order = juce::KeyboardFocusTraverser().getAllComponents (host.editor.get());
    const auto interactive = host.findAll<juce::Component> ([] (juce::Component& c) {
        const bool control = dynamic_cast<juce::Button*> (&c) != nullptr || dynamic_cast<juce::Slider*> (&c) != nullptr
                             || dynamic_cast<juce::ComboBox*> (&c) != nullptr || dynamic_cast<eq1::EqDisplay*> (&c) != nullptr
                             || dynamic_cast<eq1::OutputMeter*> (&c) != nullptr || c.getName().startsWith ("Band ");
        return control && c.isShowing() && c.isEnabled();
    });
    for (auto* control : interactive)
        if (const auto times = std::count (order.begin(), order.end(), control); times != 1)
            problems.push_back (describe (*control) + " reached " + juce::String (times) + " times");
    int group = -1;
    for (auto* component : order)
    {
        if (groupOf (*component) < group)
            problems.push_back (describe (*component) + " out of its group's order");
        group = std::max (group, groupOf (*component));
    }
    return problems;
}

std::vector<juce::String> slice (const std::vector<juce::String>& order, size_t from, size_t count)
{
    return { order.begin() + static_cast<std::ptrdiff_t> (std::min (from, order.size())),
             order.begin() + static_cast<std::ptrdiff_t> (std::min (from + count, order.size())) };
}
} // namespace

TEST_CASE ("Tab walks the header, Display Range, the Bands, the Band panel and the footer, reaching every control once")
{
    EveryControl host;
    host.addBand (2, 200.0f, 0.0f);
    host.set (2, "dynamic_range", -4.0f); // a Dynamic Band, so its grip shows
    host.addBand (3, 1000.0f, -3.0f); // ties with Band 1: the lower Band Slot first
    undoAndRedoEnabled (host);

    const auto order = focusOrder (*host.editor);
    // The header, left to right.
    CHECK (slice (order, 0, 7) == std::vector<juce::String> { "Previous Preset", "Presets", "Next Preset", "Undo", "Redo", "A/B Compare", "Copy" });
    // Display Range, the display and its Bands by Frequency, each shown Dynamic Range grip (the Dynamic
    // Band's, and the selected Band's) after its Band, then the Output Meter's Clip Lights.
    CHECK (slice (order, 7, 8) == std::vector<juce::String> { "Display Range", "EQ Display", "Band 2", "Band 2 Dynamic Range Handle", "Band 1",
                                                              "Band 1 Dynamic Range Handle", "Band 3", "Output Meter" });
    // The footer, left to right, last.
    REQUIRE (order.size() >= 5);
    CHECK (slice (order, order.size() - 5, 5) == std::vector<juce::String> { "Global Bypass", "Analyzer", "Gain Scale", "Output", "UI Scale" });
    CHECK (focusProblems (host).empty());

    SECTION ("a control Tab can't reach is a problem")
    {
        named<juce::Slider> (host, "Gain Scale").setWantsKeyboardFocus (false);
        CHECK (focusProblems (host) == std::vector<juce::String> { "Gain Scale reached 0 times" });
    }
}

TEST_CASE ("The output popover takes focus to Output Gain as it opens from the keyboard, and Tab walks its controls in order")
{
    EveryControl host;
    const auto closed = focusOrder (*host.editor);
    host.openCallOut ("Output");
    auto* popover = harness::findChild<staple::Popover> (*host.editor, [] (staple::Popover& p) { return p.isOpen(); });
    REQUIRE (popover != nullptr);
    CHECK (named<juce::Slider> (host, "Output Gain").hasKeyboardFocus (false));
    CHECK (focusOrder (*popover) == std::vector<juce::String> { "Output Gain", "Pan Mode", "Output Pan", "Phase Invert", "Auto Gain", "Output Meter" });
    // The rest of the editor is walked as before, with the popover's controls among it.
    checkWalkedWith (*host.editor, *popover, closed);
    host.closeCallOut();
    CHECK_FALSE (popover->isOpen());
    CHECK (named<juce::Button> (host, "Output").hasKeyboardFocus (false));
}

TEST_CASE ("Tab from a popover's last control leaves it for the rest of the editor; it doesn't keep Tab inside")
{
    EveryControl host;
    const juce::String opener = GENERATE ("Output", "Analyzer");
    CAPTURE (opener);
    host.openCallOut (opener);
    auto* popover = harness::findChild<staple::Popover> (*host.editor, [] (staple::Popover& p) { return p.isOpen(); });
    REQUIRE (popover != nullptr);
    const auto inside = juce::KeyboardFocusTraverser().getAllComponents (popover);
    REQUIRE_FALSE (inside.empty());
    inside.back()->grabKeyboardFocus();
    host.press (juce::KeyPress (juce::KeyPress::tabKey));
    auto* focused = juce::Component::getCurrentlyFocusedComponent();
    REQUIRE (focused != nullptr);
    CHECK_FALSE (popover->isParentOf (focused));
}

TEST_CASE ("The Analyzer popover takes focus to Pre as it opens from the keyboard, and Tab walks its controls in order")
{
    EveryControl host;
    const auto closed = focusOrder (*host.editor);
    host.openCallOut ("Analyzer");
    auto* popover = harness::findChild<staple::Popover> (*host.editor, [] (staple::Popover& p) { return p.isOpen(); });
    REQUIRE (popover != nullptr);
    CHECK (named<juce::Button> (host, "Analyzer Pre-EQ").hasKeyboardFocus (false));
    CHECK (focusOrder (*popover)
           == std::vector<juce::String> { "Analyzer Pre-EQ", "Analyzer Post-EQ", "Analyzer Sidechain", "Analyzer Range", "Analyzer Resolution",
                                          "Analyzer Speed", "Analyzer Tilt", "Peak Hold" });
    // The rest of the editor is walked as before, with the popover's controls among it.
    checkWalkedWith (*host.editor, *popover, closed);
    for (const juce::String name : { "Analyzer Pre-EQ", "Analyzer Range", "Peak Hold" })
        CHECK (std::find (closed.begin(), closed.end(), name) == closed.end());
    host.closeCallOut();
    CHECK_FALSE (popover->isOpen());
    CHECK (named<juce::Button> (host, "Analyzer").hasKeyboardFocus (false));
    // And it opens again.
    host.openCallOut ("Analyzer");
    CHECK (popover->isOpen());
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
    CHECK (harness::findChild<eq1::BandPanel> (*host.editor)->shownSlot() == 2);
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

TEST_CASE ("Clicking Undo, Redo, A/B Compare, Copy or Presets leaves keyboard focus where it was, so Delete still deletes")
{
    EveryControl host;
    host.addBand (2, 200.0f, 0.0f);
    undoAndRedoEnabled (host);
    host.display.grabKeyboardFocus();
    const auto button = [&host] (const juce::String& name) {
        return host.findAll<juce::Button> ([&name] (juce::Button& b) { return b.getName() == name; }).front();
    };
    // A click runs a button's onClick, and takes keyboard focus only if the button lets it (a real
    // click, through the OS window, can't be made here). Presets opens the browser, whose search takes
    // focus, then closes it, giving focus back.
    for (const juce::String name : { "Undo", "Redo", "A/B Compare", "A/B Compare", "Copy", "Presets", "Presets" })
    {
        CAPTURE (name);
        auto* clicked = button (name);
        CHECK (clicked->getWantsKeyboardFocus());
        CHECK_FALSE (clicked->getMouseClickGrabsKeyboardFocus());
        clicked->onClick();
    }
    CHECK (host.display.hasKeyboardFocus (false));
    host.click (host.at (200.0));
    host.press (juce::KeyPress (juce::KeyPress::deleteKey));
    CHECK (host.value (2, "in_use") == 0.0f);
}

TEST_CASE ("A click on the panel's and footer's controls, or on a popover's opener, leaves keyboard focus where it was, so Delete still deletes")
{
    EveryControl host;
    host.addBand (2, 200.0f, 0.0f);
    host.settle();
    host.display.grabKeyboardFocus();
    host.click (host.at (200.0));
    host.settle();
    const auto titled = [&host] (const juce::String& title) -> juce::Component& {
        auto found = host.findAll<juce::Component> ([&title] (juce::Component& c) { return c.getTitle() == title && c.isShowing(); });
        REQUIRE_FALSE (found.empty());
        return *found.front();
    };
    // A click, as the editor sees it: the press hides the focus ring, then the control's own click. (A
    // real click, through the OS window, can't be made here, so a control taking focus on a click is
    // checked by its flag.)
    const auto click = [&] (const juce::String& title) {
        CAPTURE (title);
        auto& control = titled (title);
        CHECK_FALSE (control.getMouseClickGrabsKeyboardFocus());
        dynamic_cast<staple::LookAndFeel&> (host.editor->getLookAndFeel()).showFocusRing (false);
        if (auto* button = dynamic_cast<juce::Button*> (&control); button != nullptr && button->onClick != nullptr)
            button->onClick();
        host.settle();
        CHECK (host.display.hasKeyboardFocus (true));
    };
    // An Icon Button, an Edge Selector and a Text Chip; the popovers' openers, opened and closed, and
    // a popover's own control.
    for (const juce::String title : { "Band 2 Solo", "Band 2 Shape", "Display Range", "Output", "Output", "Analyzer", "Analyzer Pre-EQ", "Analyzer" })
        click (title);
    host.press (juce::KeyPress (juce::KeyPress::deleteKey));
    CHECK (host.value (2, "in_use") == 0.0f);
}

namespace
{
const juce::KeyPress tab (juce::KeyPress::tabKey), space (juce::KeyPress::spaceKey), returnKey (juce::KeyPress::returnKey),
    escape (juce::KeyPress::escapeKey);

staple::LookAndFeel& stapleOf (OpenEditor& host)
{
    return dynamic_cast<staple::LookAndFeel&> (host.editor->getLookAndFeel());
}


juce::Button& buttonWithText (OpenEditor& host, const juce::String& text)
{
    return *host.findAll<juce::Button> ([&text] (juce::Button& b) { return b.getButtonText() == text; }).front();
}
} // namespace

TEST_CASE ("Every control Tab reaches has a focus ring, which Tab shows")
{
    EveryControl host;
    for (auto* component : juce::KeyboardFocusTraverser().getAllComponents (host.editor.get()))
    {
        CAPTURE (describe (*component));
        CHECK (component->hasFocusOutline());
    }
    host.display.grabKeyboardFocus();
    CHECK_FALSE (stapleOf (host).isFocusRingShown());
    CHECK (host.press (tab));
    CHECK (stapleOf (host).isFocusRingShown());
    CHECK (bandElement (host, 1).hasKeyboardFocus (false));
    host.press (withShift (tab));
    CHECK (host.display.hasKeyboardFocus (false));
}

TEST_CASE ("With nothing focused, Tab focuses the first control and Shift+Tab the last")
{
    EveryControl host;
    juce::Component::unfocusAllComponents();
    host.press (tab);
    CHECK (named<juce::Button> (host, "Previous Preset").hasKeyboardFocus (false));
    juce::Component::unfocusAllComponents();
    host.press (withShift (tab));
    CHECK (named<juce::Button> (host, "UI Scale").hasKeyboardFocus (false));
}

TEST_CASE ("Space or Return toggles a toggle and presses a button, each press one undo step")
{
    EveryControl host;
    auto& history = host.processor.editHistory();
    const int steps = history.undoSteps();
    auto& bypass = buttonWithText (host, "Bypass");
    bypass.grabKeyboardFocus();
    REQUIRE (bypass.hasKeyboardFocus (false));

    CHECK (host.press (space));
    CHECK (host.value (1, "bypass") == 1.0f);
    CHECK (host.press (returnKey));
    CHECK (host.value (1, "bypass") == 0.0f);
    CHECK (history.undoSteps() == steps + 2);

    auto& deleteButton = buttonWithText (host, "Delete");
    deleteButton.grabKeyboardFocus();
    CHECK (host.press (returnKey));
    CHECK (host.value (1, "in_use") == 0.0f);
}

TEST_CASE ("Detection Audition plays while Space is held on it")
{
    EveryControl host;
    auto& audition = buttonWithText (host, "Detection Audition");
    audition.grabKeyboardFocus();
    REQUIRE (audition.hasKeyboardFocus (false));
    host.hold (space);
    host.hold (space);
    CHECK (host.processor.detectionAuditionSlot() == 1);
    host.release();
    CHECK (host.processor.detectionAuditionSlot() == 0);
}

TEST_CASE ("Up and down step a ComboBox to the previous or next item; a held key is one undo step")
{
    EveryControl host;
    auto& history = host.processor.editHistory();
    auto& shape = named<juce::ComboBox> (host, "Shape");
    shape.grabKeyboardFocus();
    REQUIRE (shape.hasKeyboardFocus (false));
    REQUIRE (host.value (1, "shape") == 1.0f); // Low Shelf
    const int steps = history.undoSteps();

    CHECK (host.press (down));
    CHECK (host.value (1, "shape") == 2.0f);
    CHECK (host.press (up));
    CHECK (host.value (1, "shape") == 1.0f);
    CHECK (history.undoSteps() == steps + 2);
    for (int repeat = 0; repeat < 3; ++repeat)
        host.hold (down);
    host.release();
    CHECK (host.value (1, "shape") == 4.0f);
    CHECK (history.undoSteps() == steps + 3);
    CHECK (stapleOf (host).isFocusRingShown());
}

TEST_CASE ("The Preset browser's list takes Tab, moves with the arrow keys, loads with Return and closes with Escape")
{
    EveryControl host;
    auto& presets = named<juce::Button> (host, "Presets");
    presets.grabKeyboardFocus();
    host.press (returnKey);
    auto& browser = named<juce::Component> (host, "Preset Browser");
    REQUIRE (browser.isVisible());
    host.press (tab); // from the search to the list
    auto& list = named<juce::ListBox> (host, "Presets List");
    REQUIRE (list.hasKeyboardFocus (true));
    host.press (down); // the first folder's name
    host.press (down); // its first Preset
    CHECK (host.press (returnKey));
    CHECK (host.processor.loadedPresetName().isNotEmpty());
    CHECK (browser.isVisible());
    host.press (escape);
    CHECK_FALSE (browser.isVisible());
    CHECK (presets.hasKeyboardFocus (false));
}

TEST_CASE ("Space or Return on the Output Meter puts out its Clip Lights")
{
    EveryControl host;
    host.processor.prepareToPlay (48000.0, 512);
    for (const auto key : { space, returnKey })
    {
        juce::AudioBuffer<float> buffer (host.processor.getTotalNumInputChannels(), 512);
        juce::MidiBuffer midi;
        buffer.clear();
        buffer.setSample (0, 10, 2.0f);
        host.processor.processBlock (buffer, midi);
        for (int ch = 0; ch < host.processor.outputLevelChannels(); ++ch)
            host.processor.readOutputLevel (ch);
        REQUIRE (host.processor.isClipLit (0));

        named<juce::Component> (host, "Output Meter").grabKeyboardFocus();
        CHECK (host.press (key));
        CHECK_FALSE (host.processor.isClipLit (0));
    }
}
