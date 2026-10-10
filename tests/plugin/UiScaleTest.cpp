#include "EditorHarness.h"
#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <memory>

namespace
{

juce::ComboBox& uiScaleMenu (juce::AudioProcessorEditor& editor)
{
    auto* menu = harness::findChild<juce::ComboBox> (editor, [] (juce::ComboBox& c) { return c.getTitle() == "UI Scale"; });
    REQUIRE (menu != nullptr);
    return *menu;
}

void pickUiScale (juce::AudioProcessorEditor& editor, int percent)
{
    auto& menu = uiScaleMenu (editor);
    REQUIRE (menu.indexOfItemId (percent) >= 0);
    menu.setSelectedId (percent, juce::sendNotificationSync);
}

std::unique_ptr<juce::AudioProcessorEditor> openEditor (eq1::PluginProcessor& processor)
{
    return std::unique_ptr<juce::AudioProcessorEditor> (processor.createEditor());
}

} // namespace

TEST_CASE ("A new instance opens at 1200 x 760 and 100%, with a UI Scale menu of 75, 100, 125, 150 and 200%")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    const auto editor = openEditor (processor);
    CHECK (editor->getWidth() == 1200);
    CHECK (editor->getHeight() == 760);
    auto& menu = uiScaleMenu (*editor);
    CHECK (menu.getSelectedId() == 100);
    CHECK (menu.getNumItems() == 5);
    for (int percent : { 75, 100, 125, 150, 200 })
        CHECK (menu.getItemText (menu.indexOfItemId (percent)) == juce::String (percent) + "%");
}

TEST_CASE ("At each UI Scale the editor is its logical size times the scale, and so are its limits and what it draws")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    const auto editor = openEditor (processor);
    auto& menu = uiScaleMenu (*editor);
    const auto atHundred = editor->getLocalArea (&menu, menu.getLocalBounds());

    const int percent = GENERATE (75, 100, 125, 150, 200);
    CAPTURE (percent);
    pickUiScale (*editor, percent);
    const auto scaled = [percent] (int logical) { return juce::roundToInt (logical * percent / 100.0); };
    CHECK (editor->getWidth() == scaled (1200));
    CHECK (editor->getHeight() == scaled (760));
    const auto* constrainer = editor->getConstrainer();
    REQUIRE (constrainer != nullptr);
    // The toolbar needs 1120 logical pixels; 960 once the Staple reskin removes it (#77).
    CHECK (constrainer->getMinimumWidth() == scaled (1120));
    CHECK (constrainer->getMinimumHeight() == scaled (600));
    CHECK (constrainer->getMaximumWidth() == scaled (2560));
    CHECK (constrainer->getMaximumHeight() == scaled (1600));
    // A control sits where it did at 100%, scaled, and is that much larger.
    const auto drawn = editor->getLocalArea (&menu, menu.getLocalBounds());
    CHECK (drawn == atHundred.transformedBy (juce::AffineTransform::scale (percent / 100.0f)));
}
