#pragma once

#include "AnalyzerSettings.h"
#include "BandEditing.h"
#include "AnalyzerSpectrum.h"
#include "display/DisplayFrame.h"
#include "display/DisplayGeometry.h"
#include "display/GhostLayer.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <set>
#include <vector>

namespace eq1
{

class PluginProcessor;

// The EQ curve with a handle per Band, over the Analyzer's pre-EQ, post-EQ and Sidechain spectra. Double-click
// adds a Bell; a ghost Bell follows the mouse over empty space to show where. Click a handle, or inside a
// Band's filled curve, to select it (a click on empty space clears the selection); drag moves the selected
// Bands (Shift or Cmd-click to select several, or drag a box around them, or Cmd/Ctrl+A for all); the
// wheel changes Q; Delete removes the selected Bands; Cmd/Ctrl+X, C and V Cut, Copy and Paste them, as
// the Band menu does, while the display has keyboard focus (a host may take these keys first). The arrow
// keys move the selected Bands, a semitone or 0.5 dB as heard per press (0.1 semitone or 0.05 dB with
// Shift), a held key being one undo step. Tab reaches each Band in use, in Frequency order (the order
// kept while a Band has focus), which selects it alone, then its Dynamic Range grip if shown; Delete then
// moves focus to the next Band. Right-click opens the Band menu (BandMenu.h) for the selection, which a
// Band outside it becomes first; on empty space it offers Paste and Select All. Holding a handle still
// Solos its Band until the mouse is released. Pressing on the spectrum, away from the handles, grabs its
// peak there (Spectrum Grab). The ▲▼ grip of the selected Band, and of each Dynamic Band, sets its
// Dynamic Range: drag it, double-click it to clear it, or ↑/↓ while it has focus. A Dynamic Band's curve
// follows its Live Gain, and the selected one has a wash between its curves at Gain and Gain + Dynamic
// Range. Hovering a handle or a curve lights the Band's curve, and Global Bypass fades every curve and
// handle to its bypassed look (plugin/display/). The curve comes from the Engine's own response maths
// (eq1/Response.h). A handle beyond the Display Range sits at its edge; a heard Gain changed to beyond
// it zooms the range out, once any drag has ended. A screen reader reads the display as a group, "EQ
// display", of the Bands in use, each named "Band 4" with its stored settings as its value (spokenBand),
// announced again whenever the Band moves, and each shown grip, "Band 4 Dynamic Range".
class EqDisplay final : public juce::Component, private juce::Timer
{
public:
    EqDisplay (PluginProcessor& processor, BandEditing& editing);
    ~EqDisplay() override;

    // Called with the Band Slot to show in the Band panel, or 0 when none is selected.
    std::function<void (int)> onSelectionChanged;
    // Selects one Band alone, as a click on its handle does (the Band panel's ‹ and ›).
    void selectBand (int slot) { select ({ slot }); }
    const std::set<int>& selection() const { return selected; }

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed (const juce::KeyPress& key) override;
    bool keyStateChanged (bool isKeyDown) override;
    void focusLost (FocusChangeType cause) override;
    void resized() override;

    // What a screen reader reads as a Band's value: "Bell, 1000.0 Hz, +3.50 dB, Q 0.707", Gain left
    // out on Shapes without one, then "Bypassed" and "Dynamic Band" when they apply.
    juce::String spokenBand (int slot) const;

    // The ghost Bell, while it shows: following the mouse over empty space (no handle, grip or Band's
    // curve under it), or resting at 1 kHz with no Bands; never while the mouse is pressed, a menu is
    // open or every Band Slot is in use.
    std::optional<display::Ghost> ghost() const;

private:
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
    // A Band's place in Tab's order: an element at its handle that takes keyboard focus. The display
    // keeps the mouse.
    class BandHandle;
    std::array<std::unique_ptr<BandHandle>, numBandSlots> handles;
    // A Dynamic Range grip's place in Tab's order, after its Band's: an element at the grip, shown while
    // the grip is, that takes keyboard focus. ↑/↓ step the Band's Dynamic Range.
    class RangeGrip;
    std::array<std::unique_ptr<RangeGrip>, numBandSlots> rangeGrips;
    // Puts each Band in use's element at its handle, and each shown grip's at the grip, and orders them
    // by Frequency, each grip after its Band, unless one has focus.
    void placeHandles();
    int gripAt (juce::Point<float> position) const; // the Band whose grip is there, or 0
    // The grip's arrow keys: Dynamic Range by dB, one undo step until the key is released.
    void stepDynamicRange (int slot, double db);
    int focusedSlot() const; // the Band whose element has keyboard focus, or 0
    // Moves the selected Bands by the arrow keys; the undo step lasts until the key is released or the
    // display and its Bands lose focus.
    void nudgeSelection (double semitones, double heardDb);
    void endHeldNudge();
    bool nudging = false;

