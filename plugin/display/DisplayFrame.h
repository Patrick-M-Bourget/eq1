#pragma once

#include "../AnalyzerSettings.h"
#include "../AnalyzerSpectrum.h"
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
    double sampleRate = 48000.0;
    bool mono = false;
};

// The Analyzer's spectra this frame.
struct AnalyzerFrame
{
    const AnalyzerSettings& settings;
    const AnalyzerSpectrum &preEq, &postEq, &sidechain;
    const AnalyzerSpectrum* held = nullptr; // the spectrum Peak Hold is drawn for, if any
};

// Each Band Slot keeps its own colour.
inline juce::Colour bandColour (int slot)
{
    return staple::tokens::band[slot - 1];
}

} // namespace eq1::display
