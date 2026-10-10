#include "HandlesLayer.h"

#include "../staple/Fonts.h"
#include "../staple/Tokens.h"

namespace eq1::display
{

namespace
{
namespace colour = staple::tokens::colour;
namespace size = staple::tokens::size;

constexpr float handleRadius = DisplayGeometry::handleRadius;
constexpr float ringRadius = handleRadius + 5.0f;

juce::String frequencyText (double frequency)
{
    return frequency >= 1000.0 ? juce::String (frequency / 1000.0, 2) + " kHz" : juce::String (juce::roundToInt (frequency)) + " Hz";
}
} // namespace

std::vector<Grip> dynamicRangeGrips (const DisplayGeometry& geometry, const DisplayFrame& frame)
{
    namespace grip = staple::tokens::grip;
    std::vector<Grip> grips;
    for (int slot = 1; slot <= numBandSlots; ++slot)
    {
        const auto& band = frame.bands.bands[static_cast<size_t> (slot - 1)];
        if (! band.inUse)
            continue;
        const bool alone = frame.selected.size() == 1 && frame.selected.contains (slot);
        const bool shown = alone ? hasGain (band.shape) && ! band.bypass : isDynamic (band) && ! band.dynamicsBypass;
        if (! shown)
            continue;
        const auto handle = geometry.handleOf (band);
        const float y = band.dynamicRange != 0.0 ? geometry.yOf (band.gain + band.dynamicRange) : handle.y + grip::belowHandle;
        const float hover = frame.hover[static_cast<size_t> (slot - 1)];
        grips.push_back ({ slot,
                           { handle.x, juce::jlimit (grip::edgeInset, static_cast<float> (geometry.height) - grip::edgeInset, y) },
                           alone ? 1.0f : juce::jmap (hover, grip::restingAlpha, 1.0f) });
    }
    return grips;
}

juce::Rectangle<float> gripArea (juce::Point<float> centre)
{
    return juce::Rectangle<float> (staple::tokens::grip::width, staple::tokens::grip::height).withCentre (centre);
}

void paintHandles (juce::Graphics& g, const DisplayGeometry& geometry, const DisplayFrame& frame)
{
    // Handles, numbered by Band Slot.
    for (int slot = 1; slot <= numBandSlots; ++slot)
    {
        const auto& band = frame.bands.bands[static_cast<size_t> (slot - 1)];
        if (! band.inUse)
            continue;
        const auto centre = geometry.handleOf (band);
        const auto circle = juce::Rectangle<float> (handleRadius * 2.0f, handleRadius * 2.0f).withCentre (centre);
        g.setColour (bandColour (slot).withAlpha (band.bypass ? 0.35f : 1.0f));
        g.fillEllipse (circle);
        if (frame.selected.contains (slot))
        {
            g.setColour (colour::text1);
            g.drawEllipse (circle.expanded (2.0f), 1.5f);
        }
        g.setColour (colour::onLight);
        g.drawText (juce::String (slot), circle, juce::Justification::centred);
        if (isDynamic (band))
        {
            // The Dynamic Range ring: from the top, clockwise for a boost and anticlockwise for a cut, half
            // a turn for 30 dB, shortened where Live Gain would go beyond +/-30 dB. Live Gain's movement
            // is drawn on top of it.
            const auto angleOf = [] (double db) { return static_cast<float> (db / liveGainLimitDb * juce::MathConstants<double>::pi); };
            const double reach = juce::jlimit (-liveGainLimitDb, liveGainLimitDb, band.gain + band.dynamicRange) - band.gain;
            const auto arc = [&] (double db) {
                juce::Path path;
                path.addCentredArc (centre.x, centre.y, ringRadius, ringRadius, 0.0f, 0.0f, angleOf (db), true);
                return path;
            };
            g.setColour (colour::dynRange.withAlpha (band.dynamicsBypass ? 0.35f : 0.9f));
            g.strokePath (arc (reach), juce::PathStrokeType (3.0f));
            if (! band.dynamicsBypass)
            {
                g.setColour (colour::dynLive);
                g.strokePath (arc (frame.drawnGains[static_cast<size_t> (slot - 1)] - band.gain), juce::PathStrokeType (3.0f));
            }
        }
        if (slot == frame.soloedSlot)
        {
            g.setColour (colour::text1);
            g.drawEllipse (circle.expanded (5.0f), 2.0f);
            g.drawText ("Solo", circle.withY (circle.getY() - 22.0f).expanded (20.0f, 0.0f), juce::Justification::centred);
        }
    }

    // Values beside the Bands being dragged.
    if (frame.dragging)
        for (int slot : frame.selected)
        {
            const auto& band = frame.bands.bands[static_cast<size_t> (slot - 1)];
            juce::String text = frequencyText (band.frequency);
            if (hasGain (band.shape))
                text << "  " << (band.gain > 0.0 ? "+" : "") << juce::String (band.gain, 1) << " dB";
            text << "  Q " << juce::String (band.q, 2);
            const auto centre = geometry.handleOf (band);
            auto box = juce::Rectangle<float> (170.0f, 18.0f).withPosition (centre.x + 12.0f, centre.y - 26.0f);
            box = box.constrainedWithin (geometry.bounds());
            g.setColour (colour::menu);
            g.fillRoundedRectangle (box, 4.0f);
            g.setColour (colour::text1);
            g.drawText (text, box, juce::Justification::centred);
        }

    if (frame.marquee)
    {
        g.setColour (colour::fill2);
        g.fillRect (*frame.marquee);
        g.setColour (colour::text3);
        g.drawRect (*frame.marquee, 1.0f);
    }

    if (frame.allInUseMessage)
    {
        const auto box = geometry.bounds().withSizeKeepingCentre (300.0f, 30.0f).withY (12.0f);
        g.setColour (colour::stateOffBg);
        g.fillRoundedRectangle (box, 6.0f);
        g.setColour (colour::text1);
        g.setFont (staple::font (size::fs4));
        g.drawText ("All 24 Bands are in use", box, juce::Justification::centred);
    }
}

} // namespace eq1::display
