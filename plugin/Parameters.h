#pragma once

#include "eq1/Settings.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace eq1::parameters
{

// Parameter IDs are stable: hosts store them in sessions and automation, so they never change.
// Each Band slot n (1 to 24) has band<n>_frequency, band<n>_gain, band<n>_q, band<n>_in_use, band<n>_bypass,
// band<n>_shape, band<n>_slope, band<n>_brickwall, band<n>_placement, and for dynamics
// band<n>_dynamic_range, band<n>_threshold, band<n>_threshold_auto, band<n>_attack, band<n>_release and
// band<n>_dynamics_bypass.
juce::String frequencyId (int slot);
juce::String gainId (int slot);
juce::String qId (int slot);
juce::String inUseId (int slot);
juce::String bypassId (int slot);
juce::String shapeId (int slot);
juce::String slopeId (int slot);
juce::String brickwallId (int slot);
juce::String placementId (int slot);
juce::String dynamicRangeId (int slot);
juce::String thresholdId (int slot);
juce::String thresholdAutoId (int slot);
juce::String attackId (int slot);
juce::String releaseId (int slot);
juce::String dynamicsBypassId (int slot);

// The Shape choices, in the order of the Shape enum: Pro-Q 4's order, frozen by ADR 0003.
const juce::StringArray& shapeNames();

// The Stereo Placement choices, in the order of the StereoPlacement enum. Like Shape's, the order is
// frozen at release: hosts store the choice as a normalised value.
const juce::StringArray& placementNames();

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// The host parameters of one Band slot, readable from any thread.
struct SlotValues
{
    std::atomic<float>* frequency;
    std::atomic<float>* gain;
    std::atomic<float>* q;
    std::atomic<float>* inUse;
    std::atomic<float>* bypass;
    std::atomic<float>* shape;
    std::atomic<float>* slope;
    std::atomic<float>* brickwall;
    std::atomic<float>* placement;
    std::atomic<float>* dynamicRange;
    std::atomic<float>* threshold;
    std::atomic<float>* thresholdAuto;
    std::atomic<float>* attack;
    std::atomic<float>* release;
    std::atomic<float>* dynamicsBypass;

    static SlotValues of (juce::AudioProcessorValueTreeState& parameters, int slot);
    BandSettings read() const;
};

} // namespace eq1::parameters
