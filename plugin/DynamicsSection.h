#pragma once

#include "KeyboardSlider.h"
#include "LevelBallistics.h"
#include "staple/controls/IconButton.h"
#include "staple/controls/Knob.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <optional>

namespace eq1
{

class PluginProcessor;

// Threshold as a vertical fader over the Metered Band's Detection Level (HANDOFF.md §5.3): −60 to 0 dB
// up its travel, then one more step at the top for Auto Threshold, its own host parameter (ADR 0003).
// Dragged only, with no jump on a click; a double-click sets Auto. The Detection Level fills the track
// from the bottom up to the thumb position of the Threshold it equals, so Threshold is set against what
// the detector hears; it rises at once and falls at 20 dB/s, and never reaches Auto.
class ThresholdFader final : public KeyboardSlider, private juce::Timer
{
public:
    // The position at the top of its travel that is Auto.
    static constexpr double autoPosition = 3.0;
    static bool isAuto (double position) { return position > 0.05; }

    explicit ThresholdFader (PluginProcessor& processor);

    // Where the thumb's centre sits for a position, in the fader's coordinates.
    float thumbCentreY (double position);
    // How high the Detection Level fills the track: the y of its top, or the track's bottom when it is
    // below the fader's bottom. Above 0 dB it stops at 0 dB, short of Auto.
    float levelTopY (double levelDb);
    // The Detection Level as last painted, in dB.
    double levelShown() const { return level; }

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent&) override {}

private:
    void timerCallback() override;
    double valueDraggedBy (double from, float pixels, bool fine) override;
    juce::Rectangle<float> track() const;

    PluginProcessor& processor;
    LevelBallistics ballistics;
    int meteredSlot = 0;
    double lastRead = 0.0; // seconds, when the Detection Level was last read
    double level = 0.0;    // dB, as last painted

};

// The Band panel's dynamics section, between Gain and Q (HANDOFF.md §5.3): the Threshold fader at its
// left, and a column with the Detection Source toggle (External lit), the held Detection Audition, the
// Band/Free Detection Range switch, and the Attack and Release knobs with Auto at their centres. Each
// click or drag is one undo step. Detection Audition plays the Band's detection signal while it is held,
// by the mouse or by Space, and lets go on its release, a Band change or the editor closing.
class DynamicsSection final : public juce::Component
{
public:
    explicit DynamicsSection (PluginProcessor& processor);
    ~DynamicsSection() override;

    // The Band Slot to edit, or 0 for none.
    void show (int slot);
    void setBandColour (juce::Colour colour);
    void releaseAudition();

    // The Detection Range switch: a bell and "Band", or ↔ and "Free".
    class DetectionRangeButton;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void showThreshold();
    void storeThreshold();
    void showChoices();

    PluginProcessor& processor;
    int slot = 0;
    ThresholdFader threshold;
    staple::IconButton detectionSource { "Detection Source", staple::Icon::detectionSource },
        audition { "Detection Audition", staple::Icon::headphones };
    std::unique_ptr<DetectionRangeButton> detectionRange;
    staple::Knob attack, release;
    juce::Label attackLabel, releaseLabel;
    bool thresholdDragging = false;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> attackAttachment, releaseAttachment;
    std::unique_ptr<juce::ParameterAttachment> thresholdAttachment, thresholdAutoAttachment, detectionSourceAttachment, detectionRangeAttachment;
};

} // namespace eq1
