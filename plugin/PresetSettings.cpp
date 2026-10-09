#include "PresetSettings.h"

#include "Parameters.h"

#include <vector>

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

// Visits every setting a Preset holds with the normalised value tree sets it to: its own, or the default.
template <typename Visit>
void forEachPresetSettingIn (juce::AudioProcessorValueTreeState& parameters, const juce::ValueTree& tree, Visit&& visit)
{
    forEachPresetSetting (parameters, [&] (juce::RangedAudioParameter& p) {
        const auto saved = tree.getChildWithProperty (idProperty, p.getParameterID());
        visit (p, saved.isValid() ? p.convertTo0to1 (static_cast<float> (saved.getProperty (valueProperty))) : p.getDefaultValue());
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


bool holdsPresetSettings (juce::AudioProcessorValueTreeState& parameters, const juce::ValueTree& tree)
{
    bool holds = true;
    forEachPresetSettingIn (parameters, tree, [&holds] (juce::RangedAudioParameter& p, float value) { holds = holds && juce::exactlyEqual (value, p.getValue()); });
    return holds;
}

} // namespace eq1
