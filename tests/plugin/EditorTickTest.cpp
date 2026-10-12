#include "EditorHarness.h"
#include "EditorTick.h"

#include <catch2/catch_test_macros.hpp>

#include <thread>

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

TEST_CASE ("Host Automation from another thread wakes the tick on its next callback, with no message posted")
{
    harness::OpenEditor host;
    auto& tick = host.editorTick();
    host.processor.setAnalyzerSettings ({ .showPreEq = false, .showPostEq = false });
    REQUIRE (host.settleUntil ([&] { return tick.rateHz() == eq1::EditorTick::idleHz; }));

    // A Client that notes the change on the tick that first shows it, and the rate of the tick after.
    struct Probe final : juce::Component, eq1::EditorTick::Client
    {
        explicit Probe (harness::OpenEditor& h) : Client (static_cast<juce::Component&> (*this)), host (h) {}
        void tick (double) override
        {
            if (saw && rateAfter == 0)
                rateAfter = host.editorTick().rateHz();
            saw = saw || host.value ("gain_scale") == 50.0f;
        }
        harness::OpenEditor& host;
        bool saw = false;
        int rateAfter = 0;
    } probe { host };
    host.editor->addChildComponent (probe);

    // The harness runs timers but never dispatches posted messages, so only what the tick's timer
    // reads can wake it: a change that posts a message instead is never seen to wake it.
    std::thread automation ([&] { host.set ("gain_scale", 50.0f); });
    automation.join();
    REQUIRE (host.settleUntil ([&] { return probe.rateAfter != 0; }));
    CHECK (probe.saw);
    CHECK (probe.rateAfter == eq1::EditorTick::activeHz);
    host.editor->removeChildComponent (&probe);
}
