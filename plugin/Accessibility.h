#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace eq1::accessibility
{

// A parameter's value as a screen reader says it: its text with its unit, a positive dB value
// signed ("+3.50 dB", "-inf dB", "1000.0 Hz", "0.707"). Words such as "Auto" go without a unit.
// The parameter's own text, which hosts show, is unchanged.
juce::String spokenValue (const juce::RangedAudioParameter& parameter, float normalisedValue);

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

// An element with a role, its title (Component::getTitle), a read-only value and, when given, an
// action for a press.
std::unique_ptr<juce::AccessibilityHandler> handler (juce::Component& component,
                                                     juce::AccessibilityRole role,
                                                     std::function<juce::String()> value,
                                                     std::function<void()> press = nullptr);

} // namespace eq1::accessibility
