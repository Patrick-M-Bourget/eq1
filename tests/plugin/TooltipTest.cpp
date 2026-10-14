#include "EditorHarness.h"
#include "BandPanel.h"
#include "AnalyzerPopover.h"
#include "FooterBar.h"
#include "HeaderBar.h"
#include "OutputMeter.h"
#include "PresetBar.h"
#include "staple/controls/Knob.h"
#include "staple/controls/KnobTooltip.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <optional>

using harness::OpenEditor;
using Catch::Matchers::WithinAbs;

// Where Tooltips and Knob Readouts land in the open editor: centred below their control over its
// label, never over another control, and above only where there is no room below.

namespace
{

// The open editor, its Tooltips shown as JUCE shows them on a rest.
struct Tooltips : OpenEditor
{
    juce::TooltipWindow& window = *harness::findChild<juce::TooltipWindow> (*editor);

    // Where c is, in the editor's coordinates.
    juce::Rectangle<int> inEditor (juce::Component& c) { return editor->getLocalArea (&c, c.getLocalBounds()); }

    // The first showing T titled title (any T without one).
    template <typename T = juce::Component>
    T& find (const juce::String& title = {})
    {
        auto found = findAll<T> ([&title] (T& c) { return (title.isEmpty() || c.getTitle() == title) && c.isShowing(); });
        INFO ("Nothing showing is titled " << title);
        REQUIRE_FALSE (found.empty());
        return *found.front();
    }

    // Shows c's Tooltip with the pointer resting at at, in c's coordinates (its centre without one), as
    // JUCE's rest does: getTipFor, then displayTip. JUCE's own timer reads the machine's real mouse, so
    // a test can't rest on c. Returns where the Tooltip shows, in the editor's coordinates.
    juce::Rectangle<int> tooltipOf (juce::Component& c, std::optional<juce::Point<int>> at = {})
    {
        window.getTipFor (c); // empty while the test isn't the foreground process; the tip itself is c's
        auto* client = dynamic_cast<juce::TooltipClient*> (&c);
        REQUIRE (client != nullptr);
        const auto tip = client->getTooltip();
        REQUIRE (tip.isNotEmpty());
        window.displayTip (c.localPointToGlobal (at.value_or (c.getLocalBounds().getCentre())), tip);
        REQUIRE (window.isVisible());
        return inEditor (window);
    }

    // Every other showing button in parent.
    std::vector<juce::Rectangle<int>> neighboursOf (juce::Component& c, juce::Component& parent)
    {
        std::vector<juce::Rectangle<int>> found;
        std::function<void (juce::Component&)> visit = [&] (juce::Component& p) {
            for (auto* child : p.getChildren())
            {
                if (child != &c && child->isShowing() && dynamic_cast<juce::Button*> (child) != nullptr)
                    found.push_back (inEditor (*child));
                visit (*child);
            }
        };
        visit (parent);
        return found;
    }
};

// t lies centred below c (within 1 px of its centre unless kept inside the window).
void checkCentredBelow (juce::Rectangle<int> t, juce::Rectangle<int> c)
{
    CHECK (t.getY() >= c.getBottom());
    CHECK (t.getY() <= c.getBottom() + 8);
    CHECK (std::abs (t.getCentreX() - c.getCentreX()) <= 1);
}

// A Band selected, its panel open, and Dynamic, so the dynamics icons and section show.
struct DynamicBandSelected : OpenEditor
{
    DynamicBandSelected()
    {
        addBand (1, 1000.0f, 0.0f);
        addBand (2, 100.0f, 0.0f, 2.0f); // a Low Cut, with a Slope
        set (1, "dynamic_range", -6.0f);
        settle();
        click (at (100.0));
        settleAnimations (300);
    }

    // Where c is, in the editor's coordinates.
    juce::Rectangle<int> inEditor (juce::Component& c) { return editor->getLocalArea (&c, c.getLocalBounds()); }

    // The showing control titled title.
    template <typename T = juce::Component>
    T& control (const juce::String& title)
    {
        auto found = findAll<T> ([&title] (T& c) { return c.getTitle() == title && c.isShowing(); });
        INFO ("No control showing is titled " << title);
        REQUIRE (found.size() == 1);
        return *found.front();
    }

