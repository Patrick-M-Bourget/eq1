#include "AnalyzerPopover.h"
#include "EditorHarness.h"
#include "FooterBar.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace
{

const juce::KeyPress up (juce::KeyPress::upKey), down (juce::KeyPress::downKey), escape (juce::KeyPress::escapeKey),
    space (juce::KeyPress::spaceKey), returnKey (juce::KeyPress::returnKey);

struct Analyzer : harness::OpenEditor
{
    eq1::FooterBar& footer = *harness::findChild<eq1::FooterBar> (*editor);
    eq1::AnalyzerButton& button = *harness::findChild<eq1::AnalyzerButton> (*editor);
    eq1::AnalyzerPopover& popover = footer.analyzerPopover();

    template <typename T>
    T& titled (const juce::String& title)
    {
        auto found = findAll<T> ([&title] (T& c) { return c.getTitle() == title; });
        if (found.empty())
            FAIL ("Nothing is titled " << title);
        return *found.front();
    }

    static juce::MouseEvent mouse (juce::Component& component, juce::Point<float> position, juce::ModifierKeys mods)
    {
        const auto now = juce::Time::getCurrentTime();
        return { juce::Desktop::getInstance().getMainMouseSource(),
                 position,
                 mods,
                 juce::MouseInputSource::defaultPressure,
                 juce::MouseInputSource::defaultOrientation,
                 juce::MouseInputSource::defaultRotation,
                 juce::MouseInputSource::defaultTiltX,
                 juce::MouseInputSource::defaultTiltY,
                 &component,
                 &component,
                 now,
                 position,
                 now,
                 1,
                 false };
    }

    // A left-click in the middle of component.
    static void click (juce::Component& component)
    {
        const auto centre = component.getLocalBounds().toFloat().getCentre();
        component.mouseDown (mouse (component, centre, juce::ModifierKeys::leftButtonModifier));
        component.mouseUp (mouse (component, centre, {}));
    }

    void open()
    {
        if (! popover.isOpen())
            button.onClick();
        REQUIRE (popover.isOpen());
    }

    eq1::AnalyzerSettings settings() { return processor.analyzerSettings(); }
    int undoSteps() { return processor.editHistory().undoSteps(); }
};

} // namespace

TEST_CASE ("The Analyzer button reads the Pre and Post settings: Pre + Post, Pre, Post or Off")
{
    CHECK (eq1::analyzerButtonText ({ .showPreEq = true, .showPostEq = true }) == "Pre + Post");
    CHECK (eq1::analyzerButtonText ({ .showPreEq = true, .showPostEq = false }) == "Pre");
    CHECK (eq1::analyzerButtonText ({ .showPreEq = false, .showPostEq = true }) == "Post");
    CHECK (eq1::analyzerButtonText ({ .showPreEq = false, .showPostEq = false, .showSidechain = true }) == "Off");

    Analyzer host;
    host.settle();
    CHECK (host.button.getTitle() == "Analyzer");
    CHECK (host.button.getButtonText() == "Pre + Post");
    auto settings = host.settings();
    settings.showPreEq = false;
    host.processor.setAnalyzerSettings (settings);
    host.settle (300);
    CHECK (host.button.getButtonText() == "Post");
    CHECK (host.button.getWidth() >= 92);
    CHECK (host.button.getHeight() == 28);
}

TEST_CASE ("The Analyzer button opens its popover above it, left-aligned to it, and a second click, Escape or a click outside closes it")
{
    Analyzer host;
    CHECK_FALSE (host.popover.isOpen());
    host.open();
    host.settle (300); // popped in
    CHECK (host.popover.getCardBounds().getWidth() == 260);
    const auto card = host.editor->getLocalArea (&host.popover, host.popover.getCardBounds());
    const auto anchor = host.editor->getLocalArea (&host.button, host.button.getLocalBounds());
    CHECK (card.getBottom() < anchor.getY());
    // Left-aligned to the "Analyzer" label before the button, as the prototype has it.
    auto* label = harness::findChild<juce::Label> (host.footer, [] (juce::Label& l) { return l.getText() == "Analyzer"; });
    REQUIRE (label != nullptr);
    CHECK (label->getRight() < host.button.getX());
    CHECK (card.getX() == host.editor->getLocalArea (label, label->getLocalBounds()).getX());

    host.button.onClick();
    CHECK_FALSE (host.popover.isOpen());

    // Opened by a click, it leaves focus on the display, where Escape closes it.
    host.display.grabKeyboardFocus();
    host.open();
    CHECK (host.display.hasKeyboardFocus (false));
    CHECK (host.press (escape));
    CHECK_FALSE (host.popover.isOpen());
    CHECK (host.display.hasKeyboardFocus (false));

    host.open();
    host.popover.mouseDown (Analyzer::mouse (host.display, { 20.0f, 20.0f }, juce::ModifierKeys::leftButtonModifier));
    CHECK_FALSE (host.popover.isOpen());
    host.open();
}

