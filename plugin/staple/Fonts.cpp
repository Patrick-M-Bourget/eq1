#include "Fonts.h"

#include "StapleFonts.h"

#include <array>

namespace staple
{

namespace
{
// Loaded from the plugin's binary data once and shared: a typeface stays registered with the system
// while something holds it.
struct Typefaces
{
    std::array<juce::Typeface::Ptr, 4> weights {
        juce::Typeface::createSystemTypefaceFor (StapleFonts::manroperegular_ttf, StapleFonts::manroperegular_ttfSize),
        juce::Typeface::createSystemTypefaceFor (StapleFonts::manropemedium_ttf, StapleFonts::manropemedium_ttfSize),
        juce::Typeface::createSystemTypefaceFor (StapleFonts::manropesemibold_ttf, StapleFonts::manropesemibold_ttfSize),
        juce::Typeface::createSystemTypefaceFor (StapleFonts::manropebold_ttf, StapleFonts::manropebold_ttfSize)
    };
};
} // namespace

juce::Typeface::Ptr typeface (Weight weight)
{
    const juce::SharedResourcePointer<Typefaces> typefaces;
    return typefaces->weights[static_cast<size_t> (weight)];
}

juce::Font font (float size, Weight weight)
{
    return juce::FontOptions {}.withTypeface (typeface (weight)).withPointHeight (size).withFeatureEnabled ("tnum");
}

} // namespace staple
