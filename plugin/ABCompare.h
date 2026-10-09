#pragma once

#include "EditHistory.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace eq1
{

enum class CompareSide
{
    A,
    B,
};

// A/B Compare: two independent sets of the settings a Preset holds. The host parameters are the side
// you're on; the other side is kept here. B starts as a copy of A, and stays one until it is first
// selected. Its state is recorded in the undo history with the parameters. Message thread only.
class ABCompare final : private EditHistory::OutsideState
{
public:
    ABCompare (juce::AudioProcessorValueTreeState& parameters, EditHistory& history);
    ~ABCompare() override;

    CompareSide side() const { return active; }

    // Puts the side's settings on the host parameters, inside gestures, as one undo step.
    void select (CompareSide side);

    // Overwrites B with A's settings, from either side, as one undo step.
    void copyAToB();

private:
    juce::ValueTree capture() const override;
    void restore (const juce::ValueTree& state) override;

    juce::AudioProcessorValueTreeState& parameters;
    EditHistory& history;
    CompareSide active = CompareSide::A;
    juce::ValueTree other; // the side not selected; invalid while B is still a copy of A

    JUCE_DECLARE_NON_COPYABLE (ABCompare)
};

} // namespace eq1