TEST_CASE ("Pre, Post, Sidechain and Peak Hold each toggle their setting, and Pre and Post show on the button")
{
    Analyzer host;
    host.open();
    const int steps = host.undoSteps();
    const auto toggle = [&host] (const char* title) {
        auto& b = host.titled<juce::Button> (title);
        b.grabKeyboardFocus();
        REQUIRE (b.hasKeyboardFocus (false));
        CHECK (host.press (returnKey));
    };

    toggle ("Analyzer Pre-EQ");
    CHECK_FALSE (host.settings().showPreEq);
    CHECK (host.button.getButtonText() == "Post");
    toggle ("Analyzer Post-EQ");
    CHECK_FALSE (host.settings().showPostEq);
    CHECK (host.button.getButtonText() == "Off");
    toggle ("Analyzer Pre-EQ");
    CHECK (host.settings().showPreEq);
    CHECK (host.button.getButtonText() == "Pre");

    CHECK_FALSE (host.settings().showSidechain);
    toggle ("Analyzer Sidechain");
    CHECK (host.settings().showSidechain);
    CHECK (host.titled<juce::Button> ("Analyzer Sidechain").getToggleState());

    CHECK (host.settings().peakHold);
    toggle ("Analyzer Peak Hold");
    CHECK_FALSE (host.settings().peakHold);
    // Space too.
    host.titled<juce::Button> ("Analyzer Peak Hold").grabKeyboardFocus();
    host.press (space);
    CHECK (host.settings().peakHold);

    CHECK (host.undoSteps() == steps);
}

TEST_CASE ("A click on Range, Resolution or Speed moves it to the next value in the code's list, wrapping")
{
    Analyzer host;
    host.open();
    const int steps = host.undoSteps();

    auto& range = host.titled<juce::ComboBox> ("Analyzer Range");
    CHECK (range.getText() == "90 dB");
    Analyzer::click (range);
    CHECK (host.settings().rangeDb == 120);
    CHECK (range.getText() == "120 dB");
    Analyzer::click (range);
    CHECK (host.settings().rangeDb == 60);
    Analyzer::click (range);
    CHECK (host.settings().rangeDb == 90);

    auto& resolution = host.titled<juce::ComboBox> ("Analyzer Resolution");
    std::vector<juce::String> seen;
    for (int i = 0; i < 4; ++i)
    {
        Analyzer::click (resolution);
        seen.push_back (resolution.getText());
    }
    CHECK (seen == std::vector<juce::String> { "High", "Maximum", "Low", "Medium" });
    CHECK (host.settings().resolution == eq1::AnalyzerResolution::medium);

    auto& speed = host.titled<juce::ComboBox> ("Analyzer Speed");
    seen.clear();
    for (int i = 0; i < 5; ++i)
    {
        Analyzer::click (speed);
        seen.push_back (speed.getText());
    }
    CHECK (seen == std::vector<juce::String> { "Fast", "Very Fast", "Very Slow", "Slow", "Medium" });
    Analyzer::click (speed);
    CHECK (host.settings().speed == eq1::AnalyzerSpeed::fast);

    // Space and Return move it on too; the arrows step it without wrapping.
    speed.grabKeyboardFocus();
    REQUIRE (speed.hasKeyboardFocus (false));
    CHECK (host.press (space));
    CHECK (host.settings().speed == eq1::AnalyzerSpeed::veryFast);
    CHECK (host.press (returnKey));
    CHECK (host.settings().speed == eq1::AnalyzerSpeed::verySlow);
    CHECK (host.press (up));
    CHECK (host.settings().speed == eq1::AnalyzerSpeed::verySlow);
    CHECK (host.press (down));
    CHECK (host.settings().speed == eq1::AnalyzerSpeed::slow);

    CHECK (host.undoSteps() == steps);
}

