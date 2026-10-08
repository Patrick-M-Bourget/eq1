#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace eq1::parameters
{

// Parameter IDs are stable: hosts store them in sessions and automation, so they never change.
// Each Band slot n (1 to 24) has band<n>_frequency, band<n>_gain, band<n>_q, band<n>_in_use, band<n>_bypass,
// band<n>_shape, band<n>_slope, band<n>_brickwall and band<n>_placement.
juce::String frequencyId (int slot);
juce::String gainId (int slot);
juce::String qId (int slot);
juce::String inUseId (int slot);
juce::String bypassId (int slot);
juce::String shapeId (int slot);
juce::String slopeId (int slot);
juce::String brickwallId (int slot);
juce::String placementId (int slot);

// The Shape choices, in the order of the Shape enum: Pro-Q 4's order, frozen by ADR 0003.
const juce::StringArray& shapeNames();

// The Stereo Placement choices, in the order of the StereoPlacement enum. Like Shape's, the order is
// frozen at release: hosts store the choice as a normalised value.
const juce::StringArray& placementNames();

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

} // namespace eq1::parameters
