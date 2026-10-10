#pragma once

#include "KeyboardSlider.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <utility>

namespace eq1
{

class PluginProcessor;
class BandEditing;
class BandPanel;

// The Free Detection Range of the Band the Band panel shows, as a bar on the display just above the
// panel (HANDOFF.md §5.5): a line across the display, the range as a segment of the Band's colour between
// two pill handles on the display's Frequency axis, their Frequencies above them, and a faint column
// of the Band's colour rising from it. Shown while that Band is a Dynamic Band, not Bypassed, on a Free
// Detection Range.
//
// Dragging a handle sets Detection Low or Detection High; dragging the segment moves both by the same
// Frequency ratio; left and right on a focused handle nudge it 1/6 octave. Each drag, press or held key
// is one undo step, and keeps Detection Low at most Detection High / 1.25, both within 20 Hz to 20 kHz.
// Typed values and Host Automation aren't held to that: the Engine takes the limits in any order.
class DetectionRangeBar final : public juce::Component, private juce::Timer, private juce::ComponentListener
{
public:
    static constexpr double lowestLimit = 20.0, highestLimit = 20000.0, minimumRatio = 1.25;

    DetectionRangeBar (PluginProcessor& processor, BandEditing& editing, BandPanel& panel);
    ~DetectionRangeBar() override;

    // Where the display is in the parent: the bar spans its width and sits above the panel.
    void setDisplayBounds (juce::Rectangle<int> display);

    // Detection Low and High as the bar's own edits keep them: low within 20 Hz and high / 1.25, high
    // within low x 1.25 and 20 kHz.
    static double clampLow (double low, double high);
    static double clampHigh (double high, double low);
    // Both moved by ratio, kept within 20 Hz to 20 kHz without changing their ratio.
    static std::pair<double, double> moved (double low, double high, double ratio);

    // Where a Frequency is on the bar, and the reverse, on the display's 10 Hz to 30 kHz axis.
    float xOf (double frequency) const;
    double frequencyAt (float x) const;
    // The y of the line, the handles and the segment.
    float lineY() const;

    class Handle;
    class Segment;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void componentMovedOrResized (juce::Component&, bool, bool) override;
    void componentVisibilityChanged (juce::Component&) override;
    void update();
    void place();

    PluginProcessor& processor;
    BandEditing& editing;
    BandPanel& panel;
    juce::Rectangle<int> display;
    int slot = 0;
    std::unique_ptr<Handle> low, high;
    std::unique_ptr<Segment> segment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> lowAttachment, highAttachment;
};

// One limit's pill handle: a slider for the keyboard and screen readers, titled "Band <n> Detection Low"
// or "... High".
class DetectionRangeBar::Handle final : public KeyboardSlider
{
public:
    Handle (DetectionRangeBar& bar, bool isLow);

    bool keyPressed (const juce::KeyPress& key) override;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent&) override {}

private:
    DetectionRangeBar& bar;
    bool isLow;
    std::unique_ptr<ScopedDragNotification> gesture;
};

// The segment between the handles: dragged, it moves both. Read as "Band <n> Detection Range" with its
// limits ("120 Hz – 4.50 kHz").
class DetectionRangeBar::Segment final : public juce::Component
{
public:
    explicit Segment (DetectionRangeBar& bar);

    juce::String valueText() const;
    juce::Colour colour;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    DetectionRangeBar& bar;
    double startFrequency = 0.0, startLow = 0.0, startHigh = 0.0;
    std::unique_ptr<juce::Slider::ScopedDragNotification> lowGesture, highGesture;
};

} // namespace eq1
