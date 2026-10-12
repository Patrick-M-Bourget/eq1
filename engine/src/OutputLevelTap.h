#pragma once

#include "RunGrid.h"
#include "Units.h"
#include "eq1/Engine.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

namespace eq1
{

// The Output Level of one channel: process() on the audio thread measures, read() on one reader
// thread takes the result. Neither allocates or locks, and read() touches only the two atomics, so
// prepare() may run while a reader reads.
//
// Peak is the largest absolute sample since the last read. RMS is over a rectangular window of the
// last 300 ms, defined in samples, so it doesn't depend on how the audio is cut into blocks. The
// window is kept as per-run sums of squares (a run of the Engine's grid each, in double), rounded up to whole runs,
// and the reading moves on at each run's end.
class OutputLevelTap
{
public:
    static constexpr double windowSeconds = 0.3;

    // Allocates, unless the window is as long as before. Starts with a silent window and no peak.
    void prepare (double sampleRate)
    {
        windowSamples = static_cast<size_t> (std::max (1L, std::lround (windowSeconds * sampleRate)));
        runSums.assign ((windowSamples + runSamples - 1) / runSamples, 0.0);
        next = 0;
        windowSum = freshSum = runSum = 0.0;
        freshRuns = runCount = 0;
        silentSamples = windowSamples;
        peak.store (0.0f, std::memory_order_relaxed);
        meanSquare.store (0.0, std::memory_order_relaxed);
    }

    void process (const float* samples, int count)
    {
        float blockPeak = 0.0f;
        for (int i = 0; i < count; ++i)
        {
            blockPeak = std::max (blockPeak, std::abs (samples[i]));
            const double square = static_cast<double> (samples[i]) * samples[i];
            runSum += square;
            silentSamples = square == 0.0 ? silentSamples + 1 : 0;
            if (++runCount == runSamples)
                endRun();
        }

        // The peak may only grow until the reader takes it.
        float held = peak.load (std::memory_order_relaxed);
        while (blockPeak > held && ! peak.compare_exchange_weak (held, blockPeak, std::memory_order_relaxed))
        {
        }
        // A window of silence is exactly silent, whatever the running sum's rounding left.
        const bool silent = silentSamples >= windowSamples;
        const auto samplesInWindow = static_cast<double> (runSums.size()) * runSamples;
        meanSquare.store (silent ? 0.0 : std::max (windowSum, 0.0) / samplesInWindow, std::memory_order_relaxed);
    }

    OutputLevel read()
    {
        return { .peakDb = toDb (peak.exchange (0.0f, std::memory_order_relaxed)),
                 .rmsDb = toDb (std::sqrt (meanSquare.load (std::memory_order_relaxed))) };
    }

private:
    // The run's sum takes the oldest's place in the window.
    void endRun()
    {
        windowSum += runSum - runSums[next];
        runSums[next] = runSum;
        if (++next == runSums.size())
            next = 0;
        // A second sum starts afresh each window and replaces the running one once it has covered a
        // whole window, so rounding in the running sum doesn't build up past one window, with no
        // resum of the window on the audio thread.
        freshSum += runSum;
        if (++freshRuns == runSums.size())
        {
            windowSum = freshSum;
            freshSum = 0.0;
            freshRuns = 0;
        }
        runSum = 0.0;
        runCount = 0;
    }

    // In dB of amplitude, at least the floor; the floor too for NaN.
    static double toDb (double amplitude)
    {
        const double db = gainToDecibels (amplitude);
        return db > levelFloorDb ? db : levelFloorDb;
    }

    size_t windowSamples = 1;
    std::vector<double> runSums; // the window's per-run sums of squares, a ring
    size_t next = 0;             // where the next run's sum goes
    double windowSum = 0.0;      // of runSums
    double freshSum = 0.0;       // of the runs since it last replaced windowSum
    size_t freshRuns = 0;
    double runSum = 0.0;      // of the run in progress
    int runCount = 0;         // samples in the run in progress
    size_t silentSamples = 0; // how many samples in a row have been exactly 0

    std::atomic<float> peak { 0.0f };
    std::atomic<double> meanSquare { 0.0 };
    static_assert (std::atomic<float>::is_always_lock_free && std::atomic<double>::is_always_lock_free, "reading never locks");
};

} // namespace eq1
