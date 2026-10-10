#include "BandPanel.h"
#include "EditorHarness.h"
#include "FooterBar.h"
#include "HeaderBar.h"
#include "OutputMeter.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <memory>

using harness::findChild;

namespace
{

// The editor's areas, in the editor's own pixels.
struct Areas
{
    juce::Rectangle<int> header, display, meter, footer;
    bool meterShown;
};

Areas areasOf (juce::AudioProcessorEditor& editor)
{
    const auto in = [&editor] (juce::Component* c) {
        REQUIRE (c != nullptr);
        return editor.getLocalArea (c->getParentComponent(), c->getBounds());
    };
    auto* meter = findChild<eq1::OutputMeter> (editor);
    return { in (findChild<eq1::HeaderBar> (editor)), in (findChild<eq1::EqDisplay> (editor)), in (meter),
             in (findChild<eq1::FooterBar> (editor)), meter->isVisible() };
}

} // namespace

TEST_CASE ("The window is a 52 px header, the display with the meter's 40 px rail, and a 44 px footer, and only the display resizes")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    REQUIRE (editor->getWidth() == 1200);
    REQUIRE (editor->getHeight() == 760);

    // 14 px padding and 12 px gaps; the display runs flush to the window's left edge.
    auto areas = areasOf (*editor);
    CHECK (areas.header == juce::Rectangle<int> (14, 14, 1172, 52));
    CHECK (areas.display == juce::Rectangle<int> (0, 78, 1134, 612));
    CHECK (areas.meterShown);
    CHECK (areas.meter == juce::Rectangle<int> (1146, 78, 40, 612));
    CHECK (areas.footer == juce::Rectangle<int> (14, 702, 1172, 44));

    // Larger: the header and footer keep their height and the rail its width.
    editor->setSize (1500, 900);
    areas = areasOf (*editor);
    CHECK (areas.header == juce::Rectangle<int> (14, 14, 1472, 52));
    CHECK (areas.display == juce::Rectangle<int> (0, 78, 1434, 752));
    CHECK (areas.meter == juce::Rectangle<int> (1446, 78, 40, 752));
    CHECK (areas.footer == juce::Rectangle<int> (14, 842, 1472, 44));

    // With the meter hidden the display takes its rail and gap.
    editor->setSize (1200, 760);
    processor.setOutputMeterShown (false);
    editor->resized();
    areas = areasOf (*editor);
    CHECK_FALSE (areas.meterShown);
    CHECK (areas.display == juce::Rectangle<int> (0, 78, 1186, 612));
}

TEST_CASE ("The Band panel floats over the display's bottom, centred, 36 px above it, at every size and UI Scale, and takes its own clicks")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    auto* display = findChild<eq1::EqDisplay> (*editor);
    auto* panel = findChild<eq1::BandPanel> (*editor);
    auto* scaleMenu = findChild<juce::ComboBox> (*editor, [] (juce::ComboBox& c) { return c.getTitle() == "UI Scale"; });
    REQUIRE (display != nullptr);
    REQUIRE (panel != nullptr);
    REQUIRE (scaleMenu != nullptr);
    editor->setVisible (true); // as a host shows it, so it takes clicks

    const int percent = GENERATE (75, 100, 200);
    CAPTURE (percent);
    scaleMenu->setSelectedId (percent, juce::sendNotificationSync);
    const auto scaled = [percent] (int logical) { return juce::roundToInt (logical * percent / 100.0); };
    for (const auto size : { juce::Point<int> (1200, 760), juce::Point<int> (960, 600), juce::Point<int> (1700, 1000) })
    {
        CAPTURE (size.x, size.y);
        editor->setSize (scaled (size.x), scaled (size.y));
        // In the display's own pixels: it and the panel share a parent.
        CHECK (panel->getBottom() == display->getBottom() - 36);
        CHECK (panel->getX() - display->getX() == display->getRight() - panel->getRight());
        CHECK (panel->getWidth() == display->getWidth() - 2 * 26);
        CHECK (panel->getHeight() == 170);

        // A click anywhere on it lands on it, never on the display under it.
        const auto inEditor = editor->getLocalArea (panel->getParentComponent(), panel->getBounds());
        for (const auto point : { inEditor.getCentre(), inEditor.getTopLeft().translated (2, 2), inEditor.getBottomRight().translated (-2, -2) })
        {
            CAPTURE (point.x, point.y);
            auto* hit = editor->getComponentAt (point);
            REQUIRE (hit != nullptr);
            CHECK ((hit == panel || panel->isParentOf (hit)));
        }
    }
}
