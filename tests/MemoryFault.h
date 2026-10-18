#pragma once
namespace TestMemory {
void ArmAllocationFailure(unsigned allocationsToSkip = 0) noexcept;
void CancelAllocationFailure() noexcept;
bool AllocationFailurePending() noexcept;
struct FailNextAllocation {
    explicit FailNextAllocation(unsigned allocationsToSkip = 0) noexcept { ArmAllocationFailure(allocationsToSkip); }
    ~FailNextAllocation() { CancelAllocationFailure(); }
    FailNextAllocation(const FailNextAllocation&) = delete;
    FailNextAllocation& operator=(const FailNextAllocation&) = delete;
};
}
