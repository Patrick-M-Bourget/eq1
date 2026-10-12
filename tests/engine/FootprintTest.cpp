#include "AllocationGuard.h"

#include "eq1/Engine.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <iostream>

using namespace eq1;

namespace
{
// What one stereo Engine may hold after prepare(): the measured figure plus about 5%, which covers
// the allocators' different rounding across platforms, so growth shows up here before it reaches a
// user. The figures are in docs/performance.md, "Footprint".
constexpr std::size_t heapCeilingBytes48k = 1'450'000;
constexpr std::size_t heapCeilingBytes192k = 2'170'000;

std::size_t heapHeldByEngine (double sampleRate)
{
    test::AllocationGuard guard;
    Engine engine;
    engine.prepare (sampleRate, 512, 2);
    const auto held = guard.bytesHeld();
    // scripts/check.sh size reads this line for its report.
    std::cout << "Engine heap at " << sampleRate / 1000.0 << " kHz stereo: " << held << " bytes\n";
    return held;
}
} // namespace

TEST_CASE ("One stereo Engine holds no more heap after prepare() than its ceiling at 48 kHz", "[footprint]")
{
    REQUIRE (heapHeldByEngine (48000.0) <= heapCeilingBytes48k);
}

TEST_CASE ("One stereo Engine holds no more heap after prepare() than its ceiling at 192 kHz", "[footprint]")
{
    REQUIRE (heapHeldByEngine (192000.0) <= heapCeilingBytes192k);
}
