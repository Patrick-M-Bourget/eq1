#include "Accessibility.h"

#include "ValueText.h"

#include <limits>

namespace eq1::accessibility
{

juce::String spokenValue (const juce::RangedAudioParameter& parameter, float normalisedValue)
{
    const auto text = parameter.getText (normalisedValue, 0).trim();
    const auto label = parameter.getLabel();
    const bool numeric = text.isNotEmpty() && (juce::CharacterFunctions::isDigit (text[0]) || text[0] == '-' || text[0] == '+' || text[0] == '.');
    if (! numeric)
        return text;
    const double value = parameter.convertFrom0to1 (normalisedValue);
    if (label == "Hz")
        return valueText::frequency (value);
    if (label == "dB")
    {
        // To as many decimals as the parameter's own text has; its silent bottom is -inf.
        const int decimals = text.containsChar ('.') ? text.length() - 1 - text.lastIndexOfChar ('.') : 0;
        const bool silent = text.endsWithIgnoreCase ("inf");
        return valueText::signedDb (silent ? -std::numeric_limits<double>::infinity() : value, { .decimals = decimals });
    }
    if (label == "dB/oct")
        return valueText::slope (value);
    if (label == "%")
        return valueText::percent (value);
    return label.isEmpty() ? text : text + " " + label;
}

} // namespace eq1::accessibility
