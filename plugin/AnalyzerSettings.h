#pragma once

#include "AnalyzerSpectrum.h"

namespace eq1
{

// What the Analyzer shows and how. Saved with the session, not in Presets.
struct AnalyzerSettings
{
    bool showPreEq = true;
    bool showPostEq = true;
    bool showSidechain = false; // off until asked for: most sessions have nothing on it
    int rangeDb = 90; // 60, 90 or 120 dB shown below the top of the display
    AnalyzerSpeed speed = AnalyzerSpeed::medium;
    AnalyzerResolution resolution = AnalyzerResolution::medium;
    double tiltDbPerOctave = 4.5; // Analyzer Tilt, 0 to 6 dB/oct around 1 kHz

    JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wfloat-equal")
    bool operator== (const AnalyzerSettings&) const = default;
    JUCE_END_IGNORE_WARNINGS_GCC_LIKE
};

} // namespace eq1
