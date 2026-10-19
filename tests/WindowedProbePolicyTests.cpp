#include "WindowedProbeSession.h"
#include "MemoryFault.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cwchar>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
using namespace MonitorPrototype;
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
enum class Action { Ownership, Review, Create, Prepare, Recovery, Confirm, Child, Host, Cleanup, None };
struct Api {
    std::array<Action, 64> trace{}; unsigned calls = 0, cleanups = 0;
    Action failure = Action::None, callbackAt = Action::None;
    void (*callback)(void*) = nullptr; void* context = nullptr;
    std::uint64_t clock = 1000;
    bool cleanupOk = true, hiddenOwned = false, childVisible = false, hostVisible = false, overflow = false;
    bool invoke(Action action) noexcept {
        if (calls == trace.size()) { overflow = true; return false; }
        trace[calls++] = action;
        if (action == callbackAt && callback) { callbackAt = Action::None; callback(context); }
        return action != failure;
    }
    std::uint64_t now() const noexcept { return clock; }
    bool acquireOwnership() noexcept { return invoke(Action::Ownership); }
    bool review(const ProbeOptions&) noexcept { return invoke(Action::Review); }
    bool createHidden(const ProbeOptions&) noexcept { hiddenOwned = true; return invoke(Action::Create); }
    bool prepareControl(const ProbeOptions&) noexcept { return invoke(Action::Prepare); }
    bool installRecovery() noexcept { return invoke(Action::Recovery); }
    bool confirmReviewed() noexcept { return invoke(Action::Confirm); }
    bool revealChild() noexcept { childVisible = true; return invoke(Action::Child); }
    bool revealHost() noexcept { hostVisible = true; return invoke(Action::Host); }
    bool cleanup() noexcept {
        ++cleanups; invoke(Action::Cleanup); // Native owner attempts hide/destroy even after a partially applied reveal.
        if (!cleanupOk) return false;
        hiddenOwned = childVisible = hostVisible = false; return true;
    }
};
using Session = WindowedProbeSession<Api>;
ProbeOptions Options() { return {1, 3, 37, RedMode::LumaRed, LumaProfile::Classic}; }
void Stop(void* context) { static_cast<Session*>(context)->stop(); }

