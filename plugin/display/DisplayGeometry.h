#pragma once

#include "eq1/Settings.h"

#include <juce_graphics/juce_graphics.h>

namespace eq1::display
{

// Where the EQ display draws things, for a display of width x height pixels: Frequency on a log scale
// from 10 Hz to 30 kHz, and dB over +/- the Display Range, a handle's radius in from the top and bottom.
struct DisplayGeometry
{
    static constexpr double lowestFrequency = 10.0, highestFrequency = 30000.0;
    static constexpr float handleRadius = 9.0f;
    static constexpr float pixelStep = 2.0f; // the curves and spectra are evaluated every this many pixels

    int width = 0, height = 0;
    int rangeDb = 12; // the Display Range

    float xOf (double frequency) const;
    double frequencyAt (float x) const;
    float yOf (double db) const;
    double dbAt (float y) const;
    // A Shape without Gain sits on the 0 dB line; one beyond the Display Range sits at its edge.
    juce::Point<float> handleOf (const BandSettings& band) const;
    juce::Rectangle<float> bounds() const { return juce::Rectangle<int> (width, height).toFloat(); }
};

} // namespace eq1::display
