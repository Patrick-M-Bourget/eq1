#include "AllocationGuard.h"

#include <cstdlib>
#if defined(_WIN32)
    #include <malloc.h>
#endif
#include <new>

namespace
{
thread_local bool guarding = false;
thread_local std::size_t count = 0;

void* allocate (std::size_t size)
{
    if (guarding)
        ++count;
    if (void* p = std::malloc (size == 0 ? 1 : size))
        return p;
    throw std::bad_alloc {};
}

void* allocateAligned (std::size_t size, std::align_val_t alignment)
{
    if (guarding)
        ++count;
    const auto align = static_cast<std::size_t> (alignment);
#if defined(_WIN32)
    if (void* p = _aligned_malloc (size == 0 ? 1 : size, align))
        return p;
#else
    if (void* p = std::aligned_alloc (align, (size + align - 1) / align * align))
        return p;
#endif
    throw std::bad_alloc {};
}
void freeAligned (void* p) noexcept
{
#if defined(_WIN32)
    _aligned_free (p);
#else
    std::free (p);
#endif
}
} // namespace

namespace eq1::test
{
AllocationGuard::AllocationGuard()
{
    count = 0;
    guarding = true;
}
AllocationGuard::~AllocationGuard() { guarding = false; }
std::size_t AllocationGuard::allocations() const { return count; }
} // namespace eq1::test

void* operator new (std::size_t size) { return allocate (size); }
void* operator new[] (std::size_t size) { return allocate (size); }
void* operator new (std::size_t size, const std::nothrow_t&) noexcept
{
    try { return allocate (size); } catch (...) { return nullptr; }
}
void* operator new[] (std::size_t size, const std::nothrow_t&) noexcept
{
    try { return allocate (size); } catch (...) { return nullptr; }
}
void* operator new (std::size_t size, std::align_val_t a) { return allocateAligned (size, a); }
void* operator new[] (std::size_t size, std::align_val_t a) { return allocateAligned (size, a); }

void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }
void operator delete (void* p, std::align_val_t) noexcept { freeAligned (p); }
void operator delete[] (void* p, std::align_val_t) noexcept { freeAligned (p); }
void operator delete (void* p, std::size_t, std::align_val_t) noexcept { freeAligned (p); }
void operator delete[] (void* p, std::size_t, std::align_val_t) noexcept { freeAligned (p); }
