#pragma once

#include "ShapeDesign.h"
#include "eq1/Settings.h"

#include <array>

namespace eq1
{

// Auto Gain's estimate (eq1/Response.h, autoGainDb), worked out a few points at a time so the audio
// thread can spread it over several blocks. Pink noise has equal power in every octave, so its
// average power through the curve is the mean of the curve's power gain at frequencies spaced
// evenly in log frequency from 20 Hz to 20 kHz.
class AutoGainEstimate
{
public:
    // About 100 points an octave: enough for the narrowest Bell (Q 40) to count fully.
    static constexpr int numPoints = 1024;

    // Starts a new estimate for settings, designing each Band's static filter. The last finished
    // estimate stays in db() until this one finishes.
    void start (const Settings& settings, double sampleRate);

    // Works out up to points more points. Returns true once the estimate has finished.
    bool advance (int points);

    bool running() const { return next < numPoints; }

    // The last finished estimate, in dB.
    double db() const { return resultDb; }

private:
    std::array<Cascade, numBandSlots> cascades;
    int numCascades = 0;
    double rate = 48000.0, lowest = 20.0, ratio = 1.0;
    int next = numPoints;
    double sum = 0.0;
    double resultDb = 0.0;
};

} // namespace eq1
