#pragma once

#include "DisplayGeometry.h"
#include "GridLayer.h"

#include <vector>

namespace eq1::display
{

// The ghost Bell over empty space: where a double-click would add a Bell. Decorative, so it has no
// accessible element.
struct Ghost
{
    double frequency = 1000.0, gain = 0.0; // gain as heard
    float x = 0.0f;                         // the pointer's
    juce::String readout;                   // "850.0 Hz", "1.25 kHz", "12.5 kHz"
};
// At the pointer's Frequency and Gain, at least 12 % of the Display Range from 0 dB (on the pointer's
// side, above it at 0 dB), with its peak at least 60 px inside the top and bottom.
Ghost ghostBell (const DisplayGeometry& geometry, juce::Point<float> pointer);
// Where it rests with no Bands: 1 kHz, at half the Display Range.
Ghost restingGhost (const DisplayGeometry& geometry);

// The labels with every Frequency label within 46 px of the ghost's readout faded to 12 %.
std::vector<Label> fadedForGhost (std::vector<Label> labels, const DisplayGeometry& geometry, const Ghost& ghost);

// The Bell (Q 1) 190 px either side, in a stroke whose white fades in and out across its width, at
// alpha. Painted under the display's edge fades, so it dissolves into them as the other curves do.
void paintGhostCurve (juce::Graphics& g, const DisplayGeometry& geometry, const Ghost& ghost, double sampleRate, float alpha);
// A 1 px line at its Frequency, a soft glow at its peak, and its readout 10 px above the bottom, all at
// alpha. Painted over the edge fades, unfaded.
void paintGhostMarker (juce::Graphics& g, const DisplayGeometry& geometry, const Ghost& ghost, float alpha);

} // namespace eq1::display
