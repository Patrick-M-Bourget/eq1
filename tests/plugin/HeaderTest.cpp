#include "EditorHarness.h"
#include "HeaderBar.h"
#include "PresetBar.h"
#include "staple/Tokens.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using eq1::CompareSide;
namespace colour = staple::tokens::colour;

namespace
{

struct Header : harness::OpenEditor
{
    template <typename T = juce::Button>
    T& titled (const juce::String& title)
    {
        auto found = findAll<T> ([&title] (T& c) { return c.getTitle() == title; });
        if (found.empty())
            FAIL ("Nothing is titled " << title);
        return *found.front();
    }

    eq1::PresetNameButton& presets() { return titled<eq1::PresetNameButton> ("Presets"); }
    eq1::CompareButton& compare() { return titled<eq1::CompareButton> ("A/B Compare"); }
    juce::Button& copy() { return *findAll<juce::Button> ([] (juce::Button& b) { return b.getName() == "Copy"; }).front(); }

    // Settings saved as a Preset, as the Presets bar would load them.
    void loadPreset (const juce::String& name)
    {
        set (2, "in_use", 1.0f);
        processor.loadPreset (processor.presetState(), name);
        settle (300);
    }
};

} // namespace

TEST_CASE ("The header shows the wordmark at its left")
{
    Header host;
    auto& header = *harness::findChild<eq1::HeaderBar> (*host.editor);
    CHECK (header.wordmarkArea().getX() == 6);
    CHECK (header.wordmarkArea().getRight() < host.titled ("Undo").getX());
    CHECK (juce::String (staple::wordmark) == "eq1");
}

TEST_CASE ("The Preset group stays centred in the window at any width")
{
    Header host;
    const int width = GENERATE (960, 1200, 1600);
    const bool longName = GENERATE (false, true);
    CAPTURE (width, longName);
    if (longName)
        host.loadPreset ("A Very Long Preset Name For Warm Vocal Presence With Plenty Of Air On Top");
    host.editor->setSize (width, 760);
    for (auto* button : { &host.titled ("Presets"), &host.titled ("Previous Preset"), &host.titled ("Next Preset") })
        REQUIRE (button->isShowing());

    const auto in = [&host] (juce::Component& c) { return host.editor->getLocalArea (&c, c.getLocalBounds()); };
    const auto group = in (host.titled ("Previous Preset")).getUnion (in (host.titled ("Next Preset")));
    CHECK (std::abs (group.getCentreX() * 2 - width) <= 2);
    CHECK (in (host.titled ("Presets")).getWidth() >= 250);
    CHECK (in (host.titled ("Presets")).getHeight() == 36);
    // The side groups don't overlap it.
    CHECK (in (host.titled ("Undo")).getX() > group.getRight());
}

TEST_CASE ("With no Loaded Preset the name reads No Preset in text3; a Modified side shows the dot")
{
    Header host;
    CHECK (host.presets().getButtonText() == "No Preset");
    CHECK (host.presets().nameInk() == colour::text3);
    CHECK_FALSE (host.presets().showsModified());

    host.loadPreset ("Warm Vocal");
    CHECK (host.presets().getButtonText() == "Warm Vocal");
    CHECK (host.presets().getTooltip() == "Warm Vocal");
    CHECK (host.presets().nameInk() == colour::text1);
    CHECK_FALSE (host.presets().showsModified());

    host.set (2, "gain", 4.0f);
    host.settle (300);
    CHECK (host.presets().showsModified());
    CHECK (host.presets().getButtonText() == "Warm Vocal"); // no *

    host.set (2, "gain", 0.0f);
    host.settle (300);
    CHECK_FALSE (host.presets().showsModified());
}

TEST_CASE ("Copy's tooltip names the direction, and it reads Copied for 1 s after a click")
{
    Header host;
    CHECK (host.copy().getTitle() == "Copy A to B");
    CHECK (host.copy().getTooltip() == "Copy A to B");
    host.processor.selectCompareSide (CompareSide::B);
    host.settle (300);
    CHECK (host.copy().getTooltip() == "Copy B to A");
    CHECK (host.copy().getTitle() == "Copy B to A");

    host.set ("band1_gain", 5.0f);
    host.copy().onClick();
    host.settle (100);
    CHECK (host.copy().getButtonText() == "Copied");
    host.processor.selectCompareSide (CompareSide::A);
    CHECK_THAT (host.value ("band1_gain"), Catch::Matchers::WithinAbs (5.0, 1.0e-4));
    host.settle (600);
    CHECK (host.copy().getButtonText() == "Copied");
    host.settle (500);
    CHECK (host.copy().getButtonText() == "Copy");
}

TEST_CASE ("The A/B Compare button switches sides as one undo step, its active letter lit")
{
    Header host;
    auto& history = host.processor.editHistory();
    host.settle (300);
    CHECK (host.compare().letterInk (CompareSide::A) == colour::text1);
    CHECK (host.compare().letterInk (CompareSide::B) == colour::text4);

    const int steps = history.undoSteps();
    host.compare().onClick();
    host.settle (300);
    CHECK (host.processor.compareSide() == CompareSide::B);
    CHECK (history.undoSteps() == steps + 1);
    CHECK (host.compare().letterInk (CompareSide::A) == colour::text4);
    CHECK (host.compare().letterInk (CompareSide::B) == colour::text1);

    host.titled ("Undo").onClick();
    host.settle (300);
    CHECK (host.processor.compareSide() == CompareSide::A);
    CHECK (host.compare().letterInk (CompareSide::A) == colour::text1);
}

TEST_CASE ("Undo and Redo are disabled when there is nothing to undo or redo")
{
    Header host;
    CHECK_FALSE (host.titled ("Undo").isEnabled());
    CHECK_FALSE (host.titled ("Redo").isEnabled());
    host.parameter ("band1_gain").beginChangeGesture();
    host.set ("band1_gain", 3.0f);
    host.parameter ("band1_gain").endChangeGesture();
    host.settle (300);
    CHECK (host.titled ("Undo").isEnabled());
    CHECK_FALSE (host.titled ("Redo").isEnabled());
    host.titled ("Undo").onClick();
    host.settle (100);
    CHECK_FALSE (host.titled ("Undo").isEnabled());
    CHECK (host.titled ("Redo").isEnabled());
}

// Renders the header for checking by hand against the prototype (harness::writeSnapshot):
// header-no-preset and header-modified-copied, each the window's top at 2x.
TEST_CASE ("Header snapshots: no Loaded Preset, and Modified on B just after Copy", "[.screens]")
{
    Header host;
    host.settle (300);
    const auto write = [&] (const char* name) {
        harness::writeSnapshot (*host.editor, juce::String ("header-") + name, host.editor->getLocalBounds().removeFromTop (80));
    };
    write ("no-preset");
    host.loadPreset ("Warm Vocal Presence");
    host.compare().onClick();
    host.set (2, "gain", 3.0f);
    host.settle (300);
    host.copy().onClick();
    host.settle (100);
    write ("modified-copied");
}
