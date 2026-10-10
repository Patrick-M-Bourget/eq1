#pragma once

#include "KeyboardSlider.h"
#include "staple/controls/Knob.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>
#include <optional>

namespace eq1
{

class PluginProcessor;

// Dynamic Range as a ring around the Gain knob (HANDOFF.md §4 "Dynamic Range ring"), in the knob's
// outer lane: a flat lane, the range from Gain to Gain + Dynamic Range (clipped to the knob's +/-30 dB
// sweep) in a red radial gradient, faint under Dynamics Bypass, the Live Gain's movement over it in
// yellow, and a ▲▼ hint at the range's end. Dragging the lane sets Dynamic Range, 60 dB per 200 px
// (800 px with Shift) rounded to 0.5 dB, and a double-click on it sets 0, each one undo step. Alt with
// the arrows on the Gain knob step it 1 dB (0.5 dB with Shift), as the arrows do on the ring itself.
//
// The ring is its own element for the keyboard and screen readers, a slider titled "Band <n> Dynamic
// Range" over the Gain knob: it paints nothing itself, and lets the mouse through to the knob.
class DynamicRangeRing final : public KeyboardSlider, public staple::Knob::RingHandler, private juce::KeyListener, private juce::Timer
{
public:
    DynamicRangeRing (PluginProcessor& processor, staple::Knob& gain);
    ~DynamicRangeRing() override;

    // The Band Slot whose Dynamic Range it sets, or 0 for none.
    void show (int slot);
    // On Shapes with Gain only: absent otherwise, from the knob and from Tab.
    void setAvailable (bool available);

    // The Live Gain arc as last painted: from Gain to this many dB, or none.
    std::optional<double> liveGainShown() const { return liveGain; }
    // The range arc's opacity: faint under Dynamics Bypass.
    float rangeAlpha() const;

    void paint (juce::Graphics&) override {}
    bool keyPressed (const juce::KeyPress& key) override;
    bool keyStateChanged (bool isKeyDown) override { return KeyboardSlider::keyStateChanged (isKeyDown); }
    void valueChanged() override;
    void focusGained (FocusChangeType cause) override;
    void focusLost (FocusChangeType cause) override;

    // staple::Knob::RingHandler
    void paintRing (juce::Graphics& g, staple::Knob& knob, juce::Point<float> centre, float inner, float outer) override;
    void ringMouseDown (staple::Knob&, const juce::MouseEvent& e) override;
    void ringMouseDrag (staple::Knob&, const juce::MouseEvent& e) override;
    void ringMouseUp (staple::Knob&, const juce::MouseEvent& e) override;
    void ringDoubleClick (staple::Knob&, const juce::MouseEvent& e) override;
    juce::String ringTitle() override;
    juce::String ringValue() override;

private:
    // Alt with an arrow on the Gain knob.
    bool keyPressed (const juce::KeyPress& key, juce::Component* origin) override;
    bool keyStateChanged (bool isKeyDown, juce::Component* origin) override;
    void timerCallback() override;
    bool parameterIsOn (const juce::String& id) const;

    PluginProcessor& processor;
    staple::Knob& gain;
    int slot = 0;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    std::optional<double> liveGain;

    // A drag on the lane: where it started, the Dynamic Range there, and its speed.
    struct Drag
    {
        float startY;
        double startValue;
        bool fine;
        ScopedDragNotification gesture;
    };
    std::unique_ptr<Drag> drag;
};

} // namespace eq1
