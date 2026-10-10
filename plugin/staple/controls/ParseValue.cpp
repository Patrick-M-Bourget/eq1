#include "ParseValue.h"

namespace staple
{

std::optional<double> parseValue (const juce::String& text, const juce::String& unit)
{
    const auto t = text.trim().toLowerCase().replaceCharacter (',', '.');
    auto p = t.getCharPointer();

    // The number: an optional sign, digits, and at most one point with a digit after it.
    juce::String number;
    if (*p == '+' || *p == '-')
        number << *p++;
    bool digits = false, point = false;
    for (; ! p.isEmpty(); ++p)
    {
        if (juce::CharacterFunctions::isDigit (*p))
            digits = true;
        else if (*p == '.' && ! point)
            point = true;
        else
            break;
        number << *p;
    }
    if (! digits || number.endsWithChar ('.'))
        return std::nullopt;

    double value = number.getDoubleValue();
    const auto suffix = juce::String (p).trim();
    const auto base = unit.trim().toLowerCase();
    if (suffix.isEmpty() || suffix == base || (suffix == "hz" && base.isEmpty()) || (suffix == "db" && base.isEmpty()))
        return value;
    if (suffix == "k" || suffix == "khz")
        return value * 1000.0;
    if (suffix == "s" && base == "ms")
        return value * 1000.0;
    if (suffix == "ms" && base == "s")
        return value / 1000.0;
    return std::nullopt;
}

} // namespace staple
