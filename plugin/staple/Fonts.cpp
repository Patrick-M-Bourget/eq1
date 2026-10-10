#include "Fonts.h"

#include "StapleFonts.h"

#include <cmath>

namespace staple
{

Typefaces::Typefaces()
    : weights { juce::Typeface::createSystemTypefaceFor (StapleFonts::manroperegular_ttf, StapleFonts::manroperegular_ttfSize),
                juce::Typeface::createSystemTypefaceFor (StapleFonts::manropemedium_ttf, StapleFonts::manropemedium_ttfSize),
                juce::Typeface::createSystemTypefaceFor (StapleFonts::manropesemibold_ttf, StapleFonts::manropesemibold_ttfSize),
                juce::Typeface::createSystemTypefaceFor (StapleFonts::manropebold_ttf, StapleFonts::manropebold_ttfSize) }
{
}

juce::Typeface::Ptr typeface (Weight weight)
{
    const juce::SharedResourcePointer<Typefaces> typefaces;
    return typefaces->weights[static_cast<size_t> (weight)];
}

juce::Font font (float size, Weight weight)
{
    return juce::FontOptions {}.withTypeface (typeface (weight)).withPointHeight (size).withFeatureEnabled ("tnum");
}

int textWidth (const juce::Font& font, const juce::String& text)
{
    return static_cast<int> (std::ceil (juce::GlyphArrangement::getStringWidth (font, text)));
}

} // namespace staple
