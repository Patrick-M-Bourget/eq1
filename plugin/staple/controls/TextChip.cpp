#include "TextChip.h"

#include "../Fonts.h"
#include "../Icons.h"

#include <cmath>

namespace staple
{

namespace
{
namespace colour = tokens::colour;
namespace motion = tokens::motion;

constexpr float filledPadding = 8.0f, plainPadding = 6.0f;
constexpr float chevronSize = 8.0f, chevronGap = 5.0f, chevronAlpha = 0.55f;
} // namespace

TextChip::TextChip (const juce::String& text, Look l, float size) : juce::Button (text), look (l), fontSize (size)
{
    setHasFocusOutline (true);
}

void TextChip::setLook (Look newLook)
{
    look = newLook;
    repaint();
}

void TextChip::setFontSize (float size)
{
    fontSize = size;
    repaint();
}

void TextChip::setChevron (bool shown)
{
    chevron = shown;
    repaint();
}

juce::Font TextChip::textFont() const { return font (fontSize, Weight::medium); }

int TextChip::getIdealWidth() const
{
    const float padding = look == Look::filled ? filledPadding : plainPadding;
    float width = juce::GlyphArrangement::getStringWidth (textFont(), getButtonText()) + 2.0f * padding;
    if (chevron)
        width += chevronGap + chevronSize;
    return static_cast<int> (std::ceil (width));
}

void TextChip::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const float alpha = isEnabled() ? 1.0f : motion::disabledAlpha;
    const auto bounds = getLocalBounds().toFloat();
    // fill1 is translucent: lighting it up makes it more opaque, as brightening it over the dark does.
    const float light = down ? motion::pressedBrightness : highlighted ? motion::hoverBrightness : 1.0f;
    if (look == Look::filled)
    {
        g.setColour (colour::fill1.withMultipliedAlpha (light * alpha));
        g.fillRoundedRectangle (bounds, tokens::size::r2);
    }
    else if (highlighted || down)
    {
        g.setColour (colour::fill1.withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (bounds, tokens::size::r2);
    }

    const auto ink = (look == Look::filled || highlighted || down || getToggleState() ? colour::text1 : colour::text2).withMultipliedAlpha (alpha);
    auto area = bounds.reduced (look == Look::filled ? filledPadding : plainPadding, 0.0f);
    if (chevron)
        drawIcon (g, Icon::dropdown, area.removeFromRight (chevronSize).withSizeKeepingCentre (chevronSize, chevronSize),
                  ink.withMultipliedAlpha (chevronAlpha));
    if (chevron)
        area.removeFromRight (chevronGap);
    g.setFont (textFont());
    g.setColour (ink);
    g.drawText (getButtonText(), area, chevron ? juce::Justification::centredLeft : juce::Justification::centred, true);
}

} // namespace staple
