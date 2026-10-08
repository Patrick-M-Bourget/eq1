#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace eq1::parameters
{

// Parameter IDs are stable: hosts store them in sessions and automation, so they never change.
inline constexpr const char* band1Frequency = "band1_frequency";
inline constexpr const char* band1Gain = "band1_gain";
inline constexpr const char* band1Q = "band1_q";

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

} // namespace eq1::parameters
