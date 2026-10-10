#include "PresetBrowser.h"

#include "Accessibility.h"
#include "staple/Fonts.h"
#include "staple/Icons.h"
#include "staple/Tokens.h"
#include "staple/controls/Overlay.h"

#include <algorithm>
#include <cmath>

namespace eq1
{

namespace
{
namespace colour = staple::tokens::colour;
namespace size = staple::tokens::size;
namespace motion = staple::tokens::motion;

// The panel's rows and columns (HANDOFF.md §5.6; prototype Main.dc.html, "Preset browser").
constexpr int searchRowHeight = 56, searchPadLeft = 16, searchPadRight = 14, searchGap = 10;
constexpr int searchWidth = 340, searchHeight = 34, searchPadIn = 12, searchIcon = 14, clearSize = 20;
constexpr int closeSize = 30, closeIcon = 12;
constexpr int folderWidth = 200, folderPadY = 10, folderPadX = 8, folderRowHeight = 32, folderIndent = 12, folderGap = 1;
constexpr int titleHeight = 34, listPadX = 8, listPadBottom = 10, rowHeight = 34, rowPadX = 10;
constexpr int footerHeight = 46, footerPad = 10, footerGap = 4, actionHeight = 30, actionPad = 10, nameFieldWidth = 220, nameFieldHeight = 26;
constexpr float folderDot = 5.0f, presetDot = 6.0f;
constexpr int frameMs = 16;

const juce::String openQuote = juce::String::fromUTF8 ("\xe2\x80\x9c"), closeQuote = juce::String::fromUTF8 ("\xe2\x80\x9d");

juce::String presetsCount (std::size_t count) { return juce::String (count) + (count == 1 ? " Preset" : " Presets"); }

void focusOn (juce::Component& component)
{
    if (component.isShowing())
        component.grabKeyboardFocus();
}
} // namespace

//==============================================================================
PresetBrowser::TextAction::TextAction (const juce::String& text) : juce::Button (text)
{
    setTitle (text);
}

void PresetBrowser::TextAction::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    if (highlighted || down)
    {
        g.setColour (down ? colour::fill2 : colour::fill1);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), size::r2);
    }
    g.setColour (colour::text1);
    g.setFont (staple::font (size::fs3));
    g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred, false);
}

//==============================================================================
PresetBrowser::FolderRow::FolderRow (PresetBrowser& b, const PresetLibrary::Folder& f) : juce::Button (f.name), browser (b), folder (f)
{
    setTitle (folder.name);
    setHasFocusOutline (true);
    setMouseClickGrabsKeyboardFocus (false);
    onClick = [this] { browser.selectFolder (folder.path); };
}

void PresetBrowser::FolderRow::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const bool selected = browser.getSelectedFolder() == folder.path;
    const auto bounds = getLocalBounds().toFloat();
    if (selected || highlighted || down)
    {
        g.setColour (colour::fill1);
        g.fillRoundedRectangle (bounds, size::r2);
    }
    auto area = getLocalBounds().reduced (10, 0);
    g.setFont (staple::font (size::fs2));
    g.setColour (colour::text3);
    const auto count = juce::String (folder.count);
    g.drawText (count, area.removeFromRight (18), juce::Justification::centredRight, false);
    area.removeFromRight (8);
    // A dot when the Loaded Preset is directly in it.
    const bool holdsLoaded = [this] {
        const auto i = browser.loadedIndex();
        return i.has_value() && browser.listing[*i].folder == folder.path;
    }();
    if (holdsLoaded)
    {
        g.setColour (colour::text1);
        g.fillEllipse (static_cast<float> (area.getRight()) - folderDot, bounds.getCentreY() - folderDot / 2.0f, folderDot, folderDot);
    }
    area.removeFromRight (static_cast<int> (folderDot) + 8);
    g.setColour (selected ? colour::text1 : colour::text2);
    g.setFont (staple::font (size::fs4));
    g.drawText (folder.name, area, juce::Justification::centredLeft, true);
}

