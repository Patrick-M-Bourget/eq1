#pragma once

#include "DetectionArc.h"
#include "KeyboardSlider.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>
#include <utility>

namespace eq1
{

class PluginProcessor;
class BandEditing;

// Exact values for one Band: Shape, Frequency, Gain, Q, Slope, Brickwall, Stereo Placement, Bypass
// and its dynamics (Dynamic Range, Threshold, Attack, Release, Dynamics Bypass, Detection Source,
// Detection Range and its Free limits), each attached to its host parameter. Gain and the dynamics
// show only on Shapes that have them, the Free limits only on a Free Detection Range, Brickwall only
// on Cuts, and Stereo Placement only on stereo tracks. Threshold's top position is Auto, which is its
// own host parameter (ADR 0003). Holding Detection Audition plays the Band's detection signal (Detection
// Audition) until it is released. The Band shown is the Metered Band while its Shape has dynamics, and
// its Detection Level is drawn around the Threshold knob.
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
    // Each control's accessible title, from its parameter's name ("Band 4 Gain"), and its spoken value.
    void describe();
    // Threshold and Auto Threshold, two host parameters, on one slider.
    void showThreshold();
    void storeThreshold();
    void releaseAudition();
    // The rotary controls, left to right, with their labels.
    std::array<std::pair<juce::Slider*, juce::Label*>, 10> rotaries();

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    PluginProcessor& processor;
    BandEditing& editing;
    int slot = 0;

    juce::Label title;
    juce::ComboBox shape, placement, detectionSource, detectionRange;
    KeyboardSlider frequency, gain, q, slope, dynamicRange, threshold, attack, release, detectionLow, detectionHigh;
    juce::Label frequencyLabel, gainLabel, qLabel, slopeLabel, dynamicRangeLabel, thresholdLabel, attackLabel, releaseLabel,
        detectionLowLabel, detectionHighLabel;
    DetectionArc detectionArc { processor, threshold };
    juce::ToggleButton brickwall { "Brickwall" }, bypass { "Bypass" }, dynamicsBypass { "Dynamics Bypass" };
    juce::TextButton deleteButton { "Delete" }, audition { "Detection Audition" };

    std::unique_ptr<ComboBoxAttachment> shapeAttachment, placementAttachment, detectionSourceAttachment, detectionRangeAttachment;
    std::unique_ptr<SliderAttachment> frequencyAttachment, gainAttachment, qAttachment, slopeAttachment, dynamicRangeAttachment,
        attackAttachment, releaseAttachment, detectionLowAttachment, detectionHighAttachment;
    std::unique_ptr<ButtonAttachment> brickwallAttachment, bypassAttachment, dynamicsBypassAttachment;
    std::unique_ptr<juce::ParameterAttachment> thresholdAttachment, thresholdAutoAttachment;
    bool thresholdDragging = false;
};

} // namespace eq1
