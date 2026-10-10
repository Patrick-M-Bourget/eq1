#include "BandClipboard.h"
#include "BandMenu.h"
#include "BandSettingsByName.h"
#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;

namespace
{

// A plugin as a host has it, with the editing rules the editor uses on top.
struct Host
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    eq1::BandEditing editing { processor.parameterState(), processor.editHistory() };
    int deletes = 0, selectAlls = 0;
    std::vector<int> selected;
    juce::String clipboard; // the system clipboard's text, which the tests never touch

    float value (int slot, const char* control)
    {
        return processor.parameterState().getRawParameterValue ("band" + juce::String (slot) + "_" + control)->load();
    }

    void set (int slot, const char* control, float plain)
    {
        auto* parameter = processor.parameterState().getParameter ("band" + juce::String (slot) + "_" + control);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
    }

    void addBand (int slot, float frequency, float gain, float shape = 0.0f)
    {
        set (slot, "in_use", 1.0f);
        set (slot, "frequency", frequency);
        set (slot, "gain", gain);
        set (slot, "shape", shape);
    }

    juce::PopupMenu menu (std::vector<int> selection, bool stereo = true)
    {
        return eq1::BandMenu { editing,
                               std::move (selection),
                               stereo,
                               [this] { ++deletes; },
                               [this] { ++selectAlls; },
                               [this] (std::vector<int> slots) { selected = std::move (slots); },
                               clipboard,
                               [this] (const juce::String& text) { clipboard = text; } }
            .build();
    }
};

// The menu's items as the user reads them, a separator as "-".
std::vector<juce::String> textsOf (const juce::PopupMenu& menu)
{
    std::vector<juce::String> texts;
    for (juce::PopupMenu::MenuItemIterator it (menu); it.next();)
        texts.push_back (it.getItem().isSeparator ? "-" : it.getItem().text);
    return texts;
}

// The item with this text, in the menu or one of its submenus.
const juce::PopupMenu::Item& item (const juce::PopupMenu& menu, const juce::String& text)
{
    for (juce::PopupMenu::MenuItemIterator it (menu, true); it.next();)
        if (it.getItem().text == text)
            return it.getItem();
    FAIL ("no item " << text);
    throw std::logic_error ("unreachable");
}

const juce::PopupMenu& submenu (const juce::PopupMenu& menu, const juce::String& text)
{
    const auto& found = item (menu, text);
    REQUIRE (found.subMenu != nullptr);
    return *found.subMenu;
}

// The ticked items of a submenu.
std::vector<juce::String> ticked (const juce::PopupMenu& menu)
{
    std::vector<juce::String> texts;
    for (juce::PopupMenu::MenuItemIterator it (menu); it.next();)
        if (it.getItem().isTicked)
            texts.push_back (it.getItem().text);
    return texts;
}

} // namespace

TEST_CASE ("The Band menu lists its actions in groups on a selection, and only Paste and Select All on empty space")
{
    Host host;
    host.addBand (1, 100.0f, 3.0f);

    const std::vector<juce::String> onBands { "Bypass", "Invert Gain", "Clear Dynamics", "-", "Shape",  "Slope",
                                              "Stereo Placement", "-", "Cut", "Copy", "Paste", "Split", "-", "Delete", "-", "Select All" };
    CHECK (textsOf (host.menu ({ 1 })) == onBands);
    CHECK (textsOf (submenu (host.menu ({ 1 }), "Shape")) == std::vector<juce::String> (eq1::parameters::shapeNames().begin(), eq1::parameters::shapeNames().end()));
    CHECK (textsOf (submenu (host.menu ({ 1 }), "Slope"))
           == std::vector<juce::String> { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct", "72 dB/oct",
                                          "96 dB/oct", "Brickwall" });
    CHECK (textsOf (submenu (host.menu ({ 1 }), "Stereo Placement"))
           == std::vector<juce::String> (eq1::parameters::placementNames().begin(), eq1::parameters::placementNames().end()));

    const auto onEmptySpace = host.menu ({});
    CHECK (textsOf (onEmptySpace) == std::vector<juce::String> { "Paste", "-", "Select All" });
    item (onEmptySpace, "Select All").action();
    CHECK (host.selectAlls == 1);
    item (host.menu ({ 1 }), "Delete").action();
    CHECK (host.deletes == 1);
}

