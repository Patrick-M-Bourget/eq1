#include "GridLayer.h"

#include "../staple/Fonts.h"
#include "../staple/Tokens.h"

namespace eq1::display
{

namespace colour = staple::tokens::colour;
namespace size = staple::tokens::size;

void paintGridBehindAnalyzer (juce::Graphics& g, const DisplayGeometry& geometry)
{
    g.fillAll (colour::bg0);

    g.setFont (staple::font (size::fs2));
    for (double f : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
    {
        const float x = geometry.xOf (f);
        g.setColour (colour::gridMajor);
        g.drawVerticalLine (juce::roundToInt (x), 0.0f, static_cast<float> (geometry.height));
        g.setColour (colour::text3);
        g.drawText (f >= 1000.0 ? juce::String (juce::roundToInt (f / 1000.0)) + "k" : juce::String (juce::roundToInt (f)),
                    juce::Rectangle<float> (x + 3.0f, static_cast<float> (geometry.height) - 16.0f, 40.0f, 14.0f), juce::Justification::left);
    }
}

void paintGridOverAnalyzer (juce::Graphics& g, const DisplayGeometry& geometry)
{
    g.setFont (staple::font (size::fs2));
    const int range = geometry.rangeDb;
    for (int step = -2; step <= 2; ++step)
    {
        const double db = range * step / 2.0;
        const float y = geometry.yOf (db);
        g.setColour (step == 0 ? colour::gridZero : colour::gridMajor);
        g.drawHorizontalLine (juce::roundToInt (y), 0.0f, static_cast<float> (geometry.width));
        g.setColour (colour::text3);
        // Above its line, except at the top edge.
        const float labelY = y - 14.0f < 0.0f ? y + 2.0f : y - 14.0f;
        g.drawText ((db > 0 ? "+" : "") + juce::String (db, 0), juce::Rectangle<float> (4.0f, labelY, 40.0f, 12.0f),
                    juce::Justification::left);
    }
}

} // namespace eq1::display
