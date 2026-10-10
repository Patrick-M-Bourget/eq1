#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace staple
{

class Knob;

// The read-out over a Knob (HANDOFF.md §4 "Knob tooltip"): its title in fs2 text3 over its value in fs4
// text1, on a menu-coloured card. It sits above the knob, or below it where there is no room above, in
// the knob's overlay layer (Overlay.h), and fades in over dur1. It is never a focus stop. A double-click
// on it opens a field for typing a value: Enter commits it as one undo step, Esc or leaving the field
// cancels. The Knob owns it and shows and hides it.
class KnobTooltip final : public juce::Component
{
public:
    explicit KnobTooltip (Knob& knob);
    ~KnobTooltip() override;

    juce::String titleText() const { return title; }
    juce::String valueText() const { return value; }

    // Reads the knob's title and value again, and places itself by the knob.
    void refresh();
    void appear();
    void disappear();

    void startEditing();
    bool isEditing() const { return editor != nullptr; }
    // The type-in field while it is open.
    juce::TextEditor* getEditor() const { return editor.get(); }

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;

private:
    void stopEditing (bool commit);

    Knob& knob;
    juce::String title, value;
    std::unique_ptr<juce::TextEditor> editor;
};

} // namespace staple
