#pragma once

#include "KeyboardSlider.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace eq1
{

class PluginProcessor;

// The whole-plugin output controls the footer's Output button opens (FooterBar): Output Gain, Output Pan
// with its Pan Mode, Auto Gain and Phase Invert, each attached to its host parameter. Output Pan and
// Pan Mode are disabled on mono tracks, where they have no effect.
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

    juce::Label outputGainLabel, outputPanLabel;
    KeyboardSlider outputGain, outputPan;
    juce::ComboBox panMode;
    juce::ToggleButton autoGain { "Auto Gain" }, phaseInvert { "Phase Invert" };

    std::unique_ptr<SliderAttachment> outputGainAttachment, outputPanAttachment;
    std::unique_ptr<ComboBoxAttachment> panModeAttachment;
    std::unique_ptr<ButtonAttachment> autoGainAttachment, phaseInvertAttachment;
};

} // namespace eq1
