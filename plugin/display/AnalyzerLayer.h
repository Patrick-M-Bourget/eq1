#pragma once

#include "DisplayFrame.h"
#include "DisplayGeometry.h"

namespace eq1::display
{

// Where a spectrum is drawn at x, with Analyzer Tilt: the top of the display is 0 dB, the bottom the
// Analyzer's range below.
float spectrumYAt (const DisplayGeometry& geometry, const AnalyzerSettings& settings, const AnalyzerSpectrum& spectrum, float x);

// The Analyzer, behind the curves: the main spectrum (post-EQ, or pre-EQ when only it is shown) with a
// gradient fill and a line, Peak Hold a faint line, pre-EQ a faint line beside post-EQ, and the
// Sidechain a line in its own tint.
void paintAnalyzer (juce::Graphics& g, const DisplayGeometry& geometry, const AnalyzerFrame& frame);

} // namespace eq1::display
