#pragma once

#include "KeyboardSlider.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace eq1
{

class PluginProcessor;

// The whole-plugin controls in one row: Gain Scale, Auto Gain, Output Gain, Output Pan with its Pan
// Mode, Phase Invert and Global Bypass, each attached to its host parameter. Output Pan and Pan Mode
// are disabled on mono tracks, where they have no effect.
class OutputPanel final : public juce::Component, private juce::Timer
{
public:
    explicit OutputPanel (PluginProcessor& processor);

    void resized() override;

private:
    void timerCallback() override;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    PluginProcessor& processor;

    juce::Label gainScaleLabel, outputGainLabel, outputPanLabel;
    KeyboardSlider gainScale, outputGain, outputPan;
    juce::ComboBox panMode;
    juce::ToggleButton autoGain { "Auto Gain" }, phaseInvert { "Phase Invert" }, globalBypass { "Global Bypass" };

    std::unique_ptr<SliderAttachment> gainScaleAttachment, outputGainAttachment, outputPanAttachment;
    std::unique_ptr<ComboBoxAttachment> panModeAttachment;
    std::unique_ptr<ButtonAttachment> autoGainAttachment, phaseInvertAttachment, globalBypassAttachment;
};

} // namespace eq1
