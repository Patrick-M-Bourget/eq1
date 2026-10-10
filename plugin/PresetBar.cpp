#include "PresetBar.h"

#include "PluginProcessor.h"

namespace eq1
{

PresetBar::PresetBar (PluginProcessor& p) : processor (p)
{
    presets.onClick = [this] {
        if (browser.isVisible())
            browser.close();
        else
            browser.open (processor.loadedPresetName(), lastLoadedEntry());
    };
    browser.opener = &presets;
    browser.onLoad = [this] (const PresetLibrary::Entry& entry) { load (entry.preset, entry.name, entry); };
    browser.onSave = [this] {
        browser.close();
        askToSave();
    };
    browser.onLoadFile = [this] {
        browser.close();
        chooseFileToLoad();
    };
    previous.onClick = [this] { step (-1); };
    next.onClick = [this] { step (1); };
    a.onClick = [this] {
        processor.selectCompareSide (CompareSide::A);
        showSide();
        edited();
    };
    b.onClick = [this] {
        processor.selectCompareSide (CompareSide::B);
        showSide();
        edited();
    };
    copyAToB.onClick = [this] {
        processor.copyAToB();
        edited();
    };
    // The side you're on is lit, in the display's blue.
    for (auto* side : { &a, &b })
        side->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2f8fd0));
    for (auto* button : { &presets, &previous, &next, &a, &b, &copyAToB })
    {
        button->setWantsKeyboardFocus (false);
        addAndMakeVisible (*button);
    }
    showSide();
    showLoadedPreset();
    startTimerHz (4);
}

void PresetBar::timerCallback()
{
    showSide();
    showLoadedPreset();
}

void PresetBar::showSide()
{
    const bool onA = processor.compareSide() == CompareSide::A;
    a.setToggleState (onA, juce::dontSendNotification);
    b.setToggleState (! onA, juce::dontSendNotification);
}

void PresetBar::showLoadedPreset()
{
    const auto name = processor.loadedPresetName();
    presets.setButtonText (name.isEmpty() ? "Presets" : name + (processor.isLoadedPresetModified() ? "*" : ""));
    presets.setTooltip (name);
    browser.showLoaded (name, lastLoadedEntry());
}

void PresetBar::edited()
{
    showLoadedPreset();
    if (onEdit != nullptr)
        onEdit();
}

void PresetBar::load (const juce::ValueTree& preset, const juce::String& name, std::optional<PresetLibrary::Entry> entry)
{
    if (processor.loadPreset (preset, name))
    {
        lastLoaded = std::move (entry);
        edited();
    }
    else
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Load Preset", "That file isn't an eq1 Preset.");
}

void PresetBar::step (int by)
{
    const auto listing = library.listing();
    if (const auto i = PresetLibrary::step (listing, processor.loadedPresetName(), by, lastLoadedEntry()))
        load (listing[*i].preset, listing[*i].name, listing[*i]);
}

// The name prompt and the file chooser call back after the editor may have closed: each callback
// holds the bar by a SafePointer and does nothing once it is gone.
void PresetBar::askToSave()
{
    namePrompt = std::make_unique<juce::AlertWindow> ("Save as User Preset", "Name:", juce::MessageBoxIconType::NoIcon, this);
    namePrompt->addTextEditor ("name", {});
    namePrompt->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    namePrompt->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    const juce::Component::SafePointer<PresetBar> bar (this);
    namePrompt->enterModalState (true, juce::ModalCallbackFunction::create ([bar] (int result) {
        if (bar != nullptr)
            bar->saveAs (result == 1 ? bar->namePrompt->getTextEditorContents ("name").trim() : juce::String());
    }));
}

void PresetBar::saveAs (const juce::String& name)
{
    namePrompt.reset();
    if (name.isEmpty())
        return;
    const auto preset = processor.presetState();
    if (const auto file = library.save (name, preset))
    {
        processor.presetSaved (preset, file->getFileNameWithoutExtension());
        lastLoaded = PresetLibrary::Entry { file->getFileNameWithoutExtension(), "User", *file, preset };
        edited();
    }
    else
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                "Save as User Preset",
                                                "Couldn't write the Preset to " + library.folder().getFullPathName() + ".");
}

void PresetBar::chooseFileToLoad()
{
    chooser = std::make_unique<juce::FileChooser> ("Load Preset File", library.folder(), "*" + PresetLibrary::fileExtension);
    const juce::Component::SafePointer<PresetBar> bar (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [bar] (const juce::FileChooser& chosen) {
        if (const auto file = chosen.getResult(); bar != nullptr && file.existsAsFile())
            bar->load (PresetLibrary::read (file), file.getFileNameWithoutExtension(), std::nullopt);
    });
}

void PresetBar::resized()
{
    auto row = getLocalBounds();
    presets.setBounds (row.removeFromLeft (200));
    row.removeFromLeft (2);
    previous.setBounds (row.removeFromLeft (24));
    next.setBounds (row.removeFromLeft (24));
    row.removeFromLeft (12);
    a.setBounds (row.removeFromLeft (28));
    row.removeFromLeft (2);
    b.setBounds (row.removeFromLeft (28));
    row.removeFromLeft (6);
    copyAToB.setBounds (row.removeFromLeft (100));
}

} // namespace eq1
