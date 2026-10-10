#pragma once

#include "LevelBallistics.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

namespace eq1
{

class PluginProcessor;

// One bar of the Output Meter, from the Output Level read each frame: the peak rises at once and falls
// at 20 dB/s; the held-peak tick holds the highest peak for 1 s, then falls the same way, never below
// the peak; RMS is drawn as read.
class OutputMeterChannel
{
public:
    static constexpr double holdSeconds = 1.0;

    // The Output Level read seconds after the one before.
    void update (OutputLevel level, double seconds);

    double peakDb() const { return peak; }
    double heldPeakDb() const { return held; }
    double rmsDb() const { return rms; }

private:
    LevelBallistics peakBallistics;
    double peak = levelFloorDb, held = levelFloorDb, rms = levelFloorDb;
    double heldFor = 0.0; // seconds since the tick last rose
};

// The Output Meter: a bar per channel of the Output Level, with a Clip Light above each. Clicking
// either Clip Light, or Space or Return while the meter has keyboard focus, puts out both. Read and
// repainted at 60 Hz, like the EQ display.
class OutputMeter final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit OutputMeter (PluginProcessor& processor);

    // Where a level sits on the scale, 0 at -60 dBFS and below to 1 at +6 dBFS and above, linear in dB.
    static double position (double db);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    void timerCallback() override;
    juce::Rectangle<float> clipLightArea() const;

    PluginProcessor& processor;
    std::array<OutputMeterChannel, 2> channels;
    int numChannels = 0;
    juce::uint32 lastFrame = 0;
};

} // namespace eq1