    // The showing label reading text.
    juce::Label& label (const juce::String& text)
    {
        auto found = findAll<juce::Label> ([&text] (juce::Label& l) { return l.getText() == text && l.isShowing(); });
        INFO ("No label showing reads " << text);
        REQUIRE (found.size() == 1);
        return *found.front();
    }

    // Hovers the knob and returns where its Knob Readout shows.
    juce::Rectangle<int> readoutOf (staple::Knob& knob)
    {
        const auto centre = knob.getLocalBounds().getCentre().toFloat();
        knob.mouseEnter (harness::mouseEvent (knob, centre, {}, centre));
        REQUIRE (knob.isTooltipShown());
        return inEditor (*knob.getKnobTooltip());
    }

    juce::Rectangle<int> footer() { return inEditor (*harness::findChild<eq1::FooterBar> (*editor)); }
};

} // namespace

TEST_CASE ("A Band panel knob's Knob Readout covers its label from the label's top, clear of the dynamics icons and the footer")
{
    DynamicBandSelected host;
    host.click (host.at (1000.0));
    host.settleAnimations (300);
    const juce::Rectangle<int> icons[] = { host.inEditor (host.control ("Band 1 Clear Dynamics")),
                                           host.inEditor (host.control ("Band 1 Dynamics Bypass")),
                                           host.inEditor (host.control ("Hide Band 1 dynamics")) };
    const std::pair<const char*, const char*> knobs[] = { { "Band 1 Frequency", "Frequency" }, { "Band 1 Gain", "Gain" }, { "Band 1 Q", "Q" },
                                                          { "Band 1 Attack", "A" }, { "Band 1 Release", "R" } };
    for (auto [title, labelText] : knobs)
    {
        CAPTURE (title);
        auto& knob = host.control<staple::Knob> (title);
        const auto readout = host.readoutOf (knob);
        const auto label = host.inEditor (host.label (labelText));
        CHECK (readout.getY() == label.getY());
        CHECK (readout.getBottom() >= label.getBottom());
        CHECK (readout.getCentreX() == host.inEditor (knob).getCentreX());
        for (const auto& icon : icons)
            CHECK_FALSE (readout.intersects (icon));
        CHECK_FALSE (readout.intersects (host.footer()));
    }
}

TEST_CASE ("The Slope's Knob Readout sits below the Slope button, clear of the Shape selector, and types a value there")
{
    DynamicBandSelected host;
    auto& slope = host.control<staple::Knob> ("Band 2 Slope");
    std::function<void()> typeIn;
    const auto list = dynamic_cast<eq1::BandPanel::SlopeButton&> (slope).list();
    for (juce::PopupMenu::MenuItemIterator it (list); it.next();)
        if (it.getItem().text == juce::String::fromUTF8 ("Type a value\xe2\x80\xa6"))
            typeIn = it.getItem().action;
    REQUIRE (typeIn != nullptr);
    typeIn();
    REQUIRE (slope.isTooltipShown());
    auto& tooltip = *slope.getKnobTooltip();
    REQUIRE (tooltip.isEditing());
    const auto readout = host.inEditor (tooltip);
    CHECK (readout.getY() >= host.inEditor (slope).getBottom());
    CHECK_FALSE (readout.intersects (host.inEditor (host.control ("Band 2 Shape"))));
    CHECK_FALSE (readout.intersects (host.footer()));
    CHECK (readout.contains (host.inEditor (*tooltip.getEditor())));
}

TEST_CASE ("A header button's Tooltip sits centred below it, clear of the buttons beside it")
{
    Tooltips host;
    auto& header = *harness::findChild<eq1::HeaderBar> (*host.editor);
    for (auto* button : { static_cast<juce::Component*> (&host.find<eq1::CompareButton> ("A/B Compare")),
                          static_cast<juce::Component*> (host.findAll<juce::Button> ([] (juce::Button& b) { return b.getName() == "Copy"; }).front()) })
    {
        CAPTURE (button->getTitle());
        const auto tip = host.tooltipOf (*button);
        checkCentredBelow (tip, host.inEditor (*button));
        for (const auto& neighbour : host.neighboursOf (*button, header))
            CHECK_FALSE (tip.intersects (neighbour));
        host.window.hideTip();
    }
}

