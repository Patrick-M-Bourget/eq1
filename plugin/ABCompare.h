#pragma once

#include "EditHistory.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

namespace eq1
{

enum class CompareSide
{
    A,
    B,
};

// A/B Compare: two independent sets of the settings a Preset holds, each with its own Loaded Preset or
// none. The host parameters are the side you're on; the other side is kept here. B starts as a copy of
// A, and stays one until it is first selected, when it takes A's Loaded Preset too. Its state is
// recorded in the undo history with the parameters. Message thread only, except side(), savedState()
// and restore().
class ABCompare final : private EditHistory::OutsideState
{
public:
    ABCompare (juce::AudioProcessorValueTreeState& parameters, EditHistory& history);
    ~ABCompare() override;

    CompareSide side() const;

    // Puts the side's settings on the host parameters, inside gestures, as one undo step.
    void select (CompareSide side);

    // Overwrites the other side with the settings and Loaded Preset of the side you're on, as one undo
    // step. You stay on your side, and the host parameters don't change.
    void copyToOther();

    // Puts a Preset's settings on the side you're on, inside gestures, and makes it the side's Loaded
    // Preset, named name, as one undo step. folder is where the browser lists it (PresetLibrary::Entry),
    // empty for a Preset from elsewhere.
    void loadPreset (const juce::ValueTree& preset, const juce::String& name, const juce::String& folder = {});
    // The side you're on was saved as a Preset holding preset, into folder: it becomes the side's Loaded
    // Preset, named name, as one undo step.
    void presetSaved (const juce::ValueTree& preset, const juce::String& name, const juce::String& folder = {});
    // The Loaded Preset of the side you're on, or an empty name for none.
    juce::String loadedPresetName() const;
    // The folder the side you're on last loaded or saved its Loaded Preset in, while the Loaded Preset
    // still has that name, as a name can be listed in more than one folder; empty otherwise. Kept in
    // memory only: neither saved with the session nor undone.
    juce::String loadedPresetFolder() const;
    // Whether the side you're on is Modified: its settings differ from its Loaded Preset's as loaded.
    // Never, with no Loaded Preset.
    bool isModified() const;

    // Saving and restoring with the session, from any thread, as hosts do from theirs. savedState()
    // is a copy of the parameters' state with a child of type stateType added: the side you're on, the
    // other side's settings, and each side's Loaded Preset with its settings as loaded, so a session
    // keeps it whatever later happens to the Preset's file. Both sides are whole in it, even when a
    // host saves while select() or loadPreset() is putting settings on the parameters.
    // restore() takes that child back; an invalid one puts the settings on A, with B a copy, and no
    // Loaded Preset on either.
    static const juce::Identifier stateType;
    juce::ValueTree savedState() const;
    void restore (const juce::ValueTree& saved) override;
    // The child for settings saved before A/B Compare existed: on A, with B a copy.
    static juce::ValueTree initialState();

private:
    // Everything A/B Compare keeps beside the parameters. Its trees are replaced, never changed.
    struct State
    {
        CompareSide active = CompareSide::A;
        juce::ValueTree other;                 // the side not selected; invalid while B is still a copy of A
        std::array<juce::ValueTree, 2> loaded; // each side's Loaded Preset, A then B; invalid for none

        juce::ValueTree& loadedOn (CompareSide side) { return loaded[side == CompareSide::A ? 0 : 1]; }
        const juce::ValueTree& loadedOn (CompareSide side) const { return loaded[side == CompareSide::A ? 0 : 1]; }
    };

    juce::ValueTree capture() const override;
    static juce::ValueTree treeOf (const State& state);
    // A Loaded Preset: its name and the settings preset puts on the parameters.
    juce::ValueTree loadedPresetOf (const juce::ValueTree& preset, const juce::String& name);

    State current() const;
    void set (State next);
    // Puts a side's or a Preset's settings on the parameters, telling a save made meanwhile what they
    // will be. The A/B state changes with it, to next, as a save sees it.
    void putOnParameters (const juce::ValueTree& settings, State next);

    juce::AudioProcessorValueTreeState& parameters;
    EditHistory& history;
    mutable juce::SpinLock lock; // a host may save or restore while the editor switches sides
    State state;
    // Each side's last load or save, A then B: the Loaded Preset's name then and its folder. Message
    // thread only.
    struct LoadedFrom
    {
        juce::String name, folder;
    };
    std::array<LoadedFrom, 2> loadedFrom;
    static std::size_t indexOf (CompareSide side) { return side == CompareSide::A ? 0 : 1; }
    juce::ValueTree arriving; // the settings putOnParameters() is putting on the parameters, while it does
    int changes = 0;          // counts every change of the above, so a save can tell it raced one

    JUCE_DECLARE_NON_COPYABLE (ABCompare)
};

} // namespace eq1
