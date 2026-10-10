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
// The rail (HANDOFF.md §4, "Window"): padded 10 at the top and 12 at the bottom, its contents centred
// with an 8 px gap between the Clip Lights and the bars.
constexpr float paddingTop = 10.0f, paddingBottom = 12.0f, gap = 8.0f;
constexpr float barWidth = 6.0f, barGap = 3.0f, corner = 1.0f, clipLightHeight = 4.0f;
// The Clip Lights take clicks over a strip at least this tall, centred on them.
constexpr float clipLightClickHeight = 10.0f;
constexpr float tickWidth = 4.0f, tickLeftOfBars = 7.0f;
constexpr float peakAlpha = 0.45f;
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
    const auto lights = juce::Rectangle<float> (columnOf (0).getStart(), paddingTop, columnOf (numChannels - 1).getEnd() - columnOf (0).getStart(), clipLightHeight);
    return lights.withSizeKeepingCentre (lights.getWidth(), clipLightClickHeight);
}

juce::Rectangle<float> OutputMeter::barsArea() const
{
    const float top = paddingTop + clipLightHeight + gap;
    return { 0.0f, top, static_cast<float> (getWidth()), juce::jmax (0.0f, static_cast<float> (getHeight()) - paddingBottom - top) };
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
    namespace colour = staple::tokens::colour;
    if (numChannels == 0)
        return;
    const auto area = barsArea();
    const auto yOf = [&] (double db) { return area.getBottom() - static_cast<float> (position (db)) * area.getHeight(); };

    // A tick every 6 dB, left of the bars, 0 dBFS marked.
    const float tickX = columnOf (0).getStart() - tickLeftOfBars;
    for (double db = bottomDb; db <= topDb; db += tickStepDb)
    {
        g.setColour (juce::exactlyEqual (db, 0.0) ? colour::text3 : colour::line3);
        g.fillRect (juce::Rectangle<float> (tickX, std::floor (yOf (db)), tickWidth, 1.0f).constrainedWithin (area));
    }

    // The gradient is fixed to the scale, not to the bar's height: meter1 at the bottom, meter2 at
    // -16 dBFS, meter3 at -6 dBFS and meterClip from 0 dBFS up.
    juce::ColourGradient gradient (colour::meter1, 0.0f, yOf (bottomDb), colour::meterClip, 0.0f, yOf (0.0), false);
    const auto stop = [] (double db) { return (db - bottomDb) / (0.0 - bottomDb); };
    gradient.addColour (stop (-16.0), colour::meter2);
    gradient.addColour (stop (-6.0), colour::meter3);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto& channel = channels[static_cast<size_t> (ch)];
        const auto column = columnOf (ch);
        const auto bar = juce::Rectangle<float> (column.getStart(), area.getY(), column.getLength(), area.getHeight());

        g.setColour (colour::meterTrack);
        g.fillRoundedRectangle (bar, corner);

        juce::Graphics::ScopedSaveState clipped (g);
        juce::Path track;
        track.addRoundedRectangle (bar, corner);
        g.reduceClipRegion (track);
        const float rmsTop = yOf (channel.rmsDb());
        if (channel.peakDb() > channel.rmsDb())
        {
            g.setGradientFill (gradient);
            g.setOpacity (peakAlpha);
            g.fillRect (bar.withTop (yOf (channel.peakDb())).withBottom (rmsTop));
        }
        g.setGradientFill (gradient);
        g.setOpacity (1.0f);
        g.fillRect (bar.withTop (rmsTop));
        if (channel.heldPeakDb() > bottomDb)
        {
            g.setColour (colour::text1);
            g.fillRect (bar.withTop (std::floor (yOf (channel.heldPeakDb()))).withHeight (1.0f));
        }
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto column = columnOf (ch);
        g.setColour (processor.isClipLit (ch) ? colour::meterClip : colour::meterClipOff);
        g.fillRoundedRectangle ({ column.getStart(), paddingTop, column.getLength(), clipLightHeight }, corner);
    }
}

juce::Range<float> OutputMeter::columnOf (int channel) const
{
    // The bars and the Clip Lights over them, centred in the rail on a whole pixel.
    const int shown = juce::jmax (1, numChannels);
    const float total = barWidth * static_cast<float> (shown) + barGap * static_cast<float> (shown - 1);
    const float x = std::floor ((static_cast<float> (getWidth()) - total) * 0.5f) + static_cast<float> (channel) * (barWidth + barGap);
    return { x, x + barWidth };
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
