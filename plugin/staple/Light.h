#pragma once

#include "Tokens.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace staple
{

// Hover and press light a fill up (brightness x 1.18, pressed x 1.3): an opaque colour brightens, a
// translucent one (fill1, fill2, stateOffBg) grows more opaque, which is what brightening it over the
// dark background looks like.
inline juce::Colour lit (juce::Colour colour, bool highlighted, bool down)
{
    const float factor = down ? tokens::motion::pressedBrightness : highlighted ? tokens::motion::hoverBrightness : 1.0f;
    return colour.isOpaque() ? colour.withMultipliedBrightness (factor) : colour.withMultipliedAlpha (factor);
}

// The alpha a control is drawn at: whole, or dimmed to 35 % while disabled.
inline float enabledAlpha (const juce::Component& component)
{
    return component.isEnabled() ? 1.0f : tokens::motion::disabledAlpha;
}

} // namespace staple