void ParserRequiresExplicitSelectionAndDuration() {
    Check(ParseProbeOptions({}).request == ProbeRequest::Help, "no-argument startup only requests help");
    const std::array<std::wstring_view, 1> help{L"--help"};
    Check(ParseProbeOptions(help).request == ProbeRequest::Help, "help is not activation consent");
    for (unsigned monitor = 1; monitor <= 8; ++monitor) for (unsigned seconds = 1; seconds <= 30; ++seconds) {
        wchar_t m[4], s[4]; std::swprintf(m, std::size(m), L"%u", monitor); std::swprintf(s, std::size(s), L"%u", seconds);
        const std::array<std::wstring_view, 4> args{L"--monitor", m, L"--seconds", s}; const auto parsed = ParseProbeOptions(args);
        Check(parsed.request == ProbeRequest::Run && parsed.options.monitor == monitor && parsed.options.seconds == seconds && parsed.options.brightness == 100 &&
            parsed.options.mode == RedMode::LumaRed && parsed.options.profile == LumaProfile::Classic, "every supported selection/deadline preserves explicit defaults without native work");
    }
}
void ParserRejectsMalformedOrAmbiguousRequests() {
    const std::array<std::wstring_view, 4> base{L"--monitor", L"1", L"--seconds", L"3"};
    for (const auto bad : {L"", L"0", L"9", L"-1", L"+1", L" 1", L"1x", L"9999999999999999999999999"}) {
        auto args = base; args[1] = bad; Check(ParseProbeOptions(args).request == ProbeRequest::Invalid, "invalid monitor values cannot trigger startup");
    }
    for (const auto bad : {L"", L"0", L"31", L"-1", L"+1", L" 1", L"1x", L"9999999999999999999999999"}) {
        auto args = base; args[3] = bad; Check(ParseProbeOptions(args).request == ProbeRequest::Invalid, "invalid durations cannot trigger startup");
    }
    const std::array<std::wstring_view, 6> duplicate{L"--monitor", L"1", L"--seconds", L"3", L"--monitor", L"2"};
    const std::array<std::wstring_view, 2> missing{L"--monitor", L"1"};
    const std::array<std::wstring_view, 5> odd{L"--monitor", L"1", L"--seconds", L"3", L"--mode"};
    Check(ParseProbeOptions(duplicate).request == ProbeRequest::Invalid && ParseProbeOptions(missing).request == ProbeRequest::Invalid && ParseProbeOptions(odd).request == ProbeRequest::Invalid,
        "duplicate/missing/unpaired options cannot grant a partial request");
}
void ParserRetainsRedOnlyChoicesAndBounds() {
    for (const auto mode : {L"strict", L"luma"}) for (const auto profile : {L"classic", L"green", L"equal"}) for (unsigned percent = 1; percent <= 100; ++percent) {
        wchar_t p[4]; std::swprintf(p, std::size(p), L"%u", percent);
        const std::array<std::wstring_view, 10> args{L"--monitor", L"1", L"--seconds", L"3", L"--mode", mode, L"--luma", profile, L"--brightness", p};
        const auto parsed = ParseProbeOptions(args);
        Check(parsed.request == ProbeRequest::Run && parsed.options.valid() && parsed.options.brightness == percent, "all brightness/mode/profile options are data, not display calls");
    }
    for (const auto option : {L"--unknown", L"--brightness", L"--mode", L"--luma"}) {
        const std::array<std::wstring_view, 6> args{L"--monitor", L"1", L"--seconds", L"3", option, L"invalid"};
        Check(ParseProbeOptions(args).request == ProbeRequest::Invalid, "unknown options or unsupported tokens do not silently use defaults");
    }
}
void StartOrderRequiresEveryGateBeforeVisibility() {
    Api api; Session session(api);
    Check(session.start(Options()) && session.phase() == ProbePhase::Visible && api.hiddenOwned && api.childVisible && api.hostVisible, "only complete confirmed setup reports a visible run");
    constexpr std::array expected{Action::Ownership, Action::Review, Action::Create, Action::Prepare, Action::Recovery, Action::Confirm, Action::Child, Action::Host};
    Check(api.calls == expected.size() && std::equal(expected.begin(), expected.end(), api.trace.begin()), "ownership, explicit review, hidden checks and recovery/freshness precede each reveal");
    const auto calls = api.calls;
    Check(!session.start(Options()) && api.calls == calls, "a second start cannot restage or extend an active deadline");
    Check(session.stop() && session.phase() == ProbePhase::Finished && !api.hiddenOwned && !api.hostVisible && !session.start(Options()), "stopping is one-way and requires a new process/review for another run");
    Check(session.stop() && api.cleanups == 1, "quit/stale callbacks after completed cleanup cannot release resources twice");
    const auto after = api.calls; session.tick((std::numeric_limits<std::uint64_t>::max)());
    Check(api.calls == after, "stale native timer callbacks after completion cannot restart or repeat native work");
}
void EveryFailureCleansPartialNativeResources() {
    for (const auto action : {Action::Ownership, Action::Review, Action::Create, Action::Prepare, Action::Recovery, Action::Confirm, Action::Child, Action::Host}) {
        Api api; api.failure = action; Session session(api);
        Check(!session.start(Options()) && session.phase() == ProbePhase::Finished && api.cleanups == 1 && !api.hiddenOwned && !api.childVisible && !api.hostVisible,
            "failed-but-partially-applied setup/reveal never reports visible and always attempts owned cleanup");
    }
}
void CancellationDuringEveryStepCannotReappear() {
    for (const auto action : {Action::Ownership, Action::Review, Action::Create, Action::Prepare, Action::Recovery, Action::Confirm, Action::Child, Action::Host}) {
        Api api; Session session(api); api.callbackAt = action; api.callback = Stop; api.context = &session;
        Check(!session.start(Options()) && session.phase() == ProbePhase::Finished && api.cleanups == 1 && !api.hostVisible && !api.hiddenOwned,
            "close/power/session cancellation during a native call is retained until its return and cannot publish the prepared run");
    }
}
void DeadlineStopsAtBoundaryAndAcrossWrap() {
    Api api; Session session(api); Check(session.start(Options()), "prepare deadline");
    session.tick(3999); Check(session.phase() == ProbePhase::Visible && api.cleanups == 0, "strictly before deadline remains active");
    session.tick(4000); Check(session.phase() == ProbePhase::Finished && api.cleanups == 1 && !api.hostVisible, "exact expiry cleans immediately without a transition or topology read");
    Api wrapped; wrapped.clock = (std::numeric_limits<std::uint64_t>::max)() - 1; Session other(wrapped);
    Check(other.start({1, 1, 100, RedMode::StrictRed, LumaProfile::Classic}), "prepare wrapped deadline");
    other.tick(998); Check(other.phase() == ProbePhase::Finished && !wrapped.hostVisible, "unsigned wrap preserves the exact deadline");
}
void FailedCleanupRetainsRetryWithoutRestart() {
    Api api; Session session(api); Check(session.start(Options()), "prepare retained owner"); api.cleanupOk = false;
    Check(!session.stop() && session.phase() == ProbePhase::CleanupPending && api.hiddenOwned && !session.start(Options()), "failed cleanup retains the owning state and blocks automatic/repeated starts");
    session.tick(1999); Check(api.cleanups == 1, "cleanup retries do not spin at native timer frequency");
    api.clock = 2000; session.tick(2000); Check(api.cleanups == 2 && session.phase() == ProbePhase::CleanupPending, "bounded retry keeps exact ownership after another failure");
    api.cleanupOk = true; api.clock = 3000; session.tick(3000);
    Check(session.phase() == ProbePhase::Finished && !api.hiddenOwned && !api.hostVisible && !session.start(Options()), "later cleanup removes the same copy and never queues a new activation");
}
void ExpiryDuringRevealDoesNotConfirmActivation() {
    for (const bool deliverTimer : {false, true}) {
        Api api; Session session(api);
        struct Context { Api* api; Session* session; bool timer; } context{&api, &session, deliverTimer};
        api.callbackAt = Action::Host; api.context = &context;
        api.callback = [](void* data) {
            auto& state = *static_cast<Context*>(data); state.api->clock = 4000;
            if (state.timer) state.session->tick(4000);
        };
        Check(!session.start(Options()) && session.phase() == ProbePhase::Finished && api.cleanups == 1 && !api.hostVisible,
            "expiry while a show call is pending wins with or without dispatched timer messages and cannot report a confirmed visible run");
    }
}
void CleanupCallbacksCannotRecurse() {
    Api api; Session session(api); Check(session.start(Options()), "prepare cleanup callback");
    api.callbackAt = Action::Cleanup; api.callback = Stop; api.context = &session;
    Check(session.stop() && api.cleanups == 1 && session.phase() == ProbePhase::Finished && !api.overflow, "nested cleanup requests cannot destroy/release twice");
}
void AmbiguousRevealRetainsRecoveryUntilCleanupConfirms() {
    for (const auto failed : {Action::Child, Action::Host}) {
        Api api; api.failure = failed; api.cleanupOk = false; Session session(api);
        Check(!session.start(Options()) && session.phase() == ProbePhase::CleanupPending && api.hiddenOwned && api.childVisible && !session.start(Options()),
            "a failed-but-applied reveal with failed cleanup is unconfirmed and retains its owner; it cannot recapture/restart");
        api.cleanupOk = true;
        Check(session.stop() && session.phase() == ProbePhase::Finished && !api.hiddenOwned && !api.hostVisible && api.cleanups == 2,
            "manual retry removes the same partially revealed resources without another setup");
    }
}
void CleanupOnlyResourcesCannotReapplyAReleasedCopy() {
    Api api; Session session(api); Check(session.start(Options()), "prepare cleanup-only transition");
    api.cleanupOk = false;
    Check(!session.stop(), "retain a native cleanup failure");
    api.hiddenOwned = api.childVisible = api.hostVisible = false; // Only timer/hook/lease cleanup remains.
    const auto before = api.calls; api.cleanupOk = true; api.clock = 2000; session.tick(2000);
    Check(session.phase() == ProbePhase::Finished && api.calls == before + 1 && api.trace[before] == Action::Cleanup && !api.hostVisible,
        "cleanup-only retries never recreate or show an already-removed copy");
}
void InvalidOptionsAndAllocationIsolation() {
    Api api; Session session(api); auto invalid = Options(); invalid.seconds = 31;
    Check(!session.start(invalid) && api.calls == 0 && session.phase() == ProbePhase::Idle, "invalid data cannot acquire native resources or create a window");
    bool started = false, stopped = false, pending = false;
    { TestMemory::FailNextAllocation failure; started = session.start(Options()); stopped = session.stop(); pending = TestMemory::AllocationFailurePending(); }
    Check(started && stopped && pending, "coordination and cancellation allocate no C++ memory");
}
}

