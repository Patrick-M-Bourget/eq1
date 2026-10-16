#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace staple
{

// The editor's Tooltip window: JUCE's, after the same 700 ms rest and with the same wording, drawn and
// sized by staple::LookAndFeel, but placed by the control it is for. It sits centred below the control,
// over its label, or above it where there is no room below in its parent (the footer row). A control
// much taller than the Tooltip (the Output Meter) has it at its left, vertically centred on the pointer.
// It always stays inside its parent, which carries the UI Scale, so placement holds at every UI Scale.
//
// JUCE's placement isn't overridable, so this remembers the control asked about in getTipFor and places
// itself again whenever JUCE moves it, both on a rest and through displayTip.
class TooltipWindow final : public juce::TooltipWindow
{
public:
    explicit TooltipWindow (juce::Component* parent);

    juce::String getTipFor (juce::Component& c) override;
    void moved() override;
    void resized() override;

private:
    void placeByControl();

    juce::Component::SafePointer<juce::Component> control;
    bool placing = false;
};

} // namespace staple
