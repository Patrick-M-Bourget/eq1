#include "GridLayer.h"

#include "../ValueText.h"
#include "../staple/Fonts.h"
#include "../staple/Tokens.h"

namespace eq1::display
{

namespace
{
namespace colour = staple::tokens::colour;
namespace size = staple::tokens::size;
namespace layout = staple::tokens::layout;

constexpr double majorFrequencies[] = { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 };
constexpr double minorFrequencies[] = { 30.0,   40.0,   60.0,   70.0,   80.0,   90.0,   300.0,  400.0,  600.0,
                                        700.0,  800.0,  900.0,  3000.0, 4000.0, 6000.0, 7000.0, 8000.0, 9000.0 };
// At this Display Range minor Gain lines sit halfway between the Gain lines: at each odd dB.
constexpr int minorGainLinesRangeDb = 6;

// The Frequency labels' baseline box: 10 px above the bottom; the first and last are pulled 12 px in.
constexpr float frequencyLabelAboveBottom = 10.0f, frequencyLabelInset = 12.0f;
// The Gain labels end 42 px from the right edge, the Analyzer's dB scale 10 px.
constexpr float gainLabelFromRight = 42.0f, analyzerLabelFromRight = 10.0f;
// The Analyzer's dB scale keeps clear of the Display Range chip at the top and the Frequency labels at
// the bottom.
constexpr float analyzerLabelClearTop = 40.0f, analyzerLabelClearBottom = 30.0f;
constexpr float labelWidth = 48.0f, labelHeight = 14.0f;
// How far a label's image reaches past its area, for glyphs that overhang it.
constexpr float labelImageMargin = 4.0f;

bool narrow (const DisplayGeometry& geometry)
{
    return geometry.width < layout::narrowDisplayWidth;
}

// Signed, with a true minus sign: "+6", "0", "−6".
juce::String dbLabel (int db) { return valueText::signedDb (db, { .decimals = 0, .unit = false }); }

juce::Rectangle<float> rightAligned (float right, float centreY)
{
    return { right - labelWidth, centreY - labelHeight / 2.0f, labelWidth, labelHeight };
}
} // namespace

GridLines gridLines (const DisplayGeometry& geometry)
{
    GridLines lines;
    for (double f : majorFrequencies)
        lines.majorX.push_back (geometry.xOf (f));
    if (! narrow (geometry))
        for (double f : minorFrequencies)
            lines.minorX.push_back (geometry.xOf (f));
    const double step = geometry.rangeDb / 3.0;
    for (int i = -3; i <= 3; ++i)
        if (i != 0)
            lines.gainY.push_back (geometry.yOf (i * step));
    if (geometry.rangeDb == minorGainLinesRangeDb)
        for (int i = -3; i < 3; ++i)
            lines.minorGainY.push_back (geometry.yOf ((i + 0.5) * step));
    lines.zeroY = geometry.yOf (0.0);
    return lines;
}

std::vector<Label> gridLabels (const DisplayGeometry& geometry)
{
    std::vector<Label> labels;
    const auto bottom = static_cast<float> (geometry.height) - frequencyLabelAboveBottom;
    const int last = static_cast<int> (std::size (majorFrequencies)) - 1;
    for (int i = 0; i <= last; ++i)
    {
        if (narrow (geometry) && i % 2 != 0)
            continue;
        const double f = majorFrequencies[i];
        const float x = geometry.xOf (f);
        const auto text = valueText::frequencyLabel (f);
        juce::Rectangle<float> area { 0.0f, bottom - labelHeight, labelWidth, labelHeight };
        auto justification = juce::Justification::centred;
        if (i == 0)
        {
            area.setX (x + frequencyLabelInset);
            justification = juce::Justification::centredLeft;
        }
        else if (i == last)
        {
            area.setX (x - frequencyLabelInset - labelWidth);
            justification = juce::Justification::centredRight;
        }
        else
        {
            area.setCentre (x, area.getCentreY());
        }
        labels.push_back ({ text, area, justification, colour::text2, size::fs2 });
    }

    const double step = geometry.rangeDb / 3.0;
    const auto right = static_cast<float> (geometry.width) - gainLabelFromRight;
    for (int i = 2; i >= -2; --i)
    {
        const int db = juce::roundToInt (i * step);
        labels.push_back ({ dbLabel (db), rightAligned (right, geometry.yOf (i * step)), juce::Justification::centredRight,
                            i == 0 ? colour::text2 : colour::text3, size::fs2 });
    }
    return labels;
}

std::vector<Label> analyzerScaleLabels (const DisplayGeometry& geometry, const AnalyzerSettings& settings)
{
    std::vector<Label> labels;
    if (! (settings.showPreEq || settings.showPostEq || settings.showSidechain))
        return labels;
    const int step = settings.rangeDb > 90 ? 20 : 10;
    const auto height = static_cast<float> (geometry.height);
    const auto right = static_cast<float> (geometry.width) - analyzerLabelFromRight;
    for (int level = -step; level > -settings.rangeDb; level -= step)
    {
        const float y = static_cast<float> (-level) / static_cast<float> (settings.rangeDb) * height;
        if (y > analyzerLabelClearTop && y < height - analyzerLabelClearBottom)
            labels.push_back ({ dbLabel (level), rightAligned (right, y), juce::Justification::centredRight, colour::text4, size::fs1 });
    }
    return labels;
}

void paintGrid (juce::Graphics& g, const DisplayGeometry& geometry)
{
    const auto lines = gridLines (geometry);
    const auto width = static_cast<float> (geometry.width), height = static_cast<float> (geometry.height);
    g.setColour (colour::gridMinor);
    for (float x : lines.minorX)
        g.drawVerticalLine (juce::roundToInt (x), 0.0f, height);
    for (float y : lines.minorGainY)
        g.drawHorizontalLine (juce::roundToInt (y), 0.0f, width);
    g.setColour (colour::gridMajor);
    for (float x : lines.majorX)
        g.drawVerticalLine (juce::roundToInt (x), 0.0f, height);
    for (float y : lines.gainY)
        g.drawHorizontalLine (juce::roundToInt (y), 0.0f, width);
    g.setColour (colour::gridZero);
    g.drawHorizontalLine (juce::roundToInt (lines.zeroY), 0.0f, width);
}

namespace
{
void paintLabel (juce::Graphics& g, const Label& label)
{
    g.setFont (staple::font (label.fontSize));
    g.setColour (label.colour);
    g.drawText (label.text, label.area, label.justification, false);
}
} // namespace

void paintLabels (juce::Graphics& g, const std::vector<Label>& labels)
{
    for (const auto& label : labels)
        paintLabel (g, label);
}

void CachedLabels::paint (juce::Graphics& g, juce::Point<float> origin, const std::vector<Label>& labels)
{
    layers.resize (labels.size());
    for (size_t i = 0; i < labels.size(); ++i)
        layers[i].draw (g, origin, labels[i].area.expanded (labelImageMargin), labels[i], juce::Image::ARGB, [&] (juce::Graphics& into) { paintLabel (into, labels[i]); });
}

} // namespace eq1::display
