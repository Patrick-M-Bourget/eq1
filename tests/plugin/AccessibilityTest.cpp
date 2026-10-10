#include "EditorHarness.h"
#include "staple/controls/Knob.h"
#include "staple/controls/Popover.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <vector>

using harness::OpenEditor;

namespace
{

// What a screen reader is told about one element of the editor.
struct Element
{
    juce::Component* component;
    juce::AccessibilityRole role;
    juce::String title, value;
};

bool isBandHandle (juce::Component& c)
{
    return dynamic_cast<eq1::EqDisplay*> (c.getParentComponent()) != nullptr && c.getName().startsWith ("Band ");
}

// The roles a screen reader stops at to operate something.
bool isInteractive (juce::Component& c, juce::AccessibilityRole role)
{
    using Role = juce::AccessibilityRole;
    return role == Role::button || role == Role::toggleButton || role == Role::radioButton || role == Role::comboBox || role == Role::slider
           || role == Role::editableText || isBandHandle (c);
}

void setLayout (juce::AudioProcessor& processor, const juce::AudioChannelSet& channels)
{
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (channels);
    layout.inputBuses.add (juce::AudioChannelSet::disabled());
    layout.outputBuses.add (channels);
    REQUIRE (processor.setBusesLayout (layout));
    processor.prepareToPlay (48000.0, 512);
}

struct Editor : OpenEditor
{
    // Every element showing, at any depth, that a screen reader is told about.
    std::vector<Element> elements()
    {
        std::vector<Element> found;
        std::function<void (juce::Component&)> visit = [&] (juce::Component& parent) {
            for (auto* child : parent.getChildren())
            {
                if (! child->isShowing())
                    continue;
                if (auto* handler = child->getAccessibilityHandler(); handler != nullptr && ! handler->getCurrentState().isIgnored())
                {
                    const auto* value = handler->getValueInterface();
                    found.push_back ({ child, handler->getRole(), handler->getTitle(), value != nullptr ? value->getCurrentValueAsString() : juce::String() });
                }
                visit (*child);
            }
        };
        visit (*editor);
        return found;
    }

    std::vector<Element> interactive()
    {
        auto all = elements();
        std::erase_if (all, [] (const Element& e) { return ! isInteractive (*e.component, e.role); });
        return all;
    }

    // The interactive elements' titles, sorted.
    std::vector<juce::String> names()
    {
        std::vector<juce::String> titles;
        for (const auto& e : interactive())
            titles.push_back (e.title);
        std::sort (titles.begin(), titles.end());
        return titles;
    }

    Element element (const juce::String& title)
    {
        for (const auto& e : elements())
            if (e.title == title)
                return e;
        FAIL ("No element is named " << title);
        return {};
    }

    void select (double frequency, double gainOnDisplay)
    {
        settle();
        click (at (frequency).translated (0.0f, -static_cast<float> (gainOnDisplay / 12.0 * (display.getHeight() * 0.5 - 9.0))));
        settle();
    }

    // Opens what the footer's button titled title shows ("Analyzer" its call-out, "Output" the output
    // popover), as a click does, and closes it again, as Escape does.
    void openCallOut (const juce::String& title)
    {
        auto* button = harness::findChild<juce::Button> (*editor, [&title] (juce::Button& b) { return b.getTitle() == title; });
        REQUIRE (button != nullptr);
        button->onClick();
    }
    void closeCallOut()
    {
        if (auto* popover = harness::findChild<staple::Popover> (*editor, [] (staple::Popover& p) { return p.isOpen(); }))
        {
            popover->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
            return;
        }
        auto* box = harness::findChild<juce::CallOutBox> (*editor, [] (juce::CallOutBox& b) { return b.isVisible(); });
        REQUIRE (box != nullptr);
        box->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
    }

    void openPresetBrowser()
    {
        auto* presets = harness::findChild<juce::Button> (*editor, [] (juce::Button& b) { return b.getTitle() == "Presets"; });
        REQUIRE (presets != nullptr);
        presets->onClick();
        settle();
    }
};

// A Dynamic Bell in Band 4 on a Free Detection Range, on stereo and selected, so the Band panel
// shows every control but Brickwall and Slope.
struct EveryControl : Editor
{
    EveryControl()
    {
        setLayout (processor, juce::AudioChannelSet::stereo());
        addBand (4, 1000.0f, 3.0f, 0.0f);
        set (4, "dynamic_range", 6.0f);
        set (4, "detection_range", 1.0f);
        select (1000.0, 3.0);
    }
};

void checkEveryControlIsNamed (Editor& host)
{
    for (const auto& e : host.interactive())
    {
        CAPTURE (e.component->getName(), static_cast<int> (e.role), e.value);
        CHECK (e.title.isNotEmpty());
        // Titled on purpose, not by JUCE falling back to a button's text.
        CHECK (e.component->getTitle() == e.title);
    }
}

} // namespace

TEST_CASE ("Every control in the editor has an accessible name in the glossary's terms")
{
    EveryControl host;
    host.openPresetBrowser();
    checkEveryControlIsNamed (host);
    auto names = host.names();
    // And what the footer's call-outs show while open.
    for (const juce::String title : { "Analyzer", "Output" })
    {
        CAPTURE (title);
        host.openCallOut (title);
        checkEveryControlIsNamed (host);
        for (const auto& name : host.names())
            if (std::find (names.begin(), names.end(), name) == names.end())
                names.push_back (name);
        host.closeCallOut();
    }
    std::sort (names.begin(), names.end());

    // Every name, for the reviewer to check against GLOSSARY.md.
    std::vector<juce::String> expected { "A/B Compare A",
                                         "A/B Compare B",
                                         "Analyzer",
                                         "Analyzer Peak Hold",
                                         "Analyzer Post-EQ",
                                         "Analyzer Pre-EQ",
                                         "Analyzer Range",
                                         "Analyzer Resolution",
                                         "Analyzer Sidechain",
                                         "Analyzer Speed",
                                         "Analyzer Tilt",
                                         "Auto Gain",
                                         "Band 4",
                                         "Band 4 Attack",
                                         "Band 4 Bypass",
                                         "Band 4 Delete",
                                         "Band 4 Detection Audition",
                                         "Band 4 Detection High",
                                         "Band 4 Detection Low",
                                         "Band 4 Detection Range",
                                         "Band 4 Detection Source",
                                         "Band 4 Dynamic Range",
                                         "Band 4 Dynamics Bypass",
                                         "Band 4 Frequency",
                                         "Band 4 Gain",
                                         "Band 4 Q",
                                         "Band 4 Release",
                                         "Band 4 Shape",
                                         "Band 4 Stereo Placement",
                                         "Band 4 Threshold",
                                         "Clip Light Left",
                                         "Clip Light Right",
                                         "Copy A to B",
                                         "Display Range",
                                         "Gain Scale",
                                         "Global Bypass",
                                         "Load Preset File...",
                                         "Next Preset",
                                         "Output",
                                         "Output Gain",
                                         "Output Meter",
                                         "Output Pan",
                                         "Pan Mode",
                                         "Phase Invert",
                                         "Presets",
                                         "Previous Preset",
                                         "Redo",
                                         "Save as User Preset...",
                                         "Search Presets",
                                         "Show User Presets Folder",
                                         "UI Scale",
                                         "Undo" };
    std::sort (expected.begin(), expected.end());
    CHECK (names == expected);
    for (const auto& name : names)
        UNSCOPED_INFO (name);

    // The Preset browser: a group, its list and each of its items named.
    CHECK (host.element ("Preset browser").role == juce::AccessibilityRole::group);
    CHECK (host.element ("Presets").role == juce::AccessibilityRole::button);
    int items = 0;
    for (const auto& e : host.elements())
        if (e.role == juce::AccessibilityRole::listItem)
        {
            ++items;
            CHECK (e.title.isNotEmpty());
        }
    CHECK (items > 0);
    auto* list = harness::findChild<juce::ListBox> (*host.editor);
    REQUIRE (list != nullptr);
    CHECK (list->getAccessibilityHandler()->getTitle() == "Presets");
    CHECK (host.element ("EQ display").role == juce::AccessibilityRole::group);

    // None of the glossary's Avoid words.
    for (const auto& name : names)
        for (const char* avoid : { "Channel", "Filter", "Zoom", "Spectrum", "Mute", "Node", "Listen", "Solo", "Freq ", "Type", "Mode " })
        {
            CAPTURE (name, avoid);
            CHECK_FALSE (name.containsIgnoreCase (avoid));
        }
}

