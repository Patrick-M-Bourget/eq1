#include "TipWatchdog.h"

namespace staple
{

namespace
{
TipWatchdog::PointerSource& standIn()
{
    static TipWatchdog::PointerSource source;
    return source;
}
} // namespace

TipWatchdog::TipWatchdog (std::function<void (juce::Point<float>)> c) : check (std::move (c)) {}

TipWatchdog::~TipWatchdog() { stopTimer(); }

void TipWatchdog::start()
{
    if (! isTimerRunning())
        startTimer (periodMs);
}

void TipWatchdog::stop() { stopTimer(); }

void TipWatchdog::timerCallback() { check (pointer()); }

bool TipWatchdog::isPointerOver (juce::Component& c, juce::Point<float> screenPosition)
{
    // Component::contains tests the hit area of c and each parent, then asks the window's peer.
    return c.isShowing() && c.contains (c.getLocalPoint (nullptr, screenPosition));
}

juce::Point<float> TipWatchdog::pointer()
{
    // The main mouse source's screen position is the OS's live one, not its last event's.
    return standIn() != nullptr ? standIn()() : juce::Desktop::getInstance().getMainMouseSource().getScreenPosition();
}

TipWatchdog::ScopedPointer::ScopedPointer (PointerSource source) : previous (std::move (standIn())) { standIn() = std::move (source); }

TipWatchdog::ScopedPointer::~ScopedPointer() { standIn() = std::move (previous); }

} // namespace staple