TEST_CASE ("Bypass on a partly Bypassed selection Bypasses all; on a fully Bypassed one it reads Remove Bypass and restores all")
{
    Host host;
    host.addBand (1, 100.0f, 3.0f);
    host.addBand (2, 1000.0f, 3.0f);
    host.set (2, "bypass", 1.0f);

    item (host.menu ({ 1, 2 }), "Bypass").action();
    CHECK (host.value (1, "bypass") == 1.0f);
    CHECK (host.value (2, "bypass") == 1.0f);

    const auto menu = host.menu ({ 1, 2 });
    CHECK (textsOf (menu)[0] == "Remove Bypass");
    item (menu, "Remove Bypass").action();
    CHECK (host.value (1, "bypass") == 0.0f);
    CHECK (host.value (2, "bypass") == 0.0f);
}

TEST_CASE ("Band menu items act on the whole selection, each as one undo step")
{
    Host host;
    host.addBand (1, 100.0f, 6.0f);
    host.set (1, "dynamic_range", -4.0f);
    host.addBand (2, 50.0f, 5.0f, 2.0f); // Low Cut
    auto& history = host.processor.editHistory();

    item (host.menu ({ 1, 2 }), "Invert Gain").action();
    CHECK_THAT (host.value (1, "gain"), WithinAbs (-6.0, 1.0e-4));
    CHECK_THAT (host.value (1, "dynamic_range"), WithinAbs (4.0, 1.0e-4));
    CHECK_THAT (host.value (2, "gain"), WithinAbs (5.0, 1.0e-4));
    item (host.menu ({ 1, 2 }), "Clear Dynamics").action();
    CHECK (host.value (1, "dynamic_range") == 0.0f);
    CHECK_FALSE (item (host.menu ({ 1, 2 }), "Clear Dynamics").isEnabled);
    item (host.menu ({ 1, 2 }), "24 dB/oct").action();
    CHECK (host.value (2, "slope") == 24.0f);
    item (host.menu ({ 1, 2 }), "Brickwall").action();
    CHECK (host.value (2, "brickwall") == 1.0f);
    item (host.menu ({ 1, 2 }), "High Shelf").action();
    CHECK (host.value (1, "shape") == 3.0f);
    CHECK (host.value (2, "shape") == 3.0f);
    item (host.menu ({ 1, 2 }), "Mid").action();
    CHECK (host.value (1, "placement") == 3.0f);
    CHECK (host.value (2, "placement") == 3.0f);
    CHECK (history.undoSteps() == 6);
}

TEST_CASE ("Band menu items that apply to no selected Band are unavailable")
{
    Host host;
    host.addBand (1, 50.0f, 3.0f, 2.0f); // Low Cut: no Gain, no dynamics
    host.set (1, "dynamic_range", -6.0f);
    host.addBand (2, 1000.0f, 3.0f); // Bell: no Slope
    host.addBand (3, 2000.0f, 3.0f, 8.0f); // Flat Tilt: no Slope

    const auto cut = host.menu ({ 1 });
    CHECK_FALSE (item (cut, "Invert Gain").isEnabled);
    CHECK_FALSE (item (cut, "Clear Dynamics").isEnabled); // its dynamics settings are kept, not cleared
    CHECK (item (cut, "Brickwall").isEnabled);

    // A Bell's dynamics all at their defaults: nothing to clear.
    const auto bells = host.menu ({ 2, 3 });
    CHECK (item (bells, "Invert Gain").isEnabled);
    CHECK_FALSE (item (bells, "Clear Dynamics").isEnabled);
    CHECK_FALSE (item (bells, "Slope").isEnabled);
    CHECK_FALSE (item (bells, "Brickwall").isEnabled);
    host.set (2, "detection_high", 8000.0f);
    CHECK (item (host.menu ({ 2, 3 }), "Clear Dynamics").isEnabled);

    CHECK (item (host.menu ({ 2 }), "Stereo Placement").isEnabled);
    CHECK_FALSE (item (host.menu ({ 2 }, false), "Stereo Placement").isEnabled);
}

