#include "PluginEditor.h"

#include "PluginProcessor.h"
#include "staple/Tokens.h"

#include "eq1/Engine.h"

namespace eq1
{

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (p), eqProcessor (p), editing (p.parameterState(), p.editHistory()), display (p, editing), panel (p, editing), output (p), presetBar (p), meter (p)
{
    display.onSelectionChanged = [this] (int slot) { panel.show (slot); };
    content.addAndMakeVisible (display);
    content.addAndMakeVisible (panel);
    content.addAndMakeVisible (output);
    presetBar.onEdit = [this] { showUndoState(); };
    content.addAndMakeVisible (presetBar);
    content.addChildComponent (presetBar.browserPanel());

    // The display's Gain range, saved with the plugin.
    for (int range : { 6, 12, 30 })
        displayRange.addItem ("+/- " + juce::String (range) + " dB", range);
    displayRange.setSelectedId (eqProcessor.displayRangeDb(), juce::dontSendNotification);
    displayRange.onChange = [this] {
        eqProcessor.setDisplayRangeDb (displayRange.getSelectedId());
        display.repaint();
    };
    content.addAndMakeVisible (displayRange);

    uiScale.setTitle ("UI Scale");
    uiScale.setTooltip ("UI Scale");
    for (int percent : uiScale::percents)
        uiScale.addItem (juce::String (percent) + "%", percent);
    uiScale.onChange = [this] {
        eqProcessor.setUiScalePercent (uiScale.getSelectedId());
        applyUiScale();
    };
    content.addAndMakeVisible (uiScale);

    content.addChildComponent (meter);
    showMeter.setToggleState (eqProcessor.isOutputMeterShown(), juce::dontSendNotification);
    showMeter.setTooltip ("Show the Output Meter");
    showMeter.onClick = [this] {
        eqProcessor.setOutputMeterShown (showMeter.getToggleState());
        resized();
    };
    content.addAndMakeVisible (showMeter);

    undoButton.onClick = [this] { undo(); };
    redoButton.onClick = [this] { redo(); };
    for (auto* button : { &undoButton, &redoButton })
    {
        button->setWantsKeyboardFocus (false);
        content.addAndMakeVisible (*button);
    }
    showUndoState();

    for (auto* toggle : { &showPreEq, &showPostEq, &showSidechain, &peakHold })
    {
        toggle->onClick = [this] { storeAnalyzerSettings(); };
        content.addAndMakeVisible (*toggle);
    }
    for (int range : { 60, 90, 120 })
        analyzerRange.addItem (juce::String (range) + " dB", range);
    analyzerSpeed.addItemList ({ "Very Slow", "Slow", "Medium", "Fast", "Very Fast" }, 1);
    analyzerResolution.addItemList ({ "Low", "Medium", "High", "Maximum" }, 1);
    for (auto* combo : { &analyzerRange, &analyzerSpeed, &analyzerResolution })
    {
        combo->onChange = [this] { storeAnalyzerSettings(); };
        content.addAndMakeVisible (*combo);
    }
    analyzerTiltLabel.setText ("Analyzer Tilt", juce::dontSendNotification);
    content.addAndMakeVisible (analyzerTiltLabel);
    analyzerTilt.setSliderStyle (juce::Slider::LinearHorizontal);
    // In 0.5 dB/oct steps (the Staple Analyzer popover, #84, cycles Off, 3, 4.5 and 6): an arrow key
    // moves it one step, as a step smaller than the interval would round back.
    analyzerTilt.setRange (0.0, 6.0, 0.5);
    analyzerTilt.setTextValueSuffix (" dB/oct");
    analyzerTilt.setTextBoxStyle (juce::Slider::TextBoxRight, false, 68, 20);
    analyzerTilt.onValueChange = [this] { storeAnalyzerSettings(); };
    content.addAndMakeVisible (analyzerTilt);
    showAnalyzerSettings();

    startTimerHz (4);

    // Once every component is in place: each one that keeps something from its look (a Slider's
    // text box, a ComboBox's label colours) takes it again from this.
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (content);

    setResizable (true, true);
    applyUiScale();
}

void PluginEditor::applyUiScale()
{
    // In logical pixels: wide enough for the output controls' row and the toolbar.
    constexpr int minimumWidth = 1120, minimumHeight = 600, maximumWidth = 2560, maximumHeight = 1600;
    const auto stored = eqProcessor.editorSize();
    const juce::Point<int> size { juce::jlimit (minimumWidth, maximumWidth, stored.x), juce::jlimit (minimumHeight, maximumHeight, stored.y) };
    const int percent = eqProcessor.uiScalePercent();
    uiScale.setSelectedId (percent, juce::dontSendNotification);
    shownScalePercent = percent;
    scale = static_cast<float> (percent) / 100.0f;
    content.setTransform (juce::AffineTransform::scale (scale));
    const auto scaled = [this] (int logical) { return juce::roundToInt (static_cast<float> (logical) * scale); };
    // New limits can resize the window on the way to its size: keep that from the processor.
    applyingScale = true;
    setResizeLimits (scaled (minimumWidth), scaled (minimumHeight), scaled (maximumWidth), scaled (maximumHeight));
    setSize (scaled (size.x), scaled (size.y));
    applyingScale = false;
    resized();
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::showAnalyzerSettings()
{
    const auto settings = eqProcessor.analyzerSettings();
    showPreEq.setToggleState (settings.showPreEq, juce::dontSendNotification);
    showPostEq.setToggleState (settings.showPostEq, juce::dontSendNotification);
    showSidechain.setToggleState (settings.showSidechain, juce::dontSendNotification);
    peakHold.setToggleState (settings.peakHold, juce::dontSendNotification);
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
                                       .tiltDbPerOctave = analyzerTilt.getValue(),
                                       .peakHold = peakHold.getToggleState() });
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
    if (showMeter.getToggleState() != eqProcessor.isOutputMeterShown())
    {
        showMeter.setToggleState (eqProcessor.isOutputMeterShown(), juce::dontSendNotification);
        resized();
    }
    if (eqProcessor.uiScalePercent() != shownScalePercent || eqProcessor.editorSize() != shownSize)
        applyUiScale();
    showAnalyzerSettings();
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (staple::tokens::colour::bg0);
}

