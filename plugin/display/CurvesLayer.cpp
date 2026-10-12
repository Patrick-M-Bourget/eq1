#include "CurvesLayer.h"

#include "eq1/Response.h"
#include "../staple/Tokens.h"

#include <algorithm>
#include <vector>

namespace eq1::display
{

namespace
{
namespace tokens = staple::tokens;
namespace curveStyle = staple::tokens::curve;

// The style without Global Bypass.
CurveStyle ownStyle (const CurveState& state)
{
    const auto index = static_cast<size_t> (state.slot - 1);
    CurveStyle style;
    style.colour = state.bypassed ? tokens::bandBypassed[index] : tokens::band[index];
    if (state.selected)
    {
        style.lineWidth = curveStyle::selectedLine;
        style.lineAlpha = state.bypassed ? curveStyle::bypassedSelectedLineAlpha : 1.0f;
        style.fillAlpha = state.bypassed ? curveStyle::bypassedSelectedFillAlpha : curveStyle::selectedFillAlpha;
        style.glowAlpha = state.bypassed ? 0.0f : curveStyle::glowAlpha;
        return style;
    }
    const float h = state.hover, scale = state.bypassed ? curveStyle::bypassedScale : 1.0f;
    style.lineWidth = juce::jmap (h, curveStyle::line, curveStyle::hoverLine);
    style.lineAlpha = juce::jmap (h, curveStyle::lineAlpha, curveStyle::hoverLineAlpha) * scale;
    style.fillAlpha = juce::jmap (h, curveStyle::fillAlpha, curveStyle::hoverFillAlpha) * scale;
    return style;
}
} // namespace

CurveStyle bandCurveStyle (const CurveState& state)
{
    const auto own = ownStyle (state);
    if (state.globalBypass <= 0.0f)
        return own;
    // Under Global Bypass: in the bypassed palette, at 45 % of its alphas when not Bypassed.
    auto unbypassed = state;
    unbypassed.bypassed = false;
    auto bypassed = ownStyle (unbypassed);
    bypassed.colour = tokens::bandBypassed[static_cast<size_t> (state.slot - 1)];
    bypassed.lineAlpha *= curveStyle::globalBypassScale;
    bypassed.fillAlpha *= curveStyle::globalBypassScale;
    bypassed.glowAlpha = 0.0f;
    const float t = state.globalBypass;
    return { own.colour.interpolatedWith (bypassed.colour, t), juce::jmap (t, own.lineWidth, bypassed.lineWidth),
             juce::jmap (t, own.lineAlpha, bypassed.lineAlpha), juce::jmap (t, own.fillAlpha, bypassed.fillAlpha),
             juce::jmap (t, own.glowAlpha, bypassed.glowAlpha) };
}

float dynamicRangeWashAlpha (bool bypassed, float globalBypass)
{
    return juce::jmap (globalBypass, bypassed ? curveStyle::bypassedWashAlpha : curveStyle::washAlpha,
                       curveStyle::washAlpha * curveStyle::globalBypassScale);
}

float sumCurveAlpha (float globalBypass)
{
    return juce::jmap (globalBypass, 1.0f, curveStyle::globalBypassSumAlpha);
}

namespace
{
// A curve through db, one value every pixelStep pixels from the left edge.
juce::Path curve (const DisplayGeometry& geometry, const std::vector<double>& db)
{
    juce::Path path;
    const auto range = static_cast<double> (geometry.rangeDb);
    for (size_t i = 0; i < db.size(); ++i)
    {
        const juce::Point<float> point { static_cast<float> (i) * DisplayGeometry::pixelStep,
                                         geometry.yOf (juce::jlimit (-range * 1.5, range * 1.5, db[i])) };
        if (i == 0)
            path.startNewSubPath (point);
        else
            path.lineTo (point);
    }
    return path;
}

// The area between a curve and the 0 dB line.
juce::Path fillTo0dB (const DisplayGeometry& geometry, juce::Path line)
{
    const float zero = geometry.yOf (0.0);
    line.lineTo (line.getCurrentPosition().withY (zero));
    line.lineTo (0.0f, zero);
    line.closeSubPath();
    return line;
}

// The area between two curves of the same length.
juce::Path between (const DisplayGeometry& geometry, const std::vector<double>& upper, const std::vector<double>& lower)
{
    auto path = curve (geometry, upper);
    const auto range = static_cast<double> (geometry.rangeDb);
    for (size_t i = lower.size(); i-- > 0;)
        path.lineTo (static_cast<float> (i) * DisplayGeometry::pixelStep, geometry.yOf (juce::jlimit (-range * 1.5, range * 1.5, lower[i])));
    path.closeSubPath();
    return path;
}

void paintCurve (juce::Graphics& g, const juce::Path& line, const juce::Path& fill, const CurveStyle& style)
{
    g.setColour (style.colour.withAlpha (style.fillAlpha));
    g.fillPath (fill);
    if (style.glowAlpha > 0.0f)
    {
        // Two stacked strokes, widest first, for a soft 2 px glow.
        g.setColour (style.colour.withAlpha (style.glowAlpha * 0.5f));
        g.strokePath (line, juce::PathStrokeType (style.lineWidth + 2.0f * curveStyle::glow, juce::PathStrokeType::curved));
        g.strokePath (line, juce::PathStrokeType (style.lineWidth + curveStyle::glow, juce::PathStrokeType::curved));
    }
    g.setColour (style.colour.withAlpha (style.lineAlpha));
    g.strokePath (line, juce::PathStrokeType (style.lineWidth, juce::PathStrokeType::curved));
}
} // namespace

int bandAreaAt (const DisplayGeometry& geometry, const DisplayFrame& frame, juce::Point<float> point)
{
    constexpr double tolerance = 0.3, smallest = 0.4;
    const double frequency = std::min (geometry.frequencyAt (point.x), 0.4999 * frame.sampleRate);
    const double db = geometry.dbAt (point.y);
    int found = 0;
    double foundSize = 0.0;
    for (int slot = 1; slot <= numBandSlots; ++slot)
    {
        auto band = frame.bands.bands[static_cast<size_t> (slot - 1)];
        if (! band.inUse)
            continue;
        band.gain = frame.drawnGains[static_cast<size_t> (slot - 1)];
        double curve = 0.0;
        bandResponseDb (band, &frequency, &curve, 1, frame.sampleRate);
        if (std::abs (curve) < smallest)
            continue;
        const bool inside = curve > 0.0 ? db >= -tolerance && db <= curve + tolerance : db <= tolerance && db >= curve - tolerance;
        if (inside && (found == 0 || std::abs (curve) < foundSize))
        {
            found = slot;
            foundSize = std::abs (curve);
        }
    }
    return found;
}

JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wfloat-equal")
bool CurveCache::BandKey::operator== (const BandKey& other) const
{
    return sameBand (designed, other.designed) && geometry == other.geometry && sampleRate == other.sampleRate;
}

bool CurveCache::WashKey::operator== (const WashKey& other) const
{
    return slot == other.slot && sameBand (band, other.band) && geometry == other.geometry && sampleRate == other.sampleRate;
}
JUCE_END_IGNORE_WARNINGS_GCC_LIKE

void CurveCache::update (const DisplayGeometry& geometry, const DisplayFrame& frame)
{
    const double sampleRate = frame.sampleRate;
    bool changed = false;
    if (frequenciesWidth != geometry.width || ! juce::exactlyEqual (frequenciesSampleRate, sampleRate))
    {
        frequencies.clear();
        for (float x = 0.0f; x <= static_cast<float> (geometry.width); x += DisplayGeometry::pixelStep)
            frequencies.push_back (std::min (geometry.frequencyAt (x), 0.4999 * sampleRate));
        frequenciesWidth = geometry.width;
        frequenciesSampleRate = sampleRate;
    }
    const auto response = [&] (const BandSettings& band, std::vector<double>& db) {
        db.resize (frequencies.size());
        bandResponseDb (band, frequencies.data(), db.data(), static_cast<int> (db.size()), sampleRate);
    };

    // Each Band in use, at its drawn Gain.
    bool anyBuilt = false;
    std::array<bool, numBandSlots> inUse {}, playing {};
    for (int slot = 1; slot <= numBandSlots; ++slot)
    {
        const auto index = static_cast<size_t> (slot - 1);
        const auto& band = frame.bands.bands[index];
        if (! band.inUse)
            continue;
        inUse[index] = true;
        // On mono a Side Band has nothing to process: it plays no part in the whole EQ's curve.
        playing[index] = ! (band.bypass || (frame.mono && band.placement == StereoPlacement::Side));
        auto live = band;
        live.gain = frame.drawnGains[index];
        const BandKey key { live, geometry, sampleRate };
        auto& entry = bands[index];
        if (entry.key == key)
            continue;
        response (live, entry.db);
        entry.line = curve (geometry, entry.db);
        entry.fill = fillTo0dB (geometry, entry.line);
        entry.key = key;
        ++entry.builds;
        anyBuilt = true;
    }

    // The sum of the Bands that play.
    if (anyBuilt || ! sum.built || sum.geometry != geometry || sum.playing != playing)
    {
        std::vector<double> total (frequencies.size(), 0.0);
        for (size_t index = 0; index < numBandSlots; ++index)
            if (playing[index])
                for (size_t i = 0; i < total.size(); ++i)
                    total[i] += bands[index].db[i];
        sum.path = curve (geometry, total);
        sum.built = true;
        sum.geometry = geometry;
        sum.playing = playing;
        ++sum.builds;
        changed = true;
    }
    if (sum.inUse != inUse)
    {
        sum.inUse = inUse;
        changed = true;
    }

    // The wash of the one selected Dynamic Band, between its curves at Gain and at Gain + Dynamic Range,
    // both as heard.
    const int washed = frame.selected.size() == 1 ? *frame.selected.begin() : 0;
    if (washed != 0)
    {
        const auto& band = frame.bands.bands[static_cast<size_t> (washed - 1)];
        if (band.inUse && isDynamic (band) && ! band.dynamicsBypass)
        {
            const WashKey key { washed, band, geometry, sampleRate };
            if (washKey != key)
            {
                std::vector<double> atGain, atReach;
                response (band, atGain);
                auto reach = band;
                reach.gain = band.gain + band.dynamicRange;
                response (reach, atReach);
                wash = between (geometry, atGain, atReach);
                washKey = key;
                changed = true;
            }
        }
    }
    if (changed)
        ++changes;
}

void CurveCache::paint (juce::Graphics& g, const DisplayFrame& frame) const
{
    // The selected Band is drawn last, on top of the others, and alone has the Dynamic Range wash.
    const int washed = frame.selected.size() == 1 ? *frame.selected.begin() : 0;
    std::vector<int> selectedInUse;
    const auto drawBand = [&] (int slot) {
        const auto index = static_cast<size_t> (slot - 1);
        const auto& band = frame.bands.bands[index];
        const bool silent = band.bypass || (frame.mono && band.placement == StereoPlacement::Side);
        const auto style = bandCurveStyle ({ .slot = slot,
                                             .selected = frame.selected.contains (slot),
                                             .bypassed = silent,
                                             .hover = frame.hover[index],
                                             .globalBypass = frame.globalBypass });
        if (slot == washed && isDynamic (band) && ! band.dynamicsBypass)
        {
            g.setColour (tokens::colour::dynRange.withAlpha (dynamicRangeWashAlpha (silent, frame.globalBypass)));
            g.fillPath (wash);
        }
        paintCurve (g, bands[index].line, bands[index].fill, style);
    };
    for (int slot = 1; slot <= numBandSlots; ++slot)
    {
        if (! frame.bands.bands[static_cast<size_t> (slot - 1)].inUse)
            continue;
        if (frame.selected.contains (slot))
            selectedInUse.push_back (slot);
        else
            drawBand (slot);
    }
    for (int slot : selectedInUse)
        drawBand (slot);

    const float alpha = sumCurveAlpha (frame.globalBypass);
    const auto stroke = [&] (juce::Colour colour, float width) {
        g.setColour (colour.withMultipliedAlpha (alpha));
        g.strokePath (sum.path, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    stroke (tokens::colour::curveMainHalo.withMultipliedAlpha (curveStyle::sumOuterHaloAlpha), curveStyle::sumOuterHalo);
    stroke (tokens::colour::curveMainHalo, curveStyle::sumHalo);
    stroke (tokens::colour::curveMain, curveStyle::sum);
}

} // namespace eq1::display
