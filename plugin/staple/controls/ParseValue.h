#pragma once

#include <juce_core/juce_core.h>

#include <optional>

namespace staple
{

// A value typed into a control, in the units of a parameter whose label is unit ("Hz", "dB", "ms", "s",
// "%" or none): a number, with an optional sign and a comma or a point, then optionally a unit. "k" and
// "kHz" multiply by 1000; "s" typed for a parameter in ms, or "ms" for one in s, converts. Nothing when
// the text is anything else, so the caller can try the parameter's own text instead.
std::optional<double> parseValue (const juce::String& text, const juce::String& unit = {});

} // namespace staple