bool PresetBrowser::FolderRow::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey)
    {
        triggerClick();
        return true;
    }
    return juce::Button::keyPressed (key);
}

std::unique_ptr<juce::AccessibilityHandler> PresetBrowser::FolderRow::createAccessibilityHandler()
{
    return accessibility::handler (
        *this, juce::AccessibilityRole::button, [this] { return juce::String (folder.count); }, [this] { triggerClick(); });
}

//==============================================================================
PresetBrowser::ClearButton::ClearButton() : juce::Button ("Clear search")
{
    setTitle ("Clear search");
    setMouseClickGrabsKeyboardFocus (false);
}

void PresetBrowser::ClearButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (down ? colour::fill3 : colour::fill2);
    g.fillEllipse (bounds);
    staple::drawIcon (g, staple::Icon::close, bounds.withSizeKeepingCentre (8.0f, 8.0f), highlighted ? colour::text1 : colour::text2);
}

bool PresetBrowser::Field::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::returnKey && onEnter != nullptr)
    {
        onEnter();
        return true;
    }
    if (key == juce::KeyPress::escapeKey && onCancel != nullptr)
    {
        onCancel();
        return true;
    }
    const auto before = getText();
    const bool used = juce::TextEditor::keyPressed (key);
    if (getText() != before && onEdit != nullptr)
        onEdit();
    return used;
}

//==============================================================================
void PresetBrowser::SearchBox::paint (juce::Graphics& g)
{
    g.setColour (colour::fill1);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), size::r3);
    const auto icon = juce::Rectangle<float> (static_cast<float> (searchPadIn), (static_cast<float> (getHeight()) - searchIcon) / 2.0f, searchIcon, searchIcon);
    staple::drawIcon (g, staple::Icon::search, icon, colour::text3);
}

void PresetBrowser::Panel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colour::dialog);
    g.fillRoundedRectangle (bounds, size::r3);

    // The rules: under the search row, right of the folders, over the footer.
    const int listTop = searchRowHeight, footerTop = getHeight() - footerHeight;
    g.setColour (colour::line1);
    g.fillRect (1, listTop - 1, getWidth() - 2, 1);
    g.fillRect (folderWidth - 1, listTop, 1, footerTop - listTop);
    g.fillRect (1, footerTop, getWidth() - 2, 1);

    // The count, at the right of the search row, before ✕.
    const auto countArea = juce::Rectangle<int> (0, 0, getWidth() - searchPadRight - closeSize - searchGap, searchRowHeight);
    g.setFont (staple::font (size::fs3));
    g.setColour (colour::text3);
    g.drawText (browser.getCountText(), countArea, juce::Justification::centredRight, false);

    // The list's title, set at the bottom of its row.
    const auto listArea = juce::Rectangle<int> (folderWidth, listTop, getWidth() - folderWidth, footerTop - listTop);
    g.setFont (staple::font (size::fs2));
    g.drawText (browser.getListTitle(), listArea.withHeight (titleHeight).reduced (16, 0).withTrimmedBottom (6), juce::Justification::bottomLeft, true);
    if (browser.rows.empty() && browser.isSearching())
    {
        g.setFont (staple::font (size::fs4));
        g.drawText ("No Presets match your search.", listArea.withTrimmedTop (titleHeight + 40).withHeight (20), juce::Justification::centred, false);
    }

    g.setColour (colour::dialogEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), size::r3, 1.0f);
}

