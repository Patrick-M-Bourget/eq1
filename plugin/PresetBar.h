#pragma once

#include "PresetBrowser.h"
#include "PresetLibrary.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <optional>

namespace eq1
{

class PluginProcessor;

// Presets and A/B Compare, in the header (HeaderBar): ‹, the Presets button, which opens and closes the
// Preset browser (PresetBrowser), and › stepping to the previous and next Preset in browser order over
// the whole library; then the A and B sides and Copy A to B. Each load, save, switch and copy is one undo step; a
// load replaces a Modified side's settings without asking, as undo brings them back. The Presets
// button shows the side's Loaded Preset, followed by * when Modified, with its full name in a tooltip.
class PresetBar final : public juce::Component, private juce::Timer
{
public:
    explicit PresetBar (PluginProcessor& processor);

    void resized() override;

    // The width of ‹, Presets and ›.
    static constexpr int centreWidth = 24 + 2 + 200 + 2 + 24;
    // Lays ‹, Presets and › out in centre, and A, B and Copy A to B at the right of right.
    void place (juce::Rectangle<int> centre, juce::Rectangle<int> right);

    // The browser, for the editor to place over the EQ display.
    juce::Component& browserPanel() { return browser; }

    // After anything here changes the settings, so the editor can show what can be undone.
    std::function<void()> onEdit;

private:
    // Follows the side and its Loaded Preset, which edits, undo and restoring a session change too.
    void timerCallback() override;
    void showSide();
    void showLoadedPreset();
    void edited();
    // Loads a Preset, remembering the listing entry it came from (none for a file from anywhere).
    void load (const juce::ValueTree& preset, const juce::String& name, std::optional<PresetLibrary::Entry> entry);
    // Loads the Preset by places after (or before, when negative) the Loaded Preset's entry (PresetLibrary::step).
    void step (int by);
    void askToSave();
    // Saves the settings as a User Preset named name, when it isn't empty, making it the side's Loaded
    // Preset, and closes the prompt.
    void saveAs (const juce::String& name);
    const PresetLibrary::Entry* lastLoadedEntry() const { return lastLoaded.has_value() ? &*lastLoaded : nullptr; }
    void chooseFileToLoad();

    PluginProcessor& processor;
    PresetLibrary library;
    PresetBrowser browser { library };
    juce::TextButton presets { "Presets" }, previous { juce::String::charToString (0x2039) }, next { juce::String::charToString (0x203a) }, a { "A" },
        b { "B" }, copyAToB { "Copy A to B" };
    // The listing entry last loaded or saved, through the browser, ‹ › or saving, which ‹ › step from
    // while its name is still the Loaded Preset's: a name can be listed more than once. Kept in memory
    // only, so the Loaded Preset stays a name.
    std::optional<PresetLibrary::Entry> lastLoaded;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::AlertWindow> namePrompt;
    juce::Rectangle<int> centreArea, rightArea;
};

} // namespace eq1