TEST_CASE ("The Preset name's Tooltip sits centred below the name, clear of the Preset arrows")
{
    Tooltips host;
    host.processor.loadPreset (host.processor.presetState(), "Vocal - Presence Lift");
    host.settle (300);
    auto& bar = *harness::findChild<eq1::PresetBar> (*host.editor);
    auto& name = host.find<eq1::PresetNameButton>();
    const auto tip = host.tooltipOf (name);
    checkCentredBelow (tip, host.inEditor (name));
    for (const auto& neighbour : host.neighboursOf (name, bar))
        CHECK_FALSE (tip.intersects (neighbour));
}

TEST_CASE ("A footer control's Tooltip sits centred above it, inside the window")
{
    Tooltips host;
    auto& footer = *harness::findChild<eq1::FooterBar> (*host.editor);
    const juce::Component* controls[] = { &host.find ("Global Bypass"), &host.find<eq1::AnalyzerButton>(), &host.find<eq1::GainScaleReadout>(),
                                          &host.find<eq1::OutputReadout>(), &host.find<eq1::UiScaleMenu>() };
    for (auto* constControl : controls)
    {
        auto& control = const_cast<juce::Component&> (*constControl);
        CAPTURE (control.getTitle());
        const auto tip = host.tooltipOf (control);
        const auto at = host.inEditor (control);
        CHECK (tip.getBottom() <= at.getY());
        CHECK (host.editor->getLocalBounds().contains (tip));
        if (tip.getX() > 0 && tip.getRight() < host.editor->getWidth())
            CHECK (std::abs (tip.getCentreX() - at.getCentreX()) <= 1);
        for (const auto& neighbour : host.neighboursOf (control, footer))
            CHECK_FALSE (tip.intersects (neighbour));
        host.window.hideTip();
    }
}

TEST_CASE ("A Tooltip in the Analyzer popover sits centred below its toggle")
{
    Tooltips host;
    host.find<eq1::AnalyzerButton>().onClick();
    host.settleAnimations (300);
    auto& pre = host.find ("Analyzer Pre-EQ");
    checkCentredBelow (host.tooltipOf (pre), host.inEditor (pre));
}

TEST_CASE ("The Output Meter's Tooltip sits left of the meter, centred on the pointer's height")
{
    Tooltips host;
    auto& meter = host.find<eq1::OutputMeter>();
    const auto at = juce::Point<int> (meter.getWidth() / 2, meter.getHeight() / 3);
    const auto tip = host.tooltipOf (meter, at);
    const auto pointer = host.editor->getLocalPoint (&meter, at);
    CHECK (tip.getRight() <= host.inEditor (meter).getX());
    CHECK (std::abs (tip.getCentreY() - pointer.y) <= 1);
    CHECK (host.editor->getLocalBounds().contains (tip));
}

TEST_CASE ("A Tooltip near the window's edge stays inside it")
{
    Tooltips host;
    auto& meter = host.find<eq1::OutputMeter>();
    const auto tip = host.tooltipOf (meter, juce::Point<int> (meter.getWidth() / 2, meter.getHeight() - 2));
    CHECK (host.editor->getLocalBounds().contains (tip));
}

TEST_CASE ("At 150% UI Scale a header button's Tooltip still sits centred below it")
{
    Tooltips host;
    host.find<eq1::UiScaleMenu>().pick (150);
    host.settle();
    REQUIRE (host.editor->getWidth() == 1800);
    auto& compare = host.find<eq1::CompareButton> ("A/B Compare");
    const auto tip = host.tooltipOf (compare);
    const auto at = host.inEditor (compare);
    CHECK (tip.getY() >= at.getBottom());
    CHECK (tip.getY() <= at.getBottom() + 12);
    CHECK (std::abs (tip.getCentreX() - at.getCentreX()) <= 2);
}
