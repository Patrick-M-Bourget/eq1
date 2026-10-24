#include "TipWatchdog.h"

namespace staple
{

namespace
{
// The innermost stand-in for the OS's pointer, or nullptr.
TipWatchdog::ScopedPointerSource*& innermost()
{
    static TipWatchdog::ScopedPointerSource* standIn = nullptr;
    return standIn;
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

bool TipWatchdog::isPointerWithin (juce::Component& c, const juce::Path& area, juce::Point<float> screenPosition)
{
    if (! c.isShowing() || ! area.contains (c.getLocalPoint (nullptr, screenPosition)))
        return false;
    auto& window = *c.getTopLevelComponent();
    return window.contains (window.getLocalPoint (nullptr, screenPosition));
}

juce::Point<float> TipWatchdog::pointer()
{
    // The main mouse source's screen position is the OS's live one, not its last event's.
    const auto* standIn = innermost();
    return standIn != nullptr ? standIn->position() : juce::Desktop::getInstance().getMainMouseSource().getScreenPosition();
}

TipWatchdog::ScopedPointerSource::ScopedPointerSource (PointerSource s) : source (std::move (s)), outer (innermost())
{
    innermost() = this;
}

TipWatchdog::ScopedPointerSource::~ScopedPointerSource()
{
    jassert (innermost() == this); // a stand-in made inside this one outlives it
    for (auto** link = &innermost(); *link != nullptr; link = &(*link)->outer)
    {
        if (*link == this)
        {
            *link = outer;
            break;
        }
    }
}

} // namespace staple
