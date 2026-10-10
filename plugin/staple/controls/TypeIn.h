#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

namespace staple
{

// The field a value is typed into (HANDOFF.md §4 "Knob tooltip"): fs3 text1, centred, on fill2 with a
// line3 outline, titled title and holding text. Enter calls onCommit and Esc onCancel at once, rather
// than in a message posted for later; losing keyboard focus calls onCancel. Its owner places it, shows
// it, focuses it (focusTypeInField) and closes it (closeTypeInField).
std::unique_ptr<juce::TextEditor> makeTypeInField (const juce::String& title, const juce::String& text, std::function<void()> onCommit,
                                                   std::function<void()> onCancel);

// Gives the field keyboard focus with its text selected, if it is showing.
void focusTypeInField (juce::TextEditor& field);

// Takes the field out of its parent and out of field before anything else, so the focus it loses on the
// way out finds nothing to cancel, and deletes it later, as this may be its own key handler running.
// Returns its text; nothing happens, and it returns nothing, when field is empty.
juce::String closeTypeInField (std::unique_ptr<juce::TextEditor>& field);

} // namespace staple
