#pragma once

#include "Parameters.h"
#include "eq1/Settings.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <optional>
#include <vector>

namespace eq1
{

// The editor's rules for adding, deleting and moving Bands. Every edit goes to the host parameters
// as a gesture, so hosts record it as Host Automation. Message thread only.
class BandEditing
{
public:
    explicit BandEditing (juce::AudioProcessorValueTreeState& parameters);
    // Ends a drag still going, so the host sees every gesture finish.
    ~BandEditing();

    // Adds a Band at the lowest free Band Slot, at frequency and gain, with the defaults for the rest:
    // Bell, Q 1, Slope 12, Stereo, not Bypassed. Returns its slot (1 to 24), or nothing when all 24
    // are in use.
    std::optional<int> add (double frequency, double gain);

    // Deletes a Band: its slot becomes free and keeps its settings. Other Bands keep their numbers.
    void deleteBand (int slot);

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
    Settings settings() const;

private:
    juce::RangedAudioParameter& parameter (const juce::String& id) const;
    // Sets a parameter to a plain value, within its range, as one gesture.
    void set (const juce::String& id, double value);
    void setWithinGesture (const juce::String& id, double value);

    struct Dragged
    {
        int slot;
        double frequency, gain;
        bool movesGain;
    };

    juce::AudioProcessorValueTreeState& parameters;
    std::array<parameters::SlotValues, numBandSlots> slots;
    std::vector<Dragged> dragged;
};

} // namespace eq1
