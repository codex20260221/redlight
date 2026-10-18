#ifndef REDLIGHT_FAKE_WIN32
#error Allocation fault injection is for isolated code tests only
#endif
#include "MemoryFault.h"
#include <cstdlib>
#include <new>
namespace {
thread_local bool failNext = false;
}
namespace TestMemory {
void ArmAllocationFailure() noexcept { failNext = true; }
void CancelAllocationFailure() noexcept { failNext = false; }
bool AllocationFailurePending() noexcept { return failNext; }
}
void* operator new(std::size_t bytes) {
    if (failNext) { failNext = false; throw std::bad_alloc(); }
    if (void* memory = std::malloc(bytes ? bytes : 1)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }
