#pragma once

#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <memory>
#include <vector>

namespace eq1
{

// Single-producer single-consumer ring of mono samples for one analysis tap. The audio thread
// pushes, one reader pops; when the reader falls behind, new samples are dropped.
//
// The ring's capacity is a power of two covering at least 150 ms at the prepared sample rate: the
// time between two editor frames with margin, which is all the Analyzer needs to bridge. The reader
// may go on popping while the host prepares the Engine again, so a ring is never freed or moved
// while the Engine lives: a prepare that needs more replaces it with a bigger one and keeps the old
// one, and a prepare that needs less keeps the ring it has.
class AnalysisFifo
{
public:
    static constexpr double minimumSeconds = 0.15;

    // The smallest power of two covering minimumSeconds at sampleRate.
    static int capacityFor (double sampleRate)
    {
        const auto needed = static_cast<unsigned> (std::max (1.0, std::ceil (minimumSeconds * sampleRate)));
        return static_cast<int> (std::bit_ceil (needed));
    }

    // Allocates when the sample rate needs a bigger ring than the one held; otherwise leaves the ring,
    // and what it holds, alone.
    void prepare (double sampleRate)
    {
        const auto needed = static_cast<size_t> (capacityFor (sampleRate));
        if (auto* ring = current.load (std::memory_order_relaxed); ring != nullptr && ring->samples.size() >= needed)
            return;
        rings.push_back (std::make_unique<Ring> (needed));
        current.store (rings.back().get(), std::memory_order_release);
    }

    // How many samples the ring holds: the most one pop can return.
    int capacity() const
    {
        const auto* ring = current.load (std::memory_order_acquire);
        return ring == nullptr ? 0 : static_cast<int> (ring->samples.size());
    }

    // Pushes sampleAt(0) ... sampleAt(count - 1).
    template <typename SampleAt>
    void push (int count, SampleAt sampleAt)
    {
        auto* ring = current.load (std::memory_order_acquire);
        if (ring == nullptr)
            return;
        const auto write = ring->writeIndex.load (std::memory_order_relaxed);
        const auto read = ring->readIndex.load (std::memory_order_acquire);
        const auto space = ring->samples.size() - (write - read);
        const auto n = std::min (static_cast<size_t> (std::max (count, 0)), space);
        for (size_t i = 0; i < n; ++i)
            ring->samples[(write + i) & ring->mask] = sampleAt (static_cast<int> (i));
        ring->writeIndex.store (write + n, std::memory_order_release);
    }

    int pop (float* destination, int maxSamples)
    {
        auto* ring = current.load (std::memory_order_acquire);
        if (ring == nullptr)
            return 0;
        const auto read = ring->readIndex.load (std::memory_order_relaxed);
        const auto write = ring->writeIndex.load (std::memory_order_acquire);
        const auto n = std::min (static_cast<size_t> (std::max (maxSamples, 0)), write - read);
        for (size_t i = 0; i < n; ++i)
            destination[i] = ring->samples[(read + i) & ring->mask];
        ring->readIndex.store (read + n, std::memory_order_release);
        return static_cast<int> (n);
    }

private:
    // A ring's indices live with its samples, so a reader finishing a pop on an outgrown ring touches
    // nothing of the new one.
    struct Ring
    {
        explicit Ring (size_t capacity) : samples (capacity, 0.0f), mask (capacity - 1) {}
        std::vector<float> samples;
        size_t mask;
        std::atomic<size_t> writeIndex { 0 }, readIndex { 0 };
    };

    std::vector<std::unique_ptr<Ring>> rings; // every ring made since construction, the current one last
    std::atomic<Ring*> current { nullptr };
};

} // namespace eq1
