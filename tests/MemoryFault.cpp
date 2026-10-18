#ifndef REDLIGHT_FAKE_WIN32
#error Allocation fault injection is for isolated code tests only
#endif
#include "MemoryFault.h"
#include <cstdlib>
#include <new>
namespace {
thread_local bool failNext = false;
thread_local unsigned allocationsBeforeFailure = 0;
}
namespace TestMemory {
void ArmAllocationFailure(unsigned allocationsToSkip) noexcept { failNext = true; allocationsBeforeFailure = allocationsToSkip; }
void CancelAllocationFailure() noexcept { failNext = false; allocationsBeforeFailure = 0; }
bool AllocationFailurePending() noexcept { return failNext; }
}
void* operator new(std::size_t bytes) {
    if (failNext) {
        if (!allocationsBeforeFailure) { failNext = false; throw std::bad_alloc(); }
        --allocationsBeforeFailure;
    }
    if (void* memory = std::malloc(bytes ? bytes : 1)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }
