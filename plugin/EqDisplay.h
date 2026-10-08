#pragma once

#include "BandEditing.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <set>
#include <vector>

namespace eq1
{

class PluginProcessor;

// The EQ curve with a handle per Band. Double-click adds a Band; drag moves the selected Bands (Shift
// or Cmd-click to select several, or drag a box around them); the wheel changes Q; Delete removes
// the selected Bands. The curve comes from the Engine's own response maths (eq1/Response.h).
class EqDisplay final : public juce::Component, private juce::Timer
{
public:
    EqDisplay (PluginProcessor& processor, BandEditing& editing);

    // Called with the Band Slot to show in the Band panel, or 0 when none is selected.
    std::function<void (int)> onSelectionChanged;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    void timerCallback() override;

    // Frequency runs on a log scale from 10 Hz to 30 kHz; dB over +/- the display range.
    float xOf (double frequency) const;
    double frequencyAt (float x) const;
    float yOf (double db) const;
    double dbAt (float y) const;
    juce::Point<float> handleOf (const BandSettings& band) const;
    int slotAt (juce::Point<float> position) const; // 0 when no handle is there

    void select (std::set<int> slots);
    // A curve through db, one value every pixelStep pixels from the left edge.
    juce::Path curve (const std::vector<double>& db) const;

    PluginProcessor& processor;
    BandEditing& editing;
    Settings shown; // what was drawn last, refreshed on the timer

    std::set<int> selected;
    std::set<int> selectedBeforeMarquee; // Shift or Cmd adds the marquee to it
    bool dragging = false;
    std::optional<juce::Rectangle<float>> marquee;
    juce::Point<float> dragStart;
    int shownRangeDb = 0;
    juce::uint32 allInUseMessageUntil = 0; // shows "All 24 Bands are in use" until this time
};

} // namespace eq1
