#include "EditorHarness.h"
#include "BandPanel.h"
#include "FooterBar.h"
#include "staple/controls/Knob.h"
#include "staple/controls/KnobTooltip.h"

#include <catch2/catch_test_macros.hpp>

using harness::OpenEditor;

// Where Tooltips and Knob Readouts land in the open editor: centred below their control over its
// label, never over another control, and above only where there is no room below.

namespace
{

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
