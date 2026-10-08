#pragma once

#include <cmath>

namespace eq1
{

// Glides a value to its target along a critically damped curve (two one-pole stages): it starts
// and settles without corners, and follows a target that keeps moving without zippering.
class Smoother
{
public:
    // timeConstant is in samples; tolerance is how close counts as arrived.
    void configure (double timeConstant, double arrivalTolerance)
    {
        coefficient = 1.0 - std::exp (-1.0 / timeConstant);
        tolerance = arrivalTolerance;
    }

    void reset (double value) { stage1 = stage2 = goal = value; }
    void setTarget (double value) { goal = value; }

    void skip (int samples)
    {
        const double k = 1.0 - std::pow (1.0 - coefficient, samples);
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

    double coefficient = 1.0, tolerance = 0.0;
    double stage1 = 0.0, stage2 = 0.0, goal = 0.0;
};

} // namespace eq1
