#pragma once

#include "../AnalyzerSettings.h"
#include "../AnalyzerSpectrum.h"
#include "../AssumedSampleRate.h"
#include "../staple/Tokens.h"

#include "eq1/Settings.h"

#include <juce_graphics/juce_graphics.h>

#include <array>
#include <optional>
#include <set>

namespace eq1::display
{

// What the EQ display shows this frame, for its layers to draw.
struct DisplayFrame
{
    const Settings& bands; // as heard: Gain and Dynamic Range under Gain Scale
    // The Gain each Band is drawn with: its Live Gain while it is a Dynamic Band.
    std::array<double, numBandSlots> drawnGains {};
    const std::set<int>& selected;
    int soloedSlot = 0;
    bool dragging = false; // the selected Bands are being dragged
    std::optional<juce::Rectangle<float>> marquee;
    bool allInUseMessage = false; // "All 24 Bands are in use"
    double sampleRate = assumedSampleRate;
    bool mono = false;
    std::array<float, numBandSlots> hover {}; // each Band's hover fade, 0 to 1
    float globalBypass = 0.0f;                 // Global Bypass's fade, 0 to 1
};

// The Analyzer's spectra this frame.
struct AnalyzerFrame
{
    const AnalyzerSettings& settings;
    const AnalyzerSpectrum &preEq, &postEq, &sidechain;
    const AnalyzerSpectrum* held = nullptr; // the spectrum Peak Hold is drawn for, if any
};

// Exactly the same in everything the display draws or reads out, field by field, so any change at all
// counts and the comparison is exact. (BandSettings' own == would do, but its float comparison warns
// where it is defined.)
JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wfloat-equal")
inline bool sameBand (const BandSettings& x, const BandSettings& y)
{
    return x.inUse == y.inUse && x.bypass == y.bypass && x.shape == y.shape && x.frequency == y.frequency && x.gain == y.gain
           && x.q == y.q && x.slope == y.slope && x.brickwall == y.brickwall && x.placement == y.placement
           && x.dynamicRange == y.dynamicRange && x.threshold == y.threshold && x.thresholdAuto == y.thresholdAuto
           && x.attack == y.attack && x.release == y.release && x.dynamicsBypass == y.dynamicsBypass;
}
JUCE_END_IGNORE_WARNINGS_GCC_LIKE

// Each Band Slot keeps its own colour.
inline juce::Colour bandColour (int slot)
{
    return staple::tokens::band[slot - 1];
}

} // namespace eq1::display
