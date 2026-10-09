#include "PresetBar.h"

#include "PluginProcessor.h"

namespace eq1
{

PresetBar::PresetBar (PluginProcessor& p) : processor (p)
{
    presets.onClick = [this] { showMenu(); };
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
    for (auto* button : { &presets, &a, &b, &copyAToB })
    {
        button->setWantsKeyboardFocus (false);
        addAndMakeVisible (*button);
    }
    showSide();
    startTimerHz (4);
}

void PresetBar::timerCallback()
{
    showSide();
}

void PresetBar::showSide()
{
    const bool onA = processor.compareSide() == CompareSide::A;
    a.setToggleState (onA, juce::dontSendNotification);
    b.setToggleState (! onA, juce::dontSendNotification);
}

void PresetBar::edited()
{
    if (onEdit != nullptr)
        onEdit();
}

void PresetBar::load (const juce::ValueTree& preset)
{
    if (processor.loadPreset (preset))
        edited();
    else
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Load Preset", "That file isn't an eq1 Preset.");
}

// The menu, the name prompt and the file chooser call back after the editor may have closed: each
// callback holds the bar by a SafePointer and does nothing once it is gone.
void PresetBar::showMenu()
{
    const juce::Component::SafePointer<PresetBar> bar (this);
    const auto loadWhileOpen = [bar] (juce::ValueTree preset) {
        return [bar, preset] {
            if (bar != nullptr)
                bar->load (preset);
        };
    };
    juce::PopupMenu menu;
    menu.addSectionHeader ("Factory");
    for (const auto& factory : PresetLibrary::factoryPresets())
        menu.addItem (factory.name, loadWhileOpen (factory.preset));
    menu.addSectionHeader ("User");
    const auto user = library.userPresets();
    if (user.empty())
        menu.addItem ("No User Presets yet", false, false, nullptr);
    for (const auto& file : user)
        menu.addItem (file.getFileNameWithoutExtension(), loadWhileOpen (PresetLibrary::read (file)));
    menu.addSeparator();
    menu.addItem ("Save as User Preset...", [bar] {
        if (bar != nullptr)
            bar->askToSave();
    });
    menu.addItem ("Load Preset File...", [bar] {
        if (bar != nullptr)
            bar->chooseFileToLoad();
    });
    menu.addItem ("Show User Presets Folder", [folder = library.folder()] {
        folder.createDirectory();
        folder.revealToUser();
    });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presets));
}

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
    if (name.isNotEmpty() && ! library.save (name, processor.presetState()))
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
            bar->load (PresetLibrary::read (file));
    });
}

void PresetBar::resized()
{
    auto row = getLocalBounds();
    presets.setBounds (row.removeFromLeft (90));
    row.removeFromLeft (12);
    a.setBounds (row.removeFromLeft (28));
    row.removeFromLeft (2);
    b.setBounds (row.removeFromLeft (28));
    row.removeFromLeft (6);
    copyAToB.setBounds (row.removeFromLeft (100));
}

} // namespace eq1
