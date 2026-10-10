#pragma once

#include "PresetLibrary.h"
#include "staple/controls/IconButton.h"
#include "staple/controls/TextChip.h"
#include "staple/controls/Tween.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace eq1
{

// The Preset browser (HANDOFF.md §5.6, §5.11), a modal over the whole editor: a scrim, and on it a
// panel with a search row and the Preset count, a column of folders (Factory, User and each User
// subfolder, PresetLibrary::folders) with their counts, the selected folder's Presets, or those whose
// names match the search across every folder, with their folders, and a footer: "Save as…" opens an
// inline name field, then "Load Preset File…" and "Show User Presets Folder". A click on a Preset, or
// Return on it in the list, loads it and the browser stays open, to audition the next. Each opening
// reads the disk again, clears the search and selects the Loaded Preset's folder (Factory when there
// is none). A click on the scrim, ✕ or Escape closes it, and keyboard focus goes back where it was:
// the Presets button when it was opened from there by keyboard, the display after a click on it (so
// Delete still reaches the display), or the Presets button when nothing had focus;
// while it is open, Tab stays within it.
class PresetBrowser final : public juce::Component, private juce::ListBoxModel
{
public:
    explicit PresetBrowser (const PresetLibrary& library);
    ~PresetBrowser() override;

    // Opens the browser, marking the Loaded Preset's entry (PresetLibrary::find).
    void open (const juce::String& loadedPreset, const PresetLibrary::Entry* lastLoaded);
    void close();
    // Marks the Loaded Preset's entry (PresetLibrary::find), which loads, undo and the other side change.
    void showLoaded (const juce::String& loadedPreset, const PresetLibrary::Entry* lastLoaded);

    std::function<void (const PresetLibrary::Entry&)> onLoad;
    // Saves the settings as a User Preset named name (never empty), making it the Loaded Preset.
    std::function<void (const juce::String& name)> onSave;
    std::function<void()> onLoadFile;
    // The button that opens it, which keyboard focus goes back to when it closes if nothing had it.
    juce::Component* opener = nullptr;

    // What the panel shows: its bounds in the browser, the folder selected (none while searching), the
    // Preset count at the top right, and the title over the list.
    juce::Rectangle<int> getPanelBounds() const;
    juce::String getSelectedFolder() const;
    juce::String getCountText() const;
    juce::String getListTitle() const;
    // Shows the Presets in the folder at path (PresetLibrary::Entry::folder), clearing the search.
    void selectFolder (const juce::String& path);

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    // A click on the scrim closes it.
    void mouseDown (const juce::MouseEvent& event) override;
    // Over the list, which lights the row under the mouse.
    void mouseMove (const juce::MouseEvent& event) override;
    void mouseExit (const juce::MouseEvent& event) override;

    // The panel's size (HANDOFF.md: 860 x 520, 64 px from the top), and the margin it keeps inside a
    // smaller window.
    static constexpr int panelWidth = 860, panelHeight = 520, panelTop = 64, margin = 14;

private:
    // The card the controls sit on: its fill, edge, rules and painted text.
    struct Panel final : juce::Component
    {
        explicit Panel (PresetBrowser& b) : browser (b) {}
        void paint (juce::Graphics& g) override;
        PresetBrowser& browser;
    };
    // The search field's box, holding the text and the clear button.
    struct SearchBox final : juce::Component
    {
        void paint (juce::Graphics& g) override;
    };
    // A folder in the folder column: its name, a dot when it holds the Loaded Preset, and its count.
    struct FolderRow final : juce::Button
    {
        FolderRow (PresetBrowser& browser, const PresetLibrary::Folder& folder);
        void paintButton (juce::Graphics& g, bool highlighted, bool down) override;
        bool keyPressed (const juce::KeyPress& key) override;
        std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
        PresetBrowser& browser;
        PresetLibrary::Folder folder;
    };
    // The ✕ in the search field while it holds text.
    struct ClearButton final : juce::Button
    {
        ClearButton();
        void paintButton (juce::Graphics& g, bool highlighted, bool down) override;
    };
    // The search and the inline name field: Enter, Esc and typing act at once, rather than in a
    // message posted for later.
    struct Field final : juce::TextEditor
    {
        std::function<void()> onEnter, onCancel, onEdit;
        bool keyPressed (const juce::KeyPress& key) override;
    };

    void showRows();
    void showFolders();
    bool isSearching() const;
    // Whether entry is the Loaded Preset's entry in the listing, the one ‹ › step from.
    bool isLoaded (const PresetLibrary::Entry& entry) const;
    std::optional<std::size_t> loadedIndex() const;
    void startSaving();
    void stopSaving (bool save);
    void layOutPanel();

    int getNumRows() override { return static_cast<int> (rows.size()); }
    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent& event) override;
    void returnKeyPressed (int row) override;
    // What a screen reader reads for a Preset: its name, with its folder while searching and "Loaded
    // Preset" for the Loaded Preset's entry.
    juce::String getNameForRow (int row) override;
    // A group titled "Preset browser".
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
    void loadRow (int row);

    const PresetLibrary& library;
    std::vector<PresetLibrary::Entry> listing;
    std::vector<PresetLibrary::Folder> folders;
    // The Presets the list shows, pointing into listing.
    std::vector<const PresetLibrary::Entry*> rows;
    juce::String loaded, selectedFolder = "Factory";
    std::optional<PresetLibrary::Entry> lastLoaded;
    staple::Tween opening { staple::tokens::motion::dur2Ms, 1.0f }; // the pop-in
    juce::Component::SafePointer<juce::Component> focusBefore; // given focus back on closing

    Panel panel { *this };
    SearchBox searchBox;
    Field search;
    ClearButton clearSearch;
    staple::IconButton closeButton { "Close Preset browser", staple::Icon::close };
    juce::Viewport folderView;
    juce::Component folderColumn;
    std::vector<std::unique_ptr<FolderRow>> folderRows;
    juce::ListBox list { {}, this };
    // The footer's actions: plain chips in text1 at 400, each named by its text.
    staple::TextChip save { juce::String::fromUTF8 ("Save as\xe2\x80\xa6"), staple::TextChip::Look::plain, staple::tokens::size::fs3, staple::Weight::regular },
        loadFile { juce::String::fromUTF8 ("Load Preset File\xe2\x80\xa6"), staple::TextChip::Look::plain, staple::tokens::size::fs3, staple::Weight::regular },
        showFolder { "Show User Presets Folder", staple::TextChip::Look::plain, staple::tokens::size::fs3, staple::Weight::regular };
    Field nameField;
};

} // namespace eq1