    void timerCallback() override;
    // Reads the analysis taps into the spectra; false when the Analyzer shows nothing.
    bool updateAnalyzer();
    // Where a spectrum is drawn at x, with Analyzer Tilt: the top of the display is 0 dB, the bottom
    // the Analyzer's range below.
    float spectrumYAt (const AnalyzerSpectrum& spectrum, float x) const;
    // The spectrum Spectrum Grab reads peaks from, and Peak Hold holds: post-EQ when shown, else
    // pre-EQ, else none.
    AnalyzerSpectrum* spectrumToGrab();

    // Frequency runs on a log scale from 10 Hz to 30 kHz; dB over +/- the display range. The layers in
    // plugin/display/ draw with it.
    display::DisplayGeometry geometry() const;
    float xOf (double frequency) const;
    double frequencyAt (float x) const;
    float yOf (double db) const;
    double dbAt (float y) const;
    juce::Point<float> handleOf (const BandSettings& band) const;
    // The Gain a Band is drawn with: its Live Gain from the Engine while it is a Dynamic Band.
    double drawnGain (int slot, const BandSettings& band) const;
    int slotAt (juce::Point<float> position) const; // 0 when no handle is there
    // What the layers draw this frame, and the Band whose filled curve is at a point, or 0.
    display::DisplayFrame frame() const;
    int bandAreaAt (juce::Point<float> position) const;

    void select (std::set<int> slots);
    void selectAll();
    void deleteSelection();
    // The selected Bands onto the system clipboard, and the clipboard's Bands into the free Band
    // Slots, selected; false when there was nothing to paste or no free slot.
    void copySelection();
    bool paste();
    void showMenu (const juce::MouseEvent& e);

    PluginProcessor& processor;
    BandEditing& editing;

    AnalyzerSpectrum preEq, postEq, sidechain;
    AnalyzerSettings analyzer; // taken once a frame
    AnalyzerSpectrum* held = nullptr; // the spectrum Peak Hold is drawn for, if any
    std::vector<float> tapSamples; // read from the taps each frame
    juce::uint32 lastFrame = 0;
    // The Bands as heard: their Gain and Dynamic Range under Gain Scale. The display draws, and the
    // mouse moves, these (BandEditing takes Gains as heard).
    Settings heardSettings() const;
    Settings shown; // what was drawn last, refreshed on the timer
    std::array<double, numBandSlots> shownLiveGains {};

    std::set<int> selected;
    std::set<int> selectedBeforeMarquee; // Shift or Cmd adds the marquee to it
    bool dragging = false;
    // Spectrum Grab, from the press until the mouse moves: only a drag makes the Bell, so a click or a
    // double-click on the spectrum doesn't.
    std::optional<double> grabFrequency;
    bool grabbing = false; // the drag sets only the grabbed Band's Gain
    // A grip's drag: the Band, and its range's heard end when the drag began.
    int rangeDragSlot = 0;
    double rangeDragStartEnd = 0.0;

    // Solo: a handle held still this long Solos its Band until the mouse is released.
    static constexpr juce::uint32 soloHoldMilliseconds = 350;
    int heldSlot = 0; // the handle being held, before it Solos or the mouse moves
    juce::uint32 heldSince = 0;
    int soloedSlot = 0;
    void releaseSolo();
    // How far the mouse moves before a press counts as a drag, in pixels.
    static constexpr int dragThreshold = 3;
    std::optional<juce::Rectangle<float>> marquee;
    // A press on empty space: a drag past dragThreshold draws a marquee, a click selects by curve.
    bool pressedOnEmpty = false, pressAdding = false;
    juce::Point<float> dragStart;
    int shownRangeDb = 0;
    juce::uint32 allInUseMessageUntil = 0; // shows "All 24 Bands are in use" until this time

    // The curves' fades, all run from the display's one timer: each Band's hover (the handle under the
    // mouse) and Global Bypass's, read from its parameter each frame. Each runs from 0 to 1.
    int hoveredSlot = 0;
    std::optional<juce::Point<float>> pointer; // the mouse, while over the display
    float ghostFade = 0.0f;                    // the ghost Bell's fade in, from when it appears
    std::array<float, numBandSlots> hoverFades {};
    float globalBypassFade = 0.0f;
    juce::uint32 lastFadeStep = 0;
    bool isGlobalBypassOn() const;
    // Moves every fade on by the time since the last step; true while any of them moved.
    bool stepFades();
};

} // namespace eq1
