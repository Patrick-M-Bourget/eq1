#pragma once

#include "controls/Tween.h"

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
//
// The moment the pointer moves off the control a tip is showing for (onto another component, or out of
// the window), the tip hides and a picture of it fades out in its place over fadeOutMs, behind any tip
// that shows next. JUCE only notices the pointer has gone on its next timer tick, and hides the tip
// abruptly then; this hears of it from the pointer's events, which JUCE sends to every Tooltip window.
// A click or a wheel turn hides a tip, and ends a fade, at once.
class TooltipWindow final : public juce::TooltipWindow
{
public:
    static constexpr int fadeOutMs = 100;

    explicit TooltipWindow (juce::Component* parent);

    juce::String getTipFor (juce::Component& c) override;
    void moved() override;
    void resized() override;
    void visibilityChanged() override;

    // The picture of the last tip fading out: hidden when none is.
    const juce::Component& fadingOut() const { return fading; }

private:
    void placeByControl();
    // The pointer is over under now (nullptr when off every component): the tip fades out unless under is
    // its control.
    void pointerOver (juce::Component* under);
    void fadeOut();
    void endFade();

    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    // The picture of a tip, drawn faded in its place.
    struct Fading final : juce::Component
    {
        juce::Image picture;
        void paint (juce::Graphics& g) override;
    };

    juce::Component::SafePointer<juce::Component> control, showingFor;
    bool placing = false;
    Fading fading;
    Tween fade { fadeOutMs, 0.0f };
};

} // namespace staple
