#pragma once

#include <juce_graphics/juce_graphics.h>

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

// Manrope at a size in px (the handoff's fs tokens), with tabular figures, so numbers keep their width
// as they change.
juce::Font font (float size, Weight weight = Weight::regular);

// The bundled typeface itself, for the LookAndFeel's default.
juce::Typeface::Ptr typeface (Weight weight);

} // namespace staple
