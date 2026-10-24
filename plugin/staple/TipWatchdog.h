#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace staple
{

// What keeps a tip (the editor's Tooltip, a Knob Readout) from staying up after the pointer has gone. Both tips hear
// of the pointer from its events, and the OS sends a plugin's window none when another window comes over it under the
// pointer, or when the host hides the window itself: JUCE still has the pointer on the control. So while a tip shows,
// its watchdog reads the pointer from the OS every periodMs and hands it to the tip, which hides unless the pointer is
// still over its control (isPointerOver).
class TipWatchdog final : private juce::Timer
{
public:
    static constexpr int periodMs = 80;

    // The pointer's position on screen.
    using PointerSource = std::function<juce::Point<float>()>;

    // check is handed the pointer every periodMs, from start until stop.
    explicit TipWatchdog (std::function<void (juce::Point<float> pointer)> check);
    ~TipWatchdog() override;

    void start();
    void stop();

    // Whether the pointer at screenPosition is over c: c is showing, its hit area holds the point, and c's window is
    // the one under it, not hidden or covered there by another (ComponentPeer::contains asks the OS).
    static bool isPointerOver (juce::Component& c, juce::Point<float> screenPosition);
    // Whether the pointer at screenPosition is in area, in c's coordinates: c is showing, area holds the point, and
    // c's window is the one under it (the window's own Component::contains asks the OS).
    static bool isPointerWithin (juce::Component& c, const juce::Path& area, juce::Point<float> screenPosition);

    // The pointer as the OS has it now, unless a ScopedPointerSource stands in for it.
    static juce::Point<float> pointer();

    // Stands in for the OS's pointer, for the whole process, while it lives: a test moves a pointer of its own without
    // sending an event. Stand-ins nest: the innermost one answers, and each goes before the one it was made inside (an
    // assertion); one gone out of order is still taken out of the chain.
    class ScopedPointerSource
    {
    public:
        explicit ScopedPointerSource (PointerSource source);
        ~ScopedPointerSource();
        ScopedPointerSource (const ScopedPointerSource&) = delete;
        ScopedPointerSource& operator= (const ScopedPointerSource&) = delete;

        juce::Point<float> position() const { return source(); }

    private:
        PointerSource source;
        // The stand-in this one was made inside, or nullptr.
        ScopedPointerSource* outer;
    };

private:
    void timerCallback() override;

    std::function<void (juce::Point<float>)> check;
};

} // namespace staple
