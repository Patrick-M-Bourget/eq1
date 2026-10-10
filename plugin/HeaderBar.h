#pragma once

#include "PresetBar.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace eq1
{

class PluginProcessor;

// The window's header (HANDOFF.md §4, §9B): a left group, a centre group with ‹, the Presets button
// and ›, and a right group with A, B, Copy A to B, Undo and Redo. The side groups share the leftover
// width equally, so the centre group stays centred in the window. The left group is empty until the
// Staple header (#78) puts the logo there.
class HeaderBar final : public juce::Component
{
public:
    explicit HeaderBar (PluginProcessor& processor);

    void resized() override;

    PresetBar& presets() { return presetBar; }

    void undo();
    void redo();
    // Enables Undo and Redo by what can be undone and redone.
    void showUndoState();

private:
    PluginProcessor& processor;
    PresetBar presetBar;
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" };
};

} // namespace eq1