TEST_CASE ("Every control is named on mono, and with no Band selected")
{
    const bool mono = GENERATE (true, false);
    Editor host;
    if (mono)
        setLayout (host.processor, juce::AudioChannelSet::mono());
    host.addBand (2, 500.0f, 0.0f);
    host.settle();
    host.openPresetBrowser();
    checkEveryControlIsNamed (host);
    auto names = host.names();
    // And what the footer's call-outs show while open.
    for (const juce::String title : { "Analyzer", "Output" })
    {
        CAPTURE (title);
        host.openCallOut (title);
        checkEveryControlIsNamed (host);
        for (const auto& name : host.names())
            if (std::find (names.begin(), names.end(), name) == names.end())
                names.push_back (name);
        host.closeCallOut();
    }
    std::sort (names.begin(), names.end());
    CHECK (host.element ("Band 2").role == juce::AccessibilityRole::slider);
    if (mono)
        CHECK (host.element ("Clip Light").role == juce::AccessibilityRole::button);
}

TEST_CASE ("The Band panel's names carry the Band it shows, and change with it")
{
    Editor host;
    host.addBand (2, 200.0f, 0.0f);
    host.addBand (4, 4000.0f, 0.0f);
    host.select (200.0, 0.0);
    CHECK (host.element ("Band 2 Gain").role == juce::AccessibilityRole::slider);
    CHECK (host.element ("Band 2 Shape").role == juce::AccessibilityRole::comboBox);
    CHECK (host.element ("Band 2 Delete").role == juce::AccessibilityRole::button);
    host.select (4000.0, 0.0);
    for (const auto& name : host.names())
        CHECK_FALSE (name.startsWith ("Band 2 "));
    CHECK (host.element ("Band 4 Gain").role == juce::AccessibilityRole::slider);
    CHECK (host.element ("Band 4 Bypass").role == juce::AccessibilityRole::toggleButton);
}

TEST_CASE ("A slider reads its value with its unit, as the control shows it")
{
    EveryControl host;
    host.set (4, "gain", 3.5f);
    host.set (4, "q", 0.707f);
    host.settle();
    CHECK (host.element ("Band 4 Gain").value == "+3.50 dB");
    CHECK (host.element ("Band 4 Frequency").value == "1000.0 Hz");
    CHECK (host.element ("Band 4 Q").value == "0.707");
    CHECK (host.element ("Band 4 Dynamic Range").value == "+6.00 dB");
    CHECK (host.element ("Band 4 Attack").value == "Auto");
    CHECK (host.element ("Band 4 Detection High").value == "20000.0 Hz");
    CHECK (host.element ("Gain Scale").value == "100.0 %");
    host.openCallOut ("Analyzer");
    CHECK (host.element ("Analyzer Tilt").value.endsWith (" dB/oct"));
    host.closeCallOut();
    host.set (4, "gain", -12.0f);
    host.settle();
    CHECK (host.element ("Band 4 Gain").value == "-12.00 dB");

    // Threshold: Auto at its top, else its dB.
    host.set (4, "threshold_auto", 1.0f);
    host.settle();
    CHECK (host.element ("Band 4 Threshold").value == "Auto");
    host.set (4, "threshold", -20.0f);
    host.set (4, "threshold_auto", 0.0f);
    host.settle();
    CHECK (host.element ("Band 4 Threshold").value == "-20.0 dB");

    // Output Gain: -inf dB at its bottom.
    host.parameter ("output_gain").setValueNotifyingHost (0.0f);
    host.settle();
    host.openCallOut ("Output");
    CHECK (host.element ("Output Gain").value == "-inf dB");
}

