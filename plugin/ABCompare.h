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
// except side(), saveInto() and restore().
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

    // Saving and restoring with the session, from any thread, as hosts do from theirs. savedState()
    // is a copy of the parameters' state with a child of type stateType added: the side you're on and
    // the other side's settings. Both sides are whole in it, even when a host saves while select()
    // or copyAToB() is putting a side on the parameters. restore() takes that child back; an invalid
    // one puts the settings on A, with B a copy.
    static const juce::Identifier stateType;
    juce::ValueTree savedState() const;
    void restore (const juce::ValueTree& saved) override;
    // The child for settings saved before A/B Compare existed: on A, with B a copy.
    static juce::ValueTree initialState();

private:
    juce::ValueTree capture() const override;
    static juce::ValueTree treeOf (CompareSide selected, const juce::ValueTree& unselected);

    juce::ValueTree otherSide() const;
    void set (CompareSide side, juce::ValueTree otherSide);
    // Puts a side's settings on the parameters, telling a save made meanwhile what they will be. The
    // A/B state changes with it, to the side and other side given, as a save sees them.
    void putOnParameters (const juce::ValueTree& settings, CompareSide side, juce::ValueTree otherSide);

    juce::AudioProcessorValueTreeState& parameters;
    EditHistory& history;
    mutable juce::SpinLock lock; // a host may save or restore while the editor switches sides
    CompareSide active = CompareSide::A;
    juce::ValueTree other;    // the side not selected; invalid while B is still a copy of A. Replaced, never changed.
    juce::ValueTree arriving; // the side putOnParameters() is putting on the parameters, while it does
    int changes = 0;          // counts every change of the above, so a save can tell it raced one

    JUCE_DECLARE_NON_COPYABLE (ABCompare)
};

} // namespace eq1
