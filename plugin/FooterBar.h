#pragma once

#include "KeyboardSlider.h"
#include "OutputPanel.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

namespace eq1
{

class PluginProcessor;

// The window's footer (HANDOFF.md §4, §5.7): Global Bypass, the Analyzer button, Gain Scale and the
// Output button from the left, the Output Meter's toggle and the UI Scale menu at the right. Analyzer
// and Output each open their controls (the Analyzer settings; Output Gain, Output Pan, Pan Mode, Auto
// Gain and Phase Invert) in a call-out over the editor, which an outside click or Escape closes; the
// controls are reachable only while it is open. The Staple footer and popovers (#83, #84) replace them.
class FooterBar final : public juce::Component
{
public:
    explicit FooterBar (PluginProcessor& processor);
    ~FooterBar() override;

    void resized() override;

    // After a UI Scale is picked, with its percent.
    std::function<void (int)> onUiScalePicked;
    // After the Output Meter's toggle shows or hides it.
    std::function<void()> onMeterToggled;
    // The UI Scale, and whether the Output Meter is shown, as the processor holds them.
    void showUiScale (int percent);
    void showMeterShown (bool shown);

private:
    class AnalyzerPanel;
    // Opens panel in a call-out pointing at from, in the component holding the footer.
    void openCallOut (juce::Component& panel, juce::Component& from);

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    PluginProcessor& processor;
    juce::ToggleButton globalBypass { "Global Bypass" };
    juce::TextButton analyzer { "Analyzer" }, output { "Output" };
    juce::Label gainScaleLabel;
    KeyboardSlider gainScale { "Gain Scale" };
    juce::ToggleButton showMeter { "Meter" };
    juce::ComboBox uiScale;
    std::unique_ptr<SliderAttachment> gainScaleAttachment;
    std::unique_ptr<ButtonAttachment> globalBypassAttachment;

    // What the call-outs show; hidden children of the footer while closed.
    std::unique_ptr<AnalyzerPanel> analyzerPanel;
    OutputPanel outputPanel;
    juce::Component::SafePointer<juce::CallOutBox> callOut;
};

} // namespace eq1
