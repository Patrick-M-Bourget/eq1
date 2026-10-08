#pragma once

#include "AnalyzerSettings.h"
#include "BandEditing.h"
#include "AnalyzerSpectrum.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <set>
#include <vector>

namespace eq1
{

class PluginProcessor;

// The EQ curve with a handle per Band, over the Analyzer's pre-EQ and post-EQ spectra. Double-click
// adds a Band; drag moves the selected Bands (Shift or Cmd-click to select several, or drag a box
// around them); the wheel changes Q; Delete removes the selected Bands. Holding a handle still Solos
// its Band until the mouse is released. Pressing on the spectrum, away from the handles, grabs its
// peak there (Spectrum Grab). A Dynamic Band has a ring around its handle for its Dynamic Range,
// with its Live Gain's movement inside it, and its curve follows its Live Gain. The curve comes from
// the Engine's own response maths (eq1/Response.h).
class EqDisplay final : public juce::Component, private juce::Timer
{
public:
    EqDisplay (PluginProcessor& processor, BandEditing& editing);
    ~EqDisplay() override;

    // Called with the Band Slot to show in the Band panel, or 0 when none is selected.
    std::function<void (int)> onSelectionChanged;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    void timerCallback() override;
    // Reads the analysis taps into the spectra; false when the Analyzer shows nothing.
    bool updateAnalyzer();
    // Where a spectrum is drawn at x, with Analyzer Tilt: the top of the display is 0 dB, the bottom
    // the Analyzer's range below.
    float spectrumYAt (const AnalyzerSpectrum& spectrum, float x) const;
    // The spectrum Spectrum Grab reads peaks from: post-EQ when shown, else pre-EQ, else none.
    const AnalyzerSpectrum* spectrumToGrab() const;

    // Frequency runs on a log scale from 10 Hz to 30 kHz; dB over +/- the display range.
    float xOf (double frequency) const;
    double frequencyAt (float x) const;
    float yOf (double db) const;
    double dbAt (float y) const;
    juce::Point<float> handleOf (const BandSettings& band) const;
    // The Gain a Band is drawn with: its Live Gain from the Engine while it is a Dynamic Band.
    double drawnGain (int slot, const BandSettings& band) const;
    int slotAt (juce::Point<float> position) const; // 0 when no handle is there

    void select (std::set<int> slots);
    // A curve through db, one value every pixelStep pixels from the left edge.
    juce::Path curve (const std::vector<double>& db) const;

    PluginProcessor& processor;
    BandEditing& editing;

    AnalyzerSpectrum preEq, postEq;
    AnalyzerSettings analyzer; // taken once a frame
    std::vector<float> tapSamples; // read from the taps each frame
    juce::uint32 lastFrame = 0;
    Settings shown; // what was drawn last, refreshed on the timer
    std::array<double, numBandSlots> shownLiveGains {};

    std::set<int> selected;
    std::set<int> selectedBeforeMarquee; // Shift or Cmd adds the marquee to it
    bool dragging = false;
    // Spectrum Grab, from the press until the mouse moves: only a drag makes the Bell, so a click or a
    // double-click on the spectrum doesn't.
    std::optional<double> grabFrequency;
    bool grabbing = false; // the drag sets only the grabbed Band's Gain

    // Solo: a handle held still this long Solos its Band until the mouse is released.
    static constexpr juce::uint32 soloHoldMilliseconds = 350;
    int heldSlot = 0; // the handle being held, before it Solos or the mouse moves
    juce::uint32 heldSince = 0;
    int soloedSlot = 0;
    void releaseSolo();
    // How far the mouse moves before a press counts as a drag, in pixels.
    static constexpr int dragThreshold = 3;
    std::optional<juce::Rectangle<float>> marquee;
    juce::Point<float> dragStart;
    int shownRangeDb = 0;
    juce::uint32 allInUseMessageUntil = 0; // shows "All 24 Bands are in use" until this time
};

} // namespace eq1
