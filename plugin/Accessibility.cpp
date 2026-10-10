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

namespace
{
class Handler final : public juce::AccessibilityHandler
{
public:
    Handler (juce::Component& c, juce::AccessibilityRole role, std::function<juce::String()> value, std::function<void()> press)
        : juce::AccessibilityHandler (c,
                                      role,
                                      press != nullptr ? juce::AccessibilityActions().addAction (juce::AccessibilityActionType::press, std::move (press))
                                                       : juce::AccessibilityActions(),
                                      value != nullptr ? Interfaces { std::make_unique<ReadOnlyText> (std::move (value)) } : Interfaces {})
    {
    }
};
} // namespace

std::unique_ptr<juce::AccessibilityHandler> handler (juce::Component& component,
                                                     juce::AccessibilityRole role,
                                                     std::function<juce::String()> value,
                                                     std::function<void()> press)
{
    return std::make_unique<Handler> (component, role, std::move (value), std::move (press));
}

std::unique_ptr<juce::AccessibilityHandler> ValuedButton::createAccessibilityHandler()
{
    return handler (
        *this,
        juce::AccessibilityRole::button,
        [this] { return spokenValue != nullptr ? spokenValue() : juce::String(); },
        [this] { triggerClick(); });
}

} // namespace eq1::accessibility
