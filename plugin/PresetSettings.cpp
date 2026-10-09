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
} // namespace

bool isPresetSetting (const juce::RangedAudioParameter& parameter)
{
    return parameter.getParameterID() != parameters::globalBypassId;
}

juce::ValueTree capturePresetSettings (juce::AudioProcessorValueTreeState& parameters, const juce::Identifier& type)
{
    juce::ValueTree tree (type);
    forEachPresetSetting (parameters, [&tree] (juce::RangedAudioParameter& p) {
        tree.appendChild (juce::ValueTree (parameterType)
                              .setProperty (idProperty, p.getParameterID(), nullptr)
                              .setProperty (valueProperty, p.convertFrom0to1 (p.getValue()), nullptr),
                          nullptr);
    });
    return tree;
}

void applyPresetSettings (juce::AudioProcessorValueTreeState& parameters, const juce::ValueTree& tree)
{
    struct Change
    {
        juce::RangedAudioParameter* parameter;
        float value;
    };
    std::vector<Change> changes;
    forEachPresetSetting (parameters, [&] (juce::RangedAudioParameter& p) {
        const auto saved = tree.getChildWithProperty (idProperty, p.getParameterID());
        const float value = saved.isValid() ? p.convertTo0to1 (static_cast<float> (saved.getProperty (valueProperty)))
                                            : p.getDefaultValue();
        if (! juce::exactlyEqual (value, p.getValue()))
            changes.push_back ({ &p, value });
    });
    for (const auto& change : changes)
        change.parameter->beginChangeGesture();
    for (const auto& change : changes)
        change.parameter->setValueNotifyingHost (change.value);
    for (const auto& change : changes)
        change.parameter->endChangeGesture();
}

} // namespace eq1
