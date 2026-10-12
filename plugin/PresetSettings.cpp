#include "PresetSettings.h"

#include <unordered_map>

namespace eq1
{

namespace
{
const juce::Identifier parameterType { "PARAM" }, idProperty { "id" }, valueProperty { "value" };

template <typename Visit>
void forEachPresetSetting (juce::AudioProcessorValueTreeState& parameters, Visit&& visit)
{
    for (auto* p : parameters.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p); ranged != nullptr && isPresetSetting (*ranged))
            visit (*ranged);
}

// Visits every setting a Preset holds with the normalised value tree sets it to: its own, or the
// default. The tree's settings are indexed by id first (the first of an id counts, as
// getChildWithProperty would find it), so a visit of them all is linear in their number.
template <typename Visit>
void forEachPresetSettingIn (juce::AudioProcessorValueTreeState& parameters, const juce::ValueTree& tree, Visit&& visit)
{
    std::unordered_map<juce::String, float> saved;
    for (const auto& setting : tree)
        if (setting.hasType (parameterType))
            saved.try_emplace (setting.getProperty (idProperty).toString(), static_cast<float> (setting.getProperty (valueProperty)));
    forEachPresetSetting (parameters, [&] (juce::RangedAudioParameter& p) {
        const auto found = saved.find (p.getParameterID());
        visit (p, found != saved.end() ? p.convertTo0to1 (found->second) : p.getDefaultValue());
    });
}

juce::ValueTree settingOf (const juce::RangedAudioParameter& p, float value)
{
    return juce::ValueTree (parameterType)
        .setProperty (idProperty, p.getParameterID(), nullptr)
        .setProperty (valueProperty, p.convertFrom0to1 (value), nullptr);
}
} // namespace

bool isPresetSetting (const juce::RangedAudioParameter& parameter)
{
    return parameter.getParameterID() != parameters::globalBypassId;
}

juce::ValueTree capturePresetSettings (juce::AudioProcessorValueTreeState& parameters, const juce::Identifier& type)
{
    juce::ValueTree tree (type);
    forEachPresetSetting (parameters, [&tree] (juce::RangedAudioParameter& p) { tree.appendChild (settingOf (p, p.getValue()), nullptr); });
    return tree;
}

void applyPresetSettings (juce::AudioProcessorValueTreeState& parameters, const juce::ValueTree& tree)
{
    std::vector<eq1::parameters::NewValue> changes;
    forEachPresetSettingIn (parameters, tree, [&changes] (juce::RangedAudioParameter& p, float value) {
        if (! juce::exactlyEqual (value, p.getValue()))
            changes.push_back ({ &p, value });
    });
    eq1::parameters::setTogether (changes);
}

juce::ValueTree presetSettingsAsLoaded (juce::AudioProcessorValueTreeState& parameters, const juce::ValueTree& preset, const juce::Identifier& type)
{
    juce::ValueTree tree (type);
    forEachPresetSettingIn (parameters, preset, [&tree] (juce::RangedAudioParameter& p, float value) { tree.appendChild (settingOf (p, value), nullptr); });
    return tree;
}

PresetSettingValues presetSettingValues (juce::AudioProcessorValueTreeState& parameters, const juce::ValueTree& tree)
{
    PresetSettingValues values;
    forEachPresetSettingIn (parameters, tree, [&values] (juce::RangedAudioParameter& p, float value) { values.push_back ({ &p, value }); });
    return values;
}

bool parametersHold (const PresetSettingValues& values)
{
    for (const auto& [parameter, value] : values)
        if (! juce::exactlyEqual (value, parameter->getValue()))
            return false;
    return true;
}

} // namespace eq1
