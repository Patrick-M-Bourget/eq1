#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace eq1
{

class PluginProcessor;
class BandEditing;

// Exact values for one Band: Shape, Frequency, Gain, Q, Slope, Brickwall, Stereo Placement and
// Bypass, each attached to its host parameter. Gain shows only on Shapes that have it, Brickwall only
// on Cuts, and Stereo Placement only on stereo tracks.
class BandPanel final : public juce::Component, private juce::Timer
{
public:
    BandPanel (PluginProcessor& processor, BandEditing& editing);
    ~BandPanel() override;

    // The Band Slot to show, or 0 for none.
    void show (int slot);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateVisibility();

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    PluginProcessor& processor;
    BandEditing& editing;
    int slot = 0;

    juce::Label title;
    juce::ComboBox shape, placement;
    juce::Slider frequency, gain, q, slope;
    juce::Label frequencyLabel, gainLabel, qLabel, slopeLabel;
    juce::ToggleButton brickwall { "Brickwall" }, bypass { "Bypass" };
    juce::TextButton deleteButton { "Delete" };

    std::unique_ptr<ComboBoxAttachment> shapeAttachment, placementAttachment;
    std::unique_ptr<SliderAttachment> frequencyAttachment, gainAttachment, qAttachment, slopeAttachment;
    std::unique_ptr<ButtonAttachment> brickwallAttachment, bypassAttachment;
};

} // namespace eq1