//==============================================================================
PresetBrowser::PresetBrowser (const PresetLibrary& l) : library (l)
{
    setName ("Preset Browser");
    setTitle ("Preset browser");
    setVisible (false);
    // Over everything, the overlay layer's tooltip window included.
    setAlwaysOnTop (true);
    setWantsKeyboardFocus (true);
    // Tab stays within it while it is open.
    setFocusContainerType (FocusContainerType::keyboardFocusContainer);
    addAndMakeVisible (panel);

    search.setTitle ("Search Presets");
    search.setTextToShowWhenEmpty ("Search Presets", colour::text3);
    search.setPopupMenuEnabled (false);
    search.setFont (staple::font (size::fs4));
    search.setIndents (0, 0);
    search.setJustification (juce::Justification::centredLeft);
    for (auto id : { juce::TextEditor::backgroundColourId, juce::TextEditor::outlineColourId, juce::TextEditor::focusedOutlineColourId })
        search.setColour (id, colour::none);
    // Typing shows the results at once; a change from elsewhere (pasting, say) a moment later.
    search.onEdit = [this] { showRows(); };
    search.onTextChange = [this] { showRows(); };
    search.onCancel = [this] { close(); };
    searchBox.addAndMakeVisible (search);
    clearSearch.onClick = [this] {
        search.clear();
        showRows();
        focusOn (search);
    };
    searchBox.addChildComponent (clearSearch);
    panel.addAndMakeVisible (searchBox);

    closeButton.setTitle ("Close Preset browser");
    closeButton.setIconSize (static_cast<float> (closeIcon));
    closeButton.setMouseClickGrabsKeyboardFocus (false);
    closeButton.onClick = [this] { close(); };
    panel.addAndMakeVisible (closeButton);

    folderView.setViewedComponent (&folderColumn, false);
    folderView.setScrollBarsShown (true, false);
    folderView.setScrollBarThickness (6);
    panel.addAndMakeVisible (folderView);

    list.setName ("Presets List");
    list.setTitle ("Presets");
    list.setRowHeight (rowHeight);
    list.setColour (juce::ListBox::backgroundColourId, colour::none);
    list.setColour (juce::ListBox::outlineColourId, colour::none);
    // Tab reaches it from the search; the arrow keys move through it and Return loads.
    list.setWantsKeyboardFocus (true);
    panel.addAndMakeVisible (list);
    // To light the row under the mouse.
    list.addMouseListener (this, true);

    save.onClick = [this] { startSaving(); };
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
        button->setMouseClickGrabsKeyboardFocus (false);
        panel.addAndMakeVisible (*button);
    }

    nameField.setTitle ("Preset name");
    nameField.setTextToShowWhenEmpty ("Preset name", colour::text3);
    nameField.setPopupMenuEnabled (false);
    nameField.setFont (staple::font (size::fs3));
    nameField.setIndents (8, 0);
    nameField.setJustification (juce::Justification::centredLeft);
    // The knob type-in field's look.
    nameField.setColour (juce::TextEditor::backgroundColourId, colour::fill2);
    nameField.setColour (juce::TextEditor::outlineColourId, colour::line3);
    nameField.setColour (juce::TextEditor::focusedOutlineColourId, colour::line3);
    nameField.setColour (juce::TextEditor::textColourId, colour::text1);
    nameField.onEnter = [this] { stopSaving (true); };
    nameField.onCancel = [this] { stopSaving (false); };
    panel.addChildComponent (nameField);

    // Tab's order: the search and its clear button, the list, the folders, the footer, then ✕.
    int order = 0;
    for (juce::Component* c : std::initializer_list<juce::Component*> { &searchBox, &list, &folderView, &save, &nameField, &loadFile, &showFolder, &closeButton })
        c->setExplicitFocusOrder (++order);
}

PresetBrowser::~PresetBrowser() { list.removeMouseListener (this); }

