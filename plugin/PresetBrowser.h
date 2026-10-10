#pragma once

#include "PresetLibrary.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <optional>
#include <vector>

namespace eq1
{

// The Preset browser, a panel over the EQ display: every Preset in browser order
// (PresetLibrary::listing) under its folder's name, a search over Preset names whose results show
// their folder, and saving as a User Preset, loading a Preset file from anywhere and showing the User
// folder. A click on a Preset loads it and the panel stays open, to audition the next. Each opening
// reads the disk again and clears the search; Escape or a click outside the panel closes it.
class PresetBrowser final : public juce::Component, private juce::ListBoxModel
{
public:
    explicit PresetBrowser (const PresetLibrary& library);
    ~PresetBrowser() override;

    // Opens the panel, marking the Loaded Preset's entry (PresetLibrary::find).
    void open (const juce::String& loadedPreset, const PresetLibrary::Entry* lastLoaded);
    void close();
    // Marks the Loaded Preset's entry (PresetLibrary::find), which loads, undo and the other side change.
    void showLoaded (const juce::String& loadedPreset, const PresetLibrary::Entry* lastLoaded);

    std::function<void (const PresetLibrary::Entry&)> onLoad;
    std::function<void()> onSave, onLoadFile;
    // A component outside the panel whose clicks don't close it: the button that opens and closes it.
    juce::Component* opener = nullptr;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    // Seen for every click in the app while open (a global mouse listener), to close on one outside.
    void mouseDown (const juce::MouseEvent& event) override;

private:
    void visibilityChanged() override;
    void showRows();
    // Whether entry is the Loaded Preset's entry in the listing, the one ‹ › step from.
    bool isLoaded (const PresetLibrary::Entry& entry) const;

    int getNumRows() override { return static_cast<int> (rows.size()); }
    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent& event) override;

    // A folder's name over its Presets while browsing (no entry), or a Preset, with its folder while searching.
    struct Row
    {
        juce::String text, folder;
        const PresetLibrary::Entry* entry = nullptr;
    };

    const PresetLibrary& library;
    std::vector<PresetLibrary::Entry> listing, found;
    std::vector<Row> rows;
    juce::String loaded;
    std::optional<PresetLibrary::Entry> lastLoaded;
    juce::TextEditor search;
    juce::ListBox list { {}, this };
    juce::TextButton save { "Save as User Preset..." }, loadFile { "Load Preset File..." }, showFolder { "Show User Presets Folder" };
};

} // namespace eq1
