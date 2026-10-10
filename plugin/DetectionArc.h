#pragma once

#include "LevelBallistics.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace eq1
{

class PluginProcessor;

// The Metered Band's Detection Level, drawn as an arc around the Threshold knob on the knob's own dB
// sweep, so Threshold is set against what the detector hears: the same whatever the dynamics state.
// It rises at once and falls at 20 dB/s, repainted at 60 Hz. Lies over the knob, with its bounds, and
// lets the mouse through.
class DetectionArc final : public juce::Component, private juce::Timer
{
public:
    DetectionArc (PluginProcessor& processor, juce::Slider& threshold);

    // How far round threshold's sweep a Detection Level reaches, 0 to 1: as far as the Threshold it
    // equals. Above 0 dB it stops at 0 dB, short of Auto; below the bottom of the knob it's 0.
    static double sweepProportion (juce::Slider& threshold, double levelDb);

    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;

    PluginProcessor& processor;
    juce::Slider& threshold;
    LevelBallistics ballistics;
    int meteredSlot = 0;
    double lastRead = 0.0;    // seconds, when the Detection Level was last read
    double proportion = 0.0;  // of the sweep, as last painted
};

} // namespace eq1
