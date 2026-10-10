#include "EditorHarness.h"
#include "FooterBar.h"
#include "PluginProcessor.h"
#include "SavedState.h"
#include "UserSettings.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <memory>
#include <vector>

namespace
{

eq1::UiScaleMenu& uiScaleMenu (juce::AudioProcessorEditor& editor)
{
    auto* menu = harness::findChild<eq1::UiScaleMenu> (editor, [] (eq1::UiScaleMenu& c) { return c.getTitle() == "UI Scale"; });
    REQUIRE (menu != nullptr);
    return *menu;
}

// The UI Scale menu's items, in order, after its "UI Scale" title.
std::vector<juce::String> uiScaleItems (eq1::UiScaleMenu& menu)
{
    std::vector<juce::String> items;
    const auto popup = menu.menu();
    for (juce::PopupMenu::MenuItemIterator it (popup); it.next();)
        if (! it.getItem().isSectionHeader)
            items.push_back (it.getItem().text);
    return items;
}

void pickUiScale (juce::AudioProcessorEditor& editor, int percent)
{
    auto& menu = uiScaleMenu (editor);
    const auto items = uiScaleItems (menu);
    REQUIRE (std::find (items.begin(), items.end(), juce::String (percent) + "%") != items.end());
    menu.pick (percent);
}

std::unique_ptr<juce::AudioProcessorEditor> openEditor (eq1::PluginProcessor& processor)
{
    return std::unique_ptr<juce::AudioProcessorEditor> (processor.createEditor());
}

void reload (eq1::PluginProcessor& into, eq1::PluginProcessor& from)
{
    juce::MemoryBlock state;
    from.getStateInformation (state);
    into.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
}

} // namespace

TEST_CASE ("A new instance opens at 1200 x 760 and 100%, with a UI Scale menu of 75, 100, 125, 150 and 200%")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    const auto editor = openEditor (processor);
    CHECK (editor->getWidth() == 1200);
    CHECK (editor->getHeight() == 760);
    auto& menu = uiScaleMenu (*editor);
    CHECK (menu.shownPercent() == 100);
    CHECK (uiScaleItems (menu) == std::vector<juce::String> { "75%", "100%", "125%", "150%", "200%" });
}

TEST_CASE ("At each UI Scale the editor is its logical size times the scale, and so are its limits and what it draws")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    const auto editor = openEditor (processor);
    auto& menu = uiScaleMenu (*editor);
    const auto atHundred = editor->getLocalArea (&menu, menu.getLocalBounds());

    const int percent = GENERATE (75, 100, 125, 150, 200);
    CAPTURE (percent);
    pickUiScale (*editor, percent);
    const auto scaled = [percent] (int logical) { return juce::roundToInt (logical * percent / 100.0); };
    CHECK (editor->getWidth() == scaled (1200));
    CHECK (editor->getHeight() == scaled (760));
    const auto* constrainer = editor->getConstrainer();
    REQUIRE (constrainer != nullptr);
    CHECK (constrainer->getMinimumWidth() == scaled (960));
    CHECK (constrainer->getMinimumHeight() == scaled (600));
    CHECK (constrainer->getMaximumWidth() == scaled (staple::tokens::layout::maximumWidth));
    CHECK (constrainer->getMaximumHeight() == scaled (staple::tokens::layout::maximumHeight));
    // A control sits where it did at 100%, scaled, and is that much larger.
    const auto drawn = editor->getLocalArea (&menu, menu.getLocalBounds());
    CHECK (drawn == atHundred.transformedBy (juce::AffineTransform::scale (percent / 100.0f)));
}

TEST_CASE ("The window's size and UI Scale survive closing and reopening the editor")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    {
        const auto editor = openEditor (processor);
        pickUiScale (*editor, 150);
        editor->setSize (1800, 1200); // 1200 x 800 logical
    }
    const auto editor = openEditor (processor);
    CHECK (uiScaleMenu (*editor).shownPercent() == 150);
    CHECK (editor->getWidth() == 1800);
    CHECK (editor->getHeight() == 1200);
}

TEST_CASE ("The window's size and UI Scale are saved with the session, not in a Preset, and change neither the undo history nor Modified")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor saved;
    // With a Loaded Preset, a change to any sound setting would make the side Modified.
    REQUIRE (saved.loadPreset (saved.presetState(), "Current"));
    const int steps = saved.editHistory().undoSteps();
    const auto preset = saved.presetState().createXml()->toString();
    {
        const auto editor = openEditor (saved);
        pickUiScale (*editor, 75);
        editor->setSize (900, 600); // 1200 x 800 logical
    }
    CHECK_FALSE (saved.isLoadedPresetModified());
    CHECK (saved.editHistory().undoSteps() == steps);
    CHECK (saved.presetState().createXml()->toString() == preset);
    CHECK (eq1::test::savedState (saved).getProperty ("version") == juce::var (eq1::PluginProcessor::stateVersion));

    eq1::PluginProcessor restored;
    reload (restored, saved);
    const auto editor = openEditor (restored);
    CHECK (uiScaleMenu (*editor).shownPercent() == 75);
    CHECK (editor->getWidth() == 900);
    CHECK (editor->getHeight() == 600);
}

