#pragma once

#include "EditorTick.h"
#include "LevelBallistics.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>

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
// either Clip Light, or Space or Return while the meter has keyboard focus, puts out both. Read on the
// editor's tick, which it keeps at 60 Hz while its bars move, and repainted only when a bar or a Clip
// Light changes: not on silence. A screen reader reads each Clip Light as a button, "Clip Light Left"
// and "Clip Light Right" ("Clip Light" on mono), "Lit" or "Off"; pressing either puts out both.
class OutputMeter final : public juce::Component, public juce::SettableTooltipClient, private EditorTick::Client
{
public:
    explicit OutputMeter (PluginProcessor& processor);
    ~OutputMeter() override;

    // Where a level sits on the scale, 0 at -60 dBFS and below to 1 at +6 dBFS and above, linear in dB.
    static double position (double db);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    bool keyPressed (const juce::KeyPress& key) override;
    void resized() override;

private:
    void tick (double seconds) override;
    bool isActive() const override { return moving; }
    // Where clicks put out the Clip Lights: over both, at least 10 px tall.
    juce::Rectangle<float> clipLightArea() const;
    // Where the bars run, from -60 dBFS at the bottom to +6 dBFS at the top, across the rail.
    juce::Rectangle<float> barsArea() const;
    // Where channel's bar is drawn across, left to right.
    juce::Range<float> columnOf (int channel) const;
    void clearClipLights();
    class ClipLight;
    std::array<std::unique_ptr<ClipLight>, 2> clipLights;
    void placeClipLights();

    PluginProcessor& processor;
    std::array<OutputMeterChannel, 2> channels;
    int numChannels = 0;
    // What the last tick drew: each channel's levels and Clip Light, compared to repaint on a change.
    struct Drawn
    {
        std::array<OutputMeterChannel, 2> channels;
        std::array<bool, 2> clipLit {};
        int numChannels = 0;
        bool operator== (const Drawn& other) const;
    };
    Drawn drawn;
    bool moving = false; // the last tick changed what is drawn
};

} // namespace eq1
