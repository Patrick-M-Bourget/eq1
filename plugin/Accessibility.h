#pragma once

#include "staple/Accessibility.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace eq1::accessibility
{

// A parameter's value as a screen reader says it, as the editor writes it (ValueText.h): "1.00 kHz",
// "+3.50 dB" and "−inf dB" (dB to the parameter's own decimals), "12 dB/oct", "100%", or the parameter's
// text when it has no such unit ("0.707"). Words such as "Auto" stay words. The parameter's own text,
// which hosts show, is unchanged.
juce::String spokenValue (const juce::RangedAudioParameter& parameter, float normalisedValue);

// The kit's handler (staple/Accessibility.h), for eq1's own components.
using staple::accessibility::handler;

} // namespace eq1::accessibility
