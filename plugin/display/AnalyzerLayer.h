#pragma once

#include "DisplayFrame.h"
#include "DisplayGeometry.h"

namespace eq1::display
{

// Where a spectrum is drawn at x, with Analyzer Tilt: the top of the display is 0 dB, the bottom the
// Analyzer's range below.
float spectrumYAt (const DisplayGeometry& geometry, const AnalyzerSettings& settings, const AnalyzerSpectrum& spectrum, float x);

// The Analyzer, behind the curves: Peak Hold faint under the spectra, pre-EQ filled, post-EQ filled and
// outlined, the Sidechain outlined.
void paintAnalyzer (juce::Graphics& g, const DisplayGeometry& geometry, const AnalyzerFrame& frame);

} // namespace eq1::display
