#include "PluginEditor.h"

#include "PluginProcessor.h"

namespace eq1
{

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (p), eqProcessor (p), editing (p.parameterState()), display (p, editing), panel (p, editing)
{
    display.onSelectionChanged = [this] (int slot) { panel.show (slot); };
    addAndMakeVisible (display);
    addAndMakeVisible (panel);

    // The display's Gain range, saved with the plugin.
    for (int range : { 6, 12, 30 })
        displayRange.addItem ("+/- " + juce::String (range) + " dB", range);
    displayRange.setSelectedId (eqProcessor.displayRangeDb(), juce::dontSendNotification);
    displayRange.onChange = [this] {
        eqProcessor.setDisplayRangeDb (displayRange.getSelectedId());
        display.repaint();
    };
    addAndMakeVisible (displayRange);

    startTimerHz (4);

    setResizable (true, true);
    setResizeLimits (640, 420, 2560, 1600);
    setSize (960, 600);
}

void PluginEditor::timerCallback()
{
    if (displayRange.getSelectedId() != eqProcessor.displayRangeDb())
        displayRange.setSelectedId (eqProcessor.displayRangeDb(), juce::dontSendNotification);
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff15171c));
}

void PluginEditor::resized()
{
    auto area = getLocalBounds();
    panel.setBounds (area.removeFromBottom (170));
    display.setBounds (area);
    displayRange.setBounds (area.removeFromTop (30).removeFromRight (120).reduced (4));
}

} // namespace eq1
