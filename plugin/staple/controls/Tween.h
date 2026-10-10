#pragma once

#include <juce_events/juce_events.h>

#include <functional>

namespace staple
{

// A value eased from where it is to a target over a duration, on the handoff's one curve (ease in
// Overlay.h), at 60 frames a second. Each frame, and a jump, calls apply with the value; the last frame
// lands on the target exactly.
class Tween final : private juce::Timer
{
public:
    Tween (int durationMs, float start);

    std::function<void (float)> apply;

    // Eases from the value now to target; a target it is already heading for changes nothing.
    void towards (float target);
    // Sets the value at once, stopping any easing.
    void jump (float target);

    float value() const { return now; }
    float target() const { return to; }

private:
    void timerCallback() override;

    int durationMs;
    float from, to, now;
    double startedMs = 0.0;
};

} // namespace staple
