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
    const juce::SharedResourcePointer<Typefaces> typefaces;
    const std::scoped_lock held (typefaces->fontsLock);
    const auto key = std::pair { weight, size };
    if (const auto made = typefaces->fonts.find (key); made != typefaces->fonts.end())
        return made->second;
    const juce::Font made = juce::FontOptions {}.withTypeface (typefaces->weights[static_cast<size_t> (weight)]).withPointHeight (size).withFeatureEnabled ("tnum");
    return typefaces->fonts.emplace (key, made).first->second;
}

int textWidth (const juce::Font& font, const juce::String& text)
{
    return static_cast<int> (std::ceil (juce::GlyphArrangement::getStringWidth (font, text)));
}

} // namespace staple
