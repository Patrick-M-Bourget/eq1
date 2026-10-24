// When a Knob Readout (staple::KnobTooltip) goes, with the pointer driven through the editor's window as the OS drives
// it (harness::HostWindow): JUCE's MouseInputSource decides which component is under the pointer and sends the enters
// and exits, so the Readout hears of the pointer only when JUCE tells it, as in a host.

#include "DynamicRangeRing.h"
#include "EditorHarness.h"
#include "staple/TipWatchdog.h"
#include "staple/controls/Knob.h"
#include "staple/controls/KnobTooltip.h"
#include "staple/controls/Overlay.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <optional>

namespace
{

// The editor in a host's window, a Bell Band selected so the Band panel shows its Frequency knob, with the label under
// it that the knob's Readout covers, and its Gain knob with the Dynamic Range ring. The panel has a Small Knob beside
// its own, in the same body, without a label, and another at the foot of the layer the Readouts show on, whose Readout
// sits above it: eq1's one Small Knob, the Slope, draws as a button.
struct Hosted : harness::OpenEditor
{
    harness::HostWindow window { *editor };
    staple::Knob small { staple::tokens::knob::small, "Small" }, atFoot { staple::tokens::knob::small, "At Foot" };
    // A ring with its own Readout content that takes no typing, which eq1's one ring, the Dynamic Range, does.
    struct UntypeableLane final : staple::Knob::RingHandler
    {
        void paintRing (juce::Graphics&, staple::Knob&, juce::Point<float>, float, float) override {}
        juce::String ringTitle() override { return "Untypeable"; }
        juce::String ringValue() override { return "+6.0 dB"; }
    } untypeableLane;

    Hosted()
    {
        addBand (1, 1000.0f, 0.0f);
        settle();
        click (at (1000.0));
        settleAnimations (300);
        auto& layer = staple::overlayLayerFor (frequency());
        layer.addAndMakeVisible (small);
        small.setBounds (300, 40, small.getIdealSize(), small.getIdealSize());
        layer.addAndMakeVisible (atFoot);
        // At the layer's foot as far as the window shows it, which may cut the layer short.
        const int foot = std::min (layer.getHeight(), layer.getLocalPoint (&window, juce::Point<int> (0, window.getHeight())).y);
        atFoot.setBounds (300, foot - atFoot.getIdealSize(), atFoot.getIdealSize(), atFoot.getIdealSize());
    }

    ~Hosted()
    {
        for (auto* c : std::initializer_list<juce::Component*> { &small, &atFoot })
            c->getParentComponent()->removeChildComponent (c);
    }

    // The Frequency knob, which the Band panel gives its label, with the Gain knob beside it hidden: a move to the far
    // corner of its Readout crosses Gain's face, which hands the Readout to Gain (one at a time on the layer).
    staple::Knob& labelFrequency()
    {
        auto& knob = frequency();
        REQUIRE (knob.getLabel() != nullptr);
        gain().setVisible (false);
        return knob;
    }

    staple::Knob& frequency() { return knobNamed ("Frequency"); }
    staple::Knob& gain() { return knobNamed ("Gain"); }

    // The Gain knob, which a Bell's Band panel gives the Dynamic Range ring (its 12 px lane at r + 10), whose Readout
    // content is its own and wider than the knob's.
    staple::Knob& ringedGain()
    {
        auto& knob = gain();
        REQUIRE (knob.isOnRing (knob.getFaceCentre().translated (knob.getFaceRadius() + staple::tokens::knob::ringOffset, 0.0f)));
        return knob;
    }

    // The Gain knob with a ring that takes no typing in place of its Dynamic Range ring (untypeableLane).
    staple::Knob& untypeableRingedGain()
    {
        gain().setRing (&untypeableLane);
        return gain();
    }

    // The Gain knob's Dynamic Range ring.
    eq1::DynamicRangeRing& dynamicRange()
    {
        auto found = findAll<eq1::DynamicRangeRing>();
        REQUIRE (found.size() == 1);
        return *found.front();
    }

