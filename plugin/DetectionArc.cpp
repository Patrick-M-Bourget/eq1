#include "DetectionArc.h"

#include "PluginProcessor.h"

#include <cmath>

namespace eq1
{

namespace
{
// Threshold's top in dB; the knob's position above it is Auto.
constexpr double thresholdTopDb = 0.0;
} // namespace

DetectionArc::DetectionArc (PluginProcessor& p, juce::Slider& t) : processor (p), threshold (t)
{
    setInterceptsMouseClicks (false, false);
    lastRead = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    startTimerHz (60);
}

double DetectionArc::sweepProportion (juce::Slider& knob, double levelDb)
{
    if (levelDb < knob.getMinimum())
        return 0.0;
    return knob.valueToProportionOfLength (std::min (levelDb, thresholdTopDb));
}

void DetectionArc::timerCallback()
{
    // A newly metered Band starts from nothing rather than from the last one's falling level.
    if (const int slot = processor.meteredSlot(); slot != meteredSlot)
    {
        meteredSlot = slot;
        ballistics.reset();
    }
    const double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    const double level = ballistics.update (processor.readDetectionLevel(), now - lastRead);
    lastRead = now;
    const double reach = sweepProportion (threshold, level);
    if (std::abs (reach - proportion) > 1.0e-4)
    {
        proportion = reach;
        repaint();
    }
}

void DetectionArc::paint (juce::Graphics& g)
{
    if (proportion <= 0.0)
        return;
    // Just outside the knob's own track, as the look and feel draws it, from the bottom of its sweep.
    const auto knob = threshold.getLookAndFeel().getSliderLayout (threshold).sliderBounds.toFloat().reduced (10.0f);
    const float radius = std::min (knob.getWidth(), knob.getHeight()) / 2.0f + 3.0f;
    const auto rotary = threshold.getRotaryParameters();
    const float end = rotary.startAngleRadians + static_cast<float> (proportion) * (rotary.endAngleRadians - rotary.startAngleRadians);
    juce::Path arc;
    arc.addCentredArc (knob.getCentreX(), knob.getCentreY(), radius, radius, 0.0f, rotary.startAngleRadians, end, true);
    g.setColour (juce::Colour (0xffe0a040));
    g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
}

} // namespace eq1
