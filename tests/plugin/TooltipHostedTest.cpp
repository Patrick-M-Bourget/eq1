// When the editor's Tooltip (staple::TooltipWindow) shows and goes, with the pointer driven through a host's window
// (harness::HostWindow), as the OS drives it: JUCE's MouseInputSource decides which component is under the pointer,
// and JUCE's own Tooltip timer shows and hides tips. The Tooltip window asks a tip source that answers as JUCE's
// getTipFor does in the foreground; JUCE's answers nothing to a test, which runs in the background.

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

// Two controls with tips and a bare one, under the editor's Tooltip window, in a host's window.
struct Hosted
{
    juce::ScopedJuceInitialiser_GUI juce;
    staple::LookAndFeel lookAndFeel;
    juce::Component content;
    juce::TextButton gain { "Gain" }, mix { "Mix" };
    juce::Component bare;
    staple::TooltipWindow tips { &content, tipOf };
    std::unique_ptr<harness::HostWindow> window;

    Hosted()
    {
        content.setLookAndFeel (&lookAndFeel);
        content.setBounds (0, 0, 300, 200);
        gain.setTooltip ("Gain");
        mix.setTooltip ("Mix");
        place (gain, { 20, 20, 60, 24 });
        place (mix, { 100, 20, 60, 24 });
        place (bare, { 20, 120, 60, 24 });
        window = std::make_unique<harness::HostWindow> (content);
    }

    ~Hosted()
    {
        window.reset();
        content.setLookAndFeel (nullptr);
    }

    void place (juce::Component& c, juce::Rectangle<int> bounds)
    {
        content.addAndMakeVisible (c);
        c.setBounds (bounds);
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

// Counts the times a component is shown while it lives.
struct ShowCount final : juce::ComponentListener
{
    explicit ShowCount (juce::Component& c) : component (c) { component.addComponentListener (this); }
    ~ShowCount() override { component.removeComponentListener (this); }

    void componentVisibilityChanged (juce::Component& c) override
    {
        if (c.isVisible())
            ++count;
    }

    juce::Component& component;
    int count = 0;
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
    INFO ((wheel ? "a wheel turn" : "a click"));
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
    // Well past the 500 ms in which JUCE shows a tip again at once, and a 700 ms rest after it.
    harness::settle (1500);
    CHECK_FALSE (h.tips.isVisible());

    CHECK (h.restOn (h.gain, at.translated (6.0f, 0.0f)) >= 700);
}

TEST_CASE ("A Tooltip stays while the pointer rests on its control")
{
    Hosted h;
    h.restOn (h.gain);
    harness::settle (4 * staple::TipWatchdog::periodMs);
    CHECK (h.tips.isVisible());
}

// The OS sends the window no event in each case, so JUCE still has the pointer on the control: only the watchdog,
// which reads the pointer from the OS, sees it has gone.
TEST_CASE ("A Tooltip fades out within one watchdog period once the OS has the pointer off its control, and stays hidden")
{
    Hosted h;
    h.restOn (h.gain);
    const std::string where = GENERATE ("off the control", "over another window", "in a window the host hid");
    INFO ("The pointer is " << where);
    if (where == "off the control")
        h.window->pointerUnseen (h.bare, Hosted::centreOf (h.bare));
    else if (where == "over another window")
        h.window->peer().covered = h.window->getLocalArea (&h.gain, h.gain.getLocalBounds());
    else
        h.window->peer().visible = false;

    harness::settle (staple::TipWatchdog::periodMs);
    CHECK_FALSE (h.tips.isVisible());
    CHECK (h.tips.fadingOut().isVisible());
    CHECK (harness::settleUntil ([&] { return ! h.tips.fadingOut().isVisible(); }));
    // JUCE's timer, which still has the pointer on the control, doesn't show it again, not even for one watchdog period.
    const ShowCount shows (h.tips);
    harness::settle (1500);
    CHECK (shows.count == 0);
}