void PresetBrowser::open (const juce::String& loadedPreset, const PresetLibrary::Entry* lastLoadedEntry)
{
    listing = library.listing();
    loaded = loadedPreset;
    lastLoaded = lastLoadedEntry != nullptr ? std::optional (*lastLoadedEntry) : std::nullopt;
    const auto i = loadedIndex();
    selectedFolder = i.has_value() ? listing[*i].folder : juce::String ("Factory");
    nameField.setVisible (false);
    search.clear();
    showFolders();
    showRows();
    if (auto* parent = getParentComponent())
        setBounds (parent->getLocalBounds());
    if (! isVisible())
        focusBefore = juce::Component::getCurrentlyFocusedComponent();
    toFront (false);
    setVisible (true);
    focusOn (search);
    // Pops in over dur2, as a popover does.
    openedAt = juce::Time::getMillisecondCounterHiRes();
    timerCallback();
    startTimer (frameMs);
}

void PresetBrowser::close()
{
    if (! isVisible())
        return;
    stopTimer();
    stopSaving (false);
    panel.setTransform ({});
    setAlpha (1.0f);
    setVisible (false);
    auto* returnTo = focusBefore != nullptr ? focusBefore.getComponent() : opener;
    if (returnTo != nullptr)
        focusOn (*returnTo);
}

void PresetBrowser::timerCallback()
{
    const float t = static_cast<float> ((juce::Time::getMillisecondCounterHiRes() - openedAt) / motion::dur2Ms);
    const float progress = staple::ease (juce::jlimit (0.0f, 1.0f, t));
    const float scale = motion::popInScale + (1.0f - motion::popInScale) * progress;
    const auto centre = panel.getLocalBounds().toFloat().getCentre();
    panel.setTransform (juce::AffineTransform::scale (scale, scale, centre.x, centre.y).translated (0.0f, motion::popInOffset * (1.0f - progress)));
    setAlpha (progress);
    if (t >= 1.0f)
    {
        stopTimer();
        panel.setTransform ({});
        setAlpha (1.0f);
    }
}

void PresetBrowser::showLoaded (const juce::String& loadedPreset, const PresetLibrary::Entry* lastLoadedEntry)
{
    const auto samePlace = [] (const PresetLibrary::Entry* a, const std::optional<PresetLibrary::Entry>& b) {
        return a == nullptr ? ! b.has_value() : b.has_value() && a->folder == b->folder && a->name == b->name;
    };
    if (loaded == loadedPreset && samePlace (lastLoadedEntry, lastLoaded))
        return;
    loaded = loadedPreset;
    lastLoaded = lastLoadedEntry != nullptr ? std::optional (*lastLoadedEntry) : std::nullopt;
    list.repaint();
    folderColumn.repaint();
}

juce::Rectangle<int> PresetBrowser::getPanelBounds() const { return panel.getBounds(); }
juce::String PresetBrowser::getSelectedFolder() const { return isSearching() ? juce::String() : selectedFolder; }
juce::String PresetBrowser::getCountText() const { return presetsCount (listing.size()); }

juce::String PresetBrowser::getListTitle() const
{
    if (isSearching())
        return juce::String (rows.size()) + (rows.size() == 1 ? " result for " : " results for ") + openQuote + search.getText().trim() + closeQuote;
    const auto folder = std::find_if (folders.begin(), folders.end(), [this] (const PresetLibrary::Folder& f) { return f.path == selectedFolder; });
    return folder != folders.end() ? folder->name : selectedFolder;
}

void PresetBrowser::selectFolder (const juce::String& path)
{
    selectedFolder = path;
    search.clear();
    showRows();
}

bool PresetBrowser::isSearching() const { return search.getText().trim().isNotEmpty(); }

void PresetBrowser::showFolders()
{
    folders = PresetLibrary::folders (listing, library.userSubfolders());
    folderRows.clear();
    int y = folderPadY;
    const int width = folderWidth - 1 - 2 * folderPadX;
    for (const auto& folder : folders)
    {
        auto& row = *folderRows.emplace_back (std::make_unique<FolderRow> (*this, folder));
        const int indent = folder.depth * folderIndent;
        row.setBounds (folderPadX + indent, y, width - indent, folderRowHeight);
        folderColumn.addAndMakeVisible (row);
        y += folderRowHeight + folderGap;
    }
    folderColumn.setSize (folderWidth - 1, y + folderPadY);
}

