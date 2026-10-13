#include "Painting.h"

#include "CachedLayer.h"
#include "Fonts.h"
#include "Tokens.h"

#include <algorithm>
#include <cmath>

namespace staple
{

namespace colour = tokens::colour;
namespace size = tokens::size;

namespace
{
constexpr int shadowLayers = 8;
// SoftShadowCache's images: as many as a display of every Band's handle, hovered and selected, uses;
// past that it starts again.
constexpr size_t maxCachedShadows = 64;

bool operator== (const juce::DropShadow& a, const juce::DropShadow& b)
{
    return a.colour == b.colour && a.radius == b.radius && a.offset == b.offset;
}
} // namespace

void drawSoftShadow (juce::Graphics& g, juce::Rectangle<float> shape, float cornerRadius, const juce::DropShadow& shadow)
{
    const auto base = shape.translated (static_cast<float> (shadow.offset.x), static_cast<float> (shadow.offset.y));
    const float blur = static_cast<float> (shadow.radius);
    // Each layer's alpha, so that all of them together reach the shadow's own.
    const float alpha = 1.0f - std::pow (1.0f - shadow.colour.getFloatAlpha(), 1.0f / shadowLayers);
    g.setColour (shadow.colour.withAlpha (alpha));
    for (int i = 0; i < shadowLayers; ++i)
    {
        const float spread = blur * (static_cast<float> (i) / (shadowLayers - 1) - 0.5f);
        const auto r = base.expanded (spread);
        if (r.isEmpty())
            continue;
        g.fillRoundedRectangle (r, std::max (0.0f, cornerRadius + spread));
    }
}

void SoftShadowCache::draw (juce::Graphics& g, juce::Point<float> origin, juce::Rectangle<float> shape, float cornerRadius, const juce::DropShadow& shadow)
{
    const auto paint = [&] (juce::Graphics& into) { drawSoftShadow (into, shape, cornerRadius, shadow); };
    const auto imageScale = imageScaleFor (g, origin);
    if (! imageScale)
    {
        paint (g);
        return;
    }
    const float scale = *imageScale;
    // drawSoftShadow's widest layer reaches half the blur past the offset shape; a pixel more for its
    // anti-aliased edge.
    const auto reach = shape.translated (static_cast<float> (shadow.offset.x), static_cast<float> (shadow.offset.y))
                           .expanded (static_cast<float> (shadow.radius) / 2.0f + 1.0f);
    const auto device = devicePixelsOf (reach, scale);
    if (device.isEmpty())
        return;
    // The image is the same wherever the shape sits on the same place within a device pixel.
    const auto relative = shape - device.getPosition().toFloat() / scale;
    auto entry = std::find_if (entries.begin(), entries.end(), [&] (const Entry& e) {
        return e.shape == relative && juce::exactlyEqual (e.cornerRadius, cornerRadius) && e.shadow == shadow && juce::exactlyEqual (e.scale, scale)
               && e.image.getBounds() == device.withZeroOrigin();
    });
    if (entry == entries.end())
    {
        if (entries.size() >= maxCachedShadows)
            entries.clear();
        entries.push_back ({ relative, cornerRadius, shadow, scale, drawnAtDevicePixels (device, scale, juce::Image::ARGB, paint) });
        entry = std::prev (entries.end());
    }
    drawBackAtDevicePixels (g, entry->image, device, scale);
}

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
