// When a Knob Readout (staple::KnobTooltip) goes, with the pointer driven through the editor's window as the OS drives
// it (harness::HostWindow): JUCE's MouseInputSource decides which component is under the pointer and sends the enters
// and exits, so the Readout hears of the pointer only when JUCE tells it, as in a host.

#include "EditorHarness.h"
#include "staple/TipWatchdog.h"
#include "staple/controls/Knob.h"
#include "staple/controls/KnobTooltip.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <optional>

namespace
{

// The editor in a host's window, a Band selected so the Band panel shows its Frequency knob, with the label under it
// that the knob's Readout covers. The panel has a Small Knob beside its own, in the same body, without a label: eq1's
// one Small Knob, the Slope, draws as a button.
struct Hosted : harness::OpenEditor
{
    harness::HostWindow window { *editor };
    staple::Knob small { staple::tokens::knob::small, "Small" };

    Hosted()
    {
        addBand (1, 1000.0f, 0.0f);
        settle();
        click (at (1000.0));
        settleAnimations (300);
        auto& body = *frequency().getParentComponent();
        body.addAndMakeVisible (small);
        small.setBounds (frequency().getRight() + 4, frequency().getY(), small.getIdealSize(), small.getIdealSize());
        REQUIRE (body.getLocalBounds().contains (small.getBounds()));
    }

    ~Hosted() { small.getParentComponent()->removeChildComponent (&small); }

    // The Frequency knob, which the Band panel gives its label.
    staple::Knob& labelFrequency()
    {
        REQUIRE (frequency().getLabel() != nullptr);
        return frequency();
    }

    staple::Knob& frequency()
    {
        auto found = findAll<staple::Knob> ([] (staple::Knob& k) { return k.getName() == "Frequency" && k.isShowing(); });
        REQUIRE (found.size() == 1);
        return *found.front();
    }

    static juce::Component* underPointer() { return juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse(); }

    // The pointer on knob's face, which shows its Readout.
    void hover (staple::Knob& knob)
    {
        window.pointerAt (knob, knob.getFaceCentre());
        REQUIRE (underPointer() == &knob);
        REQUIRE (knob.isTooltipShown());
    }
};

// A point in knob's bounds but off its round face, beside it: in the margin its shadow takes round the face (20 px at a
// regular Knob, 11 px at a Small one), which its hitTest leaves to its parent.
juce::Point<float> marginBeside (staple::Knob& knob)
{
    const auto centre = knob.getFaceCentre();
    const auto margin = static_cast<float> (knob.getWidth()) - (centre.x + knob.getFaceRadius());
    const juce::Point<float> at { centre.x + knob.getFaceRadius() + margin / 2.0f, centre.y };
    REQUIRE (knob.getLocalBounds().toFloat().contains (at));
    REQUIRE_FALSE (knob.hitTest (juce::roundToInt (at.x), juce::roundToInt (at.y)));
    return at;
}

// A point right of knob, outside its bounds.
juce::Point<float> rightOf (staple::Knob& knob) { return { static_cast<float> (knob.getWidth()) + 10.0f, knob.getFaceCentre().y }; }

} // namespace

TEST_CASE ("A Knob Readout hides when the pointer leaves the knob slowly, through the margin round its face")
{
    Hosted e;
    auto& knob = GENERATE (true, false) ? e.frequency() : e.small;
    INFO (knob.getName());
    e.hover (knob);

    // Off the face into the margin: JUCE sends the knob its exit here, still inside its bounds.
    e.window.pointerAt (knob, marginBeside (knob));
    REQUIRE (Hosted::underPointer() != &knob);
    // On out of the knob: it hears nothing more.
    e.window.pointerAt (knob, rightOf (knob));
    CHECK_FALSE (knob.isTooltipShown());
}

TEST_CASE ("A Knob Readout hides when its knob, or the knob's parent, is hidden under the pointer")
{
    Hosted e;
    auto& knob = e.frequency();
    e.hover (knob);
    // As eq1's Band panel closing hides a knob's parent, or comp1's switch between its compact and expanded views.
    auto& hidden = GENERATE (true, false) ? static_cast<juce::Component&> (knob) : *knob.getParentComponent();
    INFO ((&hidden == &knob ? "the knob" : "its parent"));
    hidden.setVisible (false);
    e.window.pointerAgain();
    CHECK_FALSE (knob.getKnobTooltip()->isVisible());
}

TEST_CASE ("A Knob Readout stays while the pointer is on it, and hides when it leaves it through the knob's margin")
{
    Hosted e;
    auto& knob = e.frequency();
    e.hover (knob);
    auto& readout = *knob.getKnobTooltip();

    e.window.pointerAt (readout, readout.getLocalBounds().toFloat().getCentre());
    REQUIRE (Hosted::underPointer() == &readout);
    CHECK (knob.isTooltipShown());

    e.window.pointerAt (knob, marginBeside (knob));
    REQUIRE (Hosted::underPointer() != &readout);
    REQUIRE (Hosted::underPointer() != &knob);
    CHECK_FALSE (knob.isTooltipShown());
}

TEST_CASE ("A Knob Readout being typed into stays when the pointer leaves it and its knob")
{
    Hosted e;
    auto& knob = e.frequency();
    e.hover (knob);
    auto& readout = *knob.getKnobTooltip();
    e.window.pointerAt (readout, readout.getLocalBounds().toFloat().getCentre());
    readout.startEditing();
    REQUIRE (readout.isEditing());

    e.window.pointerAt (knob, rightOf (knob));
    REQUIRE (Hosted::underPointer() != &readout);
    CHECK (knob.isTooltipShown());
    CHECK (readout.isEditing());
}

