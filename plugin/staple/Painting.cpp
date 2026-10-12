#include "Painting.h"

#include "Fonts.h"
#include "Tokens.h"
#include "controls/Overlay.h"

#include <algorithm>

namespace staple
{

namespace colour = tokens::colour;
namespace size = tokens::size;

void drawKnobFace (juce::Graphics& g, juce::Rectangle<float> face, const juce::DropShadow& shadow, float rimWidth, juce::Colour rimColour,
                   float opacity)
{
    drawSoftShadow (g, face, face.getWidth() / 2.0f, shadow);
    juce::ColourGradient gradient (colour::knobFaceTop, face.getCentreX(), face.getY() + 0.12f * face.getHeight(), colour::knobFaceEdge,
                                   face.getCentreX(), face.getY() + 1.32f * face.getHeight(), true);
    gradient.addColour (0.45, colour::knobFaceMid);
    g.setGradientFill (gradient);
    g.setOpacity (opacity);
    g.fillEllipse (face);
    g.setColour (rimColour);
    g.drawEllipse (face.reduced (rimWidth / 2.0f), rimWidth);
}

juce::ColourGradient meterGradient (float bottomY, float clipY, double stop2, double stop3)
{
    juce::ColourGradient gradient (colour::meter1, 0.0f, bottomY, colour::meterClip, 0.0f, clipY, false);
    gradient.addColour (stop2, colour::meter2);
    gradient.addColour (stop3, colour::meter3);
    return gradient;
}

float readoutCardWidth (const juce::String& title, const juce::String& value)
{
    const int text = std::max (textWidth (font (size::fs2), title), textWidth (font (size::fs4), value));
    return static_cast<float> (text + 2 * readoutCard::paddingSide);
}

void drawReadoutCard (juce::Graphics& g, juce::Rectangle<float> box, const juce::String& title, const juce::String& value)
{
    drawSoftShadow (g, box, size::r2, tokens::shadow::shadow1);
    g.setColour (colour::menu);
    g.fillRoundedRectangle (box, size::r2);
    g.setColour (colour::line2);
    g.drawRoundedRectangle (box.reduced (0.5f), size::r2, 1.0f);

    auto text = box.withTrimmedTop (readoutCard::paddingTop).reduced (readoutCard::paddingSide, 0.0f);
    g.setFont (font (size::fs2));
    g.setColour (colour::text3);
    g.drawText (title, text.removeFromTop (readoutCard::titleLine), juce::Justification::centred, false);
    if (value.isNotEmpty())
    {
        g.setFont (font (size::fs4));
        g.setColour (colour::text1);
        g.drawText (value, text.removeFromTop (readoutCard::valueLine), juce::Justification::centred, false);
    }
}

void drawPillThumb (juce::Graphics& g, juce::Rectangle<float> thumb, float cornerRadius, juce::Colour top, juce::Colour bottom,
                    float highlightInset)
{
    drawSoftShadow (g, thumb, cornerRadius, tokens::shadow::thumb);
    g.setGradientFill (juce::ColourGradient::vertical (top, thumb.getY(), bottom, thumb.getBottom()));
    g.fillRoundedRectangle (thumb, cornerRadius);
    g.setColour (colour::thumbHighlight);
    g.fillRect (thumb.withHeight (1.0f).reduced (highlightInset, 0.0f));
}

void fillSquashedRadial (juce::Graphics& g, juce::Rectangle<float> area, juce::Point<float> centre, float radius, float verticalScale,
                         juce::Colour colour, std::optional<std::pair<double, juce::Colour>> mid)
{
    const juce::Graphics::ScopedSaveState saved (g);
    // A circle, squashed vertically about its centre.
    const auto squash = juce::AffineTransform::scale (1.0f, verticalScale, centre.x, centre.y);
    g.addTransform (squash);
    juce::ColourGradient gradient (colour, centre, colour.withAlpha (0.0f), centre.translated (radius, 0.0f), true);
    if (mid.has_value())
        gradient.addColour (mid->first, mid->second);
    g.setGradientFill (gradient);
    g.fillRect (area.transformedBy (squash.inverted()));
}

} // namespace staple
