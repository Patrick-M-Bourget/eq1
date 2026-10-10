#include "OutputPanel.h"

#include "Parameters.h"
#include "PluginProcessor.h"

namespace eq1
{

OutputPanel::OutputPanel (PluginProcessor& p) : processor (p)
{
    auto& state = processor.parameterState();
    const std::pair<juce::Slider*, juce::Label*> sliders[] = { { &outputGain, &outputGainLabel }, { &outputPan, &outputPanLabel } };
    const char* names[] = { "Output Gain", "Output Pan" };
    for (size_t i = 0; i < std::size (sliders); ++i)
    {
        auto [slider, label] = sliders[i];
        slider->setName (names[i]);
        slider->setSliderStyle (juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
        label->setText (names[i], juce::dontSendNotification);
        label->setJustificationType (juce::Justification::centredRight);
        // The slider is titled with its parameter's name, so a screen reader doesn't stop at the label too.
        label->setAccessible (false);
        addAndMakeVisible (*slider);
        addAndMakeVisible (*label);
    }
    panMode.setName ("Pan Mode");
    panMode.addItemList (parameters::panModeNames(), 1);
    addAndMakeVisible (panMode);
    for (auto* button : { &autoGain, &phaseInvert })
        addAndMakeVisible (*button);

    outputGainAttachment = std::make_unique<SliderAttachment> (state, parameters::outputGainId, outputGain);
    outputPanAttachment = std::make_unique<SliderAttachment> (state, parameters::outputPanId, outputPan);
    panModeAttachment = std::make_unique<ComboBoxAttachment> (state, parameters::panModeId, panMode);
    autoGainAttachment = std::make_unique<ButtonAttachment> (state, parameters::autoGainId, autoGain);
    phaseInvertAttachment = std::make_unique<ButtonAttachment> (state, parameters::phaseInvertId, phaseInvert);

    outputGain.describe (*state.getParameter (parameters::outputGainId));
    outputPan.describe (*state.getParameter (parameters::outputPanId));
    const std::pair<juce::Component*, juce::String> others[] = { { &panMode, parameters::panModeId },
                                                                  { &autoGain, parameters::autoGainId },
                                                                  { &phaseInvert, parameters::phaseInvertId }
                                                                  };
    for (const auto& [control, id] : others)
        control->setTitle (state.getParameter (id)->getName (100));

    timerCallback();
    startTimerHz (4);
    setSize (360, 3 * 24 + 2 * 6 + 2 * 6);
}

void OutputPanel::timerCallback()
{
    // The track can change between mono and stereo while the editor is open.
    const bool stereo = processor.isOutputPanAvailable();
    outputPan.setEnabled (stereo);
    panMode.setEnabled (stereo);
}

void OutputPanel::resized()
{
    // Output Gain, then Output Pan, then the toggles and Pan Mode, a row each.
    constexpr int rowHeight = 24, gap = 6;
    auto area = getLocalBounds().reduced (gap);
    for (auto [slider, label] : { std::pair<juce::Slider*, juce::Label*> { &outputGain, &outputGainLabel }, { &outputPan, &outputPanLabel } })
    {
        auto row = area.removeFromTop (rowHeight);
        label->setBounds (row.removeFromLeft (76));
        row.removeFromLeft (gap);
        slider->setBounds (row);
        area.removeFromTop (gap);
    }
    auto row = area.removeFromTop (rowHeight);
    panMode.setBounds (row.removeFromLeft (64));
    row.removeFromLeft (gap);
    autoGain.setBounds (row.removeFromLeft (84));
    row.removeFromLeft (gap);
    phaseInvert.setBounds (row.removeFromLeft (96));
}

} // namespace eq1
