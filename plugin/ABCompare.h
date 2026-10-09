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
// selected. Its state is recorded in the undo history with the parameters. Message thread only,
// except side(), capture() and restore().
class ABCompare final : private EditHistory::OutsideState
{
public:
    ABCompare (juce::AudioProcessorValueTreeState& parameters, EditHistory& history);
    ~ABCompare() override;

    CompareSide side() const;

    // Puts the side's settings on the host parameters, inside gestures, as one undo step.
    void select (CompareSide side);

    // Overwrites B with A's settings, from either side, as one undo step.
    void copyAToB();

    // The side you're on and the other side's settings, as a tree of type stateType, and putting
    // them back. From any thread, as hosts save and restore sessions from theirs. A tree without the
    // other side puts B back to being a copy of A.
    static const juce::Identifier stateType;
    juce::ValueTree capture() const override;
    void restore (const juce::ValueTree& state) override;

private:
    juce::ValueTree otherSide() const;
    void set (CompareSide side, juce::ValueTree otherSide);

    juce::AudioProcessorValueTreeState& parameters;
    EditHistory& history;
    mutable juce::SpinLock lock; // a host may save or restore while the editor switches sides
    CompareSide active = CompareSide::A;
    juce::ValueTree other; // the side not selected; invalid while B is still a copy of A. Replaced, never changed.

    JUCE_DECLARE_NON_COPYABLE (ABCompare)
};

} // namespace eq1
