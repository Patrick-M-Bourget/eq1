#pragma once

#include "../Icons.h"
#include "../Tokens.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace staple
{

// Staple's icon button (HANDOFF.md §1.2): one of the kit's icons, centred in a square of 22, 24, 28 or
// 32 px. Hover lights the icon from text3 to text1 (a lit icon brightens x 1.18) and a press x 1.3,
// with no outline. Lit, the icon takes a caller-set colour (a Band's, or text1). A Bypass-style power
// button, whose "on" means bypassed, shows Off instead while on: stateOff on a stateOffBg tint.
// Disabled, it dims to 35 %.
//
// As a toggle it works with ButtonAttachment (setClickingTogglesState). Momentary, it is lit only
// while held, and reports the press and the release (Solo, Detection Audition).
class IconButton : public juce::Button
{
public:
    IconButton (const juce::String& name, Icon icon);

    void setIcon (Icon newIcon);
    Icon getIcon() const { return icon; }
    void setLitColour (juce::Colour colour);
    // While on, Off rather than lit.
    void setOffLook (bool offLook);
    void setMomentary (bool momentary);

    bool isLit() const;
    bool isOff() const;

    std::function<void()> onPress, onRelease; // momentary only

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void enablementChanged() override;

private:
    void setHeld (bool nowHeld);

    Icon icon;
    juce::Colour litColour = tokens::colour::text1;
    bool offLook = false, momentary = false, held = false;
};

} // namespace staple
