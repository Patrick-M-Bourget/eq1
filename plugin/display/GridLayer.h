#pragma once

#include "../AnalyzerSettings.h"
#include "DisplayGeometry.h"

#include <vector>

namespace eq1::display
{

// The grid, behind the Analyzer, from 20 Hz to 20 kHz: major Frequency lines at 20, 50, 100 ... 20k,
// minor ones between them (none below layout::narrowDisplayWidth), Gain lines a third of the Display
// Range apart, and the 0 dB line.
struct GridLines
{
    std::vector<float> majorX, minorX;
    std::vector<float> gainY; // from the bottom up, 0 dB left out
    float zeroY = 0.0f;
};
GridLines gridLines (const DisplayGeometry& geometry);

// A piece of text on the display, over the edge fades.
struct Label
{
    juce::String text;
    juce::Rectangle<float> area;
    juce::Justification justification = juce::Justification::centred;
    juce::Colour colour;
    float fontSize = 0.0f;
};

// The Frequency labels along the bottom, then the Gain labels at the right, from the top down, signed,
// without +/- the Display Range itself.
std::vector<Label> gridLabels (const DisplayGeometry& geometry);

// The Analyzer's own dB scale at the outer right, on its scale (the top of the display is 0 dB): 10 dB
// steps, 20 dB above a 90 dB range, kept clear of the top and bottom; none when the Analyzer shows
// nothing.
std::vector<Label> analyzerScaleLabels (const DisplayGeometry& geometry, const AnalyzerSettings& settings);

void paintGrid (juce::Graphics& g, const DisplayGeometry& geometry);
void paintLabels (juce::Graphics& g, const std::vector<Label>& labels);

} // namespace eq1::display
