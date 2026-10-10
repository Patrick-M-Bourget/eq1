#include "HandlesLayer.h"

#include "../staple/Fonts.h"
#include "../staple/Tokens.h"
#include "../staple/controls/Overlay.h"

namespace eq1::display
{

namespace
{
namespace tokens = staple::tokens;
namespace colour = staple::tokens::colour;
namespace size = staple::tokens::size;
namespace handle = staple::tokens::handle;

juce::String frequencyText (double frequency)
{
    return frequency >= 1000.0 ? juce::String (frequency / 1000.0, 2) + " kHz" : juce::String (juce::roundToInt (frequency)) + " Hz";
}

// The readout's card, as the knob tooltip's (KnobTooltip.cpp).
constexpr float readoutPaddingTop = 5.0f, readoutPaddingSide = 10.0f, readoutPaddingBottom = 6.0f;
constexpr float readoutTitleLine = 14.0f, readoutValueLine = 18.0f, readoutOffset = 12.0f;
} // namespace

HandleStyle handleStyle (const HandleState& state)
{
    const auto index = static_cast<size_t> (state.slot - 1);
    // How far into the bypassed look: all the way when Bypassed, else as far as Global Bypass has faded.
    const float t = state.bypassed ? 1.0f : state.globalBypass;
    HandleStyle style;
    const float scale = state.dragged || ! state.selected ? juce::jmap (state.dragged ? 1.0f : state.hover, 1.0f, handle::hoverScale) : 1.0f;
    style.diameter = (state.selected ? handle::selectedDiameter : handle::diameter) * scale;
    style.fill = tokens::band[index].interpolatedWith (tokens::bandBypassed[index].withAlpha (handle::bypassedAlpha), t);
    if (state.selected)
    {
        style.ring = colour::handleSelectedRing.interpolatedWith (colour::handleSelectedRing.withAlpha (handle::bypassedRingAlpha), t);
        style.ringWidth = handle::selectedRing;
        style.glowAlpha = handle::glowAlpha * (1.0f - t);
        style.shadow = tokens::shadow::selectedHandle;
    }
    else
    {
        style.ring = colour::handleRing;
        style.ringWidth = handle::ring;
        style.shadow = tokens::shadow::handle;
    }
    return style;
}

std::vector<DynamicRangeHandle> dynamicRangeHandles (const DisplayGeometry& geometry, const DisplayFrame& frame)
{
    namespace token = staple::tokens::dynamicRangeHandle;
    std::vector<DynamicRangeHandle> handles;
    for (int slot = 1; slot <= numBandSlots; ++slot)
    {
        const auto& band = frame.bands.bands[static_cast<size_t> (slot - 1)];
        if (! band.inUse)
            continue;
        const bool alone = frame.selected.size() == 1 && frame.selected.contains (slot);
        const bool shown = alone ? hasGain (band.shape) && ! band.bypass : isDynamic (band) && ! band.dynamicsBypass;
        if (! shown)
            continue;
        const auto centre = geometry.handleOf (band);
        const float y = band.dynamicRange != 0.0 ? geometry.yOf (band.gain + band.dynamicRange) : centre.y + token::belowHandle;
        const float hover = frame.hover[static_cast<size_t> (slot - 1)];
        handles.push_back ({ slot,
                           { centre.x, juce::jlimit (token::edgeInset, static_cast<float> (geometry.height) - token::edgeInset, y) },
                           alone ? 1.0f : juce::jmap (hover, token::restingAlpha, 1.0f) });
    }
    return handles;
}

juce::Rectangle<float> dynamicRangeHandleArea (juce::Point<float> centre)
{
    return juce::Rectangle<float> (tokens::dynamicRangeHandle::width, tokens::dynamicRangeHandle::height).withCentre (centre);
}

Readout dragReadout (int slot, const BandSettings& band)
{
    juce::String value = frequencyText (band.frequency);
    if (hasGain (band.shape))
        value << "  " << (band.gain > 0.0 ? "+" : "") << juce::String (band.gain, 1) << " dB";
    value << "  Q " << juce::String (band.q, 2);
    return { "Band " + juce::String (slot), value };
}

namespace
{
void paintDynamicRangeHandle (juce::Graphics& g, const DynamicRangeHandle& dynamicRangeHandle, juce::Colour bandColour)
{
    namespace size = tokens::dynamicRangeHandle;
    const auto c = dynamicRangeHandle.centre;
    const float half = size::triangleWidth / 2.0f, offset = size::gap / 2.0f;
    juce::Path triangles;
    triangles.addTriangle (c.x, c.y - offset - size::triangleHeight, c.x + half, c.y - offset, c.x - half, c.y - offset);
    triangles.addTriangle (c.x, c.y + offset + size::triangleHeight, c.x + half, c.y + offset, c.x - half, c.y + offset);
    g.setColour (bandColour.withMultipliedAlpha (dynamicRangeHandle.alpha));
    g.fillPath (triangles);
}

void paintHandle (juce::Graphics& g, juce::Point<float> centre, const HandleStyle& style, juce::Colour bandColour)
{
    const float r = style.diameter / 2.0f;
    const auto circle = juce::Rectangle<float> (style.diameter, style.diameter).withCentre (centre);
    // The shadow and the glow are stacked translucent circles, with no blur.
    staple::drawSoftShadow (g, circle, r, style.shadow);
    if (style.glowAlpha > 0.0f)
        staple::drawSoftShadow (g, circle.expanded (1.0f), r + 1.0f, { bandColour.withAlpha (style.glowAlpha), static_cast<int> (handle::glow), {} });
    g.setColour (style.fill);
    g.fillEllipse (circle);
    // A centred white sheen, gone at 70 % of the radius.
    const auto sheen = colour::handleSelectedRing.withAlpha (handle::sheenAlpha);
    g.setGradientFill (juce::ColourGradient (sheen, centre, sheen.withAlpha (0.0f), centre.translated (r * handle::sheenReach, 0.0f), true));
    g.fillEllipse (circle);
    g.setColour (style.ring);
    g.drawEllipse (circle.reduced (style.ringWidth / 2.0f), style.ringWidth);
}

void paintReadout (juce::Graphics& g, const DisplayGeometry& geometry, juce::Point<float> handleCentre, const Readout& readout)
{
    const auto titleFont = staple::font (size::fs2), valueFont = staple::font (size::fs4);
    const float textWidth = std::max (juce::GlyphArrangement::getStringWidth (titleFont, readout.title),
                                      juce::GlyphArrangement::getStringWidth (valueFont, readout.value));
    const float height = readoutPaddingTop + readoutTitleLine + readoutValueLine + readoutPaddingBottom;
    auto box = juce::Rectangle<float> (std::ceil (textWidth) + 2.0f * readoutPaddingSide, height)
                   .withPosition (handleCentre.x + readoutOffset, handleCentre.y - readoutOffset - height)
                   .constrainedWithin (geometry.bounds());
    staple::drawSoftShadow (g, box, size::r2, tokens::shadow::shadow1);
    g.setColour (colour::menu);
    g.fillRoundedRectangle (box, size::r2);
    g.setColour (colour::line2);
    g.drawRoundedRectangle (box.reduced (0.5f), size::r2, 1.0f);
    auto text = box.withTrimmedTop (readoutPaddingTop).reduced (readoutPaddingSide, 0.0f);
    g.setFont (titleFont);
    g.setColour (colour::text3);
    g.drawText (readout.title, text.removeFromTop (readoutTitleLine), juce::Justification::centred, false);
    g.setFont (valueFont);
    g.setColour (colour::text1);
    g.drawText (readout.value, text.removeFromTop (readoutValueLine), juce::Justification::centred, false);
}

void paintSoloCue (juce::Graphics& g, juce::Point<float> centre, float diameter)
{
    const float r = diameter / 2.0f + handle::soloRingOffset;
    g.setColour (colour::text1);
    g.drawEllipse (juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (centre), handle::soloRing);
    const auto font = staple::font (size::fs1);
    const juce::String text ("Solo");
    const auto pill = juce::Rectangle<float> (std::ceil (juce::GlyphArrangement::getStringWidth (font, text)) + 8.0f, 14.0f)
                          .withCentre ({ centre.x, centre.y - r - 4.0f - 7.0f });
    g.setColour (colour::menu);
    g.fillRoundedRectangle (pill, size::r1);
    g.setColour (colour::text1);
    g.setFont (font);
    g.drawText (text, pill, juce::Justification::centred, false);
}
} // namespace

void paintHandles (juce::Graphics& g, const DisplayGeometry& geometry, const DisplayFrame& frame)
{
    for (const auto& dynamicRangeHandle : dynamicRangeHandles (geometry, frame))
        paintDynamicRangeHandle (g, dynamicRangeHandle, bandColour (dynamicRangeHandle.slot));

    // The selected handles go on top.
    const auto draw = [&] (int slot) {
        const auto& band = frame.bands.bands[static_cast<size_t> (slot - 1)];
        const bool selected = frame.selected.contains (slot);
        const auto style = handleStyle ({ .slot = slot,
                                          .selected = selected || slot == frame.soloedSlot,
                                          .bypassed = band.bypass || (frame.mono && band.placement == StereoPlacement::Side),
                                          .hover = frame.hover[static_cast<size_t> (slot - 1)],
                                          .dragged = frame.dragging && selected,
                                          .globalBypass = frame.globalBypass });
        const auto centre = geometry.handleOf (band);
        paintHandle (g, centre, style, bandColour (slot));
        if (slot == frame.soloedSlot)
            paintSoloCue (g, centre, style.diameter);
    };
    for (int pass = 0; pass < 2; ++pass)
        for (int slot = 1; slot <= numBandSlots; ++slot)
            if (frame.bands.bands[static_cast<size_t> (slot - 1)].inUse && frame.selected.contains (slot) == (pass == 1))
                draw (slot);

    if (frame.dragging)
        for (int slot : frame.selected)
        {
            const auto& band = frame.bands.bands[static_cast<size_t> (slot - 1)];
            paintReadout (g, geometry, geometry.handleOf (band), dragReadout (slot, band));
        }

    if (frame.marquee)
    {
        g.setColour (colour::fill1);
        g.fillRect (*frame.marquee);
        g.setColour (colour::line3);
        g.drawRect (*frame.marquee, 1.0f);
    }

    if (frame.allInUseMessage)
    {
        const auto font = staple::font (size::fs3);
        const juce::String text ("All 24 Bands are in use");
        const auto box = juce::Rectangle<float> (std::ceil (juce::GlyphArrangement::getStringWidth (font, text)) + 28.0f, 30.0f)
                             .withCentre ({ geometry.bounds().getCentreX(), 12.0f + 15.0f });
        staple::drawSoftShadow (g, box, size::r3, tokens::shadow::shadow1);
        g.setColour (colour::menu);
        g.fillRoundedRectangle (box, size::r3);
        g.setColour (colour::text1);
        g.setFont (font);
        g.drawText (text, box, juce::Justification::centred, false);
    }
}

} // namespace eq1::display
