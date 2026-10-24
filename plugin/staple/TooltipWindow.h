#pragma once

#include "TipWatchdog.h"
#include "controls/Tween.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

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
// abruptly then; this hears of it from the pointer's events, which JUCE sends to every Tooltip window. When the OS
// sends none (another window over the editor's under the pointer, or the host hiding the editor's window itself), its
// TipWatchdog sees the pointer has gone within TipWatchdog::periodMs, and the tip fades out the same way. A control
// the pointer isn't over, as the watchdog reads it, has no tip, so JUCE doesn't show it again meanwhile.
// A click or a wheel turn hides a tip, and ends a fade, at once. The control it happened on then has no tip until
// the pointer moves more than dismissalMovePx or onto another component, and JUCE's 500 ms in which it shows a tip
// again at once have passed: the tip shows again only after a fresh 700 ms rest.
//
// It asks a control for its tip through a TipSource: JUCE's getTipFor unless one is given. JUCE's answers
// nothing to a process in the background, such as a test's, so a test gives one that answers as JUCE's does
// in the foreground, and JUCE's own timer shows and hides tips there as in a host.
class TooltipWindow final : public juce::TooltipWindow
{
public:
    static constexpr int fadeOutMs = 100;
    static constexpr float dismissalMovePx = 4.0f;

    // A control's tip, or nothing for none.
    using TipSource = std::function<juce::String (juce::Component&)>;

    explicit TooltipWindow (juce::Component* parent, TipSource tipFor = {});

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
    // A click or a wheel turn: hides the tip, and keeps the control's from showing again (dismissed).
    void dismiss (const juce::MouseEvent& e);
    bool isDismissed (juce::Component& c);

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

    TipSource tipSource;
    juce::Component::SafePointer<juce::Component> control, showingFor;
    // The control a click or wheel turn last dismissed a tip on, where and when, and whether the pointer has moved
    // on since.
    struct Dismissed
    {
        juce::Component::SafePointer<juce::Component> control;
        juce::Point<int> at;
        juce::uint32 time = 0;
        bool moved = false;
    } dismissed;
    TipWatchdog watchdog { [this] (juce::Point<float> pointer) { watch (pointer); } };
    void watch (juce::Point<float> pointer);
    bool placing = false;
    Fading fading;
    Tween fade { fadeOutMs, 0.0f };
};

} // namespace staple
