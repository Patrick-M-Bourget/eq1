#pragma once

#include "DisplayGeometry.h"

namespace eq1::display
{

// The display's background and grid, around the Analyzer: behind it the background and the Frequency
// lines (decades and their halves) with their labels; over it the Gain lines, a quarter of the
// Display Range apart, with their labels.
void paintGridBehindAnalyzer (juce::Graphics& g, const DisplayGeometry& geometry);
void paintGridOverAnalyzer (juce::Graphics& g, const DisplayGeometry& geometry);

} // namespace eq1::display
