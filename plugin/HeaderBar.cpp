#include "HeaderBar.h"

#include "PluginProcessor.h"

namespace eq1
{

HeaderBar::HeaderBar (PluginProcessor& p) : processor (p), presetBar (p)
{
    presetBar.onEdit = [this] { showUndoState(); };
    // It spans the header, under Undo and Redo; only its buttons take clicks.
    presetBar.setInterceptsMouseClicks (false, true);
    addAndMakeVisible (presetBar);

    undoButton.onClick = [this] { undo(); };
    redoButton.onClick = [this] { redo(); };
    for (auto* button : { &undoButton, &redoButton })
    {
        // Tab reaches them, but a click leaves focus where it was, so Delete still reaches the display.
        button->setMouseClickGrabsKeyboardFocus (false);
        addAndMakeVisible (*button);
    }
    showUndoState();

    int order = 0;
    for (juce::Component* child : std::initializer_list<juce::Component*> { &presetBar, &undoButton, &redoButton })
        child->setExplicitFocusOrder (++order);
}

void HeaderBar::undo()
{
    processor.editHistory().undo();
    showUndoState();
}

void HeaderBar::redo()
{
    processor.editHistory().redo();
    showUndoState();
}

void HeaderBar::showUndoState()
{
    undoButton.setEnabled (processor.editHistory().canUndo());
    redoButton.setEnabled (processor.editHistory().canRedo());
}

void HeaderBar::resized()
{
    constexpr int buttonHeight = 24, gap = 16, undoWidth = 52;
    auto row = getLocalBounds().reduced (6, 0).withSizeKeepingCentre (getWidth() - 12, buttonHeight);
    const int side = juce::jmax (0, (row.getWidth() - PresetBar::centreWidth - 2 * gap) / 2);
    row.removeFromLeft (side + gap); // the left group
    const auto centre = row.removeFromLeft (PresetBar::centreWidth);
    row.removeFromLeft (gap);

    auto right = row;
    redoButton.setBounds (right.removeFromRight (undoWidth));
    right.removeFromRight (4);
    undoButton.setBounds (right.removeFromRight (undoWidth));
    right.removeFromRight (12);
    presetBar.setBounds (getLocalBounds());
    presetBar.place (centre, right);
}

} // namespace eq1
