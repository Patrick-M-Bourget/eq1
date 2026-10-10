#include "OutputMeter.h"

#include "PluginProcessor.h"
#include "staple/Tokens.h"

namespace eq1
{

void OutputMeterChannel::update (OutputLevel level, double seconds)
{
    peak = peakBallistics.update (level.peakDb, seconds);
    rms = level.rmsDb;
    if (peak >= held)
    {
        held = peak;
        heldFor = 0.0;
        return;
    }
    // Only the part of this frame past the hold falls.
    const double falling = std::min (seconds, heldFor + seconds - holdSeconds);
    heldFor += seconds;
    if (falling > 0.0)
        held = std::max (peak, held - LevelBallistics::fallDbPerSecond * falling);
}

namespace
{
constexpr double bottomDb = -60.0, topDb = 6.0, tickStepDb = 6.0;
constexpr float clipLightHeight = 8.0f, margin = 3.0f;
} // namespace

OutputMeter::OutputMeter (PluginProcessor& p) : processor (p)
{
    setName ("Output Meter");
    setTooltip ("Output Meter: click a Clip Light to put both out");
    // Tab reaches the Clip Lights, which Space or Return puts out; a click leaves focus where it was.
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (false);
    timerCallback();
    startTimerHz (60);
}

double OutputMeter::position (double db)
{
    return juce::jlimit (0.0, 1.0, (db - bottomDb) / (topDb - bottomDb));
}

juce::Rectangle<float> OutputMeter::clipLightArea() const
{
    return getLocalBounds().toFloat().reduced (margin).removeFromTop (clipLightHeight);
}

void OutputMeter::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounter();
    const double seconds = lastFrame == 0 ? 1.0 / 60.0 : juce::jlimit (0.001, 0.25, (now - lastFrame) / 1000.0);
    lastFrame = now;

    // Read even while hidden, so an over lights its Clip Light when it happens.
    const int shownChannels = juce::jlimit (0, static_cast<int> (channels.size()), processor.outputLevelChannels());
    if (shownChannels != numChannels)
    {
        channels = {};
        numChannels = shownChannels;
    }
    for (int ch = 0; ch < numChannels; ++ch)
        channels[static_cast<size_t> (ch)].update (processor.readOutputLevel (ch), seconds);
    if (isShowing())
        repaint();
}

void OutputMeter::paint (juce::Graphics& g)
{
    g.fillAll (staple::tokens::colour::bg0);
    auto area = getLocalBounds().toFloat().reduced (margin);
    const auto lights = area.removeFromTop (clipLightHeight);
    area.removeFromTop (margin);
    const auto yOf = [&] (double db) { return area.getBottom() - static_cast<float> (position (db)) * area.getHeight(); };

    // A tick every 6 dB, 0 dBFS marked.
    for (double db = bottomDb; db <= topDb; db += tickStepDb)
    {
        g.setColour (juce::exactlyEqual (db, 0.0) ? staple::tokens::colour::text3 : staple::tokens::colour::line2);
        g.drawHorizontalLine (juce::roundToInt (yOf (db)), area.getX(), area.getRight());
    }

    if (numChannels == 0)
        return;
    const float gap = 2.0f;
    const float width = (area.getWidth() - gap * static_cast<float> (numChannels - 1)) / static_cast<float> (numChannels);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto& channel = channels[static_cast<size_t> (ch)];
        const float x = area.getX() + static_cast<float> (ch) * (width + gap);
        const auto bar = juce::Rectangle<float> (x, area.getY(), width, area.getHeight());

        g.setColour (staple::tokens::colour::meter1.withAlpha (0.35f));
        g.fillRect (bar.withTop (yOf (channel.peakDb())));
        g.setColour (staple::tokens::colour::meter1);
        g.fillRect (bar.withTop (yOf (channel.rmsDb())));
        if (channel.heldPeakDb() > bottomDb)
        {
            g.setColour (staple::tokens::colour::text1);
            g.fillRect (bar.withTop (yOf (channel.heldPeakDb()) - 1.0f).withHeight (2.0f));
        }

        g.setColour (processor.isClipLit (ch) ? staple::tokens::colour::meterClip : staple::tokens::colour::stateOffBg);
        g.fillRect (lights.withX (x).withWidth (width));
    }
}

bool OutputMeter::keyPressed (const juce::KeyPress& key)
{
    if (key != juce::KeyPress::spaceKey && key != juce::KeyPress::returnKey)
        return false;
    processor.clearClipLights();
    repaint();
    return true;
}

void OutputMeter::mouseDown (const juce::MouseEvent& e)
{
    if (clipLightArea().contains (e.position))
    {
        processor.clearClipLights();
        repaint();
    }
}

} // namespace eq1
