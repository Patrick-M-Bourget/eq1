#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace eq1
{

// The settings a Preset holds, as each side of A/B Compare does: every host parameter that affects
// the sound, which is all of them but Global Bypass. Message thread only.

bool isPresetSetting (const juce::RangedAudioParameter& parameter);

// The settings a Preset holds, as a tree of the given type, with a PARAM child for each, holding
// its id and plain value as the saved state does.
juce::ValueTree capturePresetSettings (juce::AudioProcessorValueTreeState& parameters, const juce::Identifier& type);

// Sets the settings a Preset holds to those in tree, inside gestures. One the tree doesn't hold
// goes to its default, so the same tree always sounds the same; the other parameters are left alone.
void applyPresetSettings (juce::AudioProcessorValueTreeState& parameters, const juce::ValueTree& tree);

// The settings applyPresetSettings() puts on the parameters from preset, as capturePresetSettings()
// gives them: every setting a Preset holds, those preset leaves out at their defaults.
juce::ValueTree presetSettingsAsLoaded (juce::AudioProcessorValueTreeState& parameters, const juce::ValueTree& preset, const juce::Identifier& type);

// Whether the parameters hold the settings applyPresetSettings() puts on them from tree.
bool holdsPresetSettings (juce::AudioProcessorValueTreeState& parameters, const juce::ValueTree& tree);

} // namespace eq1
