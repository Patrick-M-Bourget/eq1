#pragma once

#include "eq1/Engine.h"

#include <atomic>

namespace eq1
{

// The metered Band's Detection Level: process() on the audio thread holds the loudest level, read()
// on one reader thread takes it. Neither allocates or locks.
class DetectionLevelTap
{
public:
    // The level may only grow until the reader takes it.
    void hold (double levelDb)
    {
        double held = level.load (std::memory_order_relaxed);
        while (levelDb > held && ! level.compare_exchange_weak (held, levelDb, std::memory_order_relaxed))
        {
        }
    }

    // Forgets what is held, so a newly metered Band doesn't report the last one's level.
    void clear() { level.store (outputLevelFloorDb, std::memory_order_relaxed); }

    // At least the floor; the floor too for NaN.
    double read()
    {
        const double db = level.exchange (outputLevelFloorDb, std::memory_order_relaxed);
        return db > outputLevelFloorDb ? db : outputLevelFloorDb;
    }

private:
    std::atomic<double> level { outputLevelFloorDb };
    static_assert (std::atomic<double>::is_always_lock_free, "reading never locks");
};

} // namespace eq1
