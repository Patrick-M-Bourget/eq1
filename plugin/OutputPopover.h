#pragma once

#include "KeyboardSlider.h"
#include "staple/Icons.h"
#include "staple/controls/Knob.h"
#include "staple/controls/Popover.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <optional>

namespace eq1
{

class PluginProcessor;

// What the Output Pan slider reads beneath it: "Centre", else how far towards which side of its Pan
// Mode, in whole percent ("40 L", "20 S").
juce::String outputPanText (double pan, bool midSide);

// Output Pan (HANDOFF.md §5.7): a 6 px track with a centre tick, filled from the centre to the value,
// and a small thumb. A click or drag sets it, in whole percent, as one gesture; a double-click centres
// it; the left and right keys step it by 5 (1 with Shift).
class OutputPanSlider final : public KeyboardSlider
{
public:
    OutputPanSlider();

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    void setFromX (float x);
    std::optional<ScopedDragNotification> drag;
};

// One of the output popover's three toggles: an icon, or a letter (Auto Gain's "A"), on fill2 in text1
// while on, else in text3.
class OutputToggle final : public juce::Button
{
public:
    OutputToggle (const juce::String& title, std::optional<staple::Icon> icon, const juce::String& letter = {});
    void paintButton (juce::Graphics& g, bool highlighted, bool down) override;

private:
    std::optional<staple::Icon> icon;
};

// The output popover the footer's Output readout opens (HANDOFF.md §5.7): the Output Gain knob with the
// Pan Mode chip at its bottom right, the Output Pan slider with its readout, then Phase Invert, Auto Gain
// and the Output Meter's toggle. Every control but the Output Meter's is attached to its host
// parameter; Pan Mode and Output Pan are dimmed and disabled on mono tracks, where they have no effect.
class OutputPopover final : public staple::Popover
{
public:
    explicit OutputPopover (PluginProcessor& processor);
    ~OutputPopover() override;

    // Opens above the readout, right-aligned to it.
    void openFrom (juce::Component& readout);

    // After the Output Meter's toggle shows or hides it.
    std::function<void()> onMeterToggled;
    // Whether the Output Meter is shown, as the processor holds it.
    void showMeterShown (bool shown);

    // What the Output Pan slider reads now.
    juce::String panReadout() const;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    // Follows the track between mono and stereo.
    void followLayout();
    juce::Rectangle<int> panLabelsArea() const;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    class PanModeChip;

    PluginProcessor& processor;
    staple::Knob outputGain;
    std::unique_ptr<PanModeChip> panMode;
    OutputPanSlider outputPan;
    OutputToggle phaseInvert, autoGain, showMeter;

    std::unique_ptr<SliderAttachment> outputGainAttachment, outputPanAttachment;
    std::unique_ptr<ButtonAttachment> panModeAttachment, phaseInvertAttachment, autoGainAttachment;
    juce::TimedCallback layoutCheck { [this] { followLayout(); } };
};

} // namespace eq1
