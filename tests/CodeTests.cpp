// These tests compile the production backends against fake headers and functions.
// No Windows display libraries are linked; never substitute real Win32 headers here.
#include "MagnificationFilter.h"
#include "GammaRampFilter.h"
#include "CommandLine.h"
#include "SessionTimer.h"
#include "BackendSelection.h"
#include <limits>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <stdexcept>
#include <vector>

namespace {
enum class Write { Apply, Fail, Ignore, FailAfterApply, LowByteLeak };
struct Fake {
    bool magInit = true, magGet = true, magSet = true, magUninit = true;
    bool dc = true, gammaGet = true, release = true;
    int magWrites = 0, gammaWrites = 0, releases = 0, uninitializes = 0;
    int magInitializes = 0, dcRequests = 0;
    MAGCOLOREFFECT effect = {};
    WORD ramp[3][256] = {};
    std::deque<Write> writes;
} fake;

void Reset() {
    fake = {};
    for (int i = 0; i < 5; ++i) fake.effect.transform[i][i] = 1;
    for (int c = 0; c < 3; ++c)
        for (int i = 0; i < 256; ++i) fake.ramp[c][i] = static_cast<WORD>(i * 257);
}
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
bool Same(const MAGCOLOREFFECT& a, const MAGCOLOREFFECT& b) {
    return std::memcmp(&a, &b, sizeof(a)) == 0;
}
void MatrixOutput(RedMode mode) {
    MagnificationFilter filter;
    Check(filter.initialize() && filter.setRedMode(mode) && filter.enable(), "matrix enable");
    for (int r : {0, 64, 128, 255}) for (int g : {0, 64, 128, 255}) for (int b : {0, 64, 128, 255}) {
        const float input[] = {r / 255.0f, g / 255.0f, b / 255.0f, 0.75f, 1.0f};
        float output[5] = {};
        for (int c = 0; c < 5; ++c) for (int i = 0; i < 5; ++i)
            output[c] += input[i] * fake.effect.transform[i][c];
        const float expected = mode == RedMode::StrictRed ? input[0] :
            0.299f * input[0] + 0.587f * input[1] + 0.114f * input[2];
        Check(std::abs(output[0] - expected) < 0.00001f, "red intensity / black / white");
        Check(output[1] == 0 && output[2] == 0, "green and blue eliminated");
        Check(output[3] == 0.75f && output[4] == 1, "alpha and homogeneous coordinate preserved");
    }
}
void StrictMatrix() { MatrixOutput(RedMode::StrictRed); }
void LumaMatrix() { MatrixOutput(RedMode::LumaRed); }
void MagCaptureEachActivation() {
    MagnificationFilter f;
    Check(f.initialize(), "initialize");
    fake.effect.transform[0][0] = 0.42f;
    auto prior = fake.effect;
    Check(f.enable() && f.enable(), "idempotent enable");
    Check(fake.magWrites == 1, "no duplicate enable write");
    Check(f.disable() && Same(fake.effect, prior), "restore latest pre-enable matrix");
    fake.effect.transform[2][2] = 0.73f;
    prior = fake.effect;
    Check(f.enable() && f.disable() && Same(fake.effect, prior), "recapture each cycle");
}
void MagCaptureFailure() {
    MagnificationFilter f;
    Check(f.initialize(), "initialize");
    fake.magGet = false;
    Check(!f.enable() && !f.isActive() && !f.needsRestore(), "capture failure stays off");
    Check(fake.magWrites == 0 && f.lastError()[0], "no write without snapshot; error available");
}
void MagInitializationFailures() {
    MagnificationFilter f;
    fake.magInit = false;
    Check(!f.initialize() && !f.enable(), "init failure");
    fake.magInit = true;
    fake.magGet = false;
    Check(!f.initialize() && fake.uninitializes == 1, "failed capture cleans up");
    Check(fake.magWrites == 0, "never identity-overwrite uncaptured state");
    fake.magGet = true;
    Check(f.initialize(), "initialization can retry");
}
void MagWriteFailure() {
    MagnificationFilter f;
    Check(f.initialize(), "initialize");
    fake.magSet = false;
    Check(!f.enable() && !f.isActive(), "failed enable never reports active");
    fake.magSet = true;
    Check(f.enable(), "enable retry");
    const auto oldMode = f.redMode();
    fake.magSet = false;
    Check(!f.setRedMode(RedMode::StrictRed) && f.redMode() == oldMode, "failed mode write retains mode");
    Check(!f.disable() && f.isActive() && f.needsRestore(), "restore failure retains active state");
    Check(!f.shutdown() && fake.uninitializes == 0, "shutdown preserves API and recovery data");
    fake.magSet = true;
    Check(f.disable() && f.shutdown(), "restoration and shutdown retry");
}
void MagUninitializeFailure() {
    MagnificationFilter f;
    Check(f.initialize(), "initialize");
    fake.magUninit = false;
    Check(!f.shutdown(), "uninit failure reported");
    fake.magUninit = true;
    Check(f.shutdown() && fake.uninitializes == 2, "uninit failure retryable");
    Check(f.shutdown() && fake.uninitializes == 2, "shutdown idempotent");
}
void InvalidModes() {
    MagnificationFilter m;
    GammaRampFilter g;
    const auto invalid = static_cast<RedMode>(99);
    Check(!m.setRedMode(invalid) && m.redMode() == RedMode::LumaRed, "invalid matrix mode rejected");
    Check(!g.setRedMode(RedMode::LumaRed) && !g.supportsRedMode(invalid), "gamma only strict red");
}
void GammaOutputAndRestore() {
    GammaRampFilter f;
    Check(f.initialize(), "initialize");
    fake.ramp[0][100] = 12345; // Calibration changed while app was off.
    WORD prior[3][256];
    std::memcpy(prior, fake.ramp, sizeof(prior));
    Check(f.enable(), "gamma enable");
    for (int i = 0; i < 256; ++i) {
        Check(fake.ramp[0][i] == i * 257, "full red range including white");
        Check(fake.ramp[1][i] == 0 && fake.ramp[2][i] == 0, "no green or blue");
    }
    Check(f.isActive() && f.needsRestore(), "confirmed active");
    Check(f.disable() && std::memcmp(prior, fake.ramp, sizeof(prior)) == 0, "restore recent calibration");
    const int writes = fake.gammaWrites;
    Check(f.shutdown() && fake.gammaWrites == writes, "inactive shutdown never overwrites external calibration");
}
void GammaIgnoredWrite() {
    GammaRampFilter f;
    Check(f.initialize(), "initialize");
    fake.writes = {Write::Ignore, Write::Apply};
    Check(!f.enable() && !f.isActive() && !f.needsRestore(), "silent rejection detected and rolled back");
    Check(fake.gammaWrites == 2 && f.lastError()[0], "rollback and diagnostic");
}
void GammaLowByteLeak() {
    GammaRampFilter f;
    Check(f.initialize(), "initialize");
    fake.writes = {Write::LowByteLeak, Write::Apply};
    Check(!f.enable() && !f.isActive() && !f.needsRestore(), "even low-byte green/blue leakage is rejected and rolled back");
}
void OffShutdownDoesNotWrite() {
    MagnificationFilter m;
    GammaRampFilter g;
    Check(m.initialize() && g.initialize(), "initialize");
    fake.effect.transform[0][0] = 0.4f;
    fake.ramp[0][100] = 23456;
    Check(m.shutdown() && g.shutdown(), "inactive shutdown");
    Check(fake.magWrites == 0 && fake.gammaWrites == 0, "off shutdown never touches display color state");
}
void GammaPartialFailureRecovery() {
    GammaRampFilter f;
    Check(f.initialize(), "initialize");
    fake.writes = {Write::FailAfterApply, Write::Fail, Write::Fail};
    Check(!f.enable() && !f.isActive() && f.needsRestore(), "partial failure keeps recovery pending without claiming active");
    const int releases = fake.releases;
    Check(!f.shutdown() && fake.releases == releases, "failed restore retains DC");
    Check(f.disable() && !f.needsRestore() && f.shutdown(), "recovery retry succeeds");
}
void GammaFailedDisable() {
    GammaRampFilter f;
    Check(f.initialize() && f.enable(), "enable");
    fake.writes = {Write::Ignore};
    Check(!f.disable() && f.isActive() && f.needsRestore(), "ignored restore retains active state");
    Check(f.disable() && !f.isActive(), "retry disable");
}
void GammaReadFailures() {
    GammaRampFilter f;
    Check(f.initialize(), "initialize");
    fake.gammaGet = false;
    Check(!f.enable() && fake.gammaWrites == 0, "capture failure writes nothing");
    fake.gammaGet = true;
    Check(f.enable(), "enable retry");
    fake.gammaGet = false;
    Check(!f.disable() && f.needsRestore(), "failed verification retains restore state");
    fake.gammaGet = true;
    Check(f.disable(), "verification retry");
}
void GammaResourceFailures() {
    GammaRampFilter f;
    fake.dc = false;
    Check(!f.initialize(), "DC failure");
    fake.dc = true;
    fake.gammaGet = false;
    Check(!f.initialize() && fake.releases == 1, "capture failure releases DC");
    fake.gammaGet = true;
    Check(f.initialize(), "initialization retry");
    fake.release = false;
    Check(!f.enable() && !f.shutdown(), "release failure does not silently discard DC");
    fake.release = true;
    Check(f.shutdown(), "release retry");
}
void SessionTimerBoundaries() {
    SessionTimer timer;
    timer.start(100);
    Check(!timer.armed() && !timer.restoreDue(999999), "default unlimited session");
    Check(timer.configure(15, 1000, false) && !timer.armed(), "configuration while off does not start session");
    timer.start(5000);
    Check(timer.remainingMilliseconds(5000) == 900000, "full limit starts on activation");
    Check(!timer.restoreDue(904999) && timer.restoreDue(905000), "exact deadline");
    Check(timer.remainingMilliseconds(99999999) == 0, "expired remaining time saturates");
    Check(!timer.configure(241, 0, true) && timer.minutes() == 15 && timer.restoreDue(905000), "invalid limit does not alter deadline");
}
void SessionTimerRetryAndCancellation() {
    SessionTimer timer;
    Check(timer.configure(1, 0, true), "one minute session");
    timer.noteRestoreAttempt(60000);
    Check(!timer.restoreDue(64999) && timer.restoreDue(65000), "throttle failed restore retries");
    timer.noteRestoreAttempt(65000);
    Check(!timer.restoreDue(69999) && timer.restoreDue(70000), "subsequent retry throttled");
    timer.cancel();
    Check(!timer.armed() && !timer.restoreDue(999999), "manual/environment restoration cancels session");
    timer.start(1000000);
    Check(!timer.restoreDue(1000000) && timer.remainingMilliseconds(1000000) == 60000, "next activation resets deadline and retry state");
    Check(timer.configure(0, 1001000, true) && !timer.armed(), "unlimited disables active time limit");
}
void SessionTimerChangesAndClockWrap() {
    SessionTimer timer;
    Check(timer.configure(15, 0, true) && timer.configure(30, 1000, true), "change active duration");
    Check(timer.remainingMilliseconds(1000) == 1800000, "changing limit restarts countdown explicitly");
    const auto nearWrap = (std::numeric_limits<std::uint64_t>::max)() - 30000;
    Check(timer.configure(1, nearWrap, true), "clock near wrap");
    Check(!timer.restoreDue(nearWrap + 59999) && timer.restoreDue(nearWrap + 60000), "deadline survives unsigned clock wrap");
}
void BackendSelectionPolicies() {
    MagnificationFilter m;
    GammaRampFilter g;
    Check(InitializePreferredBackend(BackendPreference::Automatic, m, g) == &m, "automatic prefers Magnification");
    Check(fake.dcRequests == 0 && fake.magWrites == 0 && fake.gammaWrites == 0, "selection never activates or initializes unused gamma");
}
void BackendSelectionStrictAndFallback() {
    MagnificationFilter m;
    GammaRampFilter g;
    fake.magInit = false;
    Check(!InitializePreferredBackend(BackendPreference::Magnification, m, g) && fake.dcRequests == 0, "Magnification-only never silently falls back");
    Check(InitializePreferredBackend(BackendPreference::Automatic, m, g) == &g, "automatic can fall back to gamma");
    Check(fake.magWrites == 0 && fake.gammaWrites == 0, "fallback remains off");
    Check(m.shutdown() && g.shutdown(), "cleanup");
    fake.magInitializes = 0;
    Check(InitializePreferredBackend(BackendPreference::Gamma, m, g) == &g && fake.magInitializes == 0, "gamma-only never initializes Magnification");
}
void BackendSelectionCleanupFailure() {
    MagnificationFilter m;
    GammaRampFilter g;
    fake.magGet = false;
    fake.magUninit = false;
    Check(!InitializePreferredBackend(BackendPreference::Automatic, m, g) && fake.dcRequests == 0, "failed cleanup prevents competing fallback ownership");
    fake.magUninit = true;
    Check(m.shutdown(), "cleanup retry");
    Check(!InitializePreferredBackend(static_cast<BackendPreference>(99), m, g), "invalid selection rejected");
}
void BackendCommandLine() {
    for (const auto arg : {L"--backend=auto", L"--backend=magnification", L"--backend=gamma"}) {
        const std::wstring_view args[] = {arg};
        const auto parsed = ParseStartupOptions(args);
        Check(parsed.action == StartupAction::Tray && parsed.backendOverride.has_value(), "valid backend override starts inactive tray mode");
    }
    const std::wstring_view gamma[] = {L"--backend=gamma"};
    Check(ParseStartupOptions(gamma).backendOverride == BackendPreference::Gamma, "gamma override");
    for (const auto arg : {L"--backend=", L"--backend=Gamma", L"--backend=anything", L"--backend=auto "}) {
        const std::wstring_view args[] = {arg};
        Check(ParseStartupOptions(args).action == StartupAction::Invalid, "invalid override rejected");
    }
    const std::wstring_view conflict[] = {L"--restore", L"--backend=gamma"};
    Check(ParseStartupOptions(conflict).action == StartupAction::Invalid, "recovery cannot be combined with a startup override");
    Check(!ParseStartupOptions({}).backendOverride.has_value(), "missing override retains saved/default policy");
    Check(IsValidBackendPreference(0) && IsValidBackendPreference(2) && !IsValidBackendPreference(3), "validate persisted backend values");
}
void LumaPresetOutput() {
    MagnificationFilter f;
    Check(f.initialize() && f.enable(), "enable luma");
    for (const auto profile : {LumaProfile::Classic, LumaProfile::GreenDetail, LumaProfile::EqualDetail}) {
        Check(f.setLumaProfile(profile), "change active profile");
        const auto& matrix = fake.effect.transform;
        Check(std::abs(matrix[0][0] + matrix[1][0] + matrix[2][0] - 1.0f) < 0.000001f, "white remains pure full red");
        for (int row = 0; row < 5; ++row) {
            Check(matrix[row][1] == 0 && matrix[row][2] == 0, "presets never emit green or blue");
        }
        Check(matrix[4][0] == 0 && matrix[3][0] == 0, "black remains black independent of alpha");
        Check(matrix[0][0] >= 0 && matrix[1][0] >= 0 && matrix[2][0] >= 0, "valid nonnegative red mapping");
        Check(matrix[3][3] == 1 && matrix[4][4] == 1, "preserve alpha and homogeneous coordinate");
    }
    const auto equal = fake.effect;
    Check(equal.transform[0][0] == equal.transform[1][0] && equal.transform[1][0] == equal.transform[2][0], "equal preset makes equally bright primaries equally visible");
    Check(f.setLumaProfile(LumaProfile::GreenDetail) && fake.effect.transform[1][0] > equal.transform[1][0] &&
        fake.effect.transform[2][0] < equal.transform[2][0], "green-detail preset favors green over blue");
}
void LumaPresetFailureAndRestore() {
    MagnificationFilter f;
    Check(f.initialize(), "initialize");
    fake.effect.transform[0][0] = 0.43f;
    const auto original = fake.effect;
    Check(f.enable(), "enable");
    fake.magSet = false;
    Check(!f.setLumaProfile(LumaProfile::EqualDetail) && f.lumaProfile() == LumaProfile::Classic && f.isActive(), "failed preset write keeps confirmed preset and active state");
    fake.magSet = true;
    Check(f.setLumaProfile(LumaProfile::EqualDetail) && f.disable() && Same(fake.effect, original), "preset changes do not overwrite restoration snapshot");
    Check(!f.setLumaProfile(static_cast<LumaProfile>(99)) && f.lumaProfile() == LumaProfile::EqualDetail, "invalid preset rejected without state change");
}
void LumaPresetInStrictModeAndGamma() {
    MagnificationFilter f;
    GammaRampFilter g;
    Check(f.initialize() && f.setRedMode(RedMode::StrictRed) && f.enable(), "enable strict");
    const auto strict = fake.effect;
    const auto writes = fake.magWrites;
    Check(f.setLumaProfile(LumaProfile::EqualDetail) && Same(fake.effect, strict) && fake.magWrites == writes, "preset preference never changes strict output");
    Check(f.setRedMode(RedMode::LumaRed) && fake.effect.transform[2][0] > 0.33f, "next luma mode uses stored profile");
    Check(!g.setLumaProfile(LumaProfile::EqualDetail) && g.redMode() == RedMode::StrictRed && fake.gammaWrites == 0, "gamma rejects cross-channel profile without display write");
}
void Arguments() {
    using S = StartupAction;
    Check(ParseArguments({}) == S::Tray, "no arguments");
    for (auto option : {L"--restore", L"--reset-display"}) {
        const std::wstring_view args[] = {option};
        Check(ParseArguments(args) == S::Restore, "restore aliases");
    }
    const std::wstring_view help[] = {L"--help"}, version[] = {L"--version"}, typo[] = {L"--restor"};
    const std::wstring_view conflict[] = {L"--restore", L"--help"};
    Check(ParseArguments(help) == S::Help && ParseArguments(version) == S::Version, "informational commands");
    Check(ParseArguments(typo) == S::Invalid && ParseArguments(conflict) == S::Invalid, "reject ambiguity before backend initialization");
}
} // namespace

