#pragma once

#include "Parameters.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

namespace eq1
{

// The settings a Preset holds, as each side of A/B Compare does: every host parameter that affects
// the sound, which is all of them but Global Bypass. Message thread only.

bool isPresetSetting (const juce::RangedAudioParameter& parameter);

// A saved number as it restores, for a parameter's value and for the other numbers a session holds
// alike: itself within minimum to maximum, the nearer end when it is finite outside, and defaultValue
// when it isn't a finite number (nan, inf). Read in double, so a value a float can't hold ("1e39") is
// still a finite one to clamp. Nothing a file holds reaches the filter design or the editor unchecked.
double restoredNumber (const juce::var& saved, double defaultValue, double minimum, double maximum);

// The plain value a saved one restores to, on a session restore and on a Preset load alike:
// restoredNumber() within the parameter's range, with the parameter's default for one that isn't a
// finite number (nan, inf). Text that isn't a number reads as 0, as juce::var converts it.
float restoredPlainValue (const juce::RangedAudioParameter& parameter, const juce::var& saved);

// Puts every PARAM child of tree that names a parameter through restoredPlainValue(), in place, leaving
// a value that restores as itself untouched. For a saved state about to replace the parameters' state.
void sanitiseSavedSettings (juce::AudioProcessorValueTreeState& parameters, juce::ValueTree& tree);

// The settings a Preset holds, as a tree of the given type, with a PARAM child for each, holding
// its id and plain value as the saved state does.
juce::ValueTree capturePresetSettings (juce::AudioProcessorValueTreeState& parameters, const juce::Identifier& type);

// Sets the settings a Preset holds to those in tree, inside gestures. One the tree doesn't hold
// goes to its default, so the same tree always sounds the same; the other parameters are left alone.
void applyPresetSettings (juce::AudioProcessorValueTreeState& parameters, const juce::ValueTree& tree);

// The settings applyPresetSettings() puts on the parameters from preset, as capturePresetSettings()
// gives them: every setting a Preset holds, those preset leaves out at their defaults.
juce::ValueTree presetSettingsAsLoaded (juce::AudioProcessorValueTreeState& parameters, const juce::ValueTree& preset, const juce::Identifier& type);

// The settings applyPresetSettings() puts on the parameters from tree, each with its parameter and
// normalised value, in parameter order: a tree indexed once, so the Modified check compares them
// with the parameters each tick (parametersHold) without searching the tree for every setting.
using PresetSettingValues = std::vector<parameters::NewValue>;
PresetSettingValues presetSettingValues (juce::AudioProcessorValueTreeState& parameters, const juce::ValueTree& tree);
// Whether every parameter in values holds its value there. Linear in the settings.
bool parametersHold (const PresetSettingValues& values);

} // namespace eq1
