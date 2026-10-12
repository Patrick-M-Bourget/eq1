#include "WindowBackground.h"

#include "Painting.h"
#include "Tokens.h"

namespace staple
{

namespace
{
// A soft elliptical highlight over the window: colour at centre (a proportion of the window's size),
// fading to nothing at fadeOut of the radii, as CSS's radial-gradient (rx ry at x y, colour, transparent fadeOut).
void paintHighlight (juce::Graphics& g, juce::Rectangle<float> window, juce::Point<float> at, float rx, float ry, juce::Colour colour, float fadeOut)
{
    const juce::Point<float> centre { window.getX() + window.getWidth() * at.x, window.getY() + window.getHeight() * at.y };
    fillSquashedRadial (g, window, centre, rx * fadeOut, ry / rx, colour);
}
} // namespace

void paintWindowBackground (juce::Graphics& g, juce::Rectangle<float> window)
{
    namespace colour = tokens::colour;
    g.fillAll (colour::bg0);
    paintHighlight (g, window, { 0.12f, 0.04f }, 700.0f, 480.0f, colour::windowHighlight1, 0.70f);
    paintHighlight (g, window, { 0.92f, 0.32f }, 800.0f, 560.0f, colour::windowHighlight2, 0.66f);
    paintHighlight (g, window, { 0.45f, 1.12f }, 720.0f, 480.0f, colour::windowHighlight3, 0.66f);
}

} // namespace staple