TEST_CASE ("A Band reads its Shape, Frequency, Gain when it has one, and Q, then Bypassed and Dynamic Band")
{
    Editor host;
    host.addBand (4, 1000.0f, 3.5f);
    host.set (4, "q", 0.707f);
    host.settle();
    CHECK (host.element ("Band 4").value == "Bell, 1000.0 Hz, +3.50 dB, Q 0.707");

    host.set (4, "bypass", 1.0f);
    host.set (4, "dynamic_range", -6.0f);
    host.settle();
    CHECK (host.element ("Band 4").value == "Bell, 1000.0 Hz, +3.50 dB, Q 0.707, Bypassed, Dynamic Band");

    // Low Cut has no Gain, nor dynamics.
    host.set (4, "shape", 2.0f);
    host.settle();
    CHECK (host.element ("Band 4").value == "Low Cut, 1000.0 Hz, Q 0.707, Bypassed");
}

TEST_CASE ("The Presets button reads the Loaded Preset, with Modified, or No Preset")
{
    Editor host;
    const auto presets = [&host] { return host.element ("Presets").value; };
    CHECK (presets() == "No Preset");
    host.openPresetBrowser();
    auto* list = harness::findChild<juce::ListBox> (*host.editor);
    REQUIRE (list != nullptr);
    list->getListBoxModel()->returnKeyPressed (1); // the first folder's first Preset
    host.settle();
    const auto name = host.processor.loadedPresetName();
    REQUIRE (name.isNotEmpty());
    CHECK (presets() == name);
    host.set (1, "gain", 5.0f);
    host.set (1, "in_use", 1.0f);
    host.settle();
    REQUIRE (host.processor.isLoadedPresetModified());
    CHECK (presets() == name + ", Modified");
}

TEST_CASE ("Each Clip Light reads Lit or Off, and pressing either puts out both")
{
    Editor host;
    setLayout (host.processor, juce::AudioChannelSet::stereo());
    juce::AudioBuffer<float> buffer (host.processor.getTotalNumInputChannels(), 512);
    juce::MidiBuffer midi;
    buffer.clear();
    buffer.setSample (1, 10, 2.0f);
    host.processor.processBlock (buffer, midi);
    host.settle();
    CHECK (host.element ("Clip Light Left").value == "Off");
    CHECK (host.element ("Clip Light Right").value == "Lit");

    auto* left = host.element ("Clip Light Left").component->getAccessibilityHandler();
    REQUIRE (left->getActions().invoke (juce::AccessibilityActionType::press));
    CHECK_FALSE (host.processor.isClipLit (1));
    CHECK (host.element ("Clip Light Right").value == "Off");
}

TEST_CASE ("A Staple Knob described by its parameter reads, and shows in its tooltip, the name and the value with its unit")
{
    Editor host;
    staple::Knob knob (staple::tokens::knob::gain);
    juce::SliderParameterAttachment attachment (host.parameter ("band4_gain"), knob);
    knob.describe (host.parameter ("band4_gain"));
    juce::Component window;
    window.setBounds (0, 0, 200, 200);
    window.addAndMakeVisible (knob);
    knob.setBounds (10, 10, knob.getIdealSize(), knob.getIdealSize());
    window.addToDesktop (0);
    host.set (4, "gain", 3.5f);

    auto* handler = knob.getAccessibilityHandler();
    REQUIRE (handler != nullptr);
    CHECK (handler->getTitle() == "Band 4 Gain");
    CHECK (handler->getValueInterface()->getCurrentValueAsString() == "+3.50 dB");
    CHECK (knob.tooltipTitle() == "Band 4 Gain");
    CHECK (knob.tooltipValue() == "+3.50 dB");
}