TEST_CASE ("Band menu submenus tick a value only when every selected Band it applies to has it")
{
    Host host;
    host.addBand (1, 50.0f, 0.0f, 2.0f); // Low Cut, Slope 24
    host.set (1, "slope", 24.0f);
    host.addBand (2, 5000.0f, 0.0f, 3.0f); // High Shelf, Slope 24
    host.set (2, "slope", 24.0f);
    host.addBand (3, 1000.0f, 3.0f); // Bell: no Slope
    host.set (3, "slope", 48.0f);

    auto menu = host.menu ({ 1, 2, 3 });
    CHECK (ticked (submenu (menu, "Shape")).empty());
    CHECK (ticked (submenu (menu, "Slope")) == std::vector<juce::String> { "24 dB/oct" });
    CHECK (ticked (submenu (menu, "Stereo Placement")) == std::vector<juce::String> { "Stereo" });
    CHECK (ticked (submenu (host.menu ({ 2 }), "Shape")) == std::vector<juce::String> { "High Shelf" });

    // A Slope between the listed values ticks none.
    host.set (2, "slope", 30.0f);
    CHECK (ticked (submenu (host.menu ({ 1, 2 }), "Slope")).empty());

    // Brickwall when every selected Cut has it; a Brickwall Cut no longer has its dB/oct value.
    host.set (2, "slope", 24.0f);
    host.set (1, "brickwall", 1.0f);
    CHECK (ticked (submenu (host.menu ({ 1 }), "Slope")) == std::vector<juce::String> { "Brickwall" });
    CHECK (ticked (submenu (host.menu ({ 1, 2 }), "Slope")) == std::vector<juce::String> { "Brickwall" });
    host.addBand (4, 15000.0f, 0.0f, 4.0f); // High Cut, not Brickwall
    host.set (4, "slope", 24.0f);
    CHECK (ticked (submenu (host.menu ({ 1, 4 }), "Slope")).empty());
}

TEST_CASE ("Split splits the selected Stereo Bands and selects both halves of each, as one undo step")
{
    Host host;
    host.addBand (1, 100.0f, 3.0f);
    host.addBand (2, 1000.0f, 3.0f);
    host.set (2, "placement", 1.0f); // Left: ignored

    item (host.menu ({ 1, 2 }), "Split").action();
    CHECK (host.value (1, "placement") == 1.0f);
    CHECK (host.value (3, "placement") == 2.0f);
    CHECK (host.selected == std::vector<int> { 1, 3 });
    CHECK (host.processor.editHistory().undoSteps() == 1);
}

TEST_CASE ("Split is unavailable on mono, with no Stereo Band selected, and with no free Band Slot")
{
    Host host;
    for (int slot = 1; slot <= 23; ++slot)
        host.addBand (slot, 100.0f * static_cast<float> (slot), 3.0f);
    host.set (2, "placement", 2.0f); // Right
    host.set (3, "placement", 4.0f); // Side

    CHECK (item (host.menu ({ 1, 2 }), "Split").isEnabled);
    CHECK_FALSE (item (host.menu ({ 1 }, false), "Split").isEnabled);
    CHECK_FALSE (item (host.menu ({ 2, 3 }), "Split").isEnabled);
    host.addBand (24, 5000.0f, 3.0f);
    CHECK_FALSE (item (host.menu ({ 1 }), "Split").isEnabled);
}

