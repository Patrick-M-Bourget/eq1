#include "PresetSettings.h"

#include <cmath>
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
    std::unordered_map<juce::String, juce::var> saved;
    for (const auto& setting : tree)
        if (setting.hasType (parameterType))
            saved.try_emplace (setting.getProperty (idProperty).toString(), setting.getProperty (valueProperty));
    forEachPresetSetting (parameters, [&] (juce::RangedAudioParameter& p) {
        const auto found = saved.find (p.getParameterID());
        visit (p, found != saved.end() ? p.convertTo0to1 (restoredPlainValue (p, found->second)) : p.getDefaultValue());
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

float restoredPlainValue (const juce::RangedAudioParameter& parameter, const juce::var& saved)
{
    // Read in double, so a value a float can't hold ("1e39") is still a finite one to clamp.
    const double value = static_cast<double> (saved);
    const auto& range = parameter.getNormalisableRange();
    if (! std::isfinite (value))
        return parameter.convertFrom0to1 (parameter.getDefaultValue());
    return static_cast<float> (juce::jlimit (static_cast<double> (range.start), static_cast<double> (range.end), value));
}

void sanitiseSavedSettings (juce::AudioProcessorValueTreeState& parameters, juce::ValueTree& tree)
{
    for (auto setting : tree)
    {
        if (! setting.hasType (parameterType))
            continue;
        const auto* p = parameters.getParameter (setting.getProperty (idProperty).toString());
        if (p == nullptr)
            continue;
        const auto saved = setting.getProperty (valueProperty);
        const double value = static_cast<double> (saved);
        if (std::isfinite (value) && value >= p->getNormalisableRange().start && value <= p->getNormalisableRange().end)
            continue;
        setting.setProperty (valueProperty, restoredPlainValue (*p, saved), nullptr);
    }
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
