#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace eq1
{

class PluginProcessor;
class BandEditing;

// Exact values for one Band: Shape, Frequency, Gain, Q, Slope, Brickwall, Stereo Placement, Bypass
// and its dynamics (Dynamic Range, Threshold, Attack, Release, Dynamics Bypass), each attached to its
// host parameter. Gain and the dynamics show only on Shapes that have them, Brickwall only on Cuts,
// and Stereo Placement only on stereo tracks. Threshold's top position is Auto, which is its own host
// parameter (ADR 0003).
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
    // Threshold and Auto Threshold, two host parameters, on one slider.
    void showThreshold();
    void storeThreshold();

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    PluginProcessor& processor;
    BandEditing& editing;
    int slot = 0;

    juce::Label title;
    juce::ComboBox shape, placement;
    juce::Slider frequency, gain, q, slope, dynamicRange, threshold, attack, release;
    juce::Label frequencyLabel, gainLabel, qLabel, slopeLabel, dynamicRangeLabel, thresholdLabel, attackLabel, releaseLabel;
    juce::ToggleButton brickwall { "Brickwall" }, bypass { "Bypass" }, dynamicsBypass { "Dynamics Bypass" };
    juce::TextButton deleteButton { "Delete" };

    std::unique_ptr<ComboBoxAttachment> shapeAttachment, placementAttachment;
    std::unique_ptr<SliderAttachment> frequencyAttachment, gainAttachment, qAttachment, slopeAttachment, dynamicRangeAttachment,
        attackAttachment, releaseAttachment;
    std::unique_ptr<ButtonAttachment> brickwallAttachment, bypassAttachment, dynamicsBypassAttachment;
    std::unique_ptr<juce::ParameterAttachment> thresholdAttachment, thresholdAutoAttachment;
    bool thresholdDragging = false;
};

} // namespace eq1
