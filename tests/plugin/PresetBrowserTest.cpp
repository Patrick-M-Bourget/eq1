#include "EditorHarness.h"
#include "PluginProcessor.h"
#include "PresetBrowser.h"
#include "PresetLibrary.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>

using eq1::PresetBrowser;
using eq1::PresetLibrary;
using harness::findChild;

namespace
{
const auto saveAs = juce::String::fromUTF8 ("Save as\xe2\x80\xa6");

// A click, as on a button: its onClick at once.
void click (juce::Button& button)
{
    REQUIRE (button.onClick != nullptr);
    button.onClick();
}

// A left-click at at in component.
juce::MouseEvent clickAt (juce::Component& component, juce::Point<float> at = {})
{
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at, juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier),
                             1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &component, &component, now, at, now, 1, false);
}

// The browser over a User folder of its own, deleted afterwards: A, Drums/Kick and Drums/Acoustic/Room,
// and an empty Drums/Brushes.
struct Browser
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    juce::TemporaryFile temporary;
    juce::File folder = temporary.getFile();
    PresetLibrary library { folder };
    PresetBrowser browser { library };
    std::vector<juce::String> loads, saves;

    Browser()
    {
        REQUIRE (library.save ("A", processor.presetState()).has_value());
        for (const auto* path : { "Drums/Kick", "Drums/Acoustic/Room" })
        {
            const auto file = folder.getChildFile (juce::String (path) + PresetLibrary::fileExtension);
            REQUIRE (file.getParentDirectory().createDirectory());
            REQUIRE (processor.presetState().createXml()->writeTo (file));
        }
        REQUIRE (folder.getChildFile ("Drums/Brushes").createDirectory());
        // On the desktop, as in a window, so it has accessibility handlers.
        browser.addToDesktop (0);
        browser.setSize (1200, 760);
        browser.onLoad = [this] (const PresetLibrary::Entry& entry) { loads.push_back (entry.folder + ": " + entry.name); };
        browser.onSave = [this] (const juce::String& name) {
            saves.push_back (name);
            const auto file = library.save (name, processor.presetState());
            REQUIRE (file.has_value());
            const PresetLibrary::Entry entry { name, "User", *file, processor.presetState() };
            browser.showLoaded (name, &entry);
        };
    }
    ~Browser() { folder.deleteRecursively(); }

    juce::Button& button (const juce::String& title)
    {
        auto* found = findChild<juce::Button> (browser, [&title] (juce::Button& b) { return b.getTitle() == title; });
        REQUIRE (found != nullptr);
        return *found;
    }
    juce::String valueOf (juce::Component& c) { return c.getAccessibilityHandler()->getValueInterface()->getCurrentValueAsString(); }

    juce::ListBox& list() { return *findChild<juce::ListBox> (browser); }
    juce::StringArray rows()
    {
        juce::StringArray names;
        auto* model = list().getListBoxModel();
        for (int row = 0; row < model->getNumRows(); ++row)
            names.add (model->getNameForRow (row));
        return names;
    }
    juce::TextEditor& search()
    {
        return *findChild<juce::TextEditor> (browser, [] (juce::TextEditor& t) { return t.getTitle() == "Search Presets"; });
    }
    // Typed into the search field, replacing what it holds.
    void type (const juce::String& text)
    {
        search().selectAll();
        search().keyPressed (juce::KeyPress (juce::KeyPress::backspaceKey));
        for (const auto character : text)
            search().keyPressed (juce::KeyPress (static_cast<int> (character), {}, character));
    }
};

} // namespace

TEST_CASE ("The Preset browser's folders are Factory, User and each User subfolder, nested ones indented, with their counts")
{
    Browser host;
    host.browser.open ({}, nullptr);
    const auto factory = PresetLibrary::factoryPresets().size();
    CHECK (host.valueOf (host.button ("Factory")) == juce::String (factory));
    CHECK (host.valueOf (host.button ("User")) == "1");
    CHECK (host.valueOf (host.button ("Drums")) == "1");
    CHECK (host.valueOf (host.button ("Acoustic")) == "1");
    // Each level 12 px further in.
    CHECK (host.button ("Drums").getX() == host.button ("User").getX() + 12);
    CHECK (host.button ("Acoustic").getX() == host.button ("Drums").getX() + 12);
    CHECK (host.browser.getCountText() == juce::String (factory + 3) + " Presets");
    // A User subfolder with no Presets in it is listed too.
    CHECK (host.valueOf (host.button ("Brushes")) == "0");
    CHECK (host.button ("Brushes").getX() == host.button ("Drums").getX() + 12);

    // With no Loaded Preset, Factory is selected and its Presets listed.
    CHECK (host.browser.getSelectedFolder() == "Factory");
    CHECK (host.browser.getListTitle() == "Factory");
    CHECK (host.rows().size() == static_cast<int> (factory));

    // Selecting a folder lists the Presets directly in it.
    click (host.button ("Drums"));
    CHECK (host.browser.getSelectedFolder() == "User/Drums");
    CHECK (host.browser.getListTitle() == "Drums");
    CHECK (host.rows() == juce::StringArray ({ "Kick" }));
}

