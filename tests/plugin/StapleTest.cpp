#include "PluginProcessor.h"
#include "staple/Fonts.h"
#include "staple/Icons.h"
#include "staple/LookAndFeel.h"
#include "staple/Tokens.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <functional>
#include <memory>

using Catch::Matchers::WithinAbs;

TEST_CASE ("Staple's font is Manrope, with digits of equal width so numbers don't jitter")
{
    juce::ScopedJuceInitialiser_GUI juce;
    const auto size = GENERATE (staple::tokens::size::fs2, staple::tokens::size::fs5);
    const auto weight = GENERATE (staple::Weight::regular, staple::Weight::semiBold);
    const auto font = staple::font (size, weight);
    CHECK (font.getTypefacePtr()->getName() == "Manrope");

    const float zero = juce::GlyphArrangement::getStringWidth (font, "0");
    CHECK (zero > 0.0f);
    for (const char* digit : { "1", "2", "3", "4", "5", "6", "7", "8", "9" })
    {
        CAPTURE (size, digit);
        CHECK_THAT (juce::GlyphArrangement::getStringWidth (font, digit), WithinAbs (zero, 1.0e-3));
    }
}

TEST_CASE ("Every icon the prototype uses parses to a path inside its grid")
{
    for (int i = 0; i < staple::numIcons; ++i)
    {
        const auto icon = static_cast<staple::Icon> (i);
        CAPTURE (i);
        const auto grid = staple::gridOf (icon);
        REQUIRE (! grid.isEmpty());
        CAPTURE (staple::nameOf (icon));
        const auto& path = staple::pathOf (icon);
        CHECK (! path.isEmpty());
        CHECK (! path.getBounds().isEmpty());
        CHECK (grid.contains (path.getBounds()));
    }
}

TEST_CASE ("The editor draws every stock widget with Staple's LookAndFeel, token colours and Manrope")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    auto* staple = dynamic_cast<staple::LookAndFeel*> (&editor->getLookAndFeel());
    REQUIRE (staple != nullptr);
    CHECK (staple->getTypefaceForFont (juce::FontOptions {})->getName() == "Manrope");
    CHECK (staple->getPopupMenuFont().getTypefacePtr()->getName() == "Manrope");
    CHECK (staple->findColour (juce::PopupMenu::backgroundColourId).withAlpha (1.0f) == staple::tokens::colour::menu);

    namespace colour = staple::tokens::colour;
    int sliders = 0, combos = 0, buttons = 0, toggles = 0, labels = 0;
    const auto isManrope = [] (const juce::Font& font) { return font.getTypefacePtr()->getName() == "Manrope"; };
    std::function<void (juce::Component&)> visit = [&] (juce::Component& component) {
        for (auto* child : component.getChildren())
        {
            CHECK (&child->getLookAndFeel() == staple);
            if (auto* slider = dynamic_cast<juce::Slider*> (child))
            {
                ++sliders;
                CHECK (slider->findColour (juce::Slider::textBoxTextColourId) == colour::text2);
            }
            else if (auto* combo = dynamic_cast<juce::ComboBox*> (child))
            {
                ++combos;
                CHECK (combo->findColour (juce::ComboBox::backgroundColourId) == colour::fill1);
                CHECK (isManrope (staple->getComboBoxFont (*combo)));
            }
            else if (auto* toggle = dynamic_cast<juce::ToggleButton*> (child))
            {
                ++toggles;
                CHECK (toggle->findColour (juce::ToggleButton::textColourId) == colour::text2);
            }
            else if (auto* button = dynamic_cast<juce::TextButton*> (child))
            {
                ++buttons;
                CHECK (button->findColour (juce::TextButton::textColourOffId) == colour::text2);
                CHECK (isManrope (staple->getTextButtonFont (*button, button->getHeight())));
            }
            else if (auto* label = dynamic_cast<juce::Label*> (child))
            {
                ++labels;
                CHECK (isManrope (staple->getLabelFont (*label)));
            }
            visit (*child);
        }
    };
    visit (*editor);
    // The editor has each kind of stock widget.
    CHECK (sliders > 0);
    CHECK (combos > 0);
    CHECK (buttons > 0);
    CHECK (toggles > 0);
    CHECK (labels > 0);
}
