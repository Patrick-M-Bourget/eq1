#include "Tween.h"

#include "Overlay.h"

namespace staple
{

Tween::Tween (int ms, float start) : durationMs (ms), from (start), to (start), now (start) {}

Tween::~Tween() { setEasing (false); }

void Tween::setEasing (bool on)
{
    if (on == isTimerRunning())
        return;
    easingCount += on ? 1 : -1;
    if (on)
        startTimerHz (60);
    else
        stopTimer();
}

void Tween::towards (float target)
{
    if (juce::exactlyEqual (target, to))
        return;
    from = now;
    to = target;
    startedMs = juce::Time::getMillisecondCounterHiRes();
    setEasing (true);
}

void Tween::jump (float target)
{
    from = to = now = target;
    setEasing (false);
    if (apply != nullptr)
        apply (now);
}

void Tween::timerCallback()
{
    const auto elapsed = static_cast<float> ((juce::Time::getMillisecondCounterHiRes() - startedMs) / durationMs);
    now = from + (to - from) * ease (elapsed);
    if (elapsed >= 1.0f)
    {
        now = to;
        setEasing (false);
    }
    if (apply != nullptr)
        apply (now);
}

} // namespace staple
