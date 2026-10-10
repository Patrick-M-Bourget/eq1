#pragma once

#include "../Tokens.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <optional>

namespace staple
{

// Staple's text button (HANDOFF.md §1.2). Filled: fill1 with r2 corners, its text in fs2 or fs3 at 500,
// with an optional 8 px chevron at 55 % (the Display Range chip, the Analyzer button). Plain: text only,
// with a fill1 box on hover (A/B Compare, Copy, the footer readouts). Numbers keep their width (Manrope's
// tabular figures). Hover lights it (staple::lit); disabled, it dims to 35 %.
class TextChip : public juce::Button
{
public:
    enum class Look
    {
        filled,
        plain
    };

    TextChip (const juce::String& text, Look look, float fontSize = tokens::size::fs3);

    void setLook (Look newLook);
    void setFontSize (float size);
    void setChevron (bool shown);
    // Its text in this colour whatever its state (the footer readouts' text1, or text4 while not
    // applied), rather than text2 lit to text1.
    void setInk (std::optional<juce::Colour> colour);
    // Its text, padding and chevron.
    int getIdealWidth() const;

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override;

private:
    juce::Font textFont() const;

    Look look;
    float fontSize;
    bool chevron = false;
    std::optional<juce::Colour> fixedInk;
};

} // namespace staple
