#pragma once

#include "BandEditing.h"
#include "BandPanel.h"
#include "EqDisplay.h"
#include "KeyboardControl.h"
#include "KeyboardSlider.h"
#include "OutputMeter.h"
#include "OutputPanel.h"
#include "PresetBar.h"
#include "staple/LookAndFeel.h"

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
    ~PluginEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    // Cmd-Z undoes, Shift-Cmd-Z (or Cmd-Y) redoes; Ctrl on Windows. With nothing focused, Tab focuses
    // the first control and Shift+Tab the last.
    bool keyPressed (const juce::KeyPress& key) override;
    // Any click in the editor hides the focus ring until a key brings it back.
    void mouseDown (const juce::MouseEvent& e) override;

private:
    // Follows the Display Range, restored with the plugin's state or zoomed out, and what can be undone.
    void timerCallback() override;

    PluginProcessor& eqProcessor;
    // Every component below draws with it, so it outlives them all.
    staple::LookAndFeel lookAndFeel;
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
    KeyboardSlider analyzerTilt { "Analyzer Tilt" };
    void showAnalyzerSettings();
    void storeAnalyzerSettings();

    // Last, so it lets go of every control before they go.
    KeyboardControl keyboard;
};

} // namespace eq1
