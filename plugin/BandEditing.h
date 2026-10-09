#pragma once

#include "EditHistory.h"
#include "Parameters.h"
#include "eq1/Settings.h"

#include <juce_audio_processors/juce_audio_processors.h>

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

    // A drag of the given Bands. dragBy is relative to where the drag began: Frequency times
    // frequencyRatio and Gain plus gainOffset. The Bands move together, so when one reaches the edge
    // of a range they all stop there. A Band whose Shape has no Gain keeps it.
    void beginDrag (std::vector<int> slotsToDrag);
    void dragBy (double frequencyRatio, double gainOffset);
    void endDrag();

    // Multiplies a Band's Q by factor, within its range: the mouse wheel.
    void scaleQ (int slot, double factor);

    void setShape (int slot, Shape shape);

    BandSettings band (int slot) const;
    // Every Band's stored settings, and the whole-plugin ones.
    Settings settings() const;

private:
    juce::RangedAudioParameter& parameter (const juce::String& id) const;
    // Sets a parameter to a plain value, within its range, as one gesture.
    void set (const juce::String& id, double value);
    void setWithinGesture (const juce::String& id, double value);
    // The stored Gain for a Gain heard under Gain Scale.
    double storedGain (double heard) const;

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
};

} // namespace eq1
