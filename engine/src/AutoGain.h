#pragma once

#include "ShapeDesign.h"
#include "SquaredMagnitude.h"
#include "eq1/Settings.h"

#include <array>

namespace eq1
{

// Auto Gain's estimate (eq1/Response.h, autoGainDb), worked out a few points at a time so the audio
// thread can spread it over several blocks. Pink noise has equal power in every octave, so its
// average power through the curve is the mean of the curve's power gain at frequencies spaced
// evenly in log frequency from 20 Hz to 20 kHz. Each section's power gain is read in real
// arithmetic (SquaredMagnitude.h): no complex division, pow or polar per point.
class AutoGainEstimate
{
public:
    // About 100 points an octave: enough for the narrowest Bell (Q 40) to count fully.
    static constexpr int numPoints = 1024;

    // Builds the table of points for sampleRate, once: sin^2(w/2) of each point's frequency, from
    // which its phi terms follow. Allocates nothing, but costs a sin per point: not for the audio
    // thread. start() and advance() read the table.
    void prepare (double sampleRate);

    // Starts a new estimate for settings, designing each Band's static filter and taking each of its
    // sections to its squared-magnitude form. The last finished estimate stays in db() until this
    // one finishes.
    void start (const Settings& settings);

    // Works out up to points more points. Returns true once the estimate has finished.
    bool advance (int points);

    bool running() const { return next < numPoints; }

    // The last finished estimate, in dB.
    double db() const { return resultDb; }

private:
    // A section's squared magnitude: its numerator's over its denominator's, one real division.
    struct SectionPower
    {
        SquaredMagnitude numerator, denominator;
    };
    struct CascadePower
    {
        std::array<SectionPower, maxSections> sections;
        int count = 0;
    };

    std::array<CascadePower, numBandSlots> cascades;
    int numCascades = 0;
    double rate = 48000.0;
    std::array<double, numPoints> sineSquared {}; // sin^2(w/2) at each point's frequency, from prepare()
    int next = numPoints;
    double sum = 0.0;
    double resultDb = 0.0;
};

} // namespace eq1