void PluginEditor::resized()
{
    const juce::Point<int> logical { juce::roundToInt (static_cast<float> (getWidth()) / scale), juce::roundToInt (static_cast<float> (getHeight()) / scale) };
    if (! applyingScale)
    {
        eqProcessor.setEditorSize (logical);
        shownSize = eqProcessor.editorSize();
    }
    content.setBounds (0, 0, logical.x, logical.y);
    auto area = content.getLocalBounds();
    auto header = area.removeFromTop (32).reduced (6, 4);
    auto toolbar = area.removeFromTop (32).reduced (6, 4);
    output.setBounds (area.removeFromBottom (32));
    panel.setBounds (area.removeFromBottom (170));
    meter.setVisible (eqProcessor.isOutputMeterShown());
    if (meter.isVisible())
        meter.setBounds (area.removeFromRight (40));
    display.setBounds (area);
    presetBar.browserPanel().setBounds (area.reduced (40, 12));

    redoButton.setBounds (header.removeFromRight (52));
    header.removeFromRight (4);
    undoButton.setBounds (header.removeFromRight (52));
    presetBar.setBounds (header);

    uiScale.setBounds (toolbar.removeFromRight (72));
    toolbar.removeFromRight (6);
    displayRange.setBounds (toolbar.removeFromRight (110));
    toolbar.removeFromRight (6);
    showMeter.setBounds (toolbar.removeFromRight (64));
    showPreEq.setBounds (toolbar.removeFromLeft (56));
    showPostEq.setBounds (toolbar.removeFromLeft (60));
    showSidechain.setBounds (toolbar.removeFromLeft (90));
    peakHold.setBounds (toolbar.removeFromLeft (84));
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
