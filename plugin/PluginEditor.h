#pragma once

#include "BandEditing.h"
#include "BandPanel.h"
#include "DisplayRangeChip.h"
#include "EqDisplay.h"
#include "FooterBar.h"
#include "HeaderBar.h"
#include "KeyboardControl.h"
#include "OutputMeter.h"
#include "staple/LookAndFeel.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace eq1
{

class PluginProcessor;

// The native editor (ADR 0002), laid out as the Staple window (HANDOFF.md §4, §9B): the header
// (HeaderBar), the EQ display with the Output Meter's rail at its right, and the footer (FooterBar),
// over bg0 and three soft highlights. The selected Band's panel floats over the bottom of the display,
// Display Range sits at its top right, and the Preset browser opens over it. The header and footer keep
// their height and the rail its width; the display takes the rest. Resizable, and drawn at the
// instance's UI Scale through one transform; everything is drawn as vectors, so it stays sharp at any
// UI Scale and display scale.
class PluginEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor& processor);
    ~PluginEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    // Cmd-Z undoes, Shift-Cmd-Z (or Cmd-Y) redoes and Cmd-B toggles Global Bypass; Ctrl on Windows.
    // With nothing focused, Tab focuses the first control and Shift+Tab the last.
    bool keyPressed (const juce::KeyPress& key) override;
    // Any click in the editor hides the focus ring until a key brings it back.
    void mouseDown (const juce::MouseEvent& e) override;

private:
    // Follows the window's size and UI Scale, restored with the plugin's state, and what can be undone.
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
    HeaderBar header;
    FooterBar footer;
    juce::TooltipWindow tooltips { &content };
    DisplayRangeChip displayRange;
    OutputMeter meter;
    // Sizes the window to the processor's logical size and UI Scale, kept within limits.
    void applyUiScale();
    float scale = 1.0f;
    // What the window last took from the processor, to follow a restored session.
    int shownScalePercent = 0;
    juce::Point<int> shownSize;
    bool applyingScale = true; // until the window first takes its size

    // Last, so it lets go of every control before they go.
    KeyboardControl keyboard;
};

} // namespace eq1
