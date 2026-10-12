#include "../../engine/src/Band.h"
#include "../../engine/src/Smoother.h"
#include "../../engine/src/Units.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cmath>

using namespace eq1;

namespace
{
// The closed form skip() stands for: k = 1 - (1 - c)^n applied to both stages at once, then the
// arrival check, exactly as next() settles.
struct Reference
{
    double coefficient, tolerance;
    double stage1 = 0.0, stage2 = 0.0, goal = 0.0;

    void skip (int samples)
    {
        const double k = 1.0 - std::pow (1.0 - coefficient, samples);
        stage1 += k * (goal - stage1);
        stage2 += k * (stage1 - stage2);
        if (std::abs (goal - stage1) < tolerance && std::abs (goal - stage2) < tolerance)
            stage1 = stage2 = goal;
    }
};
} // namespace

TEST_CASE ("Smoother::skip over a run, and over any other length, equals the closed form bit for bit")
{
    const double timeConstant = GENERATE (glideTimeConstantSeconds * 44100.0, glideTimeConstantSeconds * 48000.0,
                                          glideTimeConstantSeconds * 192000.0, 3.0);
    const double tolerance = GENERATE (1.0e-6, 1.0e-4);
    const int length = GENERATE (Band::maxSubBlock, 7);
    CAPTURE (timeConstant, tolerance, length);

    Smoother smoother;
    smoother.configure (timeConstant, tolerance);
    smoother.reset (0.0);
    Reference reference { onePoleCoefficient (timeConstant), tolerance };

    for (int step = 0; step < 400; ++step)
    {
        // A target that jumps, then keeps moving, then holds.
        const double target = step < 50 ? 1.0 : step < 120 ? -0.5 + 0.01 * (step - 50) : 0.25;
        smoother.setTarget (target);
        reference.goal = target;
        smoother.skip (length);
        reference.skip (length);
        REQUIRE (smoother.value() == reference.stage2);
        REQUIRE (smoother.isMoving() == (reference.stage2 != reference.goal));
    }
}

TEST_CASE ("A settled Smoother returns its goal exactly from next(); one whose first stage still moves isn't settled")
{
    Smoother smoother;
    smoother.configure (336.0, 1.0e-6);
    smoother.reset (0.7);
    REQUIRE (smoother.isSettled());
    for (int i = 0; i < 100; ++i)
        CHECK (smoother.next() == 0.7);

    // Gliding up, then retargeted to where the second stage is: it isn't moving by its value, but its
    // first stage is ahead, so next() still changes it.
    smoother.setTarget (1.0);
    for (int i = 0; i < 50; ++i)
        smoother.next();
    smoother.setTarget (smoother.value());
    CHECK_FALSE (smoother.isMoving());
    CHECK_FALSE (smoother.isSettled());
    const double before = smoother.value();
    CHECK (smoother.next() != before);

    while (! smoother.isSettled())
        smoother.next();
    const double goal = smoother.value();
    CHECK (smoother.next() == goal);
}