TEST_CASE ("A session saved before the window's size and UI Scale opens like a new instance, with the state version unchanged")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    {
        const auto editor = openEditor (processor);
        pickUiScale (*editor, 200);
        editor->setSize (2600, 1400);
    }
    const auto older = juce::XmlDocument::parse (juce::File (EQ1_TEST_FIXTURES).getChildFile ("state-v2.xml"));
    REQUIRE (older != nullptr);
    juce::MemoryBlock state;
    juce::AudioProcessor::copyXmlToBinary (*older, state);
    processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    const auto editor = openEditor (processor);
    CHECK (uiScaleMenu (*editor).shownPercent() == 100);
    CHECK (editor->getWidth() == 1200);
    CHECK (editor->getHeight() == 760);
    CHECK (eq1::PluginProcessor::stateVersion == 3);
}

TEST_CASE ("Picking a UI Scale in one instance makes a new instance open at it; instances already open keep their own")
{
    juce::ScopedJuceInitialiser_GUI juce;
    const juce::TemporaryFile settings;
    eq1::PluginProcessor first (settings.getFile()), open (settings.getFile());
    const auto openEditorOfOpen = openEditor (open);
    {
        const auto editor = openEditor (first);
        pickUiScale (*editor, 125);
    }
    CHECK (uiScaleMenu (*openEditorOfOpen).shownPercent() == 100);

    eq1::PluginProcessor next (settings.getFile());
    const auto editor = openEditor (next);
    CHECK (uiScaleMenu (*editor).shownPercent() == 125);
    CHECK (editor->getWidth() == 1500);
    CHECK (editor->getHeight() == 950);
}

TEST_CASE ("A new instance opens at 100% with no settings file, or one it can't read")
{
    juce::ScopedJuceInitialiser_GUI juce;
    const juce::TemporaryFile settings;
    SECTION ("none") { REQUIRE_FALSE (settings.getFile().exists()); }
    SECTION ("unreadable") { REQUIRE (settings.getFile().replaceWithText ("not settings")); }
    SECTION ("not a UI Scale")
    {
        eq1::PluginProcessor picker (settings.getFile());
        pickUiScale (*openEditor (picker), 150);
        REQUIRE (settings.getFile().loadFileAsString().contains ("150"));
        REQUIRE (settings.getFile().replaceWithText (settings.getFile().loadFileAsString().replace ("150", "110")));
    }
    eq1::PluginProcessor processor (settings.getFile());
    CHECK (uiScaleMenu (*openEditor (processor)).shownPercent() == 100);
}

TEST_CASE ("An instance takes the per-user UI Scale when its editor first opens and keeps its own from then on")
{
    juce::ScopedJuceInitialiser_GUI juce;
    const juce::TemporaryFile settings;
    eq1::PluginProcessor other (settings.getFile()), processor (settings.getFile());
    pickUiScale (*openEditor (other), 150);
    openEditor (processor).reset();
    pickUiScale (*openEditor (other), 75);
    CHECK (uiScaleMenu (*openEditor (processor)).shownPercent() == 150);

    SECTION ("and an older session takes the per-user UI Scale")
    {
        const auto older = juce::XmlDocument::parse (juce::File (EQ1_TEST_FIXTURES).getChildFile ("state-v2.xml"));
        REQUIRE (older != nullptr);
        juce::MemoryBlock state;
        juce::AudioProcessor::copyXmlToBinary (*older, state);
        processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
        CHECK (uiScaleMenu (*openEditor (processor)).shownPercent() == 75);
    }
}

TEST_CASE ("The per-user settings file is in Application Support/eq1 on macOS and %APPDATA%/eq1 on Windows")
{
    const auto folder = eq1::UserSettings::defaultFile().getParentDirectory();
    CHECK (folder.getFileName() == "eq1");
#if JUCE_MAC
    CHECK (folder.getParentDirectory() == juce::File ("~/Library/Application Support"));
#elif JUCE_WINDOWS
    CHECK (folder.getParentDirectory() == juce::File (juce::SystemStats::getEnvironmentVariable ("APPDATA", {})));
#endif
}
