#include "UserSettings.h"

#include "UiScale.h"

namespace eq1
{

namespace
{
const juce::String uiScaleKey { "uiScalePercent" };
}

UserSettings::UserSettings (juce::File f) : file (std::move (f)) {}

juce::File UserSettings::defaultFile()
{
    auto folder = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
#if JUCE_MAC
    folder = folder.getChildFile ("Application Support");
#endif
    return folder.getChildFile ("eq1").getChildFile ("eq1.settings");
}

juce::PropertiesFile::Options UserSettings::options() const
{
    juce::PropertiesFile::Options o;
    o.storageFormat = juce::PropertiesFile::storeAsXML;
    o.millisecondsBeforeSaving = -1; // saved at once, below
    o.processLock = &lock;
    return o;
}

int UserSettings::uiScalePercent() const
{
    if (file == juce::File())
        return uiScale::defaultPercent;
    const juce::PropertiesFile settings (file, options());
    const int percent = settings.getIntValue (uiScaleKey, uiScale::defaultPercent);
    return uiScale::isOffered (percent) ? percent : uiScale::defaultPercent;
}

void UserSettings::setUiScalePercent (int percent)
{
    if (file == juce::File() || ! uiScale::isOffered (percent))
        return;
    juce::PropertiesFile settings (file, options());
    settings.setValue (uiScaleKey, percent);
    settings.save();
}

} // namespace eq1