TEST_CASE ("The Preset browser opens on the Loaded Preset's folder, marking it")
{
    Browser host;
    const auto listing = host.library.listing();
    const auto& room = listing.back();
    REQUIRE (room.name == "Room");
    host.browser.open ("Room", &room);
    CHECK (host.browser.getSelectedFolder() == "User/Drums/Acoustic");
    CHECK (host.rows() == juce::StringArray ({ "Room, Loaded Preset" }));
}

TEST_CASE ("Searching lists matching Presets across every folder, with their folders; with no match, the empty message")
{
    Browser host;
    host.browser.open ({}, nullptr);
    host.type ("ROO");
    CHECK (host.rows() == juce::StringArray ({ "Room, User/Drums/Acoustic" }));
    CHECK (host.browser.getListTitle() == juce::String::fromUTF8 ("1 result for \xe2\x80\x9cROO\xe2\x80\x9d"));
    CHECK (host.browser.getSelectedFolder().isEmpty());
    CHECK (host.button ("Clear search").isVisible());

    host.type ("Nothing like it");
    CHECK (host.rows().isEmpty());
    CHECK (host.browser.getListTitle() == juce::String::fromUTF8 ("0 results for \xe2\x80\x9cNothing like it\xe2\x80\x9d"));

    click (host.button ("Clear search"));
    CHECK (host.search().isEmpty());
    CHECK_FALSE (host.button ("Clear search").isVisible());
    CHECK (host.browser.getSelectedFolder() == "Factory");
}

TEST_CASE ("A single click on a Preset loads it, and the browser stays open")
{
    Browser host;
    host.browser.open ({}, nullptr);
    click (host.button ("Drums"));
    host.list().getListBoxModel()->listBoxItemClicked (0, clickAt (host.list()));
    CHECK (host.loads == std::vector<juce::String> { "User/Drums: Kick" });
    CHECK (host.browser.isVisible());
}

TEST_CASE ("Every button in the Preset browser is named by its text")
{
    Browser host;
    host.browser.open ({}, nullptr);
    std::vector<juce::Button*> withText;
    findChild<juce::Button> (host.browser, [&withText] (juce::Button& b) {
        if (b.getButtonText().isNotEmpty())
            withText.push_back (&b);
        return false;
    });
    CHECK (withText.size() >= 3u); // Save as…, Load Preset File… and Show User Presets Folder at least
    for (auto* button : withText)
        CHECK (button->getTitle() == button->getButtonText());
    CHECK (host.button (saveAs).getButtonText() == saveAs);
}

TEST_CASE ("Save as opens an inline name field: Enter saves into the User folder as the Loaded Preset, Esc cancels")
{
    Browser host;
    host.browser.open ({}, nullptr);
    const auto field = [&host] {
        return findChild<juce::TextEditor> (host.browser, [] (juce::TextEditor& t) { return t.getTitle() == "Preset name"; });
    };
    REQUIRE (field() != nullptr);
    CHECK_FALSE (field()->isVisible());

    click (host.button (saveAs));
    REQUIRE (field()->isVisible());
    field()->setText ("Warm");
    field()->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
    CHECK_FALSE (field()->isVisible());
    CHECK (host.saves.empty());

    click (host.button (saveAs));
    // The field opens empty.
    CHECK (field()->isEmpty());
    field()->setText ("Warm");
    field()->keyPressed (juce::KeyPress (juce::KeyPress::returnKey));
    CHECK_FALSE (field()->isVisible());
    CHECK (host.saves == std::vector<juce::String> { "Warm" });
    CHECK (host.browser.isVisible());
    // Listed in User, as the Loaded Preset.
    CHECK (host.browser.getSelectedFolder() == "User");
    CHECK (host.rows() == juce::StringArray ({ "A", "Warm, Loaded Preset" }));
    CHECK (host.valueOf (host.button ("User")) == "2");

    // No name, no save.
    click (host.button (saveAs));
    field()->keyPressed (juce::KeyPress (juce::KeyPress::returnKey));
    CHECK (host.saves.size() == 1u);
}

TEST_CASE ("The Preset browser has no favourites, stars, preview curve, tags or Copy and Paste")
{
    Browser host;
    host.browser.open ({}, nullptr);
    std::function<void (juce::Component&)> visit = [&] (juce::Component& parent) {
        for (auto* child : parent.getChildren())
        {
            for (const char* word : { "avourite", "Star", "Preview", "Tag", "Copy", "Paste" })
            {
                CAPTURE (child->getTitle(), word);
                CHECK_FALSE (child->getTitle().containsIgnoreCase (word));
            }
            if (auto* icon = dynamic_cast<staple::IconButton*> (child))
                CHECK (icon->getIcon() != staple::Icon::star);
            visit (*child);
        }
    };
    visit (host.browser);
}

