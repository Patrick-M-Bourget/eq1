#pragma once

#include "Units.h"

#include <cmath>

namespace eq1
{

// The time constant of every glide and crossfade in the Engine: a Band's Frequency, Gain and Q, its
// fades in and out and between filters, Dynamic Range and Dynamics Bypass, the Output's amounts,
// Global Bypass, Solo and Detection Audition. They settle in about 7 time constants, so about 50 ms.
inline constexpr double glideTimeConstantSeconds = 0.007;

// The Engine's grid: a run is this many samples (Band::maxSubBlock), and the glides a Band or its
// dynamics step once per run skip by it.
inline constexpr int glideRunSamples = 16;

// Glides a value to its target along a critically damped curve (two one-pole stages): it starts
// and settles without corners, and follows a target that keeps moving without zippering.
class Smoother
{
public:
    // timeConstant is in samples; tolerance is how close counts as arrived.
    void configure (double timeConstant, double arrivalTolerance)
    {
        coefficient = onePoleCoefficient (timeConstant);
        runStep = stepOver (glideRunSamples);
        tolerance = arrivalTolerance;
    }

    void reset (double value) { stage1 = stage2 = goal = value; }
    void setTarget (double value) { goal = value; }

    // Moves on by samples at once. A run's step is worked out when configured; any other length's here.
    void skip (int samples)
    {
        const double k = samples == glideRunSamples ? runStep : stepOver (samples);
        stage1 += k * (goal - stage1);
        stage2 += k * (stage1 - stage2);
        settleIfArrived();
    }

    double next()
    {
        stage1 += coefficient * (goal - stage1);
        stage2 += coefficient * (stage1 - stage2);
        settleIfArrived();
        return stage2;
    }

    double value() const { return stage2; }
    bool isMoving() const { return stage2 != goal; }

private:
    void settleIfArrived()
    {
        if (std::abs (goal - stage1) < tolerance && std::abs (goal - stage2) < tolerance)
            stage1 = stage2 = goal;
    }

    // The one-pole step over samples at once: 1 - (1 - coefficient)^samples.
    double stepOver (int samples) const { return 1.0 - std::pow (1.0 - coefficient, samples); }

    double coefficient = 1.0, runStep = 1.0, tolerance = 0.0;
    double stage1 = 0.0, stage2 = 0.0, goal = 0.0;
};

} // namespace eq1