    staple::Knob& knobNamed (const juce::String& name)
    {
        auto found = findAll<staple::Knob> ([&] (staple::Knob& k) { return k.getName() == name && k.isShowing(); });
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

    // Two clicks at at, in c's coordinates, through the window.
    void doubleClick (juce::Component& c, juce::Point<float> at)
    {
        for (int click = 0; click < 2; ++click)
        {
            window.pointerAt (c, at, juce::ModifierKeys::leftButtonModifier);
            window.pointerAt (c, at);
        }
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

// A rest of the pointer from now: the time it has lasted, and the watchdog's periods it has lasted, counted by a timer
// of its own at TipWatchdog::periodMs. A loaded machine's timer thread runs that timer late just as it runs the
// watchdog's, so it counts the watchdog's checks where a wall clock can't.
struct Rest final : private juce::Timer
{
    Rest() { startTimer (staple::TipWatchdog::periodMs); }
    ~Rest() override { stopTimer(); }

    juce::uint32 elapsedMs() const { return juce::Time::getMillisecondCounter() - startedAt; }
    int periods = 0;

private:
    void timerCallback() override { ++periods; }
    const juce::uint32 startedAt = juce::Time::getMillisecondCounter();
};

// Lets the timers run, the pointer sent again where it is each round, until done() is true (harness::settleUntil).
bool restUntil (harness::HostWindow& window, const std::function<bool()>& done)
{
    return harness::settleUntil ([&] {
        window.pointerAgain();
        return done();
    });
}

// A point right of knob, outside its bounds.
juce::Point<float> rightOf (staple::Knob& knob) { return { static_cast<float> (knob.getWidth()) + 10.0f, knob.getFaceCentre().y }; }

// The unit step at degrees up from level, right, round a face's centre (screen y runs down).
juce::Point<float> direction (float degrees)
{
    return { std::cos (juce::degreesToRadians (degrees)), -std::sin (juce::degreesToRadians (degrees)) };
}

// The radius of a ringed knob's hit area, the ring's outer edge: the Gain knob's 12 px lane at r + 10 (ringedGain),
// derived here once from the tokens, as Knob::hitRadius is the knob's own.
float ringOuterRadius (staple::Knob& knob)
{
    return knob.getFaceRadius() + staple::tokens::knob::ringOffset + staple::tokens::knob::ringLane / 2.0f;
}

// The radius of the middle of a ringed knob's lane.
float ringMiddleRadius (staple::Knob& knob) { return knob.getFaceRadius() + staple::tokens::knob::ringOffset; }

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
    // The way to the Readout (the hull of the face and the Readout) reaches out beside the face, level with its centre,
    // the further the wider the Readout is than the face. At the Small knob (a 30 px face under an 86 px Readout) it
    // reaches the margin point, so the exit there kept the Readout, and only the watchdog, on its next check, sees the
    // pointer has gone on off the way. At the Frequency knob (50 px) it falls short of it, and the exit hides it.
    if (&knob == &e.small)
        CHECK (harness::settleUntil ([&] { return ! knob.isTooltipShown(); }));
    else
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
            e.window.pointerAt (knob, rightOf (knob));
            break;
    }
    // Through the watchdog's checks, which fire before the harness's timer as they fall due sooner, and the pointer
    // again where the test left it, which JUCE's Desktop timer may have moved to the machine's real mouse meanwhile.
    const harness::VisibilityChanges changes (*knob.getKnobTooltip());
    harness::settle (4 * staple::TipWatchdog::periodMs);
    e.window.pointerAgain();
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

    e.doubleClick (readout, onCard);
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

namespace
{
// The knobs the way to the Readout is checked on: one with a label, a Small one without, a Small one with its Readout
// above it, and one with a ring, whose hit area reaches the ring's outer edge.
enum class Kind
{
    labelled,
    small,
    readoutAbove,
    ringed
};

// The knob of a kind in e, set up as the kind says, and what the kind is, for the test's INFO.
struct KnobOfKind
{
    staple::Knob& knob;
    const char* description;
};

KnobOfKind knobOf (Hosted& e, Kind kind)
{
    switch (kind)
    {
        case Kind::labelled: return { e.labelFrequency(), "a knob with a label" };
        case Kind::small: return { e.small, "a Small knob without one" };
        case Kind::readoutAbove: return { e.atFoot, "a Small knob with its Readout above it" };
        case Kind::ringed: return { e.ringedGain(), "a knob with a ring" };
    }
    return { e.small, "" };
}

// Points on knob's hit area, on its half away from its Readout, off the face's centre with the Readout below (y flips
// when it's above): round the face near its edge, from level with the centre over the top; round the ring's lane where
// it has one; and two starts whose moves to the Readout's near corners run close to the way's edge (a Small face's top
// left, the ring's left just below its centre's level).
std::vector<juce::Point<float>> startsAwayFromReadout (staple::Knob& knob, Kind kind)
{
    std::vector<juce::Point<float>> starts;
    const float r = knob.getFaceRadius();
    for (const float degrees : { 0.0f, 30.0f, 60.0f, 90.0f, 120.0f, 150.0f, 180.0f })
    {
        starts.push_back (direction (degrees) * (0.9f * r));
        if (kind == Kind::ringed)
            starts.push_back (direction (degrees) * ringMiddleRadius (knob));
    }
    if (kind == Kind::small || kind == Kind::readoutAbove)
        starts.push_back ({ -13.0f, -7.0f });
    if (kind == Kind::ringed)
        starts.push_back ({ -46.0f, 5.0f });
    return starts;
}
} // namespace

TEST_CASE ("A Knob Readout stays on every straight move from its knob's face or ring, on the half away from it, to its corners and centre")
{
    Hosted e;
    const auto kind = GENERATE (Kind::labelled, Kind::small, Kind::readoutAbove, Kind::ringed);
    const auto of = knobOf (e, kind);
    INFO (of.description);
    auto& knob = of.knob;
    e.hover (knob);
    auto& readout = *knob.getKnobTooltip();
    const auto centre = e.window.getLocalPoint (&knob, knob.getFaceCentre());
    const auto card = e.window.getLocalArea (&readout, readout.getLocalBounds()).toFloat();
    const bool above = card.getCentreY() < centre.y;
    REQUIRE (above == (kind == Kind::readoutAbove));

    // 1 px in from each corner, so JUCE's whole-pixel point stays on the Readout.
    const auto inside = card.reduced (1.0f);
    for (const auto target : { inside.getTopLeft(), inside.getTopRight(), inside.getBottomLeft(), inside.getBottomRight(), card.getCentre() })
    {
        for (auto start : startsAwayFromReadout (knob, kind))
        {
            if (above)
                start.y = -start.y;
            const auto from = centre + start;
            INFO ("From " << start.toString() << " off the face's centre to " << (target - centre).toString());
            e.window.pointerAtWindow (from);
            REQUIRE (Hosted::underPointer() == &knob);
            REQUIRE (knob.isTooltipShown());
            // On eq1's Dynamic Range ring the Readout is the ring's, wider than card and round it, and keeps that content
            // on the way to it (the ring's case below).
            if (! knob.isOnRing (knob.getLocalPoint (&e.window, from)))
                REQUIRE (e.window.getLocalArea (&readout, readout.getLocalBounds()).toFloat() == card);
            int lostAt = 0, step = 0;
            walk (e.window, from, target, [&] {
                ++step;
                if (lostAt == 0 && ! knob.isTooltipShown())
                    lostAt = step;
            });
            CHECK (lostAt == 0);
            CHECK (Hosted::underPointer() == &readout);
        }
    }
}

TEST_CASE ("A Knob Readout stays while the pointer moves on the way from its knob's face to a corner of it, and hides once it rests there")
{
    Hosted e;
    const auto kind = GENERATE (Kind::small, Kind::readoutAbove, Kind::ringed);
    const auto of = knobOf (e, kind);
    INFO (of.description);
    auto& knob = of.knob;
    e.hover (knob);
    auto& readout = *knob.getKnobTooltip();
    const auto centre = e.window.getLocalPoint (&knob, knob.getFaceCentre());
    const auto card = e.window.getLocalArea (&readout, readout.getLocalBounds()).toFloat().reduced (0.5f);
    const bool above = kind == Kind::readoutAbove;
    const auto start = startsAwayFromReadout (knob, kind).back();
    const auto from = centre + juce::Point<float> (start.x, above ? -start.y : start.y);
    const auto corner = above ? card.getBottomLeft() : card.getTopLeft();
    e.window.pointerAtWindow (from);
    REQUIRE (knob.isTooltipShown());

    // Most of the way, slowly, through a watchdog check at each step: longer than a rest the Readout keeps.
    const harness::VisibilityChanges changes (readout);
    int steps = 0;
    walk (e.window, from, from + (corner - from) * 0.75f, [&] {
        ++steps;
        harness::settle (staple::TipWatchdog::periodMs);
        e.window.pointerAgain();
    });
    REQUIRE (steps > staple::Knob::restOnWayChecks);
    // One step more, and a rest there, off the knob and the Readout.
    e.window.pointerAtWindow (from + (corner - from) * 0.8f);
    REQUIRE (Hosted::underPointer() != &knob);
    REQUIRE (Hosted::underPointer() != &readout);
    CHECK (changes.hides == 0);
    CHECK (knob.isTooltipShown());

    const Rest rest;
    CHECK (restUntil (e.window, [&] { return ! knob.isTooltipShown(); }));
    CHECK (rest.elapsedMs() >= staple::Knob::restOnWayChecks * staple::TipWatchdog::periodMs);
}

namespace
{
// The radius of knob's hit area: its face's, or its ring's outer edge.
float hitRadius (staple::Knob& knob, Kind kind)
{
    return kind == Kind::ringed ? ringOuterRadius (knob) : knob.getFaceRadius();
}

// A point just inside knob's hit area, on side (-1 left, 1 right), height px from its centre's level away from its
// Readout.
juce::Point<float> justInside (Hosted& e, staple::Knob& knob, Kind kind, float side, float height)
{
    const auto centre = e.window.getLocalPoint (&knob, knob.getFaceCentre());
    const float r = hitRadius (knob, kind);
    const float awayFromReadout = kind == Kind::readoutAbove ? 1.0f : -1.0f;
    return { centre.x + side * (std::sqrt (r * r - height * height) - 1.0f), centre.y + awayFromReadout * height };
}

// Whether the Readout showing is wider than knob's hit area: the way to it then reaches out beside the hit area level
// with its centre. eq1's ringed Gain knob's is, on its ring, where the Readout is the Dynamic Range's.
bool isReadoutWiderThanHitArea (staple::Knob& knob, Kind kind)
{
    return static_cast<float> (knob.getKnobTooltip()->getWidth()) > 2.0f * hitRadius (knob, kind);
}
} // namespace

// The way to the Readout reaches out beside the hit area only below where the lines from the Readout's far corners
// touch it: above that, and level with the centre where the Readout is narrower than the hit area.
TEST_CASE ("A Knob Readout hides at once when the pointer leaves its knob sideways above where the way to it reaches out")
{
    Hosted e;
    const auto kind = GENERATE (Kind::labelled, Kind::small, Kind::readoutAbove, Kind::ringed);
    const auto of = knobOf (e, kind);
    INFO (of.description);
    auto& knob = of.knob;
    e.hover (knob);
    const auto side = GENERATE (-1.0f, 1.0f);
    const auto height = GENERATE (0.0f, 0.85f);
    INFO ((side < 0 ? "Left, " : "Right, ") << height << " of the radius from the centre's level");
    const auto onEdge = justInside (e, knob, kind, side, height * hitRadius (knob, kind));
    e.window.pointerAtWindow (onEdge);
    REQUIRE (Hosted::underPointer() == &knob);
    REQUIRE (knob.isTooltipShown());
    if (height < 0.5f && isReadoutWiderThanHitArea (knob, kind))
        return; // the way reaches out level with the centre (the case below)

    // 2 px off the hit area, sideways.
    e.window.pointerAtWindow (onEdge.translated (side * 3.0f, 0.0f));
    REQUIRE (Hosted::underPointer() != &knob);
    CHECK_FALSE (knob.isTooltipShown());
}

TEST_CASE ("A Knob Readout wider than its knob's face hides on its watchdog's next check when the pointer leaves the face sideways, level with its centre")
{
    Hosted e;
    const auto kind = GENERATE (Kind::labelled, Kind::small, Kind::readoutAbove, Kind::ringed);
    const auto of = knobOf (e, kind);
    INFO (of.description);
    auto& knob = of.knob;
    e.hover (knob);
    const auto side = GENERATE (-1.0f, 1.0f);
    INFO ((side < 0 ? "Left" : "Right"));
    const auto onEdge = justInside (e, knob, kind, side, 0.0f);
    e.window.pointerAtWindow (onEdge);
    REQUIRE (knob.isTooltipShown());
    REQUIRE (isReadoutWiderThanHitArea (knob, kind));

    // Out of the face where the way to the Readout reaches out, then on out of it: the knob hears nothing more.
    e.window.pointerAtWindow (onEdge.translated (side * 3.0f, 0.0f));
    REQUIRE (Hosted::underPointer() != &knob);
    e.window.pointerAtWindow (onEdge.translated (side * 20.0f, 0.0f));
    CHECK (harness::settleUntil ([&] { return ! knob.isTooltipShown(); }));
}

// The strip beside a Small face, level with its centre, is on the way to its Readout (the Readout is the wider), off
// the face, ring and Readout: a pointer resting there is not crossing to the Readout.
TEST_CASE ("A Knob Readout hides once the pointer rests in the strip beside its knob's face, within the rest it keeps plus one watchdog period")
{
    Hosted e;
    auto& knob = e.small;
    e.hover (knob);
    const auto side = GENERATE (-1.0f, 1.0f);
    INFO ((side < 0 ? "Left" : "Right"));
    const auto onEdge = justInside (e, knob, Kind::small, side, 0.0f);
    e.window.pointerAtWindow (onEdge);
    REQUIRE (knob.isTooltipShown());

    // 2 px off the face, sideways, into the strip.
    e.window.pointerAtWindow (onEdge.translated (side * 3.0f, 0.0f));
    REQUIRE (Hosted::underPointer() != &knob);
    REQUIRE (Hosted::underPointer() != knob.getKnobTooltip());
    REQUIRE (knob.isTooltipShown());

    const Rest rest;
    CHECK (restUntil (e.window, [&] { return ! knob.isTooltipShown(); }));
    // The watchdog's first check, within one period of the rest's start, finds the pointer there; the restOnWayChecks
    // after it span the rest the Readout keeps, and the last of them hides it: no sooner than that rest, and within it
    // plus one period.
    CHECK (rest.elapsedMs() >= staple::Knob::restOnWayChecks * staple::TipWatchdog::periodMs);
    CHECK (rest.periods <= staple::Knob::restOnWayChecks + 1);
}

namespace
{
// The shortest distance from p to the segment from a to b.
float distanceToSegment (juce::Point<float> p, juce::Point<float> a, juce::Point<float> b)
{
    const auto ab = b - a;
    const float t = juce::jlimit (0.0f, 1.0f, (p - a).getDotProduct (ab) / ab.getDotProduct (ab));
    return p.getDistanceFrom (a + ab * t);
}
} // namespace

// The ring's Readout (eq1's Dynamic Range one, 136 px) is wider than the knob's own (86 px), and the way to it is built
// from it: the content that showed as the pointer left the ring stays until the pointer is back on the face or ring,
// read again as the knob's value and the ring's move on the way (automation, eq1's Dynamic Range).
TEST_CASE ("A ring's own Knob Readout keeps its content on every straight move from the ring, past the face, to its corners")
{
    Hosted e;
    auto& knob = e.ringedGain();
    auto& ring = e.dynamicRange();
    const auto centre = e.window.getLocalPoint (&knob, knob.getFaceCentre());
    const float r = knob.getFaceRadius();
    e.hover (knob);
    auto& readout = *knob.getKnobTooltip();
    const auto ownCard = e.window.getLocalArea (&readout, readout.getLocalBounds()).toFloat();
    const auto ownTitle = readout.titleText();
    const auto ringTitle = ring.ringTitle();
    REQUIRE (ownTitle != ringTitle);

    int moves = 0;
    for (const float degrees : { 0.0f, 30.0f, 60.0f, 90.0f, 120.0f, 150.0f, 180.0f })
    {
        const auto from = centre + direction (degrees) * ringMiddleRadius (knob);
        e.window.pointerAtWindow (from);
        REQUIRE (Hosted::underPointer() == &knob);
        REQUIRE (knob.isTooltipShown());
        REQUIRE (readout.titleText() == ringTitle);
        const auto card = e.window.getLocalArea (&readout, readout.getLocalBounds()).toFloat();
        REQUIRE (card.getWidth() > ownCard.getWidth());

        const auto inside = card.reduced (1.0f);
        for (const auto target : { inside.getTopLeft(), inside.getTopRight(), inside.getBottomLeft(), inside.getBottomRight() })
        {
            // Past the face, not over it: back on the face, the Readout is the knob's own again.
            if (distanceToSegment (centre, from, target) <= r + 2.0f)
                continue;
            ++moves;
            INFO ("From " << (from - centre).toString() << " off the face's centre to " << (target - centre).toString());
            e.window.pointerAtWindow (from);
            REQUIRE (readout.titleText() == ringTitle);
            const auto gainBefore = knob.getValue(), rangeBefore = ring.getValue();
            int lostAt = 0, changedAt = 0, step = 0;
            walk (e.window, from, target, [&] {
                ++step;
                if (step == 4)
                    knob.setValue (gainBefore + 1.0);
                if (step == 6)
                    ring.setValue (rangeBefore + (rangeBefore > 0.0 ? -3.0 : 3.0)); // calls knob.ringChanged
                if (lostAt == 0 && ! knob.isTooltipShown())
                    lostAt = step;
                if (changedAt == 0 && e.window.getLocalArea (&readout, readout.getLocalBounds()).toFloat() != card)
                    changedAt = step;
            });
            REQUIRE (step > 6);
            CHECK (lostAt == 0);
            CHECK (changedAt == 0);
            CHECK (Hosted::underPointer() == &readout);
            CHECK (readout.titleText() == ringTitle);
            CHECK (ring.getValue() != rangeBefore);
            CHECK (readout.valueText() == ring.ringValue());
            knob.setValue (gainBefore);
            ring.setValue (rangeBefore);
        }
    }
    REQUIRE (moves >= 4);

    // Back on the face: the knob's own.
    e.window.pointerAtWindow (centre);
    CHECK (readout.titleText() == ownTitle);
    CHECK (e.window.getLocalArea (&readout, readout.getLocalBounds()).toFloat() == ownCard);
}

namespace
{
// Where the pointer lands round a ringed knob's outer edge, at fractional positions as the UI Scale gives them. JUCE
// finds the component under the pointer at the whole pixel nearest it, so it can be on the knob at a position just
// outside the edge, or off it at one just inside.
enum class Edge
{
    // Guards: on the ring at its position and at its pixel alike, so they pass whether the knob reads the ring at
    // either (Knob::isOnRingAsHit).
    exactly,
    justInside,
    // The case that fails if the knob reads the ring at the pointer's position rather than its pixel: JUCE has the
    // pointer on the knob, at its pixel on the ring, while its position is off the ring.
    justOutsideOnItsPixel,
    // A guard too: JUCE has the pointer off the knob, which hears nothing there to read the ring by, so the Readout
    // keeps its content as the pointer leaving keeps it, wherever the knob reads the ring.
    justOutsideOffItsPixel
};

const char* describe (Edge edge)
{
    switch (edge)
    {
        case Edge::exactly: return "exactly on the ring's outer edge";
        case Edge::justInside: return "just inside it";
        case Edge::justOutsideOnItsPixel: return "just outside it, its nearest whole pixel on the ring";
        case Edge::justOutsideOffItsPixel: return "just outside it, its nearest whole pixel off the ring";
    }
    return "";
}

// A point near 30 degrees above level, right of the face's centre, in the window's coordinates, edge's distance from
// the ring's outer edge (radius) as JUCE sees it.
std::optional<juce::Point<float>> pointByEdge (Hosted& e, staple::Knob& knob, float radius, Edge edge)
{
    const auto centre = knob.getFaceCentre();
    for (float degrees = 30.0f; degrees < 40.0f; degrees += 0.25f)
    {
        for (float off = 0.05f; off < 0.5f; off += 0.05f)
        {
            const float distance = edge == Edge::exactly ? radius : edge == Edge::justInside ? radius - off : radius + off;
            const auto at = e.window.getLocalPoint (&knob, centre + direction (degrees) * distance);
            const bool onItsPixel = knob.getLocalPoint (&e.window, at).roundToInt().toFloat().getDistanceFrom (centre) <= radius;
            if (edge != Edge::justOutsideOnItsPixel && edge != Edge::justOutsideOffItsPixel)
                return at;
            if (onItsPixel == (edge == Edge::justOutsideOnItsPixel))
                return at;
        }
    }
    return std::nullopt;
}
} // namespace

TEST_CASE ("A ring's own Knob Readout keeps its content as the pointer leaves the ring across its outer edge")
{
    Hosted e;
    auto& knob = e.ringedGain();
    const auto ringTitle = e.dynamicRange().ringTitle();
    const float radius = ringOuterRadius (knob);
    const auto edge = GENERATE (Edge::exactly, Edge::justInside, Edge::justOutsideOnItsPixel, Edge::justOutsideOffItsPixel);
    INFO ("The pointer lands " << describe (edge));
    const auto at = pointByEdge (e, knob, radius, edge);
    REQUIRE (at.has_value());

    // On the ring at 30 degrees, then out to the edge.
    e.window.pointerAt (knob, knob.getFaceCentre() + direction (30.0f) * ringMiddleRadius (knob));
    REQUIRE (knob.isTooltipShown());
    auto& readout = *knob.getKnobTooltip();
    REQUIRE (readout.titleText() == ringTitle);
    e.window.pointerAtWindow (*at);
    CHECK (readout.titleText() == ringTitle);

    // On toward the Readout's far bottom corner.
    const auto card = e.window.getLocalArea (&readout, readout.getLocalBounds()).toFloat();
    walk (e.window, *at, card.reduced (1.0f).getBottomRight(), [&] {
        CHECK (knob.isTooltipShown());
        CHECK (readout.titleText() == ringTitle);
    });
    CHECK (Hosted::underPointer() == &readout);
}

// A press where JUCE finds the knob under the pointer, just outside the ring's outer edge, goes to the ring, as the
// pointer there is on it. On eq1's Dynamic Range ring, a press starts the ring's drag: the drag moves the Band's Dynamic
// Range, not its Gain.
TEST_CASE ("A press just outside a ring's outer edge, on its nearest whole pixel, goes to the ring")
{
    Hosted e;
    auto& knob = e.ringedGain();
    auto& ring = e.dynamicRange();
    const float radius = ringOuterRadius (knob);
    const auto at = pointByEdge (e, knob, radius, Edge::justOutsideOnItsPixel);
    REQUIRE (at.has_value());
    e.window.pointerAtWindow (*at);
    REQUIRE (Hosted::underPointer() == &knob);
    const auto before = knob.getValue(), rangeBefore = ring.getValue();
    {
        harness::HostWindow::ButtonHeld held (e.window, knob, knob.getLocalPoint (&e.window, *at));
        held.dragTo (knob, knob.getLocalPoint (&e.window, *at).translated (0.0f, -40.0f));
    }
    CHECK (ring.getValue() != rangeBefore);
    CHECK (knob.getValue() == before);
}

namespace
{
// The pointer on the Gain knob's ring, then straight onto its Readout's centre in one move, which leaves the face
// alone: the Readout keeps the ring's content.
staple::KnobTooltip& ontoRingsReadout (Hosted& e, staple::Knob& knob)
{
    e.window.pointerAt (knob, knob.getFaceCentre() + direction (30.0f) * ringMiddleRadius (knob));
    REQUIRE (knob.isTooltipShown());
    REQUIRE (knob.isTooltipOnRing());
    auto& readout = *knob.getKnobTooltip();
    e.window.pointerAt (readout, readout.getLocalBounds().toFloat().getCentre());
    REQUIRE (Hosted::underPointer() == &readout);
    REQUIRE (knob.isTooltipOnRing());
    return readout;
}

// value typed into the open field, and Enter.
void typeAndEnter (staple::KnobTooltip& readout, const juce::String& value)
{
    REQUIRE (readout.getEditor() != nullptr);
    readout.getEditor()->setText (value);
    readout.getEditor()->keyPressed (juce::KeyPress (juce::KeyPress::returnKey));
    REQUIRE_FALSE (readout.isEditing());
}
} // namespace

TEST_CASE ("A double-click on a ring's own Knob Readout opens no field when the ring takes no typing")
{
    Hosted e;
    auto& readout = ontoRingsReadout (e, e.untypeableRingedGain());
    e.doubleClick (readout, readout.getLocalBounds().toFloat().getCentre());
    CHECK_FALSE (readout.isEditing());
}

TEST_CASE ("A double-click on a ringed knob's own Knob Readout types into the knob")
{
    Hosted e;
    auto& knob = e.ringedGain();
    auto& ring = e.dynamicRange();
    e.hover (knob);
    auto& readout = *knob.getKnobTooltip();
    e.window.pointerAt (readout, readout.getLocalBounds().toFloat().getCentre());
    REQUIRE_FALSE (knob.isTooltipOnRing());
    const auto rangeBefore = ring.getValue();

    e.doubleClick (readout, readout.getLocalBounds().toFloat().getCentre());
    REQUIRE (readout.isEditing());
    typeAndEnter (readout, "4");

    CHECK (std::abs (knob.getValue() - 4.0) < 0.05);
    CHECK (ring.getValue() == rangeBefore);
}

// Closely packed knobs, as on eq1's Band panel: the way to one knob's Readout crosses its neighbour's face.
TEST_CASE ("One Knob Readout shows at a time: a neighbour's hides it on the way to it, unless it is being typed into")
{
    Hosted e;
    // The Band panel's Frequency knob and the Gain knob beside it.
    auto& a = e.frequency();
    auto& b = e.gain();
    REQUIRE (a.getParentComponent() == b.getParentComponent());

    const bool typing = GENERATE (false, true);
    INFO ((typing ? "a's Readout is being typed into" : "a's Readout is not being typed into"));
    e.hover (a);
    auto& readoutA = *a.getKnobTooltip();
    if (typing)
    {
        readoutA.startEditing();
        REQUIRE (readoutA.isEditing());
    }

    const auto onB = e.window.getLocalPoint (&b, b.getFaceCentre());
    int both = 0, step = 0;
    walk (e.window, e.window.getLocalPoint (&a, a.getFaceCentre()), onB, [&] {
        ++step;
        if (! typing && both == 0 && a.isTooltipShown() && b.isTooltipShown())
            both = step;
    });
    REQUIRE (Hosted::underPointer() == &b);
    CHECK (b.isTooltipShown());
    CHECK (both == 0);
    CHECK (a.isTooltipShown() == typing);

    // Through the watchdogs' checks, and the pointer again on b, which JUCE's Desktop timer may have moved to the
    // machine's real mouse meanwhile.
    harness::settle (3 * staple::TipWatchdog::periodMs);
    e.window.pointerAgain();
    CHECK (b.isTooltipShown());
    CHECK (a.isTooltipShown() == typing);
    if (typing)
        CHECK (readoutA.isEditing());
}
