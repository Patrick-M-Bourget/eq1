#include "Accessibility.h"

namespace eq1::accessibility
{

juce::String spokenValue (const juce::RangedAudioParameter& parameter, float normalisedValue)
{
    auto text = parameter.getText (normalisedValue, 0).trim();
    const auto label = parameter.getLabel();
    const bool numeric = text.isNotEmpty() && (juce::CharacterFunctions::isDigit (text[0]) || text[0] == '-' || text[0] == '+' || text[0] == '.');
    if (! numeric)
        return text;
    if (label == "dB" && ! text.startsWithChar ('-') && ! text.startsWithChar ('+') && text.getDoubleValue() > 0.0)
        text = "+" + text;
    return label.isEmpty() ? text : text + " " + label;
}

} // namespace eq1::accessibility
