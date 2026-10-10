#pragma once

#include "staple/Accessibility.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace eq1::accessibility
{

// A parameter's value as a screen reader says it: its text with its unit, a positive dB value
// signed ("+3.50 dB", "-inf dB", "1000.0 Hz", "0.707"). Words such as "Auto" go without a unit.
// The parameter's own text, which hosts show, is unchanged.
juce::String spokenValue (const juce::RangedAudioParameter& parameter, float normalisedValue);

// The kit's handler (staple/Accessibility.h), for eq1's own components.
using staple::accessibility::handler;

} // namespace eq1::accessibility
