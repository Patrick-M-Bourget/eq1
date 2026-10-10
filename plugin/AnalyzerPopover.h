#pragma once

#include "AnalyzerSettings.h"
#include "staple/controls/Popover.h"
#include "staple/controls/TextChip.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

namespace eq1
{

class PluginProcessor;

// What the footer's Analyzer button reads: the sources it shows of Pre and Post, "Pre + Post", "Pre",
// "Post" or "Off".
juce::String analyzerButtonText (const AnalyzerSettings& settings);

// Analyzer Tilt after a click on its row: the next of Off, 3, 4.5 and 6 dB/oct above tilt, wrapping
// from 6 to Off.
double nextAnalyzerTilt (double tilt);

// The footer's Analyzer button (HANDOFF.md §5.7): a filled chip 28 tall reading analyzerButtonText,
// with an up chevron. A click opens and closes the Analyzer popover.
class AnalyzerButton final : public staple::TextChip
{
public:
    AnalyzerButton();

    int getIdealWidth() const;
    void paintButton (juce::Graphics& g, bool highlighted, bool down) override;
};

// One of the Analyzer popover's rows (Range, Resolution, Speed, Analyzer Tilt): a ComboBox drawn as a
// row, its label on the left in text3 and its value on the right in text1, with a box on hover. Its items
// run from the largest value down, so the up key, which steps a ComboBox to its previous item
// (KeyboardControl), moves to the larger value. A click, Space or Return moves it to the next larger
// value, wrapping, rather than opening a menu.
class AnalyzerRow : public juce::ComboBox
{
public:
    AnalyzerRow (const juce::String& title, const juce::String& label);

    // Moves it to its next value, as a click does.
    void cycle();

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent&) override {}
    void mouseUp (const juce::MouseEvent&) override {}
    bool keyPressed (const juce::KeyPress& key) override;
    void showPopup() override { cycle(); }

protected:
    // The item index a click moves to; by default the previous one (the next larger value), wrapping.
    virtual int nextIndex() const;

private:
    juce::String label;
};

// The Analyzer popover the footer's Analyzer button opens (HANDOFF.md §5.7, §3 "Analyzer"): Pre, Post
// and Sidechain as a segmented group, the Range, Resolution, Speed and Analyzer Tilt rows, then Peak
// Hold. Every change goes to PluginProcessor::setAnalyzerSettings, never to the undo history; it follows
// settings restored with the session.
class AnalyzerPopover final : public staple::Popover
{
public:
    explicit AnalyzerPopover (PluginProcessor& processor);
    ~AnalyzerPopover() override;

    // Opens above button, its left edge at alignTo's (the footer's "Analyzer" label).
    void openFrom (juce::Component& button, juce::Component& alignTo);

    // After one of its controls changes a setting.
    std::function<void()> onSettingsChanged;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    class SourceButton;
    class PeakHoldButton;
    class TiltRow;

    // Shows the settings the processor holds.
    void show();
    void store();
    juce::Rectangle<int> sourcesArea() const;

    PluginProcessor& processor;
    std::unique_ptr<SourceButton> showPreEq, showPostEq, showSidechain;
    AnalyzerRow range, resolution, speed;
    std::unique_ptr<TiltRow> tilt;
    std::unique_ptr<PeakHoldButton> peakHold;
    // Follows restored settings while it is open.
    juce::TimedCallback follow { [this] { show(); } };
};

} // namespace eq1
