#pragma once

#include <algorithm>
#include <atomic>
#include <vector>

namespace eq1
{

// Single-producer single-consumer ring of mono samples for one analysis tap. The audio thread
// pushes, one reader pops; when the reader falls behind, new samples are dropped.
class AnalysisFifo
{
public:
    void allocate (int capacity)
    {
        buffer.assign (static_cast<size_t> (capacity), 0.0f);
        writeIndex.store (0);
        readIndex.store (0);
    }

    // Pushes sampleAt(0) ... sampleAt(count - 1).
    template <typename SampleAt>
    void push (int count, SampleAt sampleAt)
    {
        const auto write = writeIndex.load (std::memory_order_relaxed);
        const auto read = readIndex.load (std::memory_order_acquire);
        const auto space = buffer.size() - (write - read);
        const auto n = std::min (static_cast<size_t> (std::max (count, 0)), space);
        for (size_t i = 0; i < n; ++i)
            buffer[(write + i) % buffer.size()] = sampleAt (static_cast<int> (i));
        writeIndex.store (write + n, std::memory_order_release);
    }

    int pop (float* destination, int maxSamples)
    {
        const auto read = readIndex.load (std::memory_order_relaxed);
        const auto write = writeIndex.load (std::memory_order_acquire);
        const auto n = std::min (static_cast<size_t> (std::max (maxSamples, 0)), write - read);
        for (size_t i = 0; i < n; ++i)
            destination[i] = buffer[(read + i) % buffer.size()];
        readIndex.store (read + n, std::memory_order_release);
        return static_cast<int> (n);
    }

private:
    std::vector<float> buffer;
    std::atomic<size_t> writeIndex { 0 }, readIndex { 0 };
};

} // namespace eq1
