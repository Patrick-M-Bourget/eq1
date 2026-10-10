#include "PresetBar.h"

#include "PluginProcessor.h"
#include "staple/Fonts.h"
#include "staple/Tokens.h"

#include <cmath>

namespace eq1
{

namespace
{
namespace colour = staple::tokens::colour;
namespace size = staple::tokens::size;

constexpr float namePadding = 14.0f, dotSize = 5.0f, dotGap = 6.0f;

juce::Font nameFont() { return staple::font (size::fs4, staple::Weight::medium); }
} // namespace

PresetNameButton::PresetNameButton() : juce::Button ("Presets")
{
    setTitle ("Presets");
    setHasFocusOutline (true);
    setButtonText ("No Preset");
}

void PresetNameButton::show (const juce::String& name, bool isModified)
{
    const bool nowModified = isModified && name.isNotEmpty();
    if (name == loadedName && nowModified == modified)
        return;
    loadedName = name;
    modified = nowModified;
    setButtonText (name.isEmpty() ? juce::String ("No Preset") : name);
    setTooltip (name);
    repaint();
}

juce::Colour PresetNameButton::nameInk() const { return loadedName.isEmpty() ? colour::text3 : colour::text1; }

int PresetNameButton::getIdealWidth() const
{
    const float width = juce::GlyphArrangement::getStringWidth (nameFont(), getButtonText()) + 2.0f * namePadding + dotGap + dotSize;
    return juce::jmax (minimumWidth, static_cast<int> (std::ceil (width)));
}

void PresetNameButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto bounds = getLocalBounds().toFloat();
    if (highlighted || down)
    {
        g.setColour (colour::fill1);
        g.fillRoundedRectangle (bounds, size::r3);
    }
    // The name and the dot, centred together; the name gives way to the dot when it doesn't fit.
    const auto area = bounds.reduced (namePadding, 0.0f);
    const float dotSpace = modified ? dotGap + dotSize : 0.0f;
    const float nameWidth = juce::jmin (juce::GlyphArrangement::getStringWidth (nameFont(), getButtonText()), area.getWidth() - dotSpace);
    const float left = area.getCentreX() - (nameWidth + dotSpace) / 2.0f;
    g.setFont (nameFont());
    g.setColour (nameInk());
    g.drawText (getButtonText(), juce::Rectangle<float> (left, area.getY(), nameWidth + 1.0f, area.getHeight()), juce::Justification::centredLeft, true);
    if (modified)
    {
        g.setColour (colour::text3);
        g.fillEllipse (left + nameWidth + dotGap, area.getCentreY() - dotSize / 2.0f, dotSize, dotSize);
    }
}

std::unique_ptr<juce::AccessibilityHandler> PresetNameButton::createAccessibilityHandler()
{
    return accessibility::handler (
        *this,
        juce::AccessibilityRole::button,
        [this] { return spokenValue != nullptr ? spokenValue() : juce::String(); },
        [this] { triggerClick(); });
}

PresetBar::PresetBar (PluginProcessor& p) : processor (p)
{
    presets.onClick = [this] {
        if (browser.isVisible())
            browser.close();
        else
            browser.open (processor.loadedPresetName(), lastLoadedEntry());
    };
    browser.opener = &presets;
    presets.spokenValue = [this] {
        const auto name = processor.loadedPresetName();
        return name.isEmpty() ? juce::String ("No Preset") : name + (processor.isLoadedPresetModified() ? ", Modified" : "");
    };
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
    for (auto* button : { &previous, &next })
    {
        button->setTitle (button->getName());
        button->setIconSize (16.0f);
    }
    for (juce::Button* button : std::initializer_list<juce::Button*> { &previous, &presets, &next })
    {
        // Tab reaches them, but a click leaves focus where it was, so Delete still reaches the display.
        button->setMouseClickGrabsKeyboardFocus (false);
        addAndMakeVisible (*button);
    }
    showLoadedPreset();
    startTimerHz (4);
}

int PresetBar::getIdealWidth() const { return 2 * (stepButtonSize + gap) + presets.getIdealWidth(); }

void PresetBar::timerCallback() { showLoadedPreset(); }

void PresetBar::showLoadedPreset()
{
    const int widthBefore = getIdealWidth();
    const auto name = processor.loadedPresetName();
    presets.show (name, processor.isLoadedPresetModified());
    browser.showLoaded (name, lastLoadedEntry());
    if (getIdealWidth() != widthBefore && onIdealWidthChange != nullptr)
        onIdealWidthChange();
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
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Load Preset", "That file isn't an eq1 Preset.", {}, this);
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
    namePrompt->setLookAndFeel (&getLookAndFeel());
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
                                                "Couldn't write the Preset to " + library.folder().getFullPathName() + ".",
                                                {},
                                                this);
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
    auto area = getLocalBounds();
    previous.setBounds (area.removeFromLeft (stepButtonSize).withSizeKeepingCentre (stepButtonSize, stepButtonSize));
    next.setBounds (area.removeFromRight (stepButtonSize).withSizeKeepingCentre (stepButtonSize, stepButtonSize));
    presets.setBounds (area.reduced (gap, 0).withSizeKeepingCentre (area.getWidth() - 2 * gap, PresetNameButton::height));
}

} // namespace eq1
