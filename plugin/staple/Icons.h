#pragma once

#include <juce_graphics/juce_graphics.h>

#include <array>

namespace staple
{

// Every icon the Staple prototype uses, from its SVG paths (HANDOFF.md §8). Named for what they show;
// one icon serves several controls (headphones for Solo and Detection Audition).
enum class Icon
{
    power, close, more, dropdown, chevronUp, submenu, check, previous, next, undo, redo,
    headphones, detectionSource, track, freeze, dynamicsOpen, phaseInvert, meter, uiScale, search, star,
    peakHold, resizeGrip, dynamicRangeHandleUp, dynamicRangeHandleDown,
    // Shapes
    bell, lowShelf, highShelf, lowCut, highCut, notch, bandPass, tiltShelf, flatTilt, allPass,
    // Stereo Placements: drawn over placementStereo at low opacity for the part not processed
    placementStereo, placementLeft, placementRight, placementMid, placementSide,
    // Detection Range
    detectionRangeBand, detectionRangeFree
};

inline constexpr int numIcons = static_cast<int> (Icon::detectionRangeFree) + 1;

// Its name in the prototype's terms, for tests and debugging.
const char* nameOf (Icon icon);

// The icon's outline in its own grid, parsed once, and that grid's area.
const juce::Path& pathOf (Icon icon);
juce::Rectangle<float> gridOf (Icon icon);

// Fitted into the area, centred, keeping its proportions: stroked 1.5 px wide with round caps and
// joins whatever its size, or filled (star, Dynamic Range Handles).
void drawIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> area, juce::Colour colour);
void fillIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> area, juce::Colour colour);

} // namespace staple
