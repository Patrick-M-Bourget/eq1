#include "Accessibility.h"

namespace staple::accessibility
{

namespace
{
// A value a screen reader reads but can't set.
class ReadOnlyText final : public juce::AccessibilityTextValueInterface
{
public:
    explicit ReadOnlyText (std::function<juce::String()> text) : read (std::move (text)) {}
    bool isReadOnly() const override { return true; }
    juce::String getCurrentValueAsString() const override { return read(); }
    void setValueAsString (const juce::String&) override {}

private:
    std::function<juce::String()> read;
};

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
