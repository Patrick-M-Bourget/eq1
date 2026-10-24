// When a Knob Readout (staple::KnobTooltip) goes, with the pointer driven through the editor's window as the OS drives
// it (harness::HostWindow): JUCE's MouseInputSource decides which component is under the pointer and sends the enters
// and exits, so the Readout hears of the pointer only when JUCE tells it, as in a host.

#include "EditorHarness.h"
#include "staple/TipWatchdog.h"
#include "staple/controls/Knob.h"
#include "staple/controls/KnobTooltip.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace
{

// The editor in a host's window, a Band selected so the Band panel shows its Frequency knob. The panel has a Small
// Knob beside its own, in the same body: eq1's one Small Knob, the Slope, draws as a button.
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
TEST_CASE ("A Knob Readout hides within one watchdog period once the OS has the pointer off its knob")
{
    Hosted e;
    auto& knob = e.frequency();
    e.hover (knob);
    const std::string where = GENERATE ("off the knob", "over another window", "in a window the host hid");
    INFO ("The pointer is " << where);
    if (where == "off the knob")
        e.window.pointerUnseen (knob, rightOf (knob));
    else if (where == "over another window")
        e.window.peer().covered = e.window.getLocalArea (&knob, knob.getLocalBounds());
    else
        e.window.peer().visible = false;

    harness::settle (staple::TipWatchdog::periodMs);
    CHECK_FALSE (knob.isTooltipShown());
}

TEST_CASE ("A Knob Readout stays while the pointer rests on its knob, drags it out of the knob, or types into it")
{
    Hosted e;
    auto& knob = e.frequency();
    e.hover (knob);
    const std::string doing = GENERATE ("rests", "drags", "types");
    INFO ("The pointer " << doing);
    if (doing == "drags")
    {
        const auto left = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier);
        e.window.pointerAt (knob, knob.getFaceCentre(), left);
        e.window.pointerAt (knob, rightOf (knob), left);
    }
    else if (doing == "types")
    {
        knob.getKnobTooltip()->startEditing();
        e.window.pointerUnseen (knob, rightOf (knob));
    }
    harness::settle (4 * staple::TipWatchdog::periodMs);
    CHECK (knob.isTooltipShown());
    if (doing == "drags")
        e.window.pointerAt (knob, rightOf (knob));
}
