#pragma once
namespace TestMemory {
void ArmAllocationFailure() noexcept;
void CancelAllocationFailure() noexcept;
bool AllocationFailurePending() noexcept;
struct FailNextAllocation {
    FailNextAllocation() noexcept { ArmAllocationFailure(); }
    ~FailNextAllocation() { CancelAllocationFailure(); }
    FailNextAllocation(const FailNextAllocation&) = delete;
    FailNextAllocation& operator=(const FailNextAllocation&) = delete;
};
}
