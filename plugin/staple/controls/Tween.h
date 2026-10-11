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
    ~Tween() override;

    std::function<void (float)> apply;

    // Eases from the value now to target; a target it is already heading for changes nothing.
    void towards (float target);
    // Sets the value at once, stopping any easing.
    void jump (float target);

    float value() const { return now; }
    float target() const { return to; }

    // Whether any Tween is easing now, on the message thread: what a test waits on before it checks
    // where an animation ends.
    static bool anyEasing() { return easingCount > 0; }

private:
    void timerCallback() override;
    // Starts or stops the frames, keeping count of the Tweens easing.
    void setEasing (bool on);
    inline static int easingCount = 0;

    int durationMs;
    float from, to, now;
    double startedMs = 0.0;
};

} // namespace staple
