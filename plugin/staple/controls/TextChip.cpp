#include "TextChip.h"

#include "../Accessibility.h"
#include "../Light.h"

#include <cmath>

namespace staple
{

namespace
{
namespace colour = tokens::colour;

constexpr float filledPadding = 8.0f, plainPadding = 6.0f;
} // namespace

TextChip::TextChip (const juce::String& text, Look l, float size, Weight w)
    : juce::Button (text), look (l), fontSize (size), weight (w), paddingLeft (look == Look::filled ? filledPadding : plainPadding),
      paddingRight (paddingLeft)
{
    setHasFocusOutline (true);
    setMouseClickGrabsKeyboardFocus (false);
}

void TextChip::setChevron (bool shown)
{
    chevron = shown ? std::optional<Chevron> (Chevron {}) : std::nullopt;
    repaint();
}

void TextChip::setChevron (Chevron newLook)
{
    chevron = newLook;
    repaint();
}

void TextChip::setPadding (float left, float right)
{
    paddingLeft = left;
    paddingRight = right;
    repaint();
}

void TextChip::setInk (std::optional<juce::Colour> colour)
{
    fixedInk = colour;
    repaint();
}

juce::Font TextChip::textFont() const { return font (fontSize, weight); }

int TextChip::getIdealWidth() const
{
    float width = juce::GlyphArrangement::getStringWidth (textFont(), getButtonText()) + paddingLeft + paddingRight;
    if (chevron)
        width += chevron->gap + chevron->size;
    return static_cast<int> (std::ceil (width));
}

void TextChip::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const float alpha = enabledAlpha (*this);
    const auto bounds = getLocalBounds().toFloat();
    if (look == Look::filled)
    {
        g.setColour (lit (colour::fill1, highlighted, down).withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (bounds, tokens::size::r2);
    }
    else if (highlighted || down)
    {
        g.setColour (colour::fill1.withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (bounds, tokens::size::r2);
    }

    const auto ink = fixedInk.value_or (look == Look::filled || highlighted || down || getToggleState() ? colour::text1 : colour::text2)
                         .withMultipliedAlpha (alpha);
    auto area = bounds.withTrimmedLeft (paddingLeft).withTrimmedRight (paddingRight);
    if (chevron)
    {
        drawIcon (g, chevron->icon, area.removeFromRight (chevron->size).withSizeKeepingCentre (chevron->size, chevron->size),
                  ink.withMultipliedAlpha (chevron->alpha));
        area.removeFromRight (chevron->gap);
    }
    g.setFont (textFont());
    g.setColour (ink);
    g.drawText (getButtonText(), area, chevron ? juce::Justification::centredLeft : juce::Justification::centred, true);
}

std::unique_ptr<juce::AccessibilityHandler> TextChip::createAccessibilityHandler()
{
    return accessibility::handler (*this, juce::AccessibilityRole::button, [this] { return getButtonText(); }, [this] { triggerClick(); });
}

} // namespace staple
