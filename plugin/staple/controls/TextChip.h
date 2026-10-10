#pragma once

#include "../Fonts.h"
#include "../Icons.h"
#include "../Tokens.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <optional>

namespace staple
{

// Staple's text button (HANDOFF.md §1.2). Filled: fill1 with r2 corners, its text in fs2 or fs3 at 500,
// with an optional chevron after it (the Display Range chip's 8 px dropdown, the Analyzer button's
// chevron up). Plain: text only, with a fill1 box on hover (A/B Compare, Copy, the footer readouts, the
// Preset browser's actions). Numbers keep their width (Manrope's tabular figures). Hover lights it
// (staple::lit); disabled, it dims to 35 %. A screen reader reads it as a button by its title, or,
// for a chip whose text is a value (setTextIsValue), reads that text as its value.
class TextChip : public juce::Button
{
public:
    enum class Look
    {
        filled,
        plain
    };

    // A chevron after the text: its icon, its side in px, the gap before it and its alpha over the ink.
    struct Chevron
    {
        Icon icon = Icon::dropdown;
        float size = 8.0f, gap = 5.0f, alpha = 0.55f;
    };

    TextChip (const juce::String& text, Look look, float fontSize = tokens::size::fs3, Weight weight = Weight::medium);

    // Shows the default chevron, the Display Range chip's, or none.
    void setChevron (bool shown);
    void setChevron (Chevron look);
    // The space either side of its text (by default 8 px filled, 6 px plain).
    void setPadding (float left, float right);
    // Whether its text is a value a screen reader reads (the Display Range chip's "±12 dB"), rather
    // than an action's name (Copy).
    void setTextIsValue (bool isValue) { textIsValue = isValue; }
    // Its text in this colour whatever its state (the footer readouts' text1, or text4 while not
    // applied), rather than text2 lit to text1.
    void setInk (std::optional<juce::Colour> colour);
    // Its text, padding and chevron.
    int getIdealWidth() const;

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override;

protected:
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

private:
    juce::Font textFont() const;

    Look look;
    float fontSize;
    Weight weight;
    float paddingLeft, paddingRight;
    std::optional<Chevron> chevron;
    bool textIsValue = false;
    std::optional<juce::Colour> fixedInk;
};

} // namespace staple