void PresetBrowser::showRows()
{
    rows.clear();
    const auto text = search.getText().trim();
    for (const auto& entry : listing)
        if (text.isEmpty() ? entry.folder == selectedFolder : entry.name.containsIgnoreCase (text))
            rows.push_back (&entry);
    clearSearch.setVisible (text.isNotEmpty());
    list.updateContent();
    list.deselectAllRows();
    list.repaint();
    folderColumn.repaint();
    panel.repaint();
}

std::optional<std::size_t> PresetBrowser::loadedIndex() const
{
    return PresetLibrary::find (listing, loaded, lastLoaded.has_value() ? &*lastLoaded : nullptr);
}

bool PresetBrowser::isLoaded (const PresetLibrary::Entry& entry) const
{
    const auto i = loadedIndex();
    return i.has_value() && listing[*i].name == entry.name && listing[*i].folder == entry.folder;
}

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (row < 0 || row >= getNumRows())
        return;
    const auto& entry = *rows[static_cast<std::size_t> (row)];
    const auto bounds = juce::Rectangle<int> (width, height).reduced (listPadX, 0);
    const bool hovered = list.getRowContainingPosition (list.getMouseXYRelative().x, list.getMouseXYRelative().y) == row && list.isMouseOver (true);
    if (selected || hovered)
    {
        g.setColour (colour::fill1);
        g.fillRoundedRectangle (bounds.toFloat().withTrimmedBottom (static_cast<float> (folderGap)), size::r2);
    }
    auto area = bounds.reduced (rowPadX, 0);
    // The Loaded Preset's entry, which ‹ › step from, has a dot.
    if (isLoaded (entry))
    {
        g.setColour (colour::text1);
        g.fillEllipse (static_cast<float> (area.getRight()) - presetDot, (static_cast<float> (height) - presetDot) / 2.0f, presetDot, presetDot);
    }
    area.removeFromRight (static_cast<int> (presetDot) + 8);
    if (isSearching())
    {
        g.setColour (colour::text3);
        g.setFont (staple::font (size::fs2));
        const auto folderWidthPx = juce::jmin (area.getWidth() / 2, static_cast<int> (std::ceil (juce::GlyphArrangement::getStringWidth (staple::font (size::fs2), entry.folder))));
        g.drawText (entry.folder, area.removeFromRight (folderWidthPx), juce::Justification::centredRight, true);
        area.removeFromRight (8);
    }
    g.setColour (colour::text1);
    g.setFont (staple::font (size::fs4));
    g.drawText (entry.name, area, juce::Justification::centredLeft, true);
}

juce::String PresetBrowser::getNameForRow (int row)
{
    if (row < 0 || row >= getNumRows())
        return {};
    const auto& entry = *rows[static_cast<std::size_t> (row)];
    auto name = isSearching() ? entry.name + ", " + entry.folder : entry.name;
    if (isLoaded (entry))
        name << ", Loaded Preset";
    return name;
}

std::unique_ptr<juce::AccessibilityHandler> PresetBrowser::createAccessibilityHandler()
{
    return accessibility::handler (*this, juce::AccessibilityRole::group, nullptr);
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent&) { loadRow (row); }
void PresetBrowser::returnKeyPressed (int row) { loadRow (row); }

void PresetBrowser::loadRow (int row)
{
    if (row < 0 || row >= getNumRows() || onLoad == nullptr)
        return;
    // A copy: loading can rescan the listing the row points into.
    const auto entry = *rows[static_cast<std::size_t> (row)];
    onLoad (entry);
}

void PresetBrowser::startSaving()
{
    nameField.clear();
    nameField.setVisible (true);
    layOutPanel();
    focusOn (nameField);
}

