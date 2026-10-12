#include "../../engine/src/Biquad.h"
#include "../../engine/src/ShapeDesign.h"
#include "Measure.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <bit>
#include <cstdint>
#include <type_traits>

using namespace eq1;

namespace
{
// This build's Double2 is a two-lane vector (NEON or SSE2), so the test below compares two different
// implementations; on a build with neither it would compare the fallback with itself.
#if EQ1_DOUBLE2_NEON || EQ1_DOUBLE2_SSE2
static_assert (! std::is_same_v<Double2, ScalarDouble2>, "this build has a two-lane Double2 to compare the fallback with");
#endif

constexpr double sampleRate = 48000.0;
constexpr int positions = 16; // a run: a glide reaches its design at the last one

// Every Shape's structure at its default Slope, and Low Cut and High Cut Brickwall.
std::vector<Structure> allStructures()
{
    std::vector<Structure> structures;
    for (auto shape : test::allShapes)
        structures.push_back (structureOf ({ .shape = shape }));
    for (auto shape : { Shape::LowCut, Shape::HighCut })
        structures.push_back (structureOf ({ .shape = shape, .brickwall = true }));
    return structures;
}

// One section's step through one Lanes implementation: both lanes in, both lanes out as floats, as a
// Band's cascade carries them between sections.
template <typename Lanes>
struct LaneRun
{
    Lanes s1 = fill2<Lanes> (0.0), s2 = fill2<Lanes> (0.0);

    std::pair<float, float> step (const BiquadCoefficients& c, float left, float right)
    {
        const auto y = biquadStep (fill2<Lanes> (c.b0), fill2<Lanes> (c.b1), fill2<Lanes> (c.b2), fill2<Lanes> (c.a1),
                                   fill2<Lanes> (c.a2), make2<Lanes> (left, right), s1, s2);
        return { static_cast<float> (lane0 (y)), static_cast<float> (lane1 (y)) };
    }
};

std::uint32_t bits (float value) { return std::bit_cast<std::uint32_t> (value); }
std::uint64_t bits (double value) { return std::bit_cast<std::uint64_t> (value); }
} // namespace

TEST_CASE ("The scalar fallback and the two-lane Double2 run a biquad bit for bit alike, over every Shape with a glide")
{
    const auto structure = GENERATE (from_range (allStructures()));
    const bool gliding = GENERATE (true, false);
    CAPTURE (static_cast<int> (structure.shape), structure.order, gliding);

    // A glide from one design to another over a run, in every section; or the design held.
    const auto from = designShape ({ structure, 1000.0, 12.0, 1.0 }, sampleRate);
    const auto to = designShape ({ structure, 1500.0, 6.0, 2.0 }, sampleRate);
    REQUIRE (from.count == to.count);
    REQUIRE (from.count > 0);

    test::Noise noise (7);
    for (int section = 0; section < from.count; ++section)
    {
        LaneRun<ScalarDouble2> fallback;
        LaneRun<Double2> lanes;
        const auto& target = to.sections[static_cast<size_t> (section)];
        const auto& start = from.sections[static_cast<size_t> (section)];
        for (int i = 0; i < positions; ++i)
        {
            const auto coefficients = gliding ? interpolate (start, target, (i + 1.0) / positions) : target;
            const float left = noise(), right = noise();
            const auto [fallbackLeft, fallbackRight] = fallback.step (coefficients, left, right);
            const auto [lanesLeft, lanesRight] = lanes.step (coefficients, left, right);
            CAPTURE (section, i, fallbackLeft, lanesLeft, fallbackRight, lanesRight);
            REQUIRE (bits (lanesLeft) == bits (fallbackLeft));
            REQUIRE (bits (lanesRight) == bits (fallbackRight));
        }
        REQUIRE (bits (lane0 (lanes.s1)) == bits (lane0 (fallback.s1)));
        REQUIRE (bits (lane1 (lanes.s1)) == bits (lane1 (fallback.s1)));
        REQUIRE (bits (lane0 (lanes.s2)) == bits (lane0 (fallback.s2)));
        REQUIRE (bits (lane1 (lanes.s2)) == bits (lane1 (fallback.s2)));
    }
}

TEST_CASE ("The scalar fallback runs a biquad as BiquadState does, lane by lane")
{
    // The scalar path a Left or Right Band takes, against the fallback's lane 0 and lane 1 on the same samples.
    const auto cascade = designShape ({ structureOf ({ .shape = Shape::Bell }), 1000.0, 12.0, 1.0 }, sampleRate);
    const auto& c = cascade.sections[0];
    BiquadState left, right;
    LaneRun<ScalarDouble2> fallback;
    test::Noise noise (3);
    for (int i = 0; i < positions; ++i)
    {
        const float l = noise(), r = noise();
        const auto [fallbackLeft, fallbackRight] = fallback.step (c, l, r);
        REQUIRE (bits (fallbackLeft) == bits (left.process (c, l)));
        REQUIRE (bits (fallbackRight) == bits (right.process (c, r)));
    }
}
