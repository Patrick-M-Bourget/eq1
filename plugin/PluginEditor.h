#pragma once

#include "BandEditing.h"
#include "BandPanel.h"
#include "EqDisplay.h"
#include "KeyboardSlider.h"
#include "OutputMeter.h"
#include "OutputPanel.h"
#include "PresetBar.h"
#include "UiScale.h"
#include "staple/LookAndFeel.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace eq1
{

class PluginProcessor;

// The native editor (ADR 0002): Presets, A/B Compare and undo at the top, the Analyzer's controls
// under them, the EQ display with the Output Meter at its right (and the Preset browser opening over
// the display), the selected Band's panel below it, and the whole-plugin output controls at the
// bottom. Resizable, and drawn at the instance's UI Scale through one transform; everything is drawn
// as vectors, so it stays sharp at any UI Scale and display scale.
class PluginEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor& processor);
    ~PluginEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    // Cmd-Z undoes, Shift-Cmd-Z (or Cmd-Y) redoes; Ctrl on Windows.
    bool keyPressed (const juce::KeyPress& key) override;

private:
    // Follows the Display Range, restored with the plugin's state or zoomed out, the window's size and
    // UI Scale, restored with it, and what can be undone.
    void timerCallback() override;

    PluginProcessor& eqProcessor;
    // Every component below draws with it, so it outlives them all.
    staple::LookAndFeel lookAndFeel;
    // Everything the editor shows, laid out in logical pixels (as at 100%) and drawn at the UI Scale
    // through one transform on it, so it all stays vectors. It holds every component below.
    juce::Component content;
    BandEditing editing;
    EqDisplay display;
    BandPanel panel;
    OutputPanel output;
    PresetBar presetBar;
    juce::TooltipWindow tooltips { &content };
    juce::ComboBox displayRange;
    juce::ComboBox uiScale;
    // Sizes the window to the processor's logical size and UI Scale, kept within limits.
    void applyUiScale();
    float scale = 1.0f;
    // What the window last took from the processor, to follow a restored session.
    int shownScalePercent = 0;
    juce::Point<int> shownSize;
    bool applyingScale = true; // until the window first takes its size
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
};

} // namespace eq1