DWORD GetLastError() { return 123; }
HDC GetDC(HWND) { ++fake.dcRequests; return fake.dc ? &fake : nullptr; }
int ReleaseDC(HWND, HDC) { ++fake.releases; return fake.release ? 1 : 0; }
BOOL GetDeviceGammaRamp(HDC, void* ramp) {
    if (!fake.gammaGet) return 0;
    std::memcpy(ramp, fake.ramp, sizeof(fake.ramp)); return 1;
}
BOOL SetDeviceGammaRamp(HDC, void* ramp) {
    ++fake.gammaWrites;
    auto behavior = Write::Apply;
    if (!fake.writes.empty()) { behavior = fake.writes.front(); fake.writes.pop_front(); }
    if (behavior == Write::Apply || behavior == Write::FailAfterApply || behavior == Write::LowByteLeak)
        std::memcpy(fake.ramp, ramp, sizeof(fake.ramp));
    if (behavior == Write::LowByteLeak) fake.ramp[1][100] = 1;
    return behavior == Write::Apply || behavior == Write::Ignore || behavior == Write::LowByteLeak;
}
BOOL MagInitialize() { ++fake.magInitializes; return fake.magInit; }
BOOL MagUninitialize() { ++fake.uninitializes; return fake.magUninit; }
BOOL MagGetFullscreenColorEffect(MAGCOLOREFFECT* effect) {
    if (!fake.magGet) return 0;
    *effect = fake.effect; return 1;
}
BOOL MagSetFullscreenColorEffect(MAGCOLOREFFECT* effect) {
    ++fake.magWrites;
    if (!fake.magSet) return 0;
    fake.effect = *effect; return 1;
}
namespace Diagnostics {
void Log(const char*) {}
void LogFormat(const char*, ...) {}
}
int main() {
    const std::pair<const char*, void(*)()> tests[] = {
        {"Strict matrix", StrictMatrix}, {"Luma matrix", LumaMatrix},
        {"Magnification recapture", MagCaptureEachActivation}, {"Magnification capture failure", MagCaptureFailure},
        {"Magnification initialization failures", MagInitializationFailures}, {"Magnification write failures", MagWriteFailure},
        {"Magnification shutdown retry", MagUninitializeFailure}, {"Unsupported modes", InvalidModes},
        {"Gamma output and restoration", GammaOutputAndRestore}, {"Gamma silent rejection", GammaIgnoredWrite},
        {"Gamma low-byte leakage", GammaLowByteLeak}, {"Inactive shutdown", OffShutdownDoesNotWrite},
        {"Gamma partial-write recovery", GammaPartialFailureRecovery}, {"Gamma failed disable", GammaFailedDisable},
        {"Gamma read failures", GammaReadFailures}, {"Gamma resource failures", GammaResourceFailures},
        {"Session timer boundaries", SessionTimerBoundaries},
        {"Session timer retries", SessionTimerRetryAndCancellation}, {"Session timer clock", SessionTimerChangesAndClockWrap},
        {"Backend default selection", BackendSelectionPolicies}, {"Backend strict/fallback selection", BackendSelectionStrictAndFallback},
        {"Backend cleanup failure", BackendSelectionCleanupFailure}, {"Backend command line", BackendCommandLine},
        {"Luma preset output", LumaPresetOutput}, {"Luma preset failure/restoration", LumaPresetFailureAndRestore},
        {"Luma preset strict/gamma behavior", LumaPresetInStrictModeAndGamma},
        {"Command line", Arguments}
    };
    int failed = 0;
    for (const auto& [name, run] : tests) {
        Reset();
        try { run(); std::printf("PASS %s\n", name); }
        catch (const std::exception& e) { ++failed; std::printf("FAIL %s: %s\n", name, e.what()); }
    }
    return failed ? 1 : 0;
}
