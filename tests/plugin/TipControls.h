#pragma once

// The controls the Tooltip window's tests hover, with the pointer's events sent to the window directly
// (StapleControlsTest's TooltipKit) or through a host's window (TooltipHostedTest).

#include <juce_gui_basics/juce_gui_basics.h>

namespace eq1::test
{

// Two controls with tips, Gain and Mix, side by side, and a bare one below them, in a content component of
// contentArea. A test holds it after its ScopedJuceInitialiser_GUI, so the controls go before JUCE shuts down.
struct TipControls
{
    juce::TextButton gain { "Gain" }, mix { "Mix" };
    juce::Component bare;

    static juce::Rectangle<int> contentArea() { return { 0, 0, 300, 200 }; }

    // Puts them in content.
    void placeIn (juce::Component& content)
    {
        gain.setTooltip ("Gain");
        mix.setTooltip ("Mix");
        place (content, gain, { 20, 20, 60, 24 });
        place (content, mix, { 100, 20, 60, 24 });
        place (content, bare, { 20, 120, 60, 24 });
    }

private:
    static void place (juce::Component& content, juce::Component& c, juce::Rectangle<int> bounds)
    {
        content.addAndMakeVisible (c);
        c.setBounds (bounds);
    }
};

} // namespace eq1::test