void PresetBrowser::stopSaving (bool commit)
{
    if (! nameField.isVisible())
        return;
    const auto name = nameField.getText().trim();
    const bool hadFocus = nameField.hasKeyboardFocus (true);
    nameField.setVisible (false);
    layOutPanel();
    if (hadFocus)
        focusOn (save);
    if (! commit || name.isEmpty() || onSave == nullptr)
        return;
    onSave (name);
    // Listed in User now, as the Loaded Preset: show it there.
    listing = library.listing();
    const auto i = loadedIndex();
    selectedFolder = i.has_value() ? listing[*i].folder : juce::String ("User");
    search.clear();
    showFolders();
    showRows();
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
    // Only the scrim's own clicks: the panel's children take theirs.
    if (event.eventComponent == this && ! panel.getBounds().contains (event.getPosition()))
        close();
}

void PresetBrowser::mouseMove (const juce::MouseEvent& event)
{
    if (event.eventComponent != this)
        list.repaint();
}

void PresetBrowser::mouseExit (const juce::MouseEvent& event) { mouseMove (event); }

void PresetBrowser::paint (juce::Graphics& g)
{
    g.fillAll (colour::scrim);
    // shadow2, approximated without blur.
    staple::drawSoftShadow (g, panel.getBounds().toFloat(), size::r3, staple::tokens::shadow::shadow2);
}

void PresetBrowser::resized() { layOutPanel(); }

void PresetBrowser::layOutPanel()
{
    // 860 x 520, centred, 64 px from the top; inside a smaller window, smaller, keeping the margin.
    const int width = juce::jmin (panelWidth, getWidth() - 2 * margin);
    const int height = juce::jmin (panelHeight, getHeight() - 2 * margin);
    const int top = juce::jlimit (margin, juce::jmax (margin, getHeight() - margin - height), panelTop);
    panel.setBounds ((getWidth() - width) / 2, top, juce::jmax (0, width), juce::jmax (0, height));

    auto area = panel.getLocalBounds();
    auto searchRow = area.removeFromTop (searchRowHeight).withTrimmedLeft (searchPadLeft).withTrimmedRight (searchPadRight);
    closeButton.setBounds (searchRow.removeFromRight (closeSize).withSizeKeepingCentre (closeSize, closeSize));
    const int boxWidth = juce::jmin (searchWidth, searchRow.getWidth() / 2);
    searchBox.setBounds (searchRow.removeFromLeft (boxWidth).withSizeKeepingCentre (boxWidth, searchHeight));
    auto inBox = searchBox.getLocalBounds().withTrimmedLeft (searchPadIn + searchIcon + 8).withTrimmedRight (10);
    clearSearch.setBounds (inBox.removeFromRight (clearSize).withSizeKeepingCentre (clearSize, clearSize));
    inBox.removeFromRight (6);
    search.setBounds (inBox.withSizeKeepingCentre (inBox.getWidth(), 30));

    auto footer = area.removeFromBottom (footerHeight).reduced (footerPad, 0).withTrimmedTop (1);
    const auto place = [this, &footer] (juce::Component& c, int w) {
        c.setBounds (footer.removeFromLeft (w).withSizeKeepingCentre (w, &c == &nameField ? nameFieldHeight : actionHeight));
        footer.removeFromLeft (footerGap);
    };
    const auto widthOf = [] (TextAction& b) {
        return static_cast<int> (std::ceil (juce::GlyphArrangement::getStringWidth (staple::font (size::fs3), b.getButtonText()))) + 2 * actionPad;
    };
    place (save, widthOf (save));
    // The name field opens beside Save as, before the other actions.
    if (nameField.isVisible())
        place (nameField, nameFieldWidth);
    place (loadFile, widthOf (loadFile));
    place (showFolder, widthOf (showFolder));

    folderView.setBounds (area.removeFromLeft (folderWidth - 1));
    area.removeFromLeft (1);
    list.setBounds (area.withTrimmedTop (titleHeight).withTrimmedBottom (listPadBottom));
}

} // namespace eq1