namespace
{

struct Editor : harness::OpenEditor
{
    juce::Button& presets() { return *findChild<juce::Button> (*editor, [] (juce::Button& b) { return b.getTitle() == "Presets"; }); }
    PresetBrowser& browser() { return *findChild<PresetBrowser> (*editor); }
    void open()
    {
        presets().grabKeyboardFocus();
        press (juce::KeyPress (juce::KeyPress::returnKey));
        REQUIRE (browser().isVisible());
    }
};

} // namespace

TEST_CASE ("The Preset browser opens as a modal over the whole editor; the scrim, its close button and Escape close it, focus back on Presets")
{
    Editor host;
    host.open();
    auto& browser = host.browser();
    auto* content = browser.getParentComponent();
    REQUIRE (content != nullptr);
    // Over the header and footer too, on top of everything.
    CHECK (browser.getBounds() == content->getLocalBounds());
    CHECK (content->getIndexOfChildComponent (&browser) == content->getNumChildComponents() - 1);
    // 860 x 520, centred, 64 px from the top.
    CHECK (browser.getPanelBounds() == juce::Rectangle<int> ((1200 - 860) / 2, 64, 860, 520));

    SECTION ("A click on the scrim")
    {
        browser.mouseDown (clickAt (browser, { 20.0f, 700.0f }));
    }
    SECTION ("Its close button")
    {
        auto* close = findChild<juce::Button> (browser, [] (juce::Button& b) { return b.getTitle() == "Close Preset browser"; });
        REQUIRE (close != nullptr);
        click (*close);
    }
    SECTION ("Escape") { CHECK (host.press (juce::KeyPress (juce::KeyPress::escapeKey))); }
    CHECK_FALSE (browser.isVisible());
    CHECK (host.presets().hasKeyboardFocus (false));
}

TEST_CASE ("While the Preset browser is open, Tab stays within it")
{
    Editor host;
    host.open();
    auto& browser = host.browser();
    for (int press = 0; press < 30; ++press)
    {
        host.press (juce::KeyPress (juce::KeyPress::tabKey));
        auto* focused = juce::Component::getCurrentlyFocusedComponent();
        REQUIRE (focused != nullptr);
        CHECK (browser.isParentOf (focused));
    }
}

TEST_CASE ("A click on a Preset in the browser loads it as one undo step")
{
    Editor host;
    host.open();
    auto& history = host.processor.editHistory();
    const int steps = history.undoSteps();
    auto* list = findChild<juce::ListBox> (host.browser());
    REQUIRE (list != nullptr);
    list->getListBoxModel()->listBoxItemClicked (0, clickAt (*list));
    CHECK (host.processor.loadedPresetName() == PresetLibrary::factoryPresets().front().name);
    CHECK (history.undoSteps() == steps + 1);
    CHECK (host.browser().isVisible());
}

TEST_CASE ("At the 960 x 600 minimum the Preset browser fits inside the window with a 14 px margin")
{
    Editor host;
    host.editor->setSize (960, 600);
    host.open();
    auto& browser = host.browser();
    const auto panel = browser.getParentComponent()->getLocalArea (&browser, browser.getPanelBounds());
    CHECK (browser.getParentComponent()->getLocalBounds().reduced (PresetBrowser::margin).contains (panel));
    // Smaller still, as at a large UI Scale in a small window, it shrinks.
    browser.setSize (700, 400);
    CHECK (browser.getLocalBounds().reduced (PresetBrowser::margin).contains (browser.getPanelBounds()));
}

// Renders the browser for checking by hand against the prototype (harness::writeSnapshot):
// browser-folder (a Factory Preset loaded, its folder shown) and browser-search, each the whole window.
TEST_CASE ("Preset browser snapshots: a folder, and a search", "[.screens]")
{
    Editor host;
    const auto factory = PresetLibrary::factoryPresets();
    REQUIRE (factory.size() > 1);
    REQUIRE (host.processor.loadPreset (factory[1].preset, factory[1].name));
    host.settle (300);
    host.open();
    host.settle (300);
    const auto write = [&] (const char* name) { harness::writeSnapshot (*host.editor, juce::String ("browser-") + name, {}, 1.0f); };
    write ("folder");
    auto* search = findChild<juce::TextEditor> (host.browser(), [] (juce::TextEditor& t) { return t.getTitle() == "Search Presets"; });
    REQUIRE (search != nullptr);
    for (const auto character : juce::String ("e"))
        search->keyPressed (juce::KeyPress (static_cast<int> (character), {}, character));
    host.settle (100);
    write ("search");
}
