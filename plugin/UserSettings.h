#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace eq1
{

// What eq1 keeps per user, across instances, formats and hosts, in one settings file: for now, the
// UI Scale last picked, the default for new instances. Every read and write goes through an
// inter-process lock, as several hosts may run eq1 at once. Message thread only.
class UserSettings
{
public:
    // Without a file nothing is kept: every read gives the default.
    explicit UserSettings (juce::File file);
    // "Application Support/eq1" on macOS, "%APPDATA%\eq1" on Windows.
    static juce::File defaultFile();

    // The UI Scale last picked, in percent, or uiScale::defaultPercent when there is none or the
    // file can't be read.
    int uiScalePercent() const;
    // Saves percent as the UI Scale last picked. False when the file can't be written (the folder
    // can't be made, no permission, a full disk), which is logged with the file's path, and the
    // instance keeps its own UI Scale; the next pick, in any instance, writes the file afresh.
    bool setUiScalePercent (int percent);

private:
    juce::File file;
    juce::PropertiesFile::Options options() const;
    mutable juce::InterProcessLock lock { "eq1-user-settings" };
};

} // namespace eq1
