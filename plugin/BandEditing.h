#pragma once

#include "EditHistory.h"
#include "Parameters.h"
#include "eq1/Settings.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <optional>
#include <vector>

namespace eq1
{

// The editor's rules for adding, deleting and moving Bands. Every edit goes to the host parameters
// as a gesture, so hosts record it as Host Automation. Message thread only.
//
// The display shows Gains as heard, under Gain Scale, so the Gains add() and dragBy() take are heard
// Gains: each is stored divided by Gain Scale. At 0% every Gain is heard as 0 dB, so a Band is added
// at Gain 0 and a vertical drag changes nothing.
class BandEditing
{
public:
    // Each edit is one step in history: adding a Band, and Spectrum Grab with its drag, change several
    // parameters but are undone together.
    BandEditing (juce::AudioProcessorValueTreeState& parameters, EditHistory& history);
    // Ends a drag still going, so the host sees every gesture finish.
    ~BandEditing();

    // Adds a Band at the lowest free Band Slot, at frequency and gain, with the defaults for the rest:
    // Bell, Q 1, Slope 12, Stereo, not Bypassed, no dynamics (Dynamic Range 0, Threshold Auto, Attack
    // and Release Auto, no Dynamics Bypass). Returns its slot (1 to 24), or nothing when all 24
    // are in use.
    std::optional<int> add (double frequency, double gain);

    // Spectrum Grab on a peak with no Band nearby: a Bell at frequency, Gain 0, added as add() does,
    // and a drag of it begun, which then sets its Gain. Nothing when all 24 slots are in use.
    std::optional<int> grab (double frequency);

    // Deletes a Band: its slot becomes free and keeps its settings. Other Bands keep their numbers.
    void deleteBand (int slot);
    // Deletes several Bands as one edit: the Delete key on a selection.
    void deleteBands (const std::vector<int>& slots);

    bool isFull() const;
    int freeSlots() const;

    // A drag of the given Bands. dragBy is relative to where the drag began: Frequency times
    // frequencyRatio and Gain plus gainOffset. The Bands move together, so when one reaches the edge
    // of a range they all stop there. A Band whose Shape has no Gain keeps it.
    void beginDrag (std::vector<int> slotsToDrag);
    void dragBy (double frequencyRatio, double gainOffset);
    void endDrag();

    // The display's Dynamic Range Handle: a drag of one Band's range end as heard, as one gesture and one
    // undo step. dragDynamicRangeTo sets the stored Dynamic Range so that Gain + Dynamic Range is heard
    // at heardEnd under Gain Scale, rounded to 0.5 dB, within the parameter's +/-30 dB. Below 1 % Gain
    // Scale nothing is heard to follow, so it changes nothing.
    void beginDynamicRangeDrag (int slot);
    void dragDynamicRangeTo (double heardEnd);
    void endDynamicRangeDrag();
    // Sets a Band's stored Dynamic Range, within +/-30 dB, as one gesture: the Dynamic Range Handle's
    // double-click and keys.
    void setDynamicRange (int slot, double dynamicRange);

    // The arrow keys: moves the given Bands from where they are, Frequency by semitones and Gain by
    // gainOffset as heard, as one edit. Like a drag, the Bands stop together at the edge of a range,
    // and a Band whose Shape has no Gain keeps it.
    void nudge (const std::vector<int>& slots, double semitones, double gainOffset);

    // Multiplies a Band's Q by factor, within its range: the mouse wheel.
    void scaleQ (int slot, double factor);
    // The same on several Bands as one edit: the wheel over a selection.
    void scaleQ (const std::vector<int>& slots, double factor);

    void setShape (int slot, Shape shape);

    // The context menu's edits on a selection. Each is one edit, on every given Band it applies to.
    void setBypass (const std::vector<int>& slots, bool bypass);
    // Negates Gain and Dynamic Range on the Bands whose Shape has Gain; the others keep theirs.
    void invertGain (const std::vector<int>& slots);
    // Every dynamics setting back to its default, on the Bands whose Shape has dynamics.
    void clearDynamics (const std::vector<int>& slots);
    void setShape (const std::vector<int>& slots, Shape shape);
    // Slope in dB/oct on the Bands that have a Slope; it turns Brickwall off on a Cut.
    void setSlope (const std::vector<int>& slots, double slope);
    // Brickwall on the Cuts; the other Bands keep theirs.
    void setBrickwall (const std::vector<int>& slots);
    void setPlacement (const std::vector<int>& slots, StereoPlacement placement);
    // Turns each Stereo Band into two, every other setting kept: a Left Band in its slot and a Right
    // Band in the lowest free Band Slot. The other Bands are ignored. With too few free slots, the
    // lowest-Frequency Stereo Bands are split, the lower slot first on a tie. Returns both halves of
    // every split Band, in slot order: the selection after a split.
    std::vector<int> split (const std::vector<int>& slots);
    // Adds Bands with every stored setting as given, In Use aside, into the lowest free Band Slots, as
    // one edit: Paste. Gain and Dynamic Range are stored ones, not heard. With too few free slots, the
    // lowest-Frequency Bands are added, the earlier given first on a tie. Returns their slots, in
    // order: the selection after a Paste.
    std::vector<int> paste (const std::vector<BandSettings>& bands);

    BandSettings band (int slot) const;
    // Every Band's stored settings, and the whole-plugin ones.
    Settings settings() const;

private:
    // The free Band Slots, lowest first: the one place add, split and Paste find where a new Band goes.
    std::vector<int> lowestFreeSlots() const;
    juce::RangedAudioParameter& parameter (const juce::String& id) const;
    // Sets a parameter to a plain value, within its range, as one gesture.
    void set (const juce::String& id, double value);
    void setWithinGesture (const juce::String& id, double value);
    // The stored Gain for a Gain heard under Gain Scale.
    double storedGain (double heard) const;
    // Calls edit with each slot and its settings before the edit, all as one edit.
    void editEach (const std::vector<int>& slots, const std::function<void (int, const BandSettings&)>& edit);

    struct Dragged
    {
        int slot;
        double frequency, gain;
        bool movesGain;
    };

    juce::AudioProcessorValueTreeState& parameters;
    EditHistory& history;
    bool grabbing = false; // a Spectrum Grab's transaction is open until its drag ends
    std::array<parameters::SlotValues, numBandSlots> slots;
    parameters::OutputValues output;
    std::vector<Dragged> dragged;
    int rangeDragged = 0; // the Band whose Dynamic Range is being dragged, or 0
};

} // namespace eq1
