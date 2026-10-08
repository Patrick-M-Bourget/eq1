#pragma once

#include <cstddef>

namespace eq1::test
{

// Counts heap allocations made on the current thread while it is alive.
class AllocationGuard
{
public:
    AllocationGuard();
    ~AllocationGuard();

    std::size_t allocations() const;
};

} // namespace eq1::test
