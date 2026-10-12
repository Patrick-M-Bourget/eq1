#pragma once

#include <juce_graphics/juce_graphics.h>

#include <array>
#include <map>
#include <mutex>
#include <utility>

namespace staple
{

// Manrope's weights bundled with the plugin (assets/fonts/).
enum class Weight
{
    regular,  // 400
    medium,   // 500
    semiBold, // 600
    bold      // 700
};

// The bundled weights, loaded from the plugin's binary data while something holds them
// (juce::SharedResourcePointer<Typefaces>): a typeface stays registered with the system as long. And
// the fonts made from them, each made once per weight and size.
struct Typefaces
{
    Typefaces();
    std::array<juce::Typeface::Ptr, 4> weights;
    std::map<std::pair<Weight, float>, juce::Font> fonts;
    std::mutex fontsLock; // fonts are made on any thread
};

// Manrope at a size in px (the handoff's fs tokens), with tabular figures, so numbers keep their width
// as they change. Made once per weight and size, and copied from then on.
juce::Font font (float size, Weight weight = Weight::regular);

// The bundled typeface itself.
juce::Typeface::Ptr typeface (Weight weight);

// The width text takes in font, rounded up to whole pixels: the width to give a label or button that
// shows it whole.
int textWidth (const juce::Font& font, const juce::String& text);

} // namespace staple
