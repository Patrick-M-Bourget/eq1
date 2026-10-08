#pragma once

#include "BandEditing.h"
#include "BandPanel.h"
#include "EqDisplay.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace eq1
{

class PluginProcessor;

// The native editor (ADR 0002): the EQ display above, the selected Band's panel below. Resizable;
// everything is drawn as vectors, so it stays sharp at any display scale.
class PluginEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor& processor);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    // Follows a display range restored with the plugin's state.
    void timerCallback() override;

    PluginProcessor& eqProcessor;
    BandEditing editing;
    EqDisplay display;
    BandPanel panel;
    juce::ComboBox displayRange;

    // The Analyzer's controls, above the display.
    juce::ToggleButton showPreEq { "Pre" }, showPostEq { "Post" };
    juce::ComboBox analyzerRange, analyzerSpeed, analyzerResolution;
    juce::Label analyzerTiltLabel;
    juce::Slider analyzerTilt;
    void showAnalyzerSettings();
    void storeAnalyzerSettings();
};

} // namespace eq1
