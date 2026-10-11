#include "GhostLayer.h"

#include "eq1/Response.h"
#include "../staple/Fonts.h"
#include "../staple/Tokens.h"

#include <cmath>

namespace eq1::display
{

namespace
{
namespace colour = staple::tokens::colour;
namespace ghostStyle = staple::tokens::ghost;

constexpr float readoutHeight = 14.0f, readoutWidth = 80.0f;

juce::String readoutOf (double frequency)
{
    if (frequency < 999.5)
        return juce::String (frequency, 1) + " Hz";
    return juce::String (frequency / 1000.0, frequency < 10000.0 ? 2 : 1) + " kHz";
}

juce::Rectangle<float> readoutArea (const DisplayGeometry& geometry, float x)
{
    const auto bottom = static_cast<float> (geometry.height) - ghostStyle::readoutAboveBottom;
    return { x - readoutWidth / 2.0f, bottom - readoutHeight, readoutWidth, readoutHeight };
}
} // namespace

Ghost ghostBell (const DisplayGeometry& geometry, juce::Point<float> pointer)
{
    const auto range = static_cast<double> (geometry.rangeDb);
    double gain = juce::jlimit (-range, range, geometry.dbAt (pointer.y));
    const double least = range * ghostStyle::minimumGainProportion;
    if (std::abs (gain) < least)
        gain = gain < 0.0 ? -least : least;
    const double most = geometry.dbAt (ghostStyle::peakClearance);
    gain = juce::jlimit (-most, most, gain);
    const double frequency = geometry.frequencyAt (pointer.x);
    return { frequency, gain, pointer.x, readoutOf (frequency) };
}

Ghost restingGhost (const DisplayGeometry& geometry)
{
    return ghostBell (geometry, { geometry.xOf (1000.0), geometry.yOf (geometry.rangeDb / 2.0) });
}

std::vector<Label> fadedForGhost (std::vector<Label> labels, const DisplayGeometry& geometry, const Ghost& ghost)
{
    const auto readout = readoutArea (geometry, ghost.x);
    for (auto& label : labels)
    {
        const bool onItsRow = label.area.getY() < readout.getBottom() && label.area.getBottom() > readout.getY();
        if (onItsRow && std::abs (label.area.getCentreX() - ghost.x) < ghostStyle::labelClearance)
            label.colour = label.colour.withMultipliedAlpha (ghostStyle::fadedLabelAlpha);
    }
    return labels;
}

void paintGhostCurve (juce::Graphics& g, const DisplayGeometry& geometry, const Ghost& ghost, double sampleRate, float alpha)
{
    const auto white = colour::sheen;
    const float x = ghost.x;

    // The Bell, 190 px either side.
    BandSettings bell;
    bell.inUse = true;
    bell.frequency = ghost.frequency;
    bell.gain = ghost.gain;
    bell.q = ghostStyle::q;
    const float from = std::max (0.0f, x - ghostStyle::span), to = std::min (static_cast<float> (geometry.width), x + ghostStyle::span);
    std::vector<double> frequencies, db;
    for (float px = from; px <= to; px += DisplayGeometry::pixelStep)
        frequencies.push_back (std::min (geometry.frequencyAt (px), 0.4999 * sampleRate));
    db.resize (frequencies.size());
    bandResponseDb (bell, frequencies.data(), db.data(), static_cast<int> (db.size()), sampleRate);
    juce::Path path;
    for (size_t i = 0; i < db.size(); ++i)
    {
        const juce::Point<float> point { from + static_cast<float> (i) * DisplayGeometry::pixelStep, geometry.yOf (db[i]) };
        if (i == 0)
            path.startNewSubPath (point);
        else
            path.lineTo (point);
    }
    juce::ColourGradient stroke (white.withAlpha (0.0f), x - ghostStyle::span, 0.0f, white.withAlpha (0.0f), x + ghostStyle::span, 0.0f, false);
    stroke.addColour (0.5, white.withAlpha (ghostStyle::strokeAlpha * alpha));
    g.setGradientFill (stroke);
    g.strokePath (path, juce::PathStrokeType (ghostStyle::stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void paintGhostMarker (juce::Graphics& g, const DisplayGeometry& geometry, const Ghost& ghost, float alpha)
{
    const auto white = colour::sheen;
    const float x = ghost.x, height = static_cast<float> (geometry.height);

    // The line at its Frequency.
    juce::ColourGradient line (white.withAlpha (ghostStyle::lineTopAlpha * alpha), x, 0.0f,
                               white.withAlpha (ghostStyle::lineBottomAlpha * alpha), x, height, false);
    line.addColour (0.5, white.withAlpha (ghostStyle::lineMiddleAlpha * alpha));
    g.setGradientFill (line);
    g.fillRect (x - 0.5f, 0.0f, 1.0f, height);

    // The glow at its peak.
    const juce::Point<float> peak { x, geometry.yOf (ghost.gain) };
    const float radius = ghostStyle::glowDiameter / 2.0f;
    juce::ColourGradient glow (colour::ghostGlow.withAlpha (ghostStyle::glowAlpha * alpha), peak,
                               colour::ghostGlow.withAlpha (0.0f), peak.translated (radius * 0.7f, 0.0f), true);
    glow.addColour (0.4 / 0.7, colour::ghostGlow.withAlpha (ghostStyle::glowMiddleAlpha * alpha));
    g.setGradientFill (glow);
    g.fillEllipse (juce::Rectangle<float> (2.0f * radius, 2.0f * radius).withCentre (peak));

    // The readout.
    g.setColour (colour::text1.withMultipliedAlpha (alpha));
    g.setFont (staple::font (staple::tokens::size::fs2, staple::Weight::medium));
    g.drawText (ghost.readout, readoutArea (geometry, x), juce::Justification::centred, false);
}

} // namespace eq1::display
