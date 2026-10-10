#include "OutputMeter.h"

#include "Accessibility.h"
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

class OutputMeter::ClipLight final : public juce::Component
{
public:
    ClipLight (OutputMeter& m, int ch) : meter (m), channel (ch) { setInterceptsMouseClicks (false, false); }

private:
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return accessibility::handler (
            *this,
            juce::AccessibilityRole::button,
            [this] { return juce::String (meter.processor.isClipLit (channel) ? "Lit" : "Off"); },
            [this] { meter.clearClipLights(); });
    }

    OutputMeter& meter;
    const int channel;
};

OutputMeter::OutputMeter (PluginProcessor& p) : processor (p)
{
    setName ("Output Meter");
    setTitle ("Output Meter");
    for (int ch = 0; ch < static_cast<int> (clipLights.size()); ++ch)
    {
        clipLights[static_cast<size_t> (ch)] = std::make_unique<ClipLight> (*this, ch);
        addChildComponent (*clipLights[static_cast<size_t> (ch)]);
    }
    setTooltip ("Output Meter: click a Clip Light to put both out");
    // Tab reaches the Clip Lights, which Space or Return puts out; a click leaves focus where it was.
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (false);
    timerCallback();
    startTimerHz (60);
}

OutputMeter::~OutputMeter() = default;

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
        placeClipLights();
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
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto& channel = channels[static_cast<size_t> (ch)];
        const auto column = columnOf (ch);
        const float x = column.getStart(), width = column.getLength();
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

juce::Range<float> OutputMeter::columnOf (int channel) const
{
    const auto area = getLocalBounds().toFloat().reduced (margin);
    const float gap = 2.0f;
    const float width = (area.getWidth() - gap * static_cast<float> (numChannels - 1)) / static_cast<float> (juce::jmax (1, numChannels));
    const float x = area.getX() + static_cast<float> (channel) * (width + gap);
    return { x, x + width };
}

void OutputMeter::resized()
{
    placeClipLights();
}

void OutputMeter::placeClipLights()
{
    const auto lights = clipLightArea();
    for (int ch = 0; ch < static_cast<int> (clipLights.size()); ++ch)
    {
        auto& light = *clipLights[static_cast<size_t> (ch)];
        light.setVisible (ch < numChannels);
        light.setTitle (numChannels == 1 ? "Clip Light" : ch == 0 ? "Clip Light Left" : "Clip Light Right");
        const auto column = columnOf (ch);
        light.setBounds (lights.withX (column.getStart()).withWidth (column.getLength()).getSmallestIntegerContainer());
    }
}

void OutputMeter::clearClipLights()
{
    processor.clearClipLights();
    repaint();
}

bool OutputMeter::keyPressed (const juce::KeyPress& key)
{
    if (key != juce::KeyPress::spaceKey && key != juce::KeyPress::returnKey)
        return false;
    clearClipLights();
    return true;
}

void OutputMeter::mouseDown (const juce::MouseEvent& e)
{
    if (clipLightArea().contains (e.position))
        clearClipLights();
}

} // namespace eq1
