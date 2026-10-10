#include "Accessibility.h"

namespace staple::accessibility
{

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

} // namespace staple::accessibility