// The OS sends the window no event in each case, so JUCE still has the pointer on the knob: only the watchdog, which
// reads the pointer from the OS, sees it has gone.
TEST_CASE ("A Knob Readout hides on its watchdog's check once the OS has the pointer off its knob")
{
    Hosted e;
    auto& knob = e.frequency();
    e.hover (knob);
    using Exit = harness::HostWindow::UnseenExit;
    const auto how = GENERATE (Exit::offTheControl, Exit::overAnotherWindow, Exit::windowHidden);
    INFO ("The pointer is " << harness::HostWindow::describe (how));
    e.window.pointerLeavesUnseen (how, knob, rightOf (knob));

    CHECK (harness::settleUntil ([&] { return ! knob.isTooltipShown(); }));
}

TEST_CASE ("A Knob Readout stays while the pointer rests on its knob, drags it out of the knob, or types into it")
{
    Hosted e;
    auto& knob = e.frequency();
    e.hover (knob);
    enum class Doing
    {
        rests,
        drags,
        types
    };
    const auto doing = GENERATE (Doing::rests, Doing::drags, Doing::types);
    INFO ("The pointer " << (doing == Doing::rests ? "rests" : doing == Doing::drags ? "drags" : "types"));
    std::optional<harness::HostWindow::ButtonHeld> held;
    switch (doing)
    {
        case Doing::rests: break;
        case Doing::drags:
            held.emplace (e.window, knob, knob.getFaceCentre());
            held->dragTo (knob, rightOf (knob));
            break;
        case Doing::types:
            knob.getKnobTooltip()->startEditing();
            e.window.pointerUnseen (knob, rightOf (knob));
            break;
    }
    // Through the watchdog's checks, which fire before the harness's timer as they fall due sooner.
    const harness::VisibilityChanges changes (*knob.getKnobTooltip());
    harness::settle (4 * staple::TipWatchdog::periodMs);
    CHECK (changes.hides == 0);
    CHECK (knob.isTooltipShown());
}

namespace
{
// The pointer walked from a to b, both in the window's coordinates, a few px per event, as a hand moves it slowly;
// each() runs after every step.
void walk (harness::HostWindow& window, juce::Point<float> a, juce::Point<float> b, const std::function<void()>& each)
{
    constexpr float stepPx = 3.0f;
    const int steps = std::max (1, static_cast<int> (std::ceil (a.getDistanceFrom (b) / stepPx)));
    for (int i = 1; i <= steps; ++i)
    {
        window.pointerAtWindow (a + (b - a) * (static_cast<float> (i) / static_cast<float> (steps)));
        each();
    }
}

// The knob with a label, whose Readout covers the label from its top, 20 px under the face; or without, whose Readout
// is 6 px under the face.
staple::Knob& labelledOrNot (Hosted& e, bool labelled) { return labelled ? e.labelFrequency() : e.small; }
} // namespace

TEST_CASE ("A Knob Readout stays while the pointer moves slowly from the knob's face onto it, and a double-click there types")
{
    Hosted e;
    const bool labelled = GENERATE (true, false);
    INFO ((labelled ? "a knob with a label" : "a knob without one"));
    auto& knob = labelledOrNot (e, labelled);
    e.hover (knob);
    auto& readout = *knob.getKnobTooltip();
    const auto onCard = readout.getLocalBounds().toFloat().getCentre();

    // Through the gap between face and Readout, where neither is under the pointer: the knob's margin, its label.
    int inGap = 0;
    walk (e.window, e.window.getLocalPoint (&knob, knob.getFaceCentre()), e.window.getLocalPoint (&readout, onCard), [&] {
        if (Hosted::underPointer() != &knob && Hosted::underPointer() != &readout)
        {
            ++inGap;
            // A pause there, which the watchdog checks.
            if (inGap == 1)
                harness::settle (2 * staple::TipWatchdog::periodMs);
        }
        CHECK (knob.isTooltipShown());
    });
    REQUIRE (inGap > 0);
    REQUIRE (Hosted::underPointer() == &readout);

    for (int click = 0; click < 2; ++click)
    {
        e.window.pointerAt (readout, onCard, juce::ModifierKeys::leftButtonModifier);
        e.window.pointerAt (readout, onCard);
    }
    CHECK (readout.isEditing());
}

TEST_CASE ("A Knob Readout hides once the pointer leaves the way from the knob's face to it sideways")
{
    Hosted e;
    const bool labelled = GENERATE (true, false);
    INFO ((labelled ? "a knob with a label" : "a knob without one"));
    auto& knob = labelledOrNot (e, labelled);
    e.hover (knob);
    auto& readout = *knob.getKnobTooltip();
    const auto face = e.window.getLocalPoint (&knob, knob.getFaceCentre());
    const auto card = e.window.getLocalArea (&readout, readout.getLocalBounds()).toFloat();

    // Down into the gap, 2 px above the Readout.
    const juce::Point<float> inGap { face.x, card.getY() - 2.0f };
    walk (e.window, face, inGap, [] {});
    REQUIRE (Hosted::underPointer() != &knob);
    REQUIRE (Hosted::underPointer() != &readout);
    REQUIRE (knob.isTooltipShown());

    // Sideways, past the Readout's edge.
    walk (e.window, inGap, { card.getRight() + 10.0f, inGap.y }, [] {});
    CHECK (harness::settleUntil ([&] { return ! knob.isTooltipShown(); }));
}
