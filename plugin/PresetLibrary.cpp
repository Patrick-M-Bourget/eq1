#include "PresetLibrary.h"

#include "FactoryPresets.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <set>

namespace eq1
{

namespace
{
juce::ValueTree fromXml (const std::unique_ptr<juce::XmlElement>& xml)
{
    return xml != nullptr ? juce::ValueTree::fromXml (*xml) : juce::ValueTree();
}

bool byName (const juce::String& a, const juce::String& b) { return a.compareNatural (b) < 0; }

// Files by the name of the Preset they hold, folders by their whole name.
std::vector<juce::File> byFileName (const juce::Array<juce::File>& found)
{
    const auto name = [] (const juce::File& f) { return f.isDirectory() ? f.getFileName() : f.getFileNameWithoutExtension(); };
    std::vector<juce::File> files (found.begin(), found.end());
    std::sort (files.begin(), files.end(), [&] (const juce::File& a, const juce::File& b) { return byName (name (a), name (b)); });
    return files;
}

// Calls visit for root, then each of its subfolders, depth-first, each level by name; a folder reached
// twice (by a link back up the tree) is visited once.
void eachFolder (const juce::File& root, const std::function<void (const juce::File&)>& visit)
{
    std::set<juce::String> visited;
    const std::function<void (const juce::File&)> walk = [&] (const juce::File& folder) {
        if (! visited.insert (folder.getLinkedTarget().getFullPathName()).second)
            return;
        visit (folder);
        for (const auto& subfolder : byFileName (folder.findChildFiles (juce::File::findDirectories, false)))
            walk (subfolder);
    };
    if (root.isDirectory())
        walk (root);
}

// A folder in the User folder as Entry::folder names it: "User", or "User/Drums" with / on every OS.
juce::String folderPath (const juce::File& folder, const juce::File& userFolder)
{
    return folder == userFolder ? PresetLibrary::userFolderName : PresetLibrary::userFolderName + "/" + folder.getRelativePathFrom (userFolder).replaceCharacter ('\\', '/');
}
} // namespace

PresetLibrary::PresetLibrary (juce::File folder) : userFolder (std::move (folder)) {}

juce::File PresetLibrary::defaultUserFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("eq1").getChildFile ("Presets");
}

std::vector<juce::File> PresetLibrary::userPresets() const
{
    std::vector<juce::File> files;
    eachFolder (userFolder, [&] (const juce::File& folder) {
        for (const auto& found : byFileName (folder.findChildFiles (juce::File::findFiles, false, "*" + fileExtension)))
            files.push_back (found);
    });
    return files;
}

std::vector<juce::String> PresetLibrary::userSubfolders() const
{
    std::vector<juce::String> paths;
    eachFolder (userFolder, [&] (const juce::File& folder) {
        if (folder != userFolder)
            paths.push_back (folderPath (folder, userFolder));
    });
    return paths;
}

std::optional<juce::File> PresetLibrary::save (const juce::String& name, const juce::ValueTree& preset) const
{
    const auto file = userFolder.getChildFile (juce::File::createLegalFileName (name.trim()) + fileExtension);
    const auto xml = preset.createXml();
    if (xml == nullptr || ! userFolder.createDirectory() || ! xml->writeTo (file))
        return std::nullopt;
    return file;
}

juce::ValueTree PresetLibrary::read (const juce::File& file)
{
    const auto preset = file.existsAsFile() ? fromXml (juce::XmlDocument::parse (file)) : juce::ValueTree();
    return preset.hasType ("eq1") ? preset : juce::ValueTree();
}

