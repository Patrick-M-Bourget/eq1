#include "AnalyzerLayer.h"

#include "../staple/Tokens.h"

namespace eq1::display
{

namespace colour = staple::tokens::colour;

float spectrumYAt (const DisplayGeometry& geometry, const AnalyzerSettings& settings, const AnalyzerSpectrum& spectrum, float x)
{
    const double level = spectrum.levelDb (geometry.frequencyAt (x), settings.tiltDbPerOctave);
    return static_cast<float> (juce::jlimit (0.0, 1.0, -level / settings.rangeDb) * geometry.height);
}

void paintAnalyzer (juce::Graphics& g, const DisplayGeometry& geometry, const AnalyzerFrame& frame)
{
    const auto& analyzer = frame.settings;
    const auto width = static_cast<float> (geometry.width), height = static_cast<float> (geometry.height);
    const auto spectrumLine = [&] (const AnalyzerSpectrum& spectrum) {
        juce::Path line;
        for (float x = 0.0f; x <= width; x += DisplayGeometry::pixelStep)
        {
            const juce::Point<float> point { x, spectrumYAt (geometry, analyzer, spectrum, x) };
            if (juce::exactlyEqual (x, 0.0f))
                line.startNewSubPath (point);
            else
                line.lineTo (point);
        }
        return line;
    };
    const auto areaUnder = [&] (juce::Path line) {
        line.lineTo (line.getCurrentPosition().withY (height));
        line.lineTo (0.0f, height);
        line.closeSubPath();
        return line;
    };
    // Peak Hold under the spectra, a faint line; nothing where it is below the
    // Analyzer's range, so silence leaves no flat line.
    if (frame.held != nullptr)
    {
        juce::Path line;
        bool drawing = false;
        for (float x = 0.0f; x <= width; x += DisplayGeometry::pixelStep)
        {
            const double level = frame.held->heldLevelDb (geometry.frequencyAt (x), analyzer.tiltDbPerOctave);
            if (level <= -analyzer.rangeDb)
            {
                drawing = false;
                continue;
            }
            const juce::Point<float> point { x, static_cast<float> (juce::jlimit (0.0, 1.0, -level / analyzer.rangeDb) * geometry.height) };
            if (drawing)
                line.lineTo (point);
            else
                line.startNewSubPath (point);
            drawing = true;
        }
        g.setColour (colour::anPeak);
        g.strokePath (line, juce::PathStrokeType (1.0f));
    }
    // The main spectrum: post-EQ, or pre-EQ when only it is shown, filled with a gradient and outlined;
    // beside the post one, pre-EQ is a faint line.
    const AnalyzerSpectrum* main = analyzer.showPostEq ? &frame.postEq : analyzer.showPreEq ? &frame.preEq : nullptr;
    if (main != nullptr)
    {
        const auto line = spectrumLine (*main);
        juce::ColourGradient gradient (colour::anFillTop, 0.0f, 0.0f, colour::anFillMid.withAlpha (0.0f), 0.0f, height, false);
        gradient.addColour (0.6, colour::anFillMid);
        g.setGradientFill (gradient);
        g.fillPath (areaUnder (line));
        if (analyzer.showPostEq && analyzer.showPreEq)
        {
            g.setColour (colour::anPre);
            g.strokePath (spectrumLine (frame.preEq), juce::PathStrokeType (1.0f));
        }
        g.setColour (colour::anLine);
        g.strokePath (line, juce::PathStrokeType (1.0f));
    }
    if (analyzer.showSidechain)
    {
        g.setColour (colour::anScLine);
        g.strokePath (spectrumLine (frame.sidechain), juce::PathStrokeType (1.0f));
    }
}

} // namespace eq1::display