TEST_CASE ("With fewer free Band Slots than selected Stereo Bands, Split reads how many it splits and splits the lowest-Frequency ones")
{
    Host host;
    for (int slot = 1; slot <= 22; ++slot)
        host.addBand (slot, 100.0f * static_cast<float> (slot), 3.0f);
    host.set (4, "placement", 3.0f); // Mid: not counted

    const auto menu = host.menu ({ 1, 2, 3, 4 });
    CHECK (textsOf (menu)[11] == "Split (2 of 3)");
    item (menu, "Split (2 of 3)").action();
    CHECK (host.selected == std::vector<int> { 1, 2, 23, 24 });
    CHECK (host.value (3, "placement") == 0.0f);
}

TEST_CASE ("Copy puts the selected Bands on the clipboard with no undo step; Cut also deletes them")
{
    Host host;
    host.addBand (1, 100.0f, 3.0f);
    host.addBand (2, 1000.0f, -3.0f);
    const auto copied = eq1::captureBands ({ host.editing.band (1), host.editing.band (2) }).toXmlString();

    item (host.menu ({ 1, 2 }), "Copy").action();
    CHECK (host.clipboard == copied);
    CHECK (host.deletes == 0);
    CHECK (host.processor.editHistory().undoSteps() == 0);

    host.clipboard = {};
    item (host.menu ({ 1, 2 }), "Cut").action();
    CHECK (host.clipboard == copied);
    CHECK (host.deletes == 1);
}

TEST_CASE ("Paste adds the clipboard's Bands and selects them, as one undo step, from a Band or empty space")
{
    Host host;
    host.addBand (1, 100.0f, 3.0f);
    item (host.menu ({ 1 }), "Copy").action();

    item (host.menu ({ 1 }), "Paste").action();
    CHECK (host.selected == std::vector<int> { 2 });
    CHECK (byName (host.editing.band (2)) == byName (host.editing.band (1)));
    item (host.menu ({}), "Paste").action();
    CHECK (host.selected == std::vector<int> { 3 });
    CHECK (host.processor.editHistory().undoSteps() == 2);
}

TEST_CASE ("Paste is unavailable with nothing of eq1's on the clipboard or no free Band Slot")
{
    Host host;
    host.addBand (1, 100.0f, 3.0f);
    CHECK_FALSE (item (host.menu ({ 1 }), "Paste").isEnabled);
    host.clipboard = "some text";
    CHECK_FALSE (item (host.menu ({}), "Paste").isEnabled);

    item (host.menu ({ 1 }), "Copy").action();
    CHECK (item (host.menu ({}), "Paste").isEnabled);
    for (int slot = 2; slot <= eq1::numBandSlots; ++slot)
        host.addBand (slot, 100.0f, 3.0f);
    CHECK_FALSE (item (host.menu ({ 1 }), "Paste").isEnabled);
}

TEST_CASE ("With fewer free Band Slots than Bands on the clipboard, Paste reads how many it pastes and pastes the lowest-Frequency ones")
{
    Host host;
    host.addBand (1, 5000.0f, 3.0f);
    host.addBand (2, 100.0f, 3.0f);
    host.addBand (3, 1000.0f, 3.0f);
    item (host.menu ({ 1, 2, 3 }), "Copy").action();
    for (int slot = 4; slot <= 22; ++slot)
        host.addBand (slot, 100.0f, 3.0f);

    const auto menu = host.menu ({});
    CHECK (textsOf (menu)[0] == "Paste (2 of 3)");
    item (menu, "Paste (2 of 3)").action();
    CHECK (host.selected == std::vector<int> { 23, 24 });
    CHECK (byName (host.editing.band (23)) == byName (host.editing.band (2)));
    CHECK (byName (host.editing.band (24)) == byName (host.editing.band (3)));
}