std::vector<PresetLibrary::FactoryPreset> PresetLibrary::factoryPresets()
{
    std::vector<FactoryPreset> presets;
    for (int i = 0; i < FactoryPresets::namedResourceListSize; ++i)
    {
        int size = 0;
        const auto* data = FactoryPresets::getNamedResource (FactoryPresets::namedResourceList[i], size);
        // Filenames are UTF-8: Factory names hold an en dash ("Drums \u2013 Kick In Punch").
        const auto name = juce::String::fromUTF8 (FactoryPresets::originalFilenames[i]).upToLastOccurrenceOf (fileExtension, false, false);
        presets.push_back ({ name, fromXml (juce::XmlDocument::parse (juce::String::fromUTF8 (data, size))) });
    }
    std::sort (presets.begin(), presets.end(), [] (const FactoryPreset& a, const FactoryPreset& b) { return byName (a.name, b.name); });
    return presets;
}

std::vector<PresetLibrary::Entry> PresetLibrary::listing() const
{
    std::vector<Entry> entries;
    for (auto& [name, preset] : factoryPresets())
        entries.push_back ({ name, "Factory", {}, preset });
    for (const auto& file : userPresets())
        if (auto preset = read (file); preset.isValid())
            entries.push_back ({ file.getFileNameWithoutExtension(), folderPath (file.getParentDirectory(), userFolder), file, preset });
    return entries;
}

std::vector<PresetLibrary::Folder> PresetLibrary::folders (const std::vector<Entry>& entries, const std::vector<juce::String>& subfolders)
{
    std::vector<Folder> found { { "Factory", "Factory", 0, 0 }, { userFolderName, userFolderName, 0, 0 } };
    const auto add = [&found] (const juce::String& path) -> Folder& {
        const auto at = std::find_if (found.begin(), found.end(), [&path] (const Folder& f) { return f.path == path; });
        if (at != found.end())
            return *at;
        const auto parts = juce::StringArray::fromTokens (path, "/", {});
        return found.emplace_back (Folder { path, parts[parts.size() - 1], parts.size() - 1, 0 });
    };
    for (const auto& path : subfolders)
        add (path);
    for (const auto& entry : entries)
    {
        // Its enclosing folders first, so one holding only subfolders is listed above them.
        for (int slash = entry.folder.indexOfChar ('/'); slash > 0; slash = entry.folder.indexOfChar (slash + 1, '/'))
            if (const auto enclosing = entry.folder.substring (0, slash); enclosing != userFolderName)
                add (enclosing);
        ++add (entry.folder).count;
    }
    return found;
}

std::vector<PresetLibrary::Entry> PresetLibrary::search (const std::vector<Entry>& entries, const juce::String& text)
{
    std::vector<Entry> found;
    std::copy_if (entries.begin(), entries.end(), std::back_inserter (found), [&] (const Entry& entry) { return entry.name.containsIgnoreCase (text); });
    return found;
}

std::optional<std::size_t> PresetLibrary::find (const std::vector<Entry>& entries, const juce::String& loadedPreset, const Entry* lastLoaded)
{
    const auto index = [&] (auto matches) -> std::optional<std::size_t> {
        const auto found = std::find_if (entries.begin(), entries.end(), matches);
        if (found == entries.end())
            return std::nullopt;
        return static_cast<std::size_t> (found - entries.begin());
    };
    if (lastLoaded != nullptr && lastLoaded->name == loadedPreset)
        if (const auto i = index ([&] (const Entry& entry) { return entry.folder == lastLoaded->folder && entry.name == lastLoaded->name; }))
            return i;
    return index ([&] (const Entry& entry) { return entry.name == loadedPreset; });
}

std::optional<std::size_t> PresetLibrary::step (const std::vector<Entry>& entries, const juce::String& loadedPreset, int by, const Entry* lastLoaded)
{
    if (entries.empty())
        return std::nullopt;
    const auto size = static_cast<int> (entries.size());
    const auto loaded = find (entries, loadedPreset, lastLoaded);
    // Not in the library: as if just before the first, for forward, or just after the last, for back.
    const auto from = loaded.has_value() ? static_cast<int> (*loaded) : (by > 0 ? -1 : size);
    return static_cast<std::size_t> (((from + by) % size + size) % size);
}

} // namespace eq1
