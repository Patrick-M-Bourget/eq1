#include "EditorHarness.h"
#include "EditorTick.h"

#include <catch2/catch_test_macros.hpp>

namespace
{

// The editor's one tick (EditorTick), as its components reach it.
eq1::EditorTick& tickOf (harness::OpenEditor& host)
{
    auto* owner = dynamic_cast<eq1::EditorTick::Host*> (host.editor.get());
    REQUIRE (owner != nullptr);
    return owner->editorTick();
}

} // namespace

TEST_CASE ("An idle editor ticks at 10 Hz, and a parameter change wakes the tick to 60 Hz at once")
{
    harness::OpenEditor host;
    auto& tick = tickOf (host);
    // Analyzer off, silence, nothing animating: idle, once what the editor opened with has settled.
    REQUIRE (host.settleUntil ([&] { return tick.rateHz() == eq1::EditorTick::idleHz; }));
    const int ticked = tick.count();
    host.settle (250);
    CHECK (tick.count() > ticked);
    CHECK (tick.rateHz() == eq1::EditorTick::idleHz);

    // A parameter change, as an edit or Host Automation makes one.
    host.set ("gain_scale", 50.0f);
    CHECK (tick.rateHz() == eq1::EditorTick::activeHz);
    // Nothing animating or metering, the tick settles back to idle.
    CHECK (host.settleUntil ([&] { return tick.rateHz() == eq1::EditorTick::idleHz; }));
}