unsigned RunWindowedProbePolicyTests() {
    const std::pair<const char*, void(*)()> tests[] = {
        {"Probe parser explicit selection/deadline", ParserRequiresExplicitSelectionAndDuration}, {"Probe parser malformed requests", ParserRejectsMalformedOrAmbiguousRequests},
        {"Probe parser red-only options", ParserRetainsRedOnlyChoicesAndBounds}, {"Probe setup/recovery/visibility order", StartOrderRequiresEveryGateBeforeVisibility},
        {"Probe partial setup/reveal failures", EveryFailureCleansPartialNativeResources}, {"Probe cancellation during native steps", CancellationDuringEveryStepCannotReappear},
        {"Probe deadline/boundary/wrap", DeadlineStopsAtBoundaryAndAcrossWrap}, {"Probe failed cleanup/retry", FailedCleanupRetainsRetryWithoutRestart},
        {"Probe expiry during native reveal", ExpiryDuringRevealDoesNotConfirmActivation},
        {"Probe cleanup reentry", CleanupCallbacksCannotRecurse}, {"Probe validation/allocation isolation", InvalidOptionsAndAllocationIsolation},
        {"Probe ambiguous reveal/recovery ownership", AmbiguousRevealRetainsRecoveryUntilCleanupConfirms},
        {"Probe cleanup-only no reapply", CleanupOnlyResourcesCannotReapplyAReleasedCopy}
    };
    unsigned failures = 0;
    for (const auto& [name, run] : tests) {
        try { run(); std::printf("PASS %s\n", name); }
        catch (const std::exception& error) { ++failures; std::printf("FAIL %s: %s\n", name, error.what()); }
    }
    return failures;
}
