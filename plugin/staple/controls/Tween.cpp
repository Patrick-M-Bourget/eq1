#include "Tween.h"

#include "Overlay.h"

namespace staple
{

Tween::Tween (int ms, float start) : durationMs (ms), from (start), to (start), now (start) {}

void Tween::towards (float target)
{
    if (juce::exactlyEqual (target, to))
        return;
    from = now;
    to = target;
    startedMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

void Tween::jump (float target)
{
    from = to = now = target;
    stopTimer();
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
        stopTimer();
    }
    if (apply != nullptr)
        apply (now);
}

} // namespace staple
