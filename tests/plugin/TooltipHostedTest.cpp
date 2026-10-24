// When the editor's Tooltip (staple::TooltipWindow) shows and goes, with the pointer driven through a host's window
// (harness::HostWindow), as the OS drives it: JUCE's MouseInputSource decides which component is under the pointer,
// and JUCE's own Tooltip timer shows and hides tips. The Tooltip window asks a tip source that answers as JUCE's
// getTipFor does in the foreground; JUCE's answers nothing to a test, which runs in the background.

#include "TipControls.h"
#include "EditorHarness.h"
#include "staple/LookAndFeel.h"
#include "staple/TipWatchdog.h"
#include "staple/TooltipWindow.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <memory>

namespace
{

// A control's tip as JUCE's getTipFor finds it in the foreground: none while a mouse button is down.
juce::String tipOf (juce::Component& c)
{
    if (juce::ModifierKeys::getCurrentModifiers().isAnyMouseButtonDown())
        return {};
    auto* client = dynamic_cast<juce::TooltipClient*> (&c);
    return client != nullptr ? client->getTooltip() : juce::String();
}

// The Tooltip tests' controls (TipControls), under the editor's Tooltip window, in a host's window.
struct Hosted
{
    juce::ScopedJuceInitialiser_GUI juce;
    staple::LookAndFeel lookAndFeel;
    juce::Component content;
    eq1::test::TipControls controls;
    juce::TextButton &gain = controls.gain, &mix = controls.mix;
    juce::Component& bare = controls.bare;
    staple::TooltipWindow tips { &content, tipOf };
    std::unique_ptr<harness::HostWindow> window;

    Hosted()
    {
        content.setLookAndFeel (&lookAndFeel);
        content.setBounds (eq1::test::TipControls::contentArea());
        controls.placeIn (content);
        window = std::make_unique<harness::HostWindow> (content);
    }

    ~Hosted()
    {
        window.reset();
        content.setLookAndFeel (nullptr);
    }

    static juce::Point<float> centreOf (juce::Component& c) { return c.getLocalBounds().toFloat().getCentre(); }

    // The pointer rests on control from now: the milliseconds until its tip shows (the test fails if none does).
    juce::uint32 restOn (juce::Component& control, juce::Point<float> at)
    {
        window->pointerAt (control, at);
        const auto restedAt = juce::Time::getMillisecondCounter();
        REQUIRE (harness::settleUntil ([this] { return tips.isVisible(); }));
        return juce::Time::getMillisecondCounter() - restedAt;
    }
    juce::uint32 restOn (juce::Component& control) { return restOn (control, centreOf (control)); }
};

} // namespace

TEST_CASE ("A Tooltip appears after the pointer rests 700 ms on its control, below it")
{
    Hosted h;
    CHECK (h.restOn (h.gain) >= 700);
    CHECK (h.tips.getY() >= h.gain.getBottom());
    CHECK (std::abs (h.tips.getBounds().getCentreX() - h.gain.getBounds().getCentreX()) <= 1);
}

TEST_CASE ("After a wheel turn or a click, a Tooltip stays hidden until the pointer moves, then rests 700 ms again")
{
    Hosted h;
    const bool wheel = GENERATE (true, false);
    // On over its control, or off it and back to where it was.
    const bool leaves = GENERATE (false, true);
    INFO ((wheel ? "a wheel turn" : "a click") << ", then the pointer " << (leaves ? "leaves and comes back" : "moves on"));
    const auto at = Hosted::centreOf (h.gain);
    h.restOn (h.gain, at);

    if (wheel)
    {
        h.window->wheelAt (h.gain, at);
    }
    else
    {
        h.window->pointerAt (h.gain, at, juce::ModifierKeys::leftButtonModifier);
        h.window->pointerAt (h.gain, at);
    }
    CHECK_FALSE (h.tips.isVisible());
    // Not shown at any moment until well past the 500 ms in which JUCE shows a tip again at once, and a 700 ms rest
    // after it: JUCE's timer, which runs before the harness's as it falls due sooner, has decided by then.
    const harness::VisibilityChanges changes (h.tips);
    harness::settle (1500);
    CHECK (changes.shows == 0);

    if (leaves)
    {
        h.window->pointerAt (h.bare, Hosted::centreOf (h.bare));
        CHECK (h.restOn (h.gain, at) >= 700);
    }
    else
    {
        CHECK (h.restOn (h.gain, at.translated (6.0f, 0.0f)) >= 700);
    }
}

TEST_CASE ("A Tooltip stays while the pointer rests on its control")
{
    Hosted h;
    h.restOn (h.gain);
    // Through the watchdog's checks, which fire before the harness's timer as they fall due sooner.
    const harness::VisibilityChanges changes (h.tips);
    harness::settle (4 * staple::TipWatchdog::periodMs);
    CHECK (changes.hides == 0);
    CHECK (h.tips.isVisible());
}

// The OS sends the window no event in each case, so JUCE still has the pointer on the control: only the watchdog,
// which reads the pointer from the OS, sees it has gone.
TEST_CASE ("A Tooltip fades out on its watchdog's check once the OS has the pointer off its control, and stays hidden")
{
    Hosted h;
    h.restOn (h.gain);
    using Exit = harness::HostWindow::UnseenExit;
    const auto how = GENERATE (Exit::offTheControl, Exit::overAnotherWindow, Exit::windowHidden);
    INFO ("The pointer is " << harness::HostWindow::describe (how));
    h.window->pointerLeavesUnseen (how, h.gain, h.gain.getLocalPoint (&h.bare, Hosted::centreOf (h.bare)));

    REQUIRE (harness::settleUntil ([&] { return ! h.tips.isVisible() && ! h.tips.fadingOut().isVisible(); }));
    // Faded to nothing, not cut: the fade's last frame leaves its picture transparent (it starts opaque).
    CHECK (h.tips.fadingOut().getAlpha() == 0.0f);
    // JUCE's timer, which still has the pointer on the control, doesn't show it again, not even for one watchdog
    // period, until well past the 500 ms in which it shows a tip again at once and a 700 ms rest after it.
    const harness::VisibilityChanges changes (h.tips);
    harness::settle (1500);
    CHECK (changes.shows == 0);
}
