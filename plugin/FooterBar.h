#pragma once

#include "AnalyzerPopover.h"
#include "KeyboardSlider.h"
#include "OutputPopover.h"
#include "Parameters.h"
#include "staple/controls/IconButton.h"
#include "staple/controls/TextChip.h"
#include "staple/controls/Tween.h"

#include "eq1/Settings.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>
#include <optional>

namespace eq1
{

class PluginProcessor;

// What the Output readout shows: a level in dB to one decimal, signed, or "-inf dB" at Output Gain's
// silent bottom.
juce::String outputReadoutText (double db);

// The footer's Gain Scale readout (HANDOFF.md §5.7): its value as text ("100%"). A vertical drag moves
// it 1% per pixel (0.25% with Shift) in whole percent, as one gesture; the up and down keys step it by
// 5% (1% with Shift); a double-click sets 100%, as one gesture.
class GainScaleReadout final : public KeyboardSlider
{
public:
    GainScaleReadout();

    // Drawn in text4 while Global Bypass leaves it unapplied.
    void setNotApplied (bool notApplied);
    bool isNotApplied() const { return notApplied; }
    // What it shows: "100%".
    juce::String text() const;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent&) override {}

private:
    double valueDraggedBy (double from, float pixels, bool fine) override;
    bool notApplied = false;
};

// The footer's Output readout: Output Gain plus Auto Gain's estimate while Auto Gain is on. A click
// opens and closes the output popover; fill2 behind it while that is open.
class OutputReadout final : public staple::TextChip
{
public:
    OutputReadout();

    void setNotApplied (bool notApplied);
    bool isNotApplied() const { return notApplied; }
    void setOpen (bool open);

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override;

private:
    bool notApplied = false, open = false;
};

// The footer's UI Scale menu: an icon that opens a menu titled "UI Scale" of the five UI Scales, the
// current one ticked.
class UiScaleMenu final : public staple::IconButton
{
public:
    UiScaleMenu();

    juce::PopupMenu menu() const;
    // As a pick in the menu: shows it and reports it.
    void pick (int percent);
    void show (int percent) { shown = percent; }
    int shownPercent() const { return shown; }

    std::function<void (int)> onPicked;

private:
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
    int shown = 100;
};

// The window's footer (HANDOFF.md §4, §5.7, §5.12): from the left, Global Bypass (with "Bypassed"
// beside it while on) and the Analyzer button; at the right, the Gain Scale and Output readouts and the
// UI Scale menu. Under Global Bypass the readouts are drawn in text4, not applied. Output opens the
// output popover; the Analyzer button, which reads the sources the Analyzer shows, opens the Analyzer
// popover.
class FooterBar final : public juce::Component, private juce::Timer
{
public:
    explicit FooterBar (PluginProcessor& processor);
    ~FooterBar() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    // After a UI Scale is picked, with its percent.
    std::function<void (int)> onUiScalePicked;
    // After the Output Meter's toggle shows or hides it.
    std::function<void()> onMeterToggled;
    // The UI Scale, and whether the Output Meter is shown, as the processor holds them.
    void showUiScale (int percent);
    void showMeterShown (bool shown);

    // Toggles Global Bypass as a click on its button does (Cmd/Ctrl+B).
    void toggleGlobalBypass();
    bool isBypassedLabelShown() const { return bypassedShown; }

    OutputPopover& getOutputPopover() { return outputPopover; }
    AnalyzerPopover& getAnalyzerPopover() { return analyzerPopover; }

private:
    void timerCallback() override;
    void showAnalyzerSources();
    void showBypass();
    void showOutputLevel();

    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    PluginProcessor& processor;
    staple::IconButton globalBypass { "Global Bypass", staple::Icon::power };
    juce::Label analyzerLabel;
    AnalyzerButton analyzer;
    GainScaleReadout gainScale;
    OutputReadout output;
    UiScaleMenu uiScale;
    std::unique_ptr<SliderAttachment> gainScaleAttachment;
    std::unique_ptr<ButtonAttachment> globalBypassAttachment;
    bool bypassedShown = false;
    staple::Tween bypassedFade { staple::tokens::motion::dur2Ms, 0.0f }; // "Bypassed" fading in
    std::vector<parameters::SlotValues> slotValues;
    parameters::OutputValues outputValues;
    // Auto Gain's estimate, and the settings it was worked out for.
    Settings estimatedFor;
    double estimateDb = 0.0;
    bool estimated = false;

    // Hidden children of the footer while closed.
    AnalyzerPopover analyzerPopover;
    OutputPopover outputPopover;
};

} // namespace eq1