TEST_CASE ("Analyzer Tilt cycles Off, 3, 4.5 and 6 dB/oct, a value in between going to the next listed one above")
{
    CHECK (eq1::nextAnalyzerTilt (0.0) == 3.0);
    CHECK (eq1::nextAnalyzerTilt (3.0) == 4.5);
    CHECK (eq1::nextAnalyzerTilt (4.5) == 6.0);
    CHECK (eq1::nextAnalyzerTilt (6.0) == 0.0);
    CHECK (eq1::nextAnalyzerTilt (0.5) == 3.0);
    CHECK (eq1::nextAnalyzerTilt (3.5) == 4.5);
    CHECK (eq1::nextAnalyzerTilt (5.5) == 6.0);

    Analyzer host;
    host.open();
    const int steps = host.undoSteps();
    auto& tilt = host.titled<juce::ComboBox> ("Analyzer Tilt");
    CHECK (tilt.getText() == "4.5 dB/oct");
    std::vector<juce::String> seen;
    for (int i = 0; i < 4; ++i)
    {
        Analyzer::click (tilt);
        seen.push_back (tilt.getText());
    }
    CHECK (seen == std::vector<juce::String> { "6 dB/oct", "Off", "3 dB/oct", "4.5 dB/oct" });
    CHECK (host.settings().tiltDbPerOctave == 4.5);

    // The arrows step it by 0.5 dB/oct within 0 to 6, up raising it.
    tilt.grabKeyboardFocus();
    REQUIRE (tilt.hasKeyboardFocus (false));
    CHECK (host.press (up));
    CHECK (host.settings().tiltDbPerOctave == 5.0);
    CHECK (tilt.getText() == "5 dB/oct");
    CHECK (host.press (down));
    CHECK (host.press (down));
    CHECK (host.settings().tiltDbPerOctave == 4.0);
    Analyzer::click (tilt);
    CHECK (host.settings().tiltDbPerOctave == 4.5);
    for (int i = 0; i < 12; ++i)
        host.press (up);
    CHECK (host.settings().tiltDbPerOctave == 6.0);
    for (int i = 0; i < 14; ++i)
        host.press (down);
    CHECK (host.settings().tiltDbPerOctave == 0.0);
    CHECK (tilt.getText() == "Off");
    host.press (up);
    CHECK (host.settings().tiltDbPerOctave == 0.5);
    CHECK (tilt.getText() == "0.5 dB/oct");

    CHECK (host.undoSteps() == steps);
}

TEST_CASE ("The popover shows settings restored with the session, and the session keeps what it sets")
{
    Analyzer host;
    host.open();
    Analyzer::click (host.titled<juce::ComboBox> ("Analyzer Range"));
    juce::MemoryBlock state;
    host.processor.getStateInformation (state);

    {
        eq1::PluginProcessor restored;
        restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
        CHECK (restored.analyzerSettings().rangeDb == 120);
    }

    // As a host restores a session into an open editor.
    host.processor.setAnalyzerSettings ({ .showPreEq = false,
                                          .showPostEq = true,
                                          .showSidechain = true,
                                          .rangeDb = 60,
                                          .speed = eq1::AnalyzerSpeed::slow,
                                          .resolution = eq1::AnalyzerResolution::maximum,
                                          .tiltDbPerOctave = 3.0,
                                          .peakHold = false });
    host.button.onClick();
    CHECK_FALSE (host.popover.isOpen());
    host.open();
    CHECK (host.titled<juce::ComboBox> ("Analyzer Range").getText() == "60 dB");
    CHECK (host.titled<juce::ComboBox> ("Analyzer Speed").getText() == "Slow");
    CHECK (host.titled<juce::ComboBox> ("Analyzer Resolution").getText() == "Maximum");
    CHECK (host.titled<juce::ComboBox> ("Analyzer Tilt").getText() == "3 dB/oct");
    CHECK_FALSE (host.titled<juce::Button> ("Analyzer Pre-EQ").getToggleState());
    CHECK (host.titled<juce::Button> ("Analyzer Post-EQ").getToggleState());
    CHECK (host.titled<juce::Button> ("Analyzer Sidechain").getToggleState());
    CHECK_FALSE (host.titled<juce::Button> ("Analyzer Peak Hold").getToggleState());
    host.settle (300);
    CHECK (host.button.getButtonText() == "Post");
}

// Renders the Analyzer popover for checking by hand against the prototype: EQ1_ANALYZER_SNAPSHOT=<path>
// writes the window's bottom left at 2x with the popover open.
TEST_CASE ("Analyzer popover snapshot", "[.screens]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("EQ1_ANALYZER_SNAPSHOT", {});
    Analyzer host;
    host.settle (300);
    host.open();
    host.settle (300);
    const auto area = host.editor->getLocalBounds().removeFromBottom (340).removeFromLeft (520);
    const auto image = host.editor->createComponentSnapshot (area, true, 2.0f);
    CHECK (image.isValid());
    if (path.isEmpty())
        return;
    juce::File file (path);
    file.deleteFile();
    juce::FileOutputStream stream (file);
    juce::PNGImageFormat().writeImageToStream (image, stream);
}
