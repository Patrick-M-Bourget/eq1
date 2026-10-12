#pragma once

#include "EditorTick.h"
#include "KeyboardSlider.h"
#include "Parameters.h"
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
// Range" over the Gain knob: it paints nothing itself, and lets the mouse through to the knob. It reads
// the Live Gain on the editor's tick (EditorTick), which it keeps at 60 Hz while the arc moves, from the
// Band's parameters resolved once when it is shown.
class DynamicRangeRing final : public KeyboardSlider, public staple::Knob::RingHandler, private juce::KeyListener, private EditorTick::Client
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
    juce::String ringTitle() override;
    juce::String ringValue() override;

private:
    // Alt with an arrow on the Gain knob.
    bool keyPressed (const juce::KeyPress& key, juce::Component* origin) override;
    bool keyStateChanged (bool isKeyDown, juce::Component* origin) override;
    void tick (double seconds) override;
    bool isActive() const override { return moving; }
    double valueDraggedBy (double from, float pixels, bool fine) override;
    static bool isOn (const std::atomic<float>* parameter) { return parameter->load() >= 0.5f; }

    PluginProcessor& processor;
    staple::Knob& gain;
    int slot = 0;
    parameters::SlotValues band {};  // the Band's parameters, while a Band is shown
    parameters::OutputValues output; // Gain Scale and Global Bypass
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    std::optional<double> liveGain;
    bool moving = false; // the last tick moved the Live Gain arc
};

} // namespace eq1
