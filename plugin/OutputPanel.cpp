#include "OutputPanel.h"

#include "Parameters.h"
#include "PluginProcessor.h"

namespace eq1
{

OutputPanel::OutputPanel (PluginProcessor& p) : processor (p)
{
    auto& state = processor.parameterState();
    const std::pair<juce::Slider*, juce::Label*> sliders[] = { { &gainScale, &gainScaleLabel },
                                                               { &outputGain, &outputGainLabel },
                                                               { &outputPan, &outputPanLabel } };
    const char* names[] = { "Gain Scale", "Output Gain", "Output Pan" };
    for (size_t i = 0; i < std::size (sliders); ++i)
    {
        auto [slider, label] = sliders[i];
        slider->setName (names[i]);
        slider->setSliderStyle (juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
        label->setText (names[i], juce::dontSendNotification);
        label->setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (*slider);
        addAndMakeVisible (*label);
    }
    panMode.setName ("Pan Mode");
    panMode.addItemList (parameters::panModeNames(), 1);
    addAndMakeVisible (panMode);
    for (auto* button : { &autoGain, &phaseInvert, &globalBypass })
        addAndMakeVisible (*button);

    gainScaleAttachment = std::make_unique<SliderAttachment> (state, parameters::gainScaleId, gainScale);
    outputGainAttachment = std::make_unique<SliderAttachment> (state, parameters::outputGainId, outputGain);
    outputPanAttachment = std::make_unique<SliderAttachment> (state, parameters::outputPanId, outputPan);
    panModeAttachment = std::make_unique<ComboBoxAttachment> (state, parameters::panModeId, panMode);
    autoGainAttachment = std::make_unique<ButtonAttachment> (state, parameters::autoGainId, autoGain);
    phaseInvertAttachment = std::make_unique<ButtonAttachment> (state, parameters::phaseInvertId, phaseInvert);
    globalBypassAttachment = std::make_unique<ButtonAttachment> (state, parameters::globalBypassId, globalBypass);

    timerCallback();
    startTimerHz (4);
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
    // The labels, buttons and Pan Mode keep their width; the three sliders share what is left.
    constexpr int gap = 6, fixedWidth = 70 + 84 + 76 + 70 + 64 + 96 + 104 + 9 * gap;
    auto row = getLocalBounds().reduced (6, 4);
    const int sliderWidth = juce::jmax (60, (row.getWidth() - fixedWidth) / 3);
    const auto place = [&row] (juce::Component& component, int width) {
        component.setBounds (row.removeFromLeft (width));
        row.removeFromLeft (gap);
    };
    place (gainScaleLabel, 70);
    place (gainScale, sliderWidth);
    place (autoGain, 84);
    place (outputGainLabel, 76);
    place (outputGain, sliderWidth);
    place (outputPanLabel, 70);
    place (outputPan, sliderWidth);
    place (panMode, 64);
    place (phaseInvert, 96);
    place (globalBypass, 104);
}

} // namespace eq1
