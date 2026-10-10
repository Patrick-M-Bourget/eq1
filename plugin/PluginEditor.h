#pragma once

#include "BandEditing.h"
#include "BandPanel.h"
#include "EqDisplay.h"
#include "OutputMeter.h"
#include "OutputPanel.h"
#include "PresetBar.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace eq1
{

class PluginProcessor;

// The native editor (ADR 0002): Presets, A/B Compare and undo at the top, the Analyzer's controls
// under them, the EQ display with the Output Meter at its right (and the Preset browser opening over
// the display), the selected Band's panel below it, and the whole-plugin output controls at the
// bottom. Resizable; everything is drawn as vectors, so it stays
// sharp at any display scale.
class PluginEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor& processor);

    void paint (juce::Graphics& g) override;
    void resized() override;
    // Cmd-Z undoes, Shift-Cmd-Z (or Cmd-Y) redoes; Ctrl on Windows.
    bool keyPressed (const juce::KeyPress& key) override;

private:
    // Follows the Display Range, restored with the plugin's state or zoomed out, and what can be undone.
    void timerCallback() override;

    PluginProcessor& eqProcessor;
    BandEditing editing;
    EqDisplay display;
    BandPanel panel;
    OutputPanel output;
    PresetBar presetBar;
    juce::TooltipWindow tooltips { this };
    juce::ComboBox displayRange;
    OutputMeter meter;
    juce::ToggleButton showMeter { "Meter" }; // shows or hides the Output Meter, saved with the plugin
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" };
    void undo();
    void redo();
    void showUndoState();

    // The Analyzer's controls, above the display.
    juce::ToggleButton showPreEq { "Pre" }, showPostEq { "Post" }, showSidechain { "Sidechain" }, peakHold { "Peak Hold" };
    juce::ComboBox analyzerRange, analyzerSpeed, analyzerResolution;
    juce::Label analyzerTiltLabel;
    juce::Slider analyzerTilt;
    void showAnalyzerSettings();
    void storeAnalyzerSettings();
};

} // namespace eq1
