#include "EditorHarness.h"
#include "EditorTick.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE ("An idle editor ticks at 10 Hz, and a parameter change wakes the tick to 60 Hz at once")
{
    harness::OpenEditor host;
    auto& tick = host.editorTick();
    // Analyzer off (it shows Pre and Post until asked otherwise), silence, nothing animating: idle, once
    // what the editor opened with has settled.
    host.processor.setAnalyzerSettings ({ .showPreEq = false, .showPostEq = false });
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

TEST_CASE ("The Analyzer keeps the tick at 60 Hz while it shows, and lets it idle once off")
{
    harness::OpenEditor host;
    auto& tick = host.editorTick();
    host.settle (100);
    CHECK (tick.rateHz() == eq1::EditorTick::activeHz);
    host.processor.setAnalyzerSettings ({ .showPreEq = false, .showPostEq = false });
    CHECK (host.settleUntil ([&] { return tick.rateHz() == eq1::EditorTick::idleHz; }));
}
