#pragma once
#include "WindowedProbeOptions.h"
#include <cstdint>

namespace MonitorPrototype {
enum class ProbePhase { Idle, Preparing, Visible, CleanupPending, Finished };
// One reviewed run. Api is the native frontend's specific resource owner; the
// isolated tests supply the same operations without native windows or queries.
template<class Api> class WindowedProbeSession {
public:
    explicit WindowedProbeSession(Api& api) noexcept : api_(api) {}
    ProbePhase phase() const noexcept { return phase_; }
    bool busy() const noexcept { return busy_; }
    bool start(const ProbeOptions& options) noexcept {
        if (phase_ != ProbePhase::Idle || busy_ || !options.valid()) return false;
        phase_ = ProbePhase::Preparing; busy_ = true;
        duration_ = options.seconds * 1000ull;
        const auto accepted = [&](bool result) {
            return result && !canceled_ && (!deadline_ || api_.now() - started_ < duration_);
        };
        bool ready = accepted(api_.acquireOwnership()) && accepted(api_.review(options)) &&
            accepted(api_.createHidden(options)) && accepted(api_.prepareControl(options)) &&
            accepted(api_.installRecovery()) && accepted(api_.confirmReviewed());
        if (ready) {
            started_ = api_.now(); deadline_ = true;
            ready = accepted(api_.revealChild()) && accepted(api_.revealHost());
        }
        busy_ = false;
        if (!ready) { stop(); return false; }
        phase_ = ProbePhase::Visible;
        return true;
    }
    bool stop() noexcept {
        canceled_ = true;
        if (phase_ == ProbePhase::Finished) return true;
        if (busy_) return false;
        busy_ = true; phase_ = ProbePhase::CleanupPending; lastCleanup_ = api_.now();
        const bool released = api_.cleanup();
        busy_ = false;
        if (released) phase_ = ProbePhase::Finished;
        return released;
    }
    void tick(std::uint64_t now) noexcept {
        if ((phase_ == ProbePhase::Preparing || phase_ == ProbePhase::Visible) && deadline_ && now - started_ >= duration_) stop();
        else if (phase_ == ProbePhase::CleanupPending && !busy_ && now - lastCleanup_ >= 1000) stop();
    }
private:
    Api& api_;
    ProbePhase phase_ = ProbePhase::Idle;
    std::uint64_t started_ = 0, duration_ = 0, lastCleanup_ = 0;
    bool busy_ = false, canceled_ = false, deadline_ = false;
};
}
