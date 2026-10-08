#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace eq1::parameters
{

// Parameter IDs are stable: hosts store them in sessions and automation, so they never change.
// Each Band slot n (1 to 24) has band<n>_frequency, band<n>_gain, band<n>_q, band<n>_in_use and band<n>_bypass.
juce::String frequencyId (int slot);
juce::String gainId (int slot);
juce::String qId (int slot);
juce::String inUseId (int slot);
juce::String bypassId (int slot);

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

} // namespace eq1::parameters
