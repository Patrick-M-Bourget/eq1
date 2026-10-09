#pragma once

#include "PresetLibrary.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

namespace eq1
{

class PluginProcessor;

// Presets and A/B Compare, in one row: the Presets menu (Factory and User Presets, saving the
// settings as a User Preset, loading a Preset file from anywhere), the A and B sides and Copy A to B.
// Each load, save, switch and copy is one undo step. The Presets button shows the side's Loaded
// Preset, followed by * when Modified, with its full name in a tooltip.
class PresetBar final : public juce::Component, private juce::Timer
{
public:
    explicit PresetBar (PluginProcessor& processor);

    void resized() override;

    // After anything here changes the settings, so the editor can show what can be undone.
    std::function<void()> onEdit;

private:
    // Follows the side and its Loaded Preset, which edits, undo and restoring a session change too.
    void timerCallback() override;
    void showSide();
    void showLoadedPreset();
    void edited();
    void showMenu();
    void load (const juce::ValueTree& preset, const juce::String& name);
    void askToSave();
    // Saves the settings as a User Preset named name, when it isn't empty, and closes the prompt.
    void saveAs (const juce::String& name);
    void chooseFileToLoad();

    PluginProcessor& processor;
    PresetLibrary library;
    juce::TextButton presets { "Presets" }, a { "A" }, b { "B" }, copyAToB { "Copy A to B" };
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::AlertWindow> namePrompt;
};

} // namespace eq1
