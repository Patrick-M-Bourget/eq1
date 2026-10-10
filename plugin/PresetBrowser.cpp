#include "PresetBrowser.h"

#include <algorithm>

namespace eq1
{

PresetBrowser::PresetBrowser (const PresetLibrary& l) : library (l)
{
    search.setTextToShowWhenEmpty ("Search Presets", juce::Colours::grey);
    search.setPopupMenuEnabled (false);
    search.onTextChange = [this] { showRows(); };
    search.onEscapeKey = [this] { close(); };
    addAndMakeVisible (search);

    list.setRowHeight (22);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    list.setWantsKeyboardFocus (false);
    addAndMakeVisible (list);

    save.onClick = [this] {
        if (onSave != nullptr)
            onSave();
    };
    loadFile.onClick = [this] {
        if (onLoadFile != nullptr)
            onLoadFile();
    };
    showFolder.onClick = [this] {
        library.folder().createDirectory();
        library.folder().revealToUser();
    };
    for (auto* button : { &save, &loadFile, &showFolder })
    {
        button->setWantsKeyboardFocus (false);
        addAndMakeVisible (*button);
    }
    setWantsKeyboardFocus (true);
}

PresetBrowser::~PresetBrowser() { juce::Desktop::getInstance().removeGlobalMouseListener (this); }

void PresetBrowser::open (const juce::String& loadedPreset)
{
    listing = library.listing();
    loaded = loadedPreset;
    search.clear();
    showRows();
    setVisible (true);
    toFront (false);
    if (isShowing())
        search.grabKeyboardFocus();
}

void PresetBrowser::close() { setVisible (false); }

void PresetBrowser::visibilityChanged()
{
    if (isVisible())
        juce::Desktop::getInstance().addGlobalMouseListener (this);
    else
        juce::Desktop::getInstance().removeGlobalMouseListener (this);
}

void PresetBrowser::showLoaded (const juce::String& loadedPreset)
{
    if (loaded == loadedPreset)
        return;
    loaded = loadedPreset;
    list.repaint();
}

void PresetBrowser::showRows()
{
    rows.clear();
    const auto text = search.getText().trim();
    if (text.isEmpty())
    {
        for (const auto& entry : listing)
        {
            if (rows.empty() || entry.folder != rows.back().entry->folder)
                rows.push_back ({ entry.folder, {}, nullptr });
            rows.push_back ({ entry.name, {}, &entry });
        }
    }
    else
    {
        found = PresetLibrary::search (listing, text);
        for (const auto& entry : found)
            rows.push_back ({ entry.name, entry.folder, &entry });
        if (rows.empty())
            rows.push_back ({ "No Presets match", {}, nullptr });
    }
    list.updateContent();
    list.repaint();
}

bool PresetBrowser::isLoaded (const PresetLibrary::Entry& entry) const
{
    const auto first = std::find_if (listing.begin(), listing.end(), [this] (const PresetLibrary::Entry& e) { return e.name == loaded; });
    return first != listing.end() && first->name == entry.name && first->folder == entry.folder;
}

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool)
{
    if (row < 0 || row >= getNumRows())
        return;
    const auto& [text, folder, entry] = rows[static_cast<std::size_t> (row)];
    auto area = juce::Rectangle<int> (width, height).reduced (8, 0);
    if (entry == nullptr)
    {
        g.setColour (juce::Colours::grey);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (text, area, juce::Justification::centredLeft, true);
        return;
    }
    // The Loaded Preset's first entry, which ‹ › step from, is lit in the display's blue.
    if (isLoaded (*entry))
    {
        g.setColour (juce::Colour (0x602f8fd0));
        g.fillRect (0, 0, width, height);
    }
    g.setFont (juce::FontOptions (14.0f));
    if (folder.isNotEmpty())
    {
        g.setColour (juce::Colours::grey);
        g.drawText (folder, area.removeFromRight (area.getWidth() / 2), juce::Justification::centredRight, true);
    }
    else
        area.removeFromLeft (12); // Indented under its folder's name.
    g.setColour (juce::Colours::white);
    g.drawText (text, area, juce::Justification::centredLeft, true);
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= getNumRows())
        return;
    if (const auto* entry = rows[static_cast<std::size_t> (row)].entry; entry != nullptr && onLoad != nullptr)
        onLoad (*entry);
}

bool PresetBrowser::keyPressed (const juce::KeyPress& key)
{
    if (key != juce::KeyPress::escapeKey)
        return false;
    close();
    return true;
}

void PresetBrowser::mouseDown (const juce::MouseEvent& event)
{
    auto* clicked = event.eventComponent;
    if (clicked == nullptr || clicked == this || isParentOf (clicked) || (opener != nullptr && (clicked == opener || opener->isParentOf (clicked))))
        return;
    close();
}

void PresetBrowser::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xf01d2027));
    g.setColour (juce::Colour (0x40ffffff));
    g.drawRect (getLocalBounds());
}

void PresetBrowser::resized()
{
    auto area = getLocalBounds().reduced (8);
    search.setBounds (area.removeFromTop (26));
    area.removeFromTop (6);
    auto buttons = area.removeFromBottom (26);
    area.removeFromBottom (6);
    list.setBounds (area);
    save.setBounds (buttons.removeFromLeft (150));
    buttons.removeFromLeft (6);
    loadFile.setBounds (buttons.removeFromLeft (130));
    buttons.removeFromLeft (6);
    showFolder.setBounds (buttons.removeFromLeft (170));
}

} // namespace eq1
