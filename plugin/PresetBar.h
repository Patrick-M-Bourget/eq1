#pragma once

#include "Accessibility.h"
#include "PresetBrowser.h"
#include "PresetLibrary.h"
#include "staple/controls/IconButton.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <optional>

namespace eq1
{

class PluginProcessor;

// The Presets button (HANDOFF.md §5.6): the side's Loaded Preset in text1, or "No Preset" in text3,
// in fs4 at 500, truncated with an ellipsis, with its full name in a tooltip; a 5 px text3 dot after
// the name while Modified. Hover shows a fill1 box with r3 corners. A screen reader reads spokenValue.
class PresetNameButton final : public juce::Button
{
public:
    PresetNameButton();

    // Shows the Loaded Preset named name (none when empty), and whether the side is Modified.
    void show (const juce::String& name, bool modified);
    // Its name, padding and dot, at least the 250 px minimum.
    int getIdealWidth() const;

    juce::Colour nameInk() const;
    bool showsModified() const { return modified; }

    // What a screen reader reads as its value: the Loaded Preset, with "Modified", or "No Preset".
    std::function<juce::String()> spokenValue;

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override;

    static constexpr int minimumWidth = 250, height = 36;

private:
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    juce::String loadedName;
    bool modified = false;
};

// The header's Preset group (HeaderBar): ‹, the Presets button, which opens and closes the Preset
// browser (PresetBrowser), and › stepping to the previous and next Preset in browser order over the
// whole library. Each load and save is one undo step; a load replaces a Modified side's settings
// without asking, as undo brings them back.
class PresetBar final : public juce::Component, private juce::Timer
{
public:
    explicit PresetBar (PluginProcessor& processor);

    void resized() override;

    // The width ‹, Presets and › want: the Presets button grows with a long name, from 250 px.
    int getIdealWidth() const;
    static constexpr int stepButtonSize = 36, gap = 4;

    // The browser, for the editor to place over its whole content.
    juce::Component& browserPanel() { return browser; }

    // After anything here changes the settings, so the editor can show what can be undone.
    std::function<void()> onEdit;
    // After the ideal width changes with the Loaded Preset's name, so the header can lay it out again.
    std::function<void()> onIdealWidthChange;

private:
    // Follows the side's Loaded Preset, which edits, undo, A/B Compare and restoring a session change too.
    void timerCallback() override;
    void showLoadedPreset();
    void edited();
    // Loads a Preset, remembering the listing entry it came from (none for a file from anywhere).
    void load (const juce::ValueTree& preset, const juce::String& name, std::optional<PresetLibrary::Entry> entry);
    // Loads the Preset by places after (or before, when negative) the Loaded Preset's entry (PresetLibrary::step).
    void step (int by);
    // Saves the settings as a User Preset named name, when it isn't empty, making it the side's Loaded
    // Preset (the browser's inline "Save as…" field).
    void saveAs (const juce::String& name);
    const PresetLibrary::Entry* lastLoadedEntry() const { return lastLoaded.has_value() ? &*lastLoaded : nullptr; }
    void chooseFileToLoad();

    PluginProcessor& processor;
    PresetLibrary library;
    PresetBrowser browser { library };
    PresetNameButton presets;
    staple::IconButton previous { "Previous Preset", staple::Icon::previous }, next { "Next Preset", staple::Icon::next };
    // The listing entry last loaded or saved, through the browser, ‹ › or saving, which ‹ › step from
    // while its name is still the Loaded Preset's: a name can be listed more than once. Kept in memory
    // only, so the Loaded Preset stays a name.
    std::optional<PresetLibrary::Entry> lastLoaded;
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace eq1
