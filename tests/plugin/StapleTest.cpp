#include "staple/Fonts.h"
#include "staple/Icons.h"
#include "staple/Tokens.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

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
