#pragma once

#include "eq1/Engine.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <numeric>
#include <vector>

namespace eq1
{

// The Output Level of one channel: process() on the audio thread measures, read() on one reader
// thread takes the result. Neither allocates or locks.
//
// Peak is the largest absolute sample since the last read. RMS is over a rectangular window of the
// last 300 ms, defined in samples, so it doesn't depend on how the audio is cut into blocks.
class LevelMeter
{
public:
    static constexpr double windowSeconds = 0.3;

    // Allocates. Starts with an empty window and no peak.
    void prepare (double sampleRate)
    {
        squares.assign (static_cast<size_t> (std::max (1L, std::lround (windowSeconds * sampleRate))), 0.0);
        next = 0;
        sum = 0.0;
        silentRun = squares.size();
        peak.store (0.0f, std::memory_order_relaxed);
        meanSquare.store (0.0, std::memory_order_relaxed);
    }

    void process (const float* samples, int count)
    {
        float blockPeak = 0.0f;
        for (int i = 0; i < count; ++i)
        {
            blockPeak = std::max (blockPeak, std::abs (samples[i]));
            // A float's square is exact in double, so the running sum subtracts exactly what it added.
            const double square = static_cast<double> (samples[i]) * samples[i];
            sum += square - squares[next];
            squares[next] = square;
            silentRun = square == 0.0 ? silentRun + 1 : 0;
            if (++next == squares.size())
            {
                next = 0;
                // Rounding in the running sum doesn't build up past one window.
                sum = std::accumulate (squares.begin(), squares.end(), 0.0);
            }
        }

        // The peak may only grow until the reader takes it.
        float held = peak.load (std::memory_order_relaxed);
        while (blockPeak > held && ! peak.compare_exchange_weak (held, blockPeak, std::memory_order_relaxed))
        {
        }
        // A window of silence is exactly silent, whatever the running sum's rounding left.
        const bool silent = silentRun >= squares.size();
        meanSquare.store (silent ? 0.0 : std::max (sum, 0.0) / static_cast<double> (squares.size()), std::memory_order_relaxed);
    }

    OutputLevel read()
    {
        return { .peakDb = toDb (peak.exchange (0.0f, std::memory_order_relaxed)),
                 .rmsDb = toDb (std::sqrt (meanSquare.load (std::memory_order_relaxed))) };
    }

private:
    // In dB of amplitude, at least the floor; the floor too for NaN.
    static double toDb (double amplitude)
    {
        const double db = 20.0 * std::log10 (amplitude);
        return db > outputLevelFloorDb ? db : outputLevelFloorDb;
    }

    std::vector<double> squares; // the window's squared samples, a ring
    size_t next = 0;             // where the next square goes
    double sum = 0.0;            // of squares
    size_t silentRun = 0;        // how many samples in a row have been exactly 0

    std::atomic<float> peak { 0.0f };
    std::atomic<double> meanSquare { 0.0 };
    static_assert (std::atomic<float>::is_always_lock_free && std::atomic<double>::is_always_lock_free, "reading never locks");
};

} // namespace eq1
