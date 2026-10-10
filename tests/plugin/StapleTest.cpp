#include "EditorHarness.h"
#include "PluginProcessor.h"
#include "staple/Fonts.h"
#include "staple/Icons.h"
#include "staple/LookAndFeel.h"
#include "staple/Tokens.h"
#include "staple/controls/Overlay.h"

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
    int sliders = 0, combos = 0, labels = 0;
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
                CHECK (toggle->findColour (juce::ToggleButton::textColourId) == colour::text2);
            }
            else if (auto* button = dynamic_cast<juce::TextButton*> (child))
            {
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
    // The editor has each kind of stock widget, but ToggleButtons and TextButtons: its toggles and
    // buttons are all Staple's own (IconButtons, Edge selectors, the Preset browser's text actions),
    // and any stock one that comes back is checked above.
    CHECK (sliders > 0);
    CHECK (combos > 0);
    CHECK (labels > 0);
}

namespace
{
// The largest difference in alpha between two images, over the pixels a predicate keeps.
int largestAlphaDifference (const juce::Image& a, const juce::Image& b, const std::function<bool (juce::Point<float>)>& compared)
{
    int largest = 0;
    for (int y = 0; y < a.getHeight(); ++y)
        for (int x = 0; x < a.getWidth(); ++x)
            if (compared ({ x + 0.5f, y + 0.5f }))
                largest = std::max (largest, std::abs (a.getPixelAt (x, y).getAlpha() - b.getPixelAt (x, y).getAlpha()));
    return largest;
}
} // namespace

// A shadow that blurs renders an image on every paint; the kit's controls repaint at frame rate, so
// their shadows are drawSoftShadow's stacked shapes instead (HANDOFF.md §4). Outside the control, what
// the LookAndFeel draws is that shadow and nothing else.
TEST_CASE ("Staple's LookAndFeel draws a stock knob's and slider thumb's shadows without blur")
{
    juce::ScopedJuceInitialiser_GUI juce;
    staple::LookAndFeel lookAndFeel;
    juce::Slider slider;
    slider.setLookAndFeel (&lookAndFeel);
    slider.setRange (0.0, 1.0);

    SECTION ("A rotary slider's knob")
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        const auto drawn = harness::paintImage (100, 100, [&] (juce::Graphics& g) {
            lookAndFeel.drawRotarySlider (g, 0, 0, 100, 100, 0.0f, -2.5f, 2.5f, slider);
        });
        // drawRotarySlider insets the knob's face 10 px.
        const auto face = juce::Rectangle<float> (10.0f, 10.0f, 80.0f, 80.0f);
        const auto shadow = harness::paintImage (100, 100, [&] (juce::Graphics& g) {
            staple::drawSoftShadow (g, face, face.getWidth() / 2.0f, staple::tokens::shadow::knob);
        });
        CHECK (largestAlphaDifference (drawn, shadow, [&] (juce::Point<float> p) {
                   return p.getDistanceFrom (face.getCentre()) > face.getWidth() / 2.0f + 1.5f;
               }) <= 1);
    }

    SECTION ("A linear slider's thumb")
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        const auto drawn = harness::paintImage (100, 100, [&] (juce::Graphics& g) {
            lookAndFeel.drawLinearSlider (g, 0, 0, 100, 100, 50.0f, 0.0f, 100.0f, juce::Slider::LinearHorizontal, slider);
        });
        const float thumbSize = 2.0f * static_cast<float> (lookAndFeel.getSliderThumbRadius (slider));
        const auto thumb = juce::Rectangle<float> (thumbSize, thumbSize).withCentre ({ 50.0f, 50.0f });
        const auto shadow = harness::paintImage (100, 100, [&] (juce::Graphics& g) {
            staple::drawSoftShadow (g, thumb, thumbSize / 2.0f, staple::tokens::shadow::handle);
        });
        // Away from the thumb and from the 2 px track through its centre.
        CHECK (largestAlphaDifference (drawn, shadow, [&] (juce::Point<float> p) {
                   return p.getDistanceFrom (thumb.getCentre()) > thumbSize / 2.0f + 1.5f && std::abs (p.y - 50.0f) > 2.5f;
               }) <= 1);
    }
}
