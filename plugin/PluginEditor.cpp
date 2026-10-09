#include "PluginEditor.h"

#include "PluginProcessor.h"

#include "eq1/Engine.h"

namespace eq1
{

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (p), eqProcessor (p), editing (p.parameterState(), p.editHistory()), display (p, editing), panel (p, editing), output (p), presetBar (p)
{
    display.onSelectionChanged = [this] (int slot) { panel.show (slot); };
    addAndMakeVisible (display);
    addAndMakeVisible (panel);
    addAndMakeVisible (output);
    presetBar.onEdit = [this] { showUndoState(); };
    addAndMakeVisible (presetBar);

    // The display's Gain range, saved with the plugin.
    for (int range : { 6, 12, 30 })
        displayRange.addItem ("+/- " + juce::String (range) + " dB", range);
    displayRange.setSelectedId (eqProcessor.displayRangeDb(), juce::dontSendNotification);
    displayRange.onChange = [this] {
        eqProcessor.setDisplayRangeDb (displayRange.getSelectedId());
        display.repaint();
    };
    addAndMakeVisible (displayRange);

    undoButton.onClick = [this] { undo(); };
    redoButton.onClick = [this] { redo(); };
    for (auto* button : { &undoButton, &redoButton })
    {
        button->setWantsKeyboardFocus (false);
        addAndMakeVisible (*button);
    }
    showUndoState();

    for (auto* toggle : { &showPreEq, &showPostEq, &showSidechain })
    {
        toggle->onClick = [this] { storeAnalyzerSettings(); };
        addAndMakeVisible (*toggle);
    }
    for (int range : { 60, 90, 120 })
        analyzerRange.addItem (juce::String (range) + " dB", range);
    analyzerSpeed.addItemList ({ "Very Slow", "Slow", "Medium", "Fast", "Very Fast" }, 1);
    analyzerResolution.addItemList ({ "Low", "Medium", "High", "Maximum" }, 1);
    for (auto* combo : { &analyzerRange, &analyzerSpeed, &analyzerResolution })
    {
        combo->onChange = [this] { storeAnalyzerSettings(); };
        addAndMakeVisible (*combo);
    }
    analyzerTiltLabel.setText ("Analyzer Tilt", juce::dontSendNotification);
    addAndMakeVisible (analyzerTiltLabel);
    analyzerTilt.setSliderStyle (juce::Slider::LinearHorizontal);
    analyzerTilt.setRange (0.0, 6.0, 0.5);
    analyzerTilt.setTextValueSuffix (" dB/oct");
    analyzerTilt.setTextBoxStyle (juce::Slider::TextBoxRight, false, 68, 20);
    analyzerTilt.onValueChange = [this] { storeAnalyzerSettings(); };
    addAndMakeVisible (analyzerTilt);
    showAnalyzerSettings();

    startTimerHz (4);

    setResizable (true, true);
    // Wide enough for the output controls' row.
    setResizeLimits (960, 484, 2560, 1600);
    setSize (1100, 664);
}

void PluginEditor::showAnalyzerSettings()
{
    const auto settings = eqProcessor.analyzerSettings();
    showPreEq.setToggleState (settings.showPreEq, juce::dontSendNotification);
    showPostEq.setToggleState (settings.showPostEq, juce::dontSendNotification);
    showSidechain.setToggleState (settings.showSidechain, juce::dontSendNotification);
    analyzerRange.setSelectedId (settings.rangeDb, juce::dontSendNotification);
    analyzerSpeed.setSelectedId (static_cast<int> (settings.speed) + 1, juce::dontSendNotification);
    analyzerResolution.setSelectedId (static_cast<int> (settings.resolution) + 1, juce::dontSendNotification);
    analyzerTilt.setValue (settings.tiltDbPerOctave, juce::dontSendNotification);
}

void PluginEditor::storeAnalyzerSettings()
{
    eqProcessor.setAnalyzerSettings ({ .showPreEq = showPreEq.getToggleState(),
                                       .showPostEq = showPostEq.getToggleState(),
                                       .showSidechain = showSidechain.getToggleState(),
                                       .rangeDb = analyzerRange.getSelectedId(),
                                       .speed = static_cast<AnalyzerSpeed> (analyzerSpeed.getSelectedId() - 1),
                                       .resolution = static_cast<AnalyzerResolution> (analyzerResolution.getSelectedId() - 1),
                                       .tiltDbPerOctave = analyzerTilt.getValue() });
}

void PluginEditor::undo()
{
    eqProcessor.editHistory().undo();
    showUndoState();
}

void PluginEditor::redo()
{
    eqProcessor.editHistory().redo();
    showUndoState();
}

void PluginEditor::showUndoState()
{
    undoButton.setEnabled (eqProcessor.editHistory().canUndo());
    redoButton.setEnabled (eqProcessor.editHistory().canRedo());
}

bool PluginEditor::keyPressed (const juce::KeyPress& key)
{
    const auto command = juce::ModifierKeys::commandModifier;
    if (key == juce::KeyPress ('z', command, 0))
    {
        undo();
        return true;
    }
    if (key == juce::KeyPress ('z', command | juce::ModifierKeys::shiftModifier, 0) || key == juce::KeyPress ('y', command, 0))
    {
        redo();
        return true;
    }
    return false;
}

void PluginEditor::timerCallback()
{
    showUndoState();
    // Follows settings restored with the plugin's state.
    if (displayRange.getSelectedId() != eqProcessor.displayRangeDb())
        displayRange.setSelectedId (eqProcessor.displayRangeDb(), juce::dontSendNotification);
    showAnalyzerSettings();
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff15171c));
}

void PluginEditor::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (32).reduced (6, 4);
    auto toolbar = area.removeFromTop (32).reduced (6, 4);
    output.setBounds (area.removeFromBottom (32));
    panel.setBounds (area.removeFromBottom (170));
    display.setBounds (area);

    redoButton.setBounds (header.removeFromRight (52));
    header.removeFromRight (4);
    undoButton.setBounds (header.removeFromRight (52));
    presetBar.setBounds (header);

    displayRange.setBounds (toolbar.removeFromRight (110));
    showPreEq.setBounds (toolbar.removeFromLeft (56));
    showPostEq.setBounds (toolbar.removeFromLeft (60));
    showSidechain.setBounds (toolbar.removeFromLeft (90));
    for (auto* combo : { &analyzerRange, &analyzerSpeed, &analyzerResolution })
    {
        toolbar.removeFromLeft (6);
        combo->setBounds (toolbar.removeFromLeft (88));
    }
    toolbar.removeFromLeft (12);
    analyzerTiltLabel.setBounds (toolbar.removeFromLeft (80));
    // What is left, up to 180 wide.
    analyzerTilt.setBounds (toolbar.removeFromLeft (180));
}

} // namespace eq1
