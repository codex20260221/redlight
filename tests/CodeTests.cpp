// These tests compile the production backends against fake headers and functions.
// No Windows display libraries are linked; never substitute real Win32 headers here.
#include "MagnificationFilter.h"
#include "GammaRampFilter.h"
#include "CommandLine.h"
#include "SessionTimer.h"
#include "SessionControl.h"
#include "BackendSelection.h"
#include "SupportReport.h"
#include "Preferences.h"
#include "PreferencesStore.h"
#include "PreferencesImport.h"
#include "HotkeyManager.h"
#include "CommandIds.h"
#include "MemoryFault.h"
#include "UiSafety.h"
#include "TrayStatus.h"
#include "StatusQuery.h"
#include "StandardOutput.h"
#include "StartupOptions.h"
#include "ProfileValidation.h"
#include "ProfileExport.h"
#include "JsonString.h"
#include "BackendComparison.h"
#include "SessionNotifications.h"
#include "LumaPreference.h"
#include "BuildMetadata.h"
#include "VerificationCache.h"
#include <map>
#include <algorithm>
#include "Clipboard.h"
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
struct RegistryValue { DWORD type; std::vector<char> bytes; };
struct Fake {
    bool magInit = true, magGet = true, magSet = true, magUninit = true;
    bool dc = true, gammaGet = true, release = true;
    int magWrites = 0, gammaWrites = 0, releases = 0, uninitializes = 0;
    int magInitializes = 0, dcRequests = 0, magReads = 0, gammaReads = 0;
    MAGCOLOREFFECT effect = {};
    WORD ramp[3][256] = {};
    std::deque<Write> writes;
    DWORD lastError = 123;
    bool clipAllocate = true, clipLock = true, clipOpen = true, clipEmpty = true, clipSet = true, clipClose = true;
    bool clipConvert = true, clipSecondConvert = true;
    bool clipRead = true, clipWideConvert = true;
    int clipReads = 0, clipUnlocks = 0;
    int clipAllocations = 0, clipFrees = 0, clipOpens = 0, clipEmpties = 0, clipSets = 0, clipCloses = 0, clipConversions = 0;
    std::vector<wchar_t> clipMemory;
    std::map<std::wstring, RegistryValue> registry;
    bool registryRead = true, registryWrite = true;
    int registryReads = 0, registryWrites = 0;
    std::map<int, ShortcutBinding> registeredHotkeys;
    std::deque<bool> hotkeyRegisterResults, hotkeyUnregisterResults;
    int hotkeyRegisters = 0, hotkeyUnregisters = 0;
    bool stdoutValid = true, stdoutInvalidHandle = false, consoleAttach = true, consoleFree = true, attachedOutputValid = true;
    bool stdoutWrite = true, stdoutZeroWrite = false, stdoutOverWrite = false;
    DWORD stdoutChunk = (std::numeric_limits<DWORD>::max)();
    int consoleAttaches = 0, consoleFrees = 0, stdoutLookups = 0, stdoutWrites = 0;
    std::string stdoutText;
    bool trayExists = true, statusMessage = true;
    std::uint64_t statusReply = 0;
    unsigned statusSends = 0, statusMessageId = 0, statusFlags = 0, statusTimeout = 0;
    WPARAM statusWparam = 0;
    LPARAM statusLparam = 0;
    bool mutexCreate = true, mutexRelease = true, mutexClose = true, mutexOwned = false, mutexOpen = false;
    DWORD mutexWaitResult = WAIT_OBJECT_0, mutexWaitMilliseconds = 999;
    int mutexCreates = 0, mutexWaits = 0, mutexReleases = 0, mutexCloses = 0;
    int mutexClosesWhileOwned = 0, gammaUnownedWrites = 0, gammaWritesAtMutexRelease = -1;
    const wchar_t* mutexName = nullptr;
    BOOL mutexInitialOwner = -1;
    bool argumentsRead = true, argumentsFree = true, argumentsOutstanding = false;
    int argumentsCount = 1, argumentReads = 0, argumentFrees = 0;
    std::array<LPWSTR, 8> arguments{};
    bool profileOpen = true, profileHandleOpen = false, profileRead = true, profileEofError = false, profileOverRead = false;
    bool profileClose = true, errorModeSet = true, errorModeRestore = true;
    DWORD profileType = FILE_TYPE_DISK, profileChunk = (std::numeric_limits<DWORD>::max)();
    DWORD profileTypeError = ERROR_SUCCESS;
    DWORD profileAccess = 0, profileShare = 0, profileDisposition = 0, profileFlags = 0, errorMode = 2;
    unsigned profileOpens = 0, profileReads = 0, profileCloses = 0, errorModeCalls = 0;
    DWORD profileModeAtOpen = 0;
    const wchar_t* profilePath = nullptr;
    std::size_t profilePosition = 0;
    std::string profileData;
    unsigned stdoutWhileProfileOpen = 0;
    DWORD stdoutErrorMode = 0;
    bool diagnosticsClobbersError = false;
    int stdoutFailOnCall = 0;
    bool sessionRegister = true, sessionUnregister = true, sessionRegisterThrows = false;
    DWORD sessionError = 1702, sessionFlags = 999;
    int sessionRegisters = 0, sessionUnregisters = 0;
    HWND sessionRegisteredOwner = nullptr, sessionUnregisterOwner = nullptr;
    SessionNotifications* sessionCallbackTarget = nullptr;
    bool nestedSessionEnsure = true, nestedSessionRelease = true;
} fake;

void Reset() {
    fake = {};
    fake.arguments[0] = const_cast<LPWSTR>(L"RedLight.exe");
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
    Check(fake.mutexOwned && fake.mutexReleases == 0 && fake.mutexCloses == 0, "failed partial-write rollback retains global ownership");
    const int releases = fake.releases;
    Check(!f.shutdown() && fake.releases == releases, "failed restore retains DC");
    Check(f.disable() && !f.needsRestore() && f.shutdown(), "recovery retry succeeds");
    Check(!fake.mutexOwned && !fake.mutexOpen && fake.mutexReleases == 1, "recovery retry releases ownership only after restoring the original ramp");
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
void GammaOwnershipLifecycle() {
    GammaRampFilter filter;
    Check(filter.initialize() && fake.mutexCreates == 0, "inactive initialization does not hold global ownership");
    Check(filter.enable() && filter.enable() && fake.mutexCreates == 1 && fake.mutexWaits == 1 && fake.mutexOwned,
        "activation acquires once and idempotent enable never recursively waits");
    Check(std::wstring_view(fake.mutexName) == L"Global\\RedLight.GammaRampOwner.v1" && !fake.mutexInitialOwner && fake.mutexWaitMilliseconds == 0,
        "cross-session name, explicit wait ownership, and nonblocking wait");
    Check(filter.disable() && !fake.mutexOwned && !fake.mutexOpen && fake.mutexReleases == 1 && fake.mutexCloses == 1 && fake.gammaWritesAtMutexRelease == 2,
        "ownership released only after confirmed restoration");
    Check(filter.disable() && fake.gammaWrites == 2 && fake.mutexReleases == 1, "inactive restore has no duplicate writes or releases");
    fake.ramp[0][80] = 4321;
    Check(filter.enable() && filter.disable() && fake.ramp[0][80] == 4321 && fake.mutexCreates == 2 && fake.mutexReleases == 2,
        "new activation captures current calibration under a new ownership cycle");
}
void GammaOwnershipConflictAndWaitFailure() {
    GammaRampFilter filter;
    Check(filter.initialize(), "initialize");
    const int reads = fake.gammaReads, dcRequests = fake.dcRequests;
    fake.mutexWaitResult = WAIT_TIMEOUT;
    Check(!filter.enable() && !filter.isActive() && !filter.needsRestore() && !fake.mutexOpen && fake.mutexReleases == 0,
        "busy other session does not grant or release ownership");
    Check(fake.gammaReads == reads && fake.gammaWrites == 0 && fake.dcRequests == dcRequests && std::string_view(filter.lastError()).find("another RedLight session") != std::string_view::npos,
        "conflict does not capture or write calibration and explains recovery");
    fake.mutexWaitResult = WAIT_FAILED;
    Check(!filter.enable() && !filter.needsRestore() && fake.gammaWrites == 0, "wait error stays off and closes unowned handle");
    fake.mutexWaitResult = 99;
    Check(!filter.enable() && !filter.needsRestore(), "unexpected wait response is not treated as acquired");
    fake.mutexWaitResult = WAIT_OBJECT_0;
    Check(filter.enable() && filter.disable(), "activation can retry after conflict is resolved");
}
void GammaOwnershipCreationFailure() {
    GammaRampFilter filter;
    Check(filter.initialize(), "initialize");
    const auto reads = fake.gammaReads;
    fake.mutexCreate = false;
    Check(!filter.enable() && !filter.isActive() && !filter.needsRestore() && fake.gammaReads == reads && fake.gammaWrites == 0 && fake.mutexWaits == 0,
        "access-denied/name-collision guard creation fails before calibration capture");
    fake.mutexCreate = true;
    Check(filter.enable() && filter.disable(), "create failure can retry");
}
void GammaOwnershipCaptureFailuresReleaseGuard() {
    GammaRampFilter filter;
    Check(filter.initialize(), "initialize");
    fake.release = false;
    Check(!filter.enable() && !filter.needsRestore() && !fake.mutexOwned && !fake.mutexOpen && fake.gammaWrites == 0,
        "DC release failure before capture gives up unused ownership");
    fake.release = true; fake.dc = false;
    Check(!filter.enable() && !filter.needsRestore() && !fake.mutexOwned && fake.gammaWrites == 0, "DC acquisition failure gives up unused ownership");
    fake.dc = true; fake.gammaGet = false;
    Check(!filter.enable() && !filter.needsRestore() && !fake.mutexOwned && fake.gammaWrites == 0, "capture failure gives up unused ownership");
    fake.gammaGet = true;
    Check(filter.enable() && filter.disable(), "capture can retry under new guard");
}
void GammaOwnershipRestoreFailureRetainsGuard() {
    GammaRampFilter filter;
    Check(filter.initialize() && filter.enable(), "activate");
    fake.writes = {Write::Fail, Write::Fail};
    Check(!filter.disable() && !filter.shutdown() && filter.isActive() && filter.needsRestore() && fake.mutexOwned && fake.mutexReleases == 0 && fake.mutexCloses == 0,
        "failed restoration/shutdown retain ownership, DC, and original calibration");
    Check(filter.disable() && !filter.needsRestore() && !fake.mutexOwned && fake.mutexReleases == 1, "successful retry finally releases ownership");
}
void GammaOwnershipCleanupRetryDoesNotRewrite() {
    GammaRampFilter filter;
    Check(filter.initialize() && filter.enable(), "activate");
    fake.mutexRelease = false;
    Check(!filter.disable() && !filter.isActive() && filter.needsRestore() && fake.mutexOwned && fake.mutexCloses == 0 && fake.gammaWrites == 2,
        "successful display restoration with failed ownership release remains recoverable off");
    const auto reads = fake.gammaReads;
    Check(filter.verifyState() == BackendVerification::Inactive && fake.gammaReads == reads, "cleanup-only state does not inspect another tool's calibration");
    fake.ramp[0][100] = 1234;
    Check(!filter.enable() && fake.gammaWrites == 2, "cleanup pending blocks a new capture/activation");
    fake.mutexRelease = true; fake.mutexClose = false;
    Check(!filter.disable() && filter.needsRestore() && !fake.mutexOwned && fake.mutexOpen && fake.gammaWrites == 2,
        "failed handle close retains handle after a single successful release");
    const auto releases = fake.mutexReleases;
    fake.mutexClose = true;
    Check(filter.disable() && !filter.needsRestore() && fake.mutexReleases == releases && fake.gammaWrites == 2 && fake.ramp[0][100] == 1234 && !filter.lastError()[0],
        "cleanup retry does not release twice, rewrite external calibration, or retain a stale failure");
}
void GammaOwnershipRejectedWaitCleanupRetry() {
    GammaRampFilter filter;
    Check(filter.initialize(), "initialize");
    fake.mutexWaitResult = WAIT_TIMEOUT; fake.mutexClose = false;
    Check(!filter.enable() && !filter.isActive() && filter.needsRestore() && fake.gammaWrites == 0 && fake.mutexReleases == 0,
        "failed close after busy wait remains visible and tracked");
    fake.mutexClose = true;
    Check(filter.disable() && !filter.needsRestore() && fake.gammaWrites == 0 && fake.mutexReleases == 0, "unowned cleanup retries without display writes or releasing another owner");
}
void GammaOwnershipAbandonmentRefusesCapture() {
    GammaRampFilter filter;
    Check(filter.initialize(), "initialize");
    const auto reads = fake.gammaReads;
    fake.mutexWaitResult = WAIT_ABANDONED;
    Check(!filter.enable() && !filter.needsRestore() && !fake.mutexOwned && fake.mutexReleases == 1 && fake.gammaReads == reads && fake.gammaWrites == 0,
        "abandoned ownership is released but uncertain calibration is not captured/written");
    fake.mutexWaitResult = WAIT_OBJECT_0;
    const auto creates = fake.mutexCreates;
    Check(!filter.enable() && fake.mutexCreates == creates && fake.gammaReads == reads && std::string_view(filter.lastError()).find("recover display calibration") != std::string_view::npos,
        "abandonment is latched for process lifetime, not bypassed by another enable");
    Check(filter.shutdown() && filter.initialize() && !filter.enable(), "backend reinitialization cannot clear the abandonment latch");
}
void GammaOwnershipAbandonmentCleanupFailure() {
    GammaRampFilter filter;
    Check(filter.initialize(), "initialize");
    fake.mutexWaitResult = WAIT_ABANDONED; fake.mutexRelease = false;
    Check(!filter.enable() && filter.needsRestore() && fake.mutexOwned && fake.gammaWrites == 0 && fake.mutexCloses == 0, "failed abandoned-owner release retains actual ownership");
    fake.mutexRelease = true;
    Check(filter.disable() && !filter.needsRestore() && !fake.mutexOwned && !filter.enable() && fake.gammaWrites == 0,
        "cleanup can recover without authorizing uncertain calibration capture");
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
void SupportReportStates() {
    SupportReportState state;
    state.version = "test-version";
    state.backend = "MockBackend";
    auto report = BuildSupportReport(state);
    Check(report.find("State: off") != std::string::npos && report.find("Session limit: unlimited") != std::string::npos, "inactive/default report");
    Check(report.find("Countdown: not armed") != std::string::npos && report.find("Last backend error: none") != std::string::npos, "no invented countdown or error");
    state.active = true;
    state.needsRestore = true;
    state.commandLineOverride = true;
    state.toggleHotkey = true;
    state.sessionMinutes = 15;
    state.remainingSeconds = 0;
    report = BuildSupportReport(state);
    Check(report.find("State: ON") != std::string::npos && report.find("0 seconds remaining") != std::string::npos, "expired timer distinct from absent timer");
    Check(report.find("command-line override") != std::string::npos && report.find("Toggle shortcut: registered") != std::string::npos, "expert policy and hotkey detail");
    state.lastError = "restore failed";
    report = BuildSupportReport(state);
    Check(report.find("State: Recovery needed") != std::string::npos && report.find("restore failed") != std::string::npos, "recovery status takes priority over prior active state");
    Check(report.find("no display output is measured") != std::string::npos, "report describes evidence scope");
    state.fileLogging = "Degraded"; state.droppedLogLines = 7; state.truncatedLogLines = 2; state.lastFileError = 5;
    state.startupPreferenceSource = "Defaults"; state.startupPreferenceWarning = true;
    report = BuildSupportReport(state);
    Check(report.find("Missing file records: 7") != std::string::npos && report.find("Last file error: 5") != std::string::npos, "logging failures are visible even when no disk log is available");
    Check(report.find("Startup preference source: Defaults") != std::string::npos && report.find("Startup preference load warnings: yes") != std::string::npos,
        "startup load warnings remain reportable after the initial notification");
}
void ClipboardAllocationAndOpenFailures() {
    fake.clipAllocate = false;
    Check(!CopyUtf8TextToClipboard(nullptr, "report") && fake.clipEmpties == 0, "allocation failure leaves clipboard untouched");
    fake.clipAllocate = true;
    fake.clipLock = false;
    Check(!CopyUtf8TextToClipboard(nullptr, "report") && fake.clipFrees == 1 && fake.clipOpens == 0, "lock failure frees prepared memory before opening clipboard");
    fake.clipLock = true;
    fake.clipOpen = false;
    Check(!CopyUtf8TextToClipboard(nullptr, "report") && fake.clipFrees == 2 && fake.clipEmpties == 0, "busy clipboard preserves existing contents and frees memory");
}
void ClipboardTransferOwnership() {
    fake.clipEmpty = false;
    Check(!CopyUtf8TextToClipboard(nullptr, "report") && fake.clipFrees == 1 && fake.clipCloses == 1 && fake.clipSets == 0, "empty failure closes and frees memory");
    fake.clipEmpty = true;
    fake.clipSet = false;
    Check(!CopyUtf8TextToClipboard(nullptr, "report") && fake.clipFrees == 2 && fake.clipCloses == 2, "transfer failure retains ownership for cleanup");
    fake.clipSet = true;
    Check(CopyUtf8TextToClipboard(nullptr, "report") && fake.clipFrees == 2 && fake.clipCloses == 3, "successful transfer relinquishes memory to Windows");
    Check(std::wstring(fake.clipMemory.data()) == L"report", "clipboard buffer is terminated UTF-16 text");
    fake.clipClose = false;
    Check(!CopyUtf8TextToClipboard(nullptr, "report") && fake.clipFrees == 2, "close failure reported without freeing Windows-owned memory");
}
void ClipboardConversionFailure() {
    fake.clipConvert = false;
    Check(!CopyUtf8TextToClipboard(nullptr, "report") && fake.clipAllocations == 0 && fake.clipOpens == 0, "sizing/conversion failure touches no clipboard or allocation");
    fake.clipConvert = true;
    fake.clipConversions = 0;
    fake.clipSecondConvert = false;
    Check(!CopyUtf8TextToClipboard(nullptr, "report") && fake.clipFrees == 1 && fake.clipOpens == 0, "second conversion failure cleans up before clipboard mutation");
    Check(!CopyUtf8TextToClipboard(nullptr, "") && fake.clipOpens == 0, "empty reports rejected without mutation");
}
void PreferenceProfileRoundTrip() {
    for (const auto mode : {RedMode::StrictRed, RedMode::LumaRed})
    for (const auto luma : {LumaProfile::Classic, LumaProfile::GreenDetail, LumaProfile::EqualDetail})
    for (const auto backend : {BackendPreference::Automatic, BackendPreference::Magnification, BackendPreference::Gamma})
    for (const unsigned minutes : {0u, 1u, 15u, 240u})
    for (const auto shortcuts : {ShortcutProfile::Default, ShortcutProfile::F9, ShortcutProfile::F10, ShortcutProfile::Disabled})
    for (const auto warning : ExpiryWarningOptions) {
        const AppPreferences original{mode, luma, backend, minutes, shortcuts, warning};
        const auto text = SerializePreferences(original);
        Check(text.has_value(), "serialize supported preferences");
        const auto parsed = ParsePreferences(*text);
        Check(parsed.preferences == original && !parsed.error, "all supported combinations round-trip");
    }
    auto bad = AppPreferences{};
    bad.sessionMinutes = 241;
    Check(!SerializePreferences(bad), "never export an out-of-range session");
    bad = {};
    bad.luma = static_cast<LumaProfile>(99);
    Check(!SerializePreferences(bad), "never export an invalid preset");
}
void PreferenceProfileStrictValidation() {
    const auto text = *SerializePreferences(AppPreferences{});
    Check(text.find("version=3") != std::string::npos && text.find("mode=luma") != std::string::npos && text.find("backend=auto") != std::string::npos, "stable human-readable versioned format");
    Check(!ParsePreferences(text + "mode=strict\n").preferences, "duplicate fields rejected");
    Check(!ParsePreferences(text + "mdoe=strict\n").preferences, "unknown fields rejected instead of ignored");
    Check(!ParsePreferences(text + "[RedLightProfile]\n").preferences, "duplicate sections rejected");
    for (const auto replacement : {"session_minutes=-1", "session_minutes=241", "session_minutes=42949672960", "session_minutes=1x", "session_minutes="}) {
        auto bad = text;
        const auto at = bad.find("session_minutes=0");
        bad.replace(at, std::strlen("session_minutes=0"), replacement);
        const auto parsed = ParsePreferences(bad);
        Check(!parsed.preferences && parsed.error && parsed.line > 0, "invalid numeric input has a line diagnostic");
    }
    auto newer = text;
    newer.replace(newer.find("version=3"), 9, "version=999");
    Check(!ParsePreferences(newer).preferences, "future schemas cannot silently reinterpret settings");
    Check(!ParsePreferences("[RedLightProfile]\nversion=1\n").preferences, "missing settings rejected");
    Check(!ParsePreferences(std::string(4097, ' ')).preferences, "bounded input size");
    auto nul = text; nul.push_back('\0');
    Check(!ParsePreferences(nul).preferences, "embedded NUL rejected");
}
void PreferenceProfileWhitespaceAndOrder() {
    const std::string text = "# profile\n [RedLightProfile] \n; note\n backend = gamma \n session_minutes = 15 \n luma = equal \n mode = strict \n version = 1 \n";
    const auto parsed = ParsePreferences(text);
    Check(parsed.preferences == AppPreferences{RedMode::StrictRed, LumaProfile::EqualDetail, BackendPreference::Gamma, 15}, "comments, whitespace and field order accepted");
    const std::string bom{static_cast<char>(0xef), static_cast<char>(0xbb), static_cast<char>(0xbf)};
    Check(ParsePreferences(bom + text).preferences == parsed.preferences, "UTF-8 BOM accepted");
}
void PutLegacy(const wchar_t* name, DWORD value) {
    std::vector<char> bytes(sizeof(value));
    std::memcpy(bytes.data(), &value, sizeof(value));
    fake.registry[name] = {REG_DWORD, bytes};
}
void PreferencesStoreDefaultsAndMigration() {
    const auto initial = LoadPreferences();
    Check(initial.preferences == AppPreferences{} && initial.source == PreferenceSource::Defaults && !initial.warning && fake.registryWrites == 0, "defaults require no writes and report their source");
    PutLegacy(L"RedMode", 0);
    PutLegacy(L"LumaProfile", 2);
    PutLegacy(L"BackendPreference", 2);
    PutLegacy(L"SessionLimitMinutes", 30);
    const AppPreferences expected{RedMode::StrictRed, LumaProfile::EqualDetail, BackendPreference::Gamma, 30};
    const auto loaded = LoadPreferences();
    Check(loaded.preferences == expected && loaded.legacy && loaded.source == PreferenceSource::LegacyValues && !loaded.warning && fake.registryWrites == 0, "read-only migration preserves all legacy choices and source");
    Check(SavePreferences(loaded.preferences) && fake.registryWrites == 1, "one save writes one record");
    PutLegacy(L"RedMode", 1);
    const auto stored = LoadPreferences();
    Check(stored.preferences == expected && !stored.legacy && stored.source == PreferenceSource::VersionedRecord, "new record takes precedence over stale legacy keys and reports its source");
}
void PreferencesStoreFailureAtomicity() {
    const AppPreferences before{RedMode::StrictRed, LumaProfile::Classic, BackendPreference::Magnification, 15};
    Check(SavePreferences(before), "seed saved profile");
    auto after = before; after.luma = LumaProfile::EqualDetail; after.sessionMinutes = 60;
    fake.registryWrite = false;
    Check(!SavePreferences(after), "failed registry write is reported");
    Check(LoadPreferences().preferences == before, "failed save cannot partially persist individual settings");
    fake.registryWrite = true;
    Check(SavePreferences(after) && LoadPreferences().preferences == after, "retrying the same preferences after storage recovery saves the complete record");
    after.sessionMinutes = 999;
    const int writes = fake.registryWrites;
    Check(!SavePreferences(after) && fake.registryWrites == writes, "invalid preferences never reach registry");
}
void PreferencesStoreCorruption() {
    PutLegacy(L"RedMode", 0);
    fake.registry[L"SettingsProfile"] = {REG_BINARY, {'b', 'a', 'd'}};
    auto loaded = LoadPreferences();
    Check(loaded.warning && loaded.preferences == AppPreferences{} && !loaded.legacy && loaded.source == PreferenceSource::Defaults, "corrupt new profile reports warned defaults and never falls back to stale legacy values");
    fake.registry[L"SettingsProfile"] = {REG_BINARY, std::vector<char>(4097, 'x')};
    Check(LoadPreferences().warning, "oversized persisted record rejected");
    fake.registryRead = false;
    Check(LoadPreferences().warning && fake.registryWrites == 0, "read failure is visible and never writes defaults");
}
void PreferencesStoreInvalidLegacy() {
    PutLegacy(L"RedMode", 99);
    PutLegacy(L"SessionLimitMinutes", 241);
    PutLegacy(L"BackendPreference", 0);
    const auto loaded = LoadPreferences();
    Check(loaded.warning && loaded.preferences == AppPreferences{} && loaded.source == PreferenceSource::LegacyValues, "invalid legacy settings use per-field defaults and keep origin/warning diagnostics");
}
void ClipboardProfileRead() {
    const auto text = *SerializePreferences(AppPreferences{});
    fake.clipMemory.assign(text.begin(), text.end()); fake.clipMemory.push_back(0);
    const auto read = ReadUtf8TextFromClipboard(nullptr);
    Check(read == text && ParsePreferences(*read).preferences == AppPreferences{}, "profile read and parser integration");
    Check(fake.clipOpens == 1 && fake.clipCloses == 1 && fake.clipUnlocks == 1, "read releases clipboard and memory lock");
    Check(fake.clipEmpties == 0 && fake.clipSets == 0 && fake.clipFrees == 0, "reading never mutates or frees clipboard contents");
}
void ClipboardReadBoundsAndFailures() {
    fake.clipMemory.assign(4098, L'x'); fake.clipMemory.back() = 0;
    Check(!ReadUtf8TextFromClipboard(nullptr) && fake.clipCloses == 1, "oversized input rejected with clipboard closed");
    fake.clipMemory.assign(10, L'x');
    Check(!ReadUtf8TextFromClipboard(nullptr) && fake.clipCloses == 2, "unterminated UTF-16 rejected with bounded scan");
    fake.clipMemory = {L'a', 0}; fake.clipWideConvert = false;
    Check(!ReadUtf8TextFromClipboard(nullptr) && fake.clipCloses == 3, "conversion failure closes clipboard");
    fake.clipWideConvert = true; fake.clipRead = false;
    Check(!ReadUtf8TextFromClipboard(nullptr) && fake.clipCloses == 4, "missing text handle closes clipboard");
    fake.clipRead = true; fake.clipClose = false;
    Check(!ReadUtf8TextFromClipboard(nullptr), "read does not claim success after close failure");
    Check(fake.clipEmpties == 0 && fake.clipFrees == 0, "failed reads do not mutate data or free Windows-owned handles");
}
void PreferencesImportRecoveryGate() {
    MagnificationFilter filter;
    Check(filter.initialize() && filter.enable(), "active filter");
    const AppPreferences original;
    const AppPreferences candidate{RedMode::StrictRed, LumaProfile::EqualDetail, BackendPreference::Gamma, 60, ShortcutProfile::Default, ExpiryWarning::OneMinute};
    Check(SavePreferences(original), "seed preferences");
    const int writes = fake.registryWrites;
    fake.magSet = false;
    auto imported = SaveImportedPreferences(candidate, [&] { return filter.disable(); });
    Check(imported == PreferenceImportResult::RestoreFailed && fake.registryWrites == writes && filter.isActive(), "failed restoration blocks persistence and retains recovery state");
    fake.magSet = true; fake.registryWrite = false;
    imported = SaveImportedPreferences(candidate, [&] { return filter.disable(); });
    Check(imported == PreferenceImportResult::SaveFailed && !filter.isActive() && LoadPreferences().preferences == original, "save failure leaves display restored and old preferences intact");
    fake.registryWrite = true;
    Check(SaveImportedPreferences(candidate, [&] { return filter.disable(); }) == PreferenceImportResult::Saved &&
        !filter.isActive() && LoadPreferences().preferences == candidate, "successful import saves one complete record without activation");
}
void PreferencesImportValidationGate() {
    auto candidate = AppPreferences{}; candidate.sessionMinutes = 999;
    bool restoreCalled = false;
    const auto result = SaveImportedPreferences(candidate, [&] { restoreCalled = true; return true; });
    Check(result == PreferenceImportResult::Rejected && !restoreCalled && fake.registryWrites == 0, "invalid import never reaches display restoration or storage");
}
void MagnificationVerificationReadOnly() {
    MagnificationFilter f;
    Check(f.verifyState() == BackendVerification::Unavailable && fake.magReads == 0, "uninitialized verification has no API reads");
    Check(f.initialize(), "initialize");
    const int reads = fake.magReads;
    Check(f.verifyState() == BackendVerification::Inactive && fake.magReads == reads, "inactive verification does not inspect another tool's display state");
    Check(f.enable(), "enable");
    const int writes = fake.magWrites;
    Check(f.verifyState() == BackendVerification::Matches, "matrix read-back matches");
    fake.effect.transform[1][1] = 0.000001f;
    Check(f.verifyState() == BackendVerification::Changed && f.isActive() && f.needsRestore(), "any nonzero green output detected without discarding recovery state");
    fake.magGet = false;
    Check(f.verifyState() == BackendVerification::Unavailable && fake.magWrites == writes, "read failure never reapplies or restores matrix");
    fake.magGet = true;
}
void MagnificationVerificationFiniteValues() {
    MagnificationFilter f;
    Check(f.initialize() && f.enable(), "enable");
    fake.effect.transform[0][0] = (std::numeric_limits<float>::quiet_NaN)();
    Check(f.verifyState() == BackendVerification::Changed, "NaN cannot pass a tolerance check");
    fake.effect.transform[0][0] = (std::numeric_limits<float>::infinity)();
    Check(f.verifyState() == BackendVerification::Changed, "infinite matrix value rejected");
}
void GammaVerificationReadOnly() {
    GammaRampFilter f;
    Check(f.initialize(), "initialize");
    const int reads = fake.gammaReads;
    Check(f.verifyState() == BackendVerification::Inactive && fake.gammaReads == reads, "inactive gamma check does not inspect display");
    Check(f.enable(), "enable");
    const int writes = fake.gammaWrites;
    Check(f.verifyState() == BackendVerification::Matches, "gamma read-back matches");
    fake.ramp[2][200] = 1;
    Check(f.verifyState() == BackendVerification::Changed && f.isActive() && f.needsRestore(), "even low-byte blue leakage detected with recovery data retained");
    fake.gammaGet = false;
    Check(f.verifyState() == BackendVerification::Unavailable && fake.gammaWrites == writes, "gamma read failure writes nothing");
    fake.gammaGet = true;
}
void VerificationStatusPriority() {
    SupportReportState state;
    state.active = true;
    state.needsRestore = true;
    state.verification = BackendVerification::Changed;
    Check(BuildSupportReport(state).find("State: Backend changed") != std::string::npos, "different read-back cannot be presented as confirmed ON");
    state.verification = BackendVerification::Unavailable;
    Check(BuildSupportReport(state).find("State: Verification unavailable") != std::string::npos, "unavailable read-back is visible");
    state.lastError = "restore failed";
    Check(BuildSupportReport(state).find("State: Recovery needed") != std::string::npos, "failed restoration remains highest priority");
    state.lastError = {}; state.verification = BackendVerification::Matches; state.restoreRequested = true;
    const auto pending = BuildSupportReport(state);
    Check(pending.find("State: Recovery needed") != std::string::npos && pending.find("Control restore requested: yes") != std::string::npos,
        "queued control restoration outranks an earlier healthy active state");
    Check(std::string_view(FilterStatusDescription(false, true, false, BackendVerification::NotChecked)) == "Recovery needed",
        "inactive pending ownership/restore state cannot be reported simply off");
}
void CustomSessionLimitParsing() {
    for (const unsigned minutes : {0u, 1u, 15u, 57u, 239u, 240u}) {
        Check(ParseSessionMinutes(std::to_wstring(minutes)) == minutes, "custom duration boundary and arbitrary minute values");
    }
    Check(ParseSessionMinutes(L"  00057\t") == 57u, "padding and leading zeros");
    for (const auto text : {L"", L" ", L"-1", L"+1", L"1.5", L"241", L"42949672960", L"15x", L"1 5", L"１"}) {
        Check(!ParseSessionMinutes(text), "invalid/overflow/non-ASCII custom duration rejected");
    }
    SessionTimer timer;
    Check(timer.configure(*ParseSessionMinutes(L"57"), 1000, true) &&
        timer.remainingMilliseconds(1000) == 3420000, "custom limit uses the same tested session timer");
}
void ShortcutRegistrationAndStaleMessages() {
    HotkeyManager manager;
    Check(manager.initialize(nullptr, ShortcutProfile::Default) && fake.registeredHotkeys.size() == 2, "initial shortcuts registered independently");
    Check(manager.accepts(ID_HOTKEY_TOGGLE, 3, 'R') && manager.accepts(ID_HOTKEY_RESTORE, 7, 'R'), "only actual bindings accepted");
    Check(!manager.accepts(ID_HOTKEY_TOGGLE, 7, 'R') && !manager.accepts(99, 3, 'R'), "wrong modifiers/IDs rejected");
    Check(manager.change(ShortcutProfile::F9) && manager.toggle() == ShortcutBinding{3, 0x78}, "alternate shortcut profile registered");
    Check(!manager.accepts(ID_HOTKEY_TOGGLE, 3, 'R') && manager.accepts(ID_HOTKEY_TOGGLE, 3 | MOD_NOREPEAT, 0x78), "queued old hotkeys ignored after change");
    const int registrations = fake.hotkeyRegisters;
    Check(manager.change(ShortcutProfile::F9) && fake.hotkeyRegisters == registrations, "idempotent profile does not duplicate registrations");
    Check(manager.change(ShortcutProfile::Disabled) && !manager.toggle() && !manager.restore() && fake.registeredHotkeys.empty(), "disable unregisters both shortcuts");
    Check(!manager.accepts(ID_HOTKEY_TOGGLE, 3, 0x78) && manager.release(), "disabled profile ignores queued shortcuts");
}
void ShortcutRegistrationRollback() {
    HotkeyManager manager;
    Check(manager.initialize(nullptr, ShortcutProfile::Default), "initial profile");
    fake.hotkeyRegisterResults = {true, false, true, true};
    Check(!manager.change(ShortcutProfile::F10), "reject incomplete new profile");
    Check(manager.toggle() == ShortcutBinding{3, 'R'} && manager.restore() == ShortcutBinding{7, 'R'}, "both previous actual bindings restored");
    Check(fake.registeredHotkeys.at(ID_HOTKEY_TOGGLE) == *manager.toggle() && fake.registeredHotkeys.at(ID_HOTKEY_RESTORE) == *manager.restore(), "reported registrations match fake OS after rollback");
    Check(manager.release(), "release registered shortcuts");
}
void ShortcutPartialRollbackAndRetry() {
    HotkeyManager manager;
    Check(manager.initialize(nullptr, ShortcutProfile::Default), "initial profile");
    fake.hotkeyRegisterResults = {true, false, true};
    fake.hotkeyUnregisterResults = {true, true, false};
    Check(!manager.change(ShortcutProfile::F9), "registration and rollback can both fail");
    Check(manager.toggle() == ShortcutBinding{3, 0x78} && manager.restore() == ShortcutBinding{7, 'R'}, "partial rollback reports actual mixed bindings without inventing prior state");
    Check(fake.registeredHotkeys.at(ID_HOTKEY_TOGGLE) == *manager.toggle() && fake.registeredHotkeys.at(ID_HOTKEY_RESTORE) == *manager.restore(), "actual and reported state agree after rollback failure");
    Check(manager.change(ShortcutProfile::Default) && manager.toggle() == ShortcutBinding{3, 'R'}, "retry can recover the prior profile");
    fake.hotkeyUnregisterResults = {false, true};
    Check(!manager.release() && manager.toggle() && !manager.restore(), "failed unregister keeps actual registration and still releases other shortcut");
    Check(manager.release() && fake.registeredHotkeys.empty(), "release failure retry");
}
void ShortcutStartupPartialAndSchemaMigration() {
    HotkeyManager manager;
    fake.hotkeyRegisterResults = {false, true};
    Check(!manager.initialize(nullptr, ShortcutProfile::Default) && !manager.toggle() && manager.restore(), "startup conflict retains available restore shortcut");
    Check(manager.release(), "cleanup partial startup");
    HotkeyManager disabled;
    const int calls = fake.hotkeyRegisters;
    Check(disabled.initialize(nullptr, ShortcutProfile::Disabled) && fake.hotkeyRegisters == calls, "disabled startup registers nothing");
    Check(!disabled.change(static_cast<ShortcutProfile>(99)), "invalid shortcut profile rejected");
    const auto old = ParsePreferences("[RedLightProfile]\nversion=1\nmode=luma\nluma=classic\nbackend=auto\nsession_minutes=0\n");
    Check(old.preferences && old.preferences->shortcuts == ShortcutProfile::Default, "v1 profile retains default shortcuts");
    auto current = *SerializePreferences(AppPreferences{});
    current.replace(current.find("hotkeys=default"), 15, "hotkeys=unexpected");
    Check(!ParsePreferences(current).preferences, "invalid shortcut token rejected");
}
void DefaultPreferencesRestoreActiveGamma() {
    GammaRampFilter filter;
    Check(filter.initialize() && filter.enable(), "active gamma fallback");
    const AppPreferences before{RedMode::StrictRed, LumaProfile::EqualDetail, BackendPreference::Gamma, 57, ShortcutProfile::F9, ExpiryWarning::FiveMinutes};
    Check(SavePreferences(before), "seed expert settings");
    const auto reset = SaveImportedPreferences(AppPreferences{}, [&] { return filter.disable(); });
    Check(reset == PreferenceImportResult::Saved && !filter.isActive() && !filter.needsRestore(), "default reset restores active filter and leaves it off");
    Check(LoadPreferences().preferences == AppPreferences{}, "default reset replaces complete configuration including shortcut/timer choices");
    Check(fake.gammaWrites == 2, "reset never enables a filter after restoration");
}
void PreferenceSaveAllocationFailure() {
    MagnificationFilter filter;
    Check(filter.initialize() && filter.enable(), "active filter");
    const AppPreferences before;
    Check(SavePreferences(before), "seed saved preferences");
    auto candidate = before; candidate.sessionMinutes = 60;
    const int writes = fake.registryWrites;
    bool saved = true, faultConsumed = false;
    {
        TestMemory::FailNextAllocation fail;
        saved = SavePreferences(candidate);
        faultConsumed = !TestMemory::AllocationFailurePending();
    }
    Check(!saved && faultConsumed && fake.registryWrites == writes, "allocation failure returns false before any storage write");
    Check(LoadPreferences().preferences == before && filter.isActive() && filter.needsRestore(), "previous configuration and active filter ownership retained");
    const auto reads = fake.magReads, displayWrites = fake.magWrites;
    Check(SavePreferences(candidate) && LoadPreferences().preferences == candidate && filter.isActive() && filter.needsRestore() && fake.magReads == reads && fake.magWrites == displayWrites,
        "explicit retry persists current choices without restarting or restoring active filtering");
    Check(!CommandMayChangeFilter(ID_TRAY_SAVE_PREFERENCES), "save-retry failures retain read-only display recovery policy");
}
void PreferenceImportAllocationFailure() {
    MagnificationFilter filter;
    Check(filter.initialize() && filter.enable() && SavePreferences(AppPreferences{}), "active filter and saved preferences");
    auto candidate = AppPreferences{}; candidate.sessionMinutes = 57;
    const int writes = fake.registryWrites;
    auto result = PreferenceImportResult::Saved;
    {
        TestMemory::FailNextAllocation fail;
        result = SaveImportedPreferences(candidate, [&] { return filter.disable(); });
    }
    Check(result == PreferenceImportResult::SaveFailed && !filter.isActive() && !filter.needsRestore(), "failed allocation after restoration remains an ordinary import failure with display off");
    Check(fake.registryWrites == writes && LoadPreferences().preferences == AppPreferences{}, "import allocation failure preserves old stored preferences");
}
void ReadOnlyUiFailureRetainsFilter() {
    MagnificationFilter filter;
    Check(filter.initialize() && filter.enable(), "active filter");
    SupportReportState state;
    bool caught = false;
    const long result = GuardUiCall<long>([&] {
        TestMemory::FailNextAllocation fail;
        const auto report = BuildSupportReport(state);
        return CopyUtf8TextToClipboard(nullptr, report) ? 1L : 2L;
    }, [&](const char*) {
        caught = true;
        if (RecoveryAfterUiFailure(CommandMayChangeFilter(ID_TRAY_COPY_REPORT), false, false) == UiFailureRecovery::Restore) filter.disable();
        return 0L;
    });
    Check(result == 0 && caught && filter.isActive() && filter.needsRestore() && fake.magWrites == 1 && fake.clipOpens == 0,
        "read-only report allocation failure preserves filter and clipboard");
}
void MutatingUiFailureRestoresAfterUnwind() {
    MagnificationFilter filter;
    Check(filter.initialize(), "initialize");
    const auto original = fake.effect;
    bool busy = false, sawUnwind = false;
    const long result = GuardUiCall<long>([&]() -> long {
        ScopedFlag call(busy);
        Check(filter.enable(), "enable inside action");
        throw std::runtime_error("injected action failure");
    }, [&](const char*) {
        sawUnwind = !busy;
        return RecoveryAfterUiFailure(CommandMayChangeFilter(ID_TRAY_TOGGLE), false, busy) == UiFailureRecovery::Restore && filter.disable() ? 0L : -1L;
    });
    Check(result == 0 && sawUnwind && !busy && !filter.isActive() && Same(fake.effect, original),
        "busy flag unwinds before failure recovery restores original display state");
}
void UiRecoveryFailureKeepsSnapshot() {
    MagnificationFilter filter;
    Check(filter.initialize() && filter.enable(), "active filter");
    const auto writes = fake.magWrites;
    fake.magSet = false;
    const long result = GuardUiCall<long>([]() -> long { throw 42; }, [&](const char*) {
        return filter.disable() ? 0L : -1L;
    });
    Check(result == -1 && filter.isActive() && filter.needsRestore() && fake.magWrites == writes + 1,
        "failed emergency restore stays visible and retains recovery state");
    fake.magSet = true;
    Check(filter.disable(), "later restore retry remains possible");
}
void NestedUiFlagsAndFailureHandler() {
    bool busy = false, deferred = false;
    {
        ScopedFlag outer(busy);
        GuardUiCall<long>([&]() -> long { ScopedFlag inner(busy); throw 7; }, [&](const char*) {
            deferred = busy;
            return 0L;
        });
        Check(busy && deferred, "inner unwind preserves outer call ownership for deferred recovery");
    }
    Check(!busy, "outer scope restores prior state");
    const auto result = GuardUiCall<long>([]() -> long { throw 1; }, [](const char*) -> long { throw 2; });
    Check(result == 0, "failure-handler exceptions cannot cross native callback boundary");
    const auto failedCreate = GuardUiCall<LRESULT>([]() -> LRESULT { throw std::runtime_error("action"); }, [](const char*) -> LRESULT { throw 2; }, LRESULT{-1});
    Check(failedCreate == -1, "native creation failure fallback cannot accidentally become success when its error handler also fails");
    Check(!CommandMayChangeFilter(ID_TRAY_COPY_PROFILE) && !CommandMayChangeFilter(ID_TRAY_LOGS) &&
        CommandMayChangeFilter(ID_TRAY_IMPORT_PROFILE) && CommandMayChangeFilter(ID_TRAY_RESET_PREFERENCES), "display recovery policy distinguishes read-only tools from mutating actions");
}
void SupportReportAllocationFailuresNeverReturnPartial() {
    const std::string longError(8192, 'e');
    SupportReportState state; state.lastError = longError;
    unsigned triggered = 0;
    for (unsigned skip = 0; skip < 9; ++skip) {
        bool caught = false, consumed = false;
        std::optional<std::string> report;
        {
            TestMemory::FailNextAllocation fail(skip);
            GuardUiCall<long>([&] { report = BuildSupportReport(state); return 1L; }, [&](const char*) { caught = true; return 0L; });
            consumed = !TestMemory::AllocationFailurePending();
        }
        if (consumed) {
            ++triggered;
            Check(caught && !report, "allocation failure during report growth never returns a partial report as success");
        } else Check(report && report->size() > longError.size(), "successful report is complete");
    }
    Check(triggered > 1, "faults covered construction and subsequent report allocations");
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
    const std::wstring_view status[] = {L"--status"}, statusConflict[] = {L"--status", L"--backend=gamma"};
    Check(ParseArguments(status) == S::Status && ParseArguments(statusConflict) == S::Invalid && ParseStartupOptions(statusConflict).errorOutput == StartupErrorOutput::Json, "read-only status accepts no startup override and invalid combinations keep JSON error policy");
    Check(ParseArguments(typo) == S::Invalid && ParseArguments(conflict) == S::Invalid, "reject ambiguity before backend initialization");
}
void StatusProtocolRoundTrip() {
    TrayStatus state;
    const auto initial = PackTrayStatus(state);
    Check(initial && UnpackTrayStatus(*initial) == state, "default state round trips");
    state.backendActive = true; state.restorePending = true; state.backendError = true;
    state.luma = LumaProfile::EqualDetail; state.verification = BackendVerification::Unavailable;
    state.sessionMinutes = 240; state.timerArmed = true; state.remainingSeconds = 14400;
    state.shortcutProfile = ShortcutProfile::Disabled; state.toggle = StatusShortcut::F9; state.restore = StatusShortcut::F10;
    state.settingsUnsaved = true; state.sessionNotifications = true; state.busy = true;
    state.fileLogging = Diagnostics::FileState::Unavailable; state.missingFileRecords = true; state.truncatedRecords = true; state.fileError = true;
    const auto maximum = PackTrayStatus(state);
    Check(maximum && UnpackTrayStatus(*maximum) == state && *maximum <= static_cast<std::uint64_t>((std::numeric_limits<LRESULT>::max)()), "maximum fields preserve all bits in positive x64 reply");
    for (unsigned backend = 0; backend < 2; ++backend) for (unsigned verification = 0; verification < 5; ++verification)
        for (unsigned shortcut = 0; shortcut < 4; ++shortcut) {
            state.backend = static_cast<StatusBackend>(backend); state.supportsLuma = backend == 0;
            state.mode = backend == 0 ? RedMode::LumaRed : RedMode::StrictRed;
            state.verification = static_cast<BackendVerification>(verification); state.shortcutProfile = static_cast<ShortcutProfile>(shortcut);
            const auto word = PackTrayStatus(state);
            Check(word && UnpackTrayStatus(*word) == state, "backend/verification/shortcut combinations round trip");
        }
    Check(fake.magReads == 0 && fake.gammaReads == 0 && fake.magWrites == 0 && fake.gammaWrites == 0, "protocol never accesses display APIs");
}
void StatusProtocolRejectsInvalid() {
    const auto valid = *PackTrayStatus(TrayStatus{});
    Check(!UnpackTrayStatus(0) && !UnpackTrayStatus(valid ^ (1ull << 48)), "reject missing and future protocol tags");
    Check(!UnpackTrayStatus(valid | (3ull << 6)) && !UnpackTrayStatus(valid | (7ull << 8)), "reject reserved luma and verification values");
    Check(!UnpackTrayStatus(valid | (255ull << 11)) && !UnpackTrayStatus(valid | (1ull << 20)), "reject invalid duration and unarmed countdown");
    Check(!UnpackTrayStatus(valid | 1) && !UnpackTrayStatus(valid | (1ull << 19)), "reject active without recovery ownership and timer without limit");
    TrayStatus state; state.backend = StatusBackend::Gamma;
    Check(!PackTrayStatus(state) && !TrayStatusJson(state), "gamma cannot claim luma support/mode");
    state.supportsLuma = false; state.mode = RedMode::StrictRed;
    Check(PackTrayStatus(state).has_value(), "strict gamma accepted");
    state.sessionMinutes = 1; state.timerArmed = true; state.remainingSeconds = 61;
    Check(!PackTrayStatus(state), "countdown cannot exceed configured duration");
    state.remainingSeconds = 0;
    Check(PackTrayStatus(state).has_value(), "expired session can remain armed for restore retries");
    state.backend = static_cast<StatusBackend>(2);
    Check(!PackTrayStatus(state), "out-of-range enum cannot overlap packed fields");
}
void StatusJsonConservativeStates() {
    TrayStatus state;
    auto json = *TrayStatusJson(state);
    Check(json.starts_with("{\n  \"protocol\":1,") && json.ends_with("}\n") && json.find("\"remaining_seconds\":null") != std::string::npos, "off response is complete JSON with null countdown");
    state.backendActive = true; state.restorePending = true;
    Check(std::string_view(TrayStatusToken(state)) == "on" && TrayStatusJson(state)->find("\"active\":true") != std::string::npos, "confirmed active state reports on");
    state.verification = BackendVerification::Changed;
    json = *TrayStatusJson(state);
    Check(std::string_view(TrayStatusToken(state)) == "backend-changed" && json.find("\"active\":false") != std::string::npos &&
        json.find("\"backend_active\":true") != std::string::npos && json.find("\"verification\":\"changed\"") != std::string::npos, "mismatch distinguishes last confirmed activation from healthy on");
    state.verification = BackendVerification::Unavailable;
    Check(std::string_view(TrayStatusToken(state)) == "verification-unavailable", "unavailable verification visible");
    state.backendError = true;
    Check(std::string_view(TrayStatusToken(state)) == "recovery-needed", "restoration failure has priority");
    state.backendError = false; state.backendActive = false;
    Check(std::string_view(TrayStatusToken(state)) == "recovery-needed", "partial activation with recovery data has priority");
    state.busy = true;
    Check(std::string_view(TrayStatusToken(state)) == "busy" && TrayStatusJson(state)->find("\"active\":false") != std::string::npos, "transient state is conservative");
    state.sessionMinutes = 2; state.timerArmed = true; state.remainingSeconds = 119;
    state.toggle = StatusShortcut::F9; state.restore = StatusShortcut::F10;
    json = *TrayStatusJson(state);
    Check(json.find("\"remaining_seconds\":119") != std::string::npos && json.find("Ctrl+Alt+F9") != std::string::npos && json.find("Ctrl+Alt+Shift+F10") != std::string::npos, "countdown and actual registrations are distinct fields");
    Check(StatusShortcutKey(ShortcutBinding{3, 'R'}) == StatusShortcut::R && StatusShortcutKey(std::nullopt) == StatusShortcut::None, "actual binding mapping");
}
void StandardOutputOwnership() {
    fake.stdoutChunk = 2;
    Check(WriteStandardOutput("abcde\n") && fake.stdoutText == "abcde\n" && fake.stdoutWrites == 3 && fake.consoleAttaches == 0 && fake.consoleFrees == 0, "partial pipe writes finish without touching console ownership");
    Reset(); fake.stdoutValid = false;
    Check(WriteStandardOutput("parent\n") && fake.stdoutText == "parent\n" && fake.consoleAttaches == 1 && fake.consoleFrees == 1 && fake.stdoutLookups == 2, "only attached parent console is detached");
    Reset(); fake.stdoutValid = false; fake.stdoutInvalidHandle = true;
    Check(WriteStandardOutput("x") && fake.consoleAttaches == 1, "invalid inherited handle follows parent fallback");
}
void StandardOutputFailures() {
    fake.stdoutValid = false; fake.consoleAttach = false;
    Check(!WriteStandardOutput("x") && fake.stdoutWrites == 0 && fake.consoleFrees == 0, "failed attach never writes or detaches");
    Reset(); fake.stdoutValid = false; fake.attachedOutputValid = false;
    Check(!WriteStandardOutput("x") && fake.consoleFrees == 1 && fake.stdoutWrites == 0, "bad attached output still releases owned console");
    Reset(); fake.stdoutWrite = false;
    Check(!WriteStandardOutput("x") && fake.stdoutText.empty(), "write failure visible");
    Reset(); fake.stdoutZeroWrite = true;
    Check(!WriteStandardOutput("x") && fake.stdoutWrites == 1, "zero progress stops instead of looping");
    Reset(); fake.stdoutOverWrite = true;
    Check(!WriteStandardOutput("x"), "invalid byte count rejected");
    Reset(); fake.stdoutValid = false; fake.stdoutWrite = false;
    Check(!WriteStandardOutput("x") && fake.consoleFrees == 1, "write failure still detaches owned console");
    Reset(); fake.stdoutValid = false; fake.consoleFree = false;
    Check(!WriteStandardOutput("x") && fake.stdoutText == "x", "detach failure is not hidden");
}
void StatusQueryExistingInstanceOnly() {
    TrayStatus state; state.backendActive = true; state.restorePending = true;
    fake.statusReply = *PackTrayStatus(state);
    Check(QueryRunningStatus() == 0 && fake.stdoutText == *TrayStatusJson(state), "successful reply formats the existing instance snapshot");
    Check(fake.statusSends == 1 && fake.statusMessageId == WM_REDLIGHT_STATUS && fake.statusWparam == 1 && fake.statusLparam == 0 && fake.statusTimeout == 3000 && fake.statusFlags == (SMTO_ABORTIFHUNG | SMTO_BLOCK), "bounded pointer-free versioned request");
    Check(fake.magInitializes == 0 && fake.dcRequests == 0 && fake.magReads == 0 && fake.gammaReads == 0 && fake.magWrites == 0 && fake.gammaWrites == 0 && fake.registryWrites == 0 && fake.hotkeyRegisters == 0, "query does not initialize/read/write displays or settings/shortcuts");
    Reset(); fake.trayExists = false;
    Check(QueryRunningStatus() == 3 && fake.statusSends == 0 && fake.stdoutText.find("not-running") != std::string::npos, "missing instance reported without starting one");
}
void StatusQueryFailures() {
    fake.statusMessage = false;
    Check(QueryRunningStatus() == 1 && fake.stdoutText.find("query-failed") != std::string::npos, "timeout returns failure without fallback startup");
    Reset(); fake.statusReply = 1;
    Check(QueryRunningStatus() == 1 && fake.stdoutText.find("invalid-response") != std::string::npos, "older/invalid protocol reply rejected");
    Reset(); fake.statusReply = *PackTrayStatus(TrayStatus{}); fake.stdoutWrite = false;
    Check(QueryRunningStatus() == 1, "output failure cannot be reported as successful query");
    Reset(); fake.trayExists = false; fake.stdoutWrite = false;
    Check(QueryRunningStatus() == 1, "missing instance still requires successful output");
    Reset(); fake.statusReply = *PackTrayStatus(TrayStatus{});
    {
        TestMemory::FailNextAllocation fail;
        Check(QueryRunningStatus() == 1 && fake.stdoutText.empty(), "response allocation failure is contained before any partial output");
    }
    Check(fake.magWrites == 0 && fake.gammaWrites == 0 && fake.magInitializes == 0, "query failures never change or initialize display state");
}
void ExpiryWarningBoundariesAndDeadline() {
    SessionTimer timer;
    Check(!timer.warningDue(999999, 60), "unarmed/unlimited session never warns");
    Check(timer.configure(2, 1000, true), "two-minute session");
    Check(!timer.warningDue(60999, 60) && timer.warningDue(61000, 60), "warning begins exactly at selected remaining period");
    Check(!timer.warningDue(61000, 0) && !timer.warningDue(61000, 301), "disabled or invalid warning is ignored");
    const auto remaining = timer.remainingMilliseconds(61000);
    timer.noteWarningAttempt();
    Check(!timer.warningDue(61001, 60) && !timer.warningDue(61001, 300) && timer.remainingMilliseconds(61000) == remaining,
        "one attempt per session even after threshold changes, without resetting countdown");
    Check(!timer.restoreDue(120999) && timer.restoreDue(121000) && !timer.warningDue(121000, 60), "warning attempt cannot postpone exact restoration deadline");
    timer.start(200000);
    Check(timer.warningDue(260000, 60), "explicitly starting a new session rearms one warning");
    timer.cancel();
    Check(!timer.warningDue(260000, 60), "restored/cancelled session never warns");
}
void ExpiryWarningShortSessionsAndClockWrap() {
    SessionTimer timer;
    Check(timer.configure(1, 500, true) && timer.warningDue(500, 300), "session shorter than warning period is eligible on first tick");
    Check(!timer.warningDue(60500, 300) && timer.restoreDue(60500), "expiry takes priority even if no warning was attempted");
    const auto start = (std::numeric_limits<std::uint64_t>::max)() - 100;
    Check(timer.configure(2, start, true) && !timer.warningDue(start + 59999, 60) && timer.warningDue(start + 60000, 60), "warning threshold tolerates monotonic clock wrap");
    timer.noteWarningAttempt();
    Check(timer.configure(1, 0, false) && !timer.warningDue(30000, 30), "configuring while off does not arm a warning");
    timer.start(0);
    Check(timer.warningDue(30000, 30), "new countdown after reconfiguration clears prior warning attempt");
}
void ExpiryWarningSchemaMigrationAndValidation() {
    const std::string v1 = "[RedLightProfile]\nversion=1\nmode=luma\nluma=classic\nbackend=auto\nsession_minutes=15\n";
    auto v2 = v1; v2.replace(v2.find("version=1"), 9, "version=2"); v2 += "hotkeys=f9\n";
    const auto old1 = ParsePreferences(v1), old2 = ParsePreferences(v2);
    Check(old1.preferences && old2.preferences && old1.preferences->expiryWarning == ExpiryWarning::Off && old2.preferences->expiryWarning == ExpiryWarning::Off,
        "older schemas default warnings to off");
    fake.registry[L"SettingsProfile"] = {REG_BINARY, std::vector<char>(v2.begin(), v2.end())};
    const auto loaded = LoadPreferences();
    Check(!loaded.warning && loaded.preferences.shortcuts == ShortcutProfile::F9 && loaded.preferences.expiryWarning == ExpiryWarning::Off && fake.registryWrites == 0,
        "loading a persisted older record migrates warning defaults without storage writes");
    Check(!ParsePreferences(v1 + "expiry_warning=off\n").preferences && !ParsePreferences(v2 + "expiry_warning=off\n").preferences,
        "older version cannot silently carry new warning field");
    const auto current = *SerializePreferences(AppPreferences{});
    for (const auto badToken : {"on", "60", "-1", "1M", "", "360s"}) {
        auto text = current; text.replace(text.find("expiry_warning=off"), 18, std::string("expiry_warning=") + badToken);
        Check(!ParsePreferences(text).preferences, "invalid warning tokens rejected");
    }
    Check(!ParsePreferences(current + "expiry_warning=off\n").preferences, "duplicate warnings rejected");
    auto missing = current; missing.erase(missing.find("expiry_warning="));
    Check(!ParsePreferences(missing).preferences, "version 3 requires explicit warning choice");
    auto invalid = AppPreferences{}; invalid.expiryWarning = static_cast<ExpiryWarning>(99);
    Check(!SerializePreferences(invalid) && !SavePreferences(invalid) && fake.registryWrites == 0, "invalid preference cannot be serialized or persisted");
    for (unsigned i = 0; i < ExpiryWarningOptions.size(); ++i) {
        const auto warning = ExpiryWarningOptions[i];
        Check(ParseExpiryWarning(ExpiryWarningToken(warning)) == warning, "warning token mapping round trips");
        Check(!CommandMayChangeFilter(ID_EXPIRY_WARNING_BASE + i), "warning preference failures cannot request display restoration");
    }
    SupportReportState state; state.expiryWarning = ExpiryWarningName(ExpiryWarning::OneMinute);
    Check(BuildSupportReport(state).find("Expiry warning: 1 minute") != std::string::npos, "support report includes selected warning");
}
void MagnificationStatisticsAndSafeReset() {
    MagnificationFilter filter;
    Check(filter.statistics() == BackendStatistics{}, "new backend counters start at zero");
    Check(!filter.enable() && !filter.disable() && filter.verifyState() == BackendVerification::Unavailable, "invalid lifecycle requests are visible failures");
    Check(filter.initialize() && filter.initialize() && filter.enable() && filter.enable(), "successful and idempotent requests");
    Check(filter.verifyState() == BackendVerification::Matches, "matching verification");
    fake.effect.transform[0][1] = 0.1f;
    Check(filter.verifyState() == BackendVerification::Changed, "mismatch verification");
    fake.magGet = false;
    Check(filter.verifyState() == BackendVerification::Unavailable, "unavailable verification");
    fake.magSet = false;
    Check(!filter.disable() && !filter.shutdown(), "failed restore and nested shutdown recovery");
    const auto statistics = filter.statistics();
    Check(statistics.initialization == BackendOperationStatistics{2, 0} && statistics.activation == BackendOperationStatistics{3, 1} &&
        statistics.restoration == BackendOperationStatistics{3, 3} && statistics.verification == BackendOperationStatistics{4, 3} &&
        statistics.shutdown == BackendOperationStatistics{1, 1}, "counters include idempotent requests, mismatch/unavailable checks and nested recovery");
    const auto error = std::string(filter.lastError());
    const auto reads = fake.magReads, writes = fake.magWrites, uninitializes = fake.uninitializes;
    Check(filter.resetStatistics() && filter.statistics() == BackendStatistics{} && filter.isActive() && filter.needsRestore() &&
        error == filter.lastError() && fake.magReads == reads && fake.magWrites == writes && fake.uninitializes == uninitializes,
        "counter reset preserves active state, recovery snapshot, last error, and all native operations");
    fake.magSet = true; fake.magGet = true;
    Check(filter.disable() && fake.effect.transform[0][0] == 1 && fake.effect.transform[0][1] == 0 && filter.shutdown(), "original restore matrix remains intact after reset");
    Check(filter.statistics().restoration == BackendOperationStatistics{1, 0} && filter.statistics().shutdown == BackendOperationStatistics{1, 0}, "counting resumes from reset");
}
void GammaStatisticsAndCleanupReset() {
    GammaRampFilter filter;
    Check(filter.initialize(), "initialize");
    fake.mutexWaitResult = WAIT_TIMEOUT;
    Check(!filter.enable() && filter.statistics().activation == BackendOperationStatistics{1, 1}, "ownership conflict counts as unsuccessful activation");
    fake.mutexWaitResult = WAIT_OBJECT_0;
    fake.writes = {Write::FailAfterApply, Write::Fail};
    Check(!filter.enable() && filter.needsRestore() && fake.mutexOwned && filter.statistics().activation == BackendOperationStatistics{2, 2} &&
        filter.statistics().restoration == BackendOperationStatistics{1, 1}, "failed activation and rollback are counted separately");
    const auto reads = fake.gammaReads, writes = fake.gammaWrites, releases = fake.mutexReleases;
    Check(filter.resetStatistics() && !filter.isActive() && filter.needsRestore() && fake.mutexOwned &&
        fake.gammaReads == reads && fake.gammaWrites == writes && fake.mutexReleases == releases, "reset cannot discard a partial-write snapshot or gamma ownership");
    fake.mutexRelease = false;
    Check(!filter.disable() && !filter.isActive() && filter.needsRestore() && filter.statistics().restoration == BackendOperationStatistics{1, 1},
        "display-restored but cleanup-failed result counts as unsuccessful recovery");
    const auto restoredWrites = fake.gammaWrites;
    fake.mutexRelease = true;
    Check(filter.disable() && fake.gammaWrites == restoredWrites && filter.statistics().restoration == BackendOperationStatistics{2, 1}, "cleanup-only retry counts without repeating a ramp write");
    Check(filter.verifyState() == BackendVerification::Inactive && filter.statistics().verification == BackendOperationStatistics{1, 0}, "inactive no-read verification is successful bookkeeping");
    Check(filter.shutdown() && filter.shutdown() && filter.statistics().shutdown == BackendOperationStatistics{2, 0}, "idempotent shutdown requests remain visible");
}
void BackendStatisticsSaturationAndInProgress() {
    BackendStatistics statistics;
    {
        RecordBackendOperation outer(statistics.activation);
        Check(!ResetBackendStatistics(statistics) && statistics.activation.requests == 1 && statistics.activation.inProgress == 1, "reset refuses in-progress work without changing counters");
        {
            RecordBackendOperation inner(statistics.activation);
            Check(statistics.activation.inProgress == 2, "nested calls are tracked separately");
            inner.success();
        }
        outer.success();
    }
    Check(statistics.activation == BackendOperationStatistics{2, 0} && ResetBackendStatistics(statistics) && statistics == BackendStatistics{}, "idle reset succeeds after nested operations finish");
    GuardUiCall<int>([&]() -> int { RecordBackendOperation operation(statistics.shutdown); throw 7; }, [](const char*) { return 1; });
    Check(statistics.shutdown == BackendOperationStatistics{1, 1}, "unwinding counts failure and clears in-progress ownership");
    statistics.activation.requests = statistics.activation.failures = (std::numeric_limits<std::uint64_t>::max)() - 1;
    { RecordBackendOperation operation(statistics.activation); }
    { RecordBackendOperation operation(statistics.activation); }
    Check(statistics.activation.requests == (std::numeric_limits<std::uint64_t>::max)() && statistics.activation.failures == statistics.activation.requests && statistics.activation.inProgress == 0,
        "long-session counters saturate instead of wrapping");
}
void BackendStatisticsSupportReport() {
    SupportReportState state;
    state.backendStatistics.initialization = {3, 1};
    state.backendStatistics.activation = {9, 2};
    state.backendStatistics.restoration = {12, 4};
    state.backendStatistics.verification = {7, 3};
    state.backendStatistics.shutdown = {2, 1};
    const auto report = BuildSupportReport(state);
    Check(report.find("Initialize: 3 / 1") != std::string::npos && report.find("Activate: 9 / 2") != std::string::npos &&
        report.find("Restore/ownership cleanup: 12 / 4") != std::string::npos && report.find("Shutdown: 2 / 1") != std::string::npos &&
        report.find("Verify (mismatch/unavailable counted as failures): 7 / 3") != std::string::npos, "report labels counts and failure meanings explicitly");
    Check(!CommandMayChangeFilter(ID_TRAY_RESET_BACKEND_COUNTS) && fake.magReads == 0 && fake.gammaReads == 0 && fake.magWrites == 0 && fake.gammaWrites == 0,
        "report/reset diagnostic paths have no display effect");
}
void NativeStartupArgumentOwnership() {
    Check(GetStartupOptions().action == StartupAction::Tray && fake.argumentReads == 1 && fake.argumentFrees == 1 && !fake.argumentsOutstanding,
        "no-option startup frees native argument block");
    fake.arguments[1] = const_cast<LPWSTR>(L"--status"); fake.argumentsCount = 2;
    Check(GetStartupOptions().action == StartupAction::Status && fake.argumentFrees == 2 && !fake.argumentsOutstanding, "read-only option is parsed and freed");
    fake.arguments[1] = const_cast<LPWSTR>(L"--backend=gamma");
    Check(GetStartupOptions().backendOverride == BackendPreference::Gamma && fake.argumentFrees == 3, "backend override survives freeing native arguments");
    fake.arguments[1] = const_cast<LPWSTR>(L"--unknown");
    Check(GetStartupOptions().action == StartupAction::Invalid && fake.argumentFrees == 4, "invalid user arguments remain distinct from internal failure");
    Check(fake.magInitializes == 0 && fake.dcRequests == 0 && fake.magWrites == 0 && fake.gammaWrites == 0 && fake.registryWrites == 0,
        "loading startup arguments cannot initialize or change display/settings state");
}
void NativeStartupArgumentFailures() {
    fake.argumentsRead = false;
    Check(GetStartupOptions().action == StartupAction::Failed && fake.argumentFrees == 0, "native argument API failure is a startup failure, not a user syntax error");
    fake.argumentsRead = true; fake.argumentsCount = 0;
    Check(GetStartupOptions().action == StartupAction::Failed && fake.argumentFrees == 1 && !fake.argumentsOutstanding, "invalid native argument count still frees returned block");
    fake.argumentsCount = 2; fake.arguments[1] = const_cast<LPWSTR>(L"--status");
    {
        TestMemory::FailNextAllocation fail;
        Check(GetStartupOptions().action == StartupAction::Failed && fake.argumentFrees == 2 && !fake.argumentsOutstanding,
            "allocation exception is contained and unwinds native ownership before returning failure");
    }
    Check(GetStartupOptions().action == StartupAction::Status && fake.argumentFrees == 3, "later startup parsing can succeed after a transient allocation failure");
}
void ProfileValidationOwnedPathArguments() {
    std::wstring option = L"--validate-profile=C:\\profiles\\night mode.txt";
    const std::wstring_view args[] = {option};
    const auto parsed = ParseStartupOptions(args);
    Check(parsed.action == StartupAction::ValidateProfile && parsed.profilePath == L"C:\\profiles\\night mode.txt" && !parsed.backendOverride,
        "validation owns a full Unicode-capable path with spaces");
    for (const auto bad : {L"--validate-profile=", L"--validate-profile"}) {
        const std::wstring_view input[] = {bad};
        Check(ParseStartupOptions(input).action == StartupAction::Invalid && ParseStartupOptions(input).errorOutput == StartupErrorOutput::Json, "empty/missing path rejected without GUI error policy");
    }
    const std::wstring_view combined[] = {option, L"--status"};
    Check(ParseStartupOptions(combined).action == StartupAction::Invalid, "offline validation cannot combine with other actions");
    std::wstring nul = option; nul.push_back(L'\0'); nul += L"extra";
    const std::wstring_view nulArgs[] = {nul};
    Check(ParseStartupOptions(nulArgs).action == StartupAction::Invalid, "embedded path NUL rejected");
    const std::wstring longOption = L"--validate-profile=" + std::wstring(32768, L'a');
    const std::wstring_view longArgs[] = {longOption};
    Check(ParseStartupOptions(longArgs).action == StartupAction::Invalid, "path length is bounded");
    fake.argumentsCount = 2; fake.arguments[1] = option.data();
    const auto loaded = GetStartupOptions();
    option.assign(L"changed native storage");
    Check(loaded.profilePath == L"C:\\profiles\\night mode.txt" && !fake.argumentsOutstanding, "native argv cleanup cannot invalidate owned path");
    option = L"--validate-profile=C:\\profiles\\long enough for allocation.txt"; fake.arguments[1] = option.data();
    {
        TestMemory::FailNextAllocation fail(1); // Views allocate first; the owned path allocates second.
        Check(GetStartupOptions().action == StartupAction::Failed && !fake.argumentsOutstanding,
            "path allocation failure unwinds native argument ownership and returns startup failure");
    }
}
void ProfileFileValidationReadOnly() {
    MagnificationFilter filter;
    Check(filter.initialize() && filter.enable(), "active backend for isolation check");
    const auto reads = fake.magReads, writes = fake.magWrites;
    fake.profileData = *SerializePreferences(AppPreferences{}); fake.profileChunk = 7;
    Check(ValidateSettingsProfileFile(L"C:\\profiles\\赤 光.txt") == 0 && fake.stdoutText.find("\"valid\":true") != std::string::npos, "valid profile returns JSON after partial disk reads");
    Check(std::wstring_view(fake.profilePath) == L"C:\\profiles\\赤 光.txt" && fake.profileAccess == GENERIC_READ && fake.profileShare == FILE_SHARE_READ &&
        fake.profileDisposition == OPEN_EXISTING && (fake.profileFlags & FILE_FLAG_SEQUENTIAL_SCAN) != 0, "opens exact Unicode path read-only without write/delete sharing");
    Check(!fake.profileHandleOpen && fake.profileCloses == 1 && fake.profilePosition == fake.profileData.size() && fake.profileReads > 2 &&
        fake.profileModeAtOpen == 3 && fake.errorMode == 2 && fake.errorModeCalls == 2 && fake.stdoutWhileProfileOpen == 0 && fake.stdoutErrorMode == 2, "file closes and original thread error mode returns before output");
    Check(filter.isActive() && filter.needsRestore() && fake.magReads == reads && fake.magWrites == writes && fake.gammaReads == 0 && fake.gammaWrites == 0 &&
        fake.statusSends == 0 && fake.registryWrites == 0 && fake.hotkeyRegisters == 0 && fake.clipOpens == 0, "offline validation cannot query/change display, tray, preferences, shortcuts or clipboard");
}
void ProfileFileValidationBoundsAndEncoding() {
    const auto valid = *SerializePreferences(AppPreferences{});
    fake.profileData = valid; fake.profileData.resize(4096, ' ');
    Check(ValidateSettingsProfileFile(L"profile.txt") == 0 && fake.profilePosition == 4096, "exact 4096-byte valid profile accepted");
    Reset(); fake.profileData.assign(20000, ' ');
    Check(ValidateSettingsProfileFile(L"large.txt") == 4 && fake.profilePosition == 4097 && fake.stdoutText.find("too large") != std::string::npos && !fake.profileHandleOpen,
        "oversized input detected with only one extra byte, never truncated into a valid profile");
    Reset(); fake.profileData = valid; fake.profileData.push_back('\0');
    Check(ValidateSettingsProfileFile(L"nul.txt") == 4 && fake.stdoutText.find("NUL byte") != std::string::npos, "NUL-containing/UTF-16-style input is rejected");
    Reset(); fake.profileData = std::string("\xef\xbb\xbf") + valid; fake.profileEofError = true;
    Check(ValidateSettingsProfileFile(L"bom.txt") == 0 && fake.profileReads == 2, "UTF-8 BOM and explicit EOF result accepted");
    Reset();
    Check(ValidateSettingsProfileFile(L"empty.txt") == 4 && fake.stdoutText.find("\"line\":0") != std::string::npos, "empty file is invalid profile data");
    Reset(); fake.profileData = "[RedLightProfile]\nversion=999\n";
    Check(ValidateSettingsProfileFile(L"future.txt") == 4 && fake.stdoutText.find("\"line\":2") != std::string::npos && fake.stdoutText.find("Unsupported profile version") != std::string::npos,
        "parser reason and exact line are machine-readable");
}
void ProfileFileValidationIoFailures() {
    fake.profileOpen = false;
    Check(ValidateSettingsProfileFile(L"missing.txt") == 1 && fake.stdoutText.find("\"stage\":\"open\"") != std::string::npos && fake.profileCloses == 0 && fake.errorMode == 2, "open failure restores thread mode");
    for (const auto type : {FILE_TYPE_PIPE, FILE_TYPE_CHAR, FILE_TYPE_UNKNOWN}) {
        Reset(); fake.profileType = type; fake.profileTypeError = ERROR_ACCESS_DENIED;
        Check(ValidateSettingsProfileFile(L"device") == 1 && fake.profileReads == 0 && fake.profileCloses == 1 && fake.stdoutText.find("\"stage\":\"type\"") != std::string::npos,
            "non-disk handles/type errors are rejected before reads");
    }
    Reset(); fake.profileRead = false;
    Check(ValidateSettingsProfileFile(L"read.txt") == 1 && fake.stdoutText.find("\"stage\":\"read\"") != std::string::npos && fake.stdoutText.find("\"error_code\":5") != std::string::npos && !fake.profileHandleOpen, "read error preserved through cleanup");
    Reset(); fake.profileOverRead = true;
    Check(ValidateSettingsProfileFile(L"count.txt") == 1 && fake.profilePosition == 0 && fake.stdoutText.find("\"error_code\":13") != std::string::npos, "impossible byte count rejected before buffer use");
    Reset(); fake.profileClose = false; fake.diagnosticsClobbersError = true;
    Check(ValidateSettingsProfileFile(L"close.txt") == 1 && fake.profileCloses == 2 && fake.stdoutText.find("\"stage\":\"close\"") != std::string::npos && fake.stdoutText.find("\"error_code\":5") != std::string::npos && fake.errorMode == 2,
        "failed close is retried without claiming successful validation");
    Reset(); fake.errorModeSet = false;
    Check(ValidateSettingsProfileFile(L"mode.txt") == 1 && fake.profileOpens == 0 && fake.errorMode == 2 && fake.stdoutText.find("\"stage\":\"error-mode\"") != std::string::npos, "suppression failure blocks file I/O");
    Reset(); fake.errorModeRestore = false; fake.diagnosticsClobbersError = true;
    Check(ValidateSettingsProfileFile(L"mode.txt") == 1 && fake.errorModeCalls == 3 && !fake.profileHandleOpen && fake.stdoutText.find("error-mode-restore") != std::string::npos && fake.stdoutText.find("\"error_code\":5") != std::string::npos, "failed thread mode restoration is reported and retried with original error preserved");
}
void ProfileFileValidationOutputFailures() {
    fake.profileData = *SerializePreferences(AppPreferences{}); fake.stdoutWrite = false;
    Check(ValidateSettingsProfileFile(L"valid.txt") == 1 && !fake.profileHandleOpen && fake.errorMode == 2, "valid data cannot hide output failure");
    Reset(); fake.stdoutValid = false; fake.consoleAttach = false;
    Check(ValidateSettingsProfileFile(L"invalid.txt") == 1 && !fake.profileHandleOpen && fake.errorMode == 2, "invalid data cannot hide unavailable output");
    Reset();
    Check(ValidateSettingsProfileFile(nullptr) == 1 && fake.profileOpens == 0 && fake.stdoutText.find("\"stage\":\"arguments\"") != std::string::npos, "defensive null path check never reaches file APIs");
}
void BoundedJsonStringEncoding() {
    std::array<char, 128> output{};
    Check(QuoteJsonString("quote\"slash\\", output) == "\"quote\\\"slash\\\\\"", "quotes and slashes escaped");
    const std::string controls("a\0\n\t", 4);
    Check(QuoteJsonString(controls, output) == "\"a\\u0000\\u000a\\u0009\"", "all control bytes are escaped, including embedded NUL");
    Check(QuoteJsonString("\xc3\xa9", output) == "\"\xc3\xa9\"", "UTF-8 bytes preserved");
    std::array<char, 3> exact{}; std::array<char, 2> small{};
    Check(QuoteJsonString("a", exact) == "\"a\"" && !QuoteJsonString("a", small) && !QuoteJsonString("", {}), "exact-fit encoding succeeds and incomplete output is never returned");
    Check(!QuoteJsonString("\xff", output) && !QuoteJsonString("\xed\xa0\x80", output), "malformed UTF-8 cannot produce an invalid JSON string");
}
void Utf8ScalarBoundaries() {
    Check(IsValidUtf8({}), "empty UTF-8 is valid");
    for (unsigned byte = 0; byte < 256; ++byte) {
        const char value = static_cast<char>(byte);
        Check(IsValidUtf8(std::string_view(&value, 1)) == (byte < 128), "only ASCII bytes are valid standalone characters");
    }
    for (const std::string_view scalar : {"\xc2\x80", "\xdf\xbf", "\xe0\xa0\x80", "\xed\x9f\xbf", "\xee\x80\x80", "\xef\xbf\xbf", "\xf0\x90\x80\x80", "\xf4\x8f\xbf\xbf"}) {
        Check(IsValidUtf8(scalar), "canonical scalar boundaries accepted");
        for (std::size_t length = 1; length < scalar.size(); ++length) Check(!IsValidUtf8(scalar.substr(0, length)), "truncated prefixes rejected");
    }
    for (const auto invalid : {"\xc0\x80", "\xc1\xbf", "\xe0\x9f\xbf", "\xed\xa0\x80", "\xed\xbf\xbf", "\xf0\x8f\xbf\xbf", "\xf4\x90\x80\x80", "\xf5\x80\x80\x80", "\xfe\xff", "\xc2x", "\xe2\x82x", "\xf0\x90\x80x", "\xc2\xa0\x80"})
        Check(!IsValidUtf8(invalid), "overlong/surrogate/out-of-range/malformed sequences rejected");
    Check(IsValidUtf8("café 赤 🌙") && IsValidUtf8("\xef\xbb\xbf"), "Unicode text and UTF-8 BOM accepted");
}
void ProfileUtf8Validation() {
    const auto text = *SerializePreferences(AppPreferences{});
    Check(ParsePreferences(std::string("# café 赤 🌙\n") + text).preferences == AppPreferences{}, "valid Unicode comments accepted");
    const auto invalid = std::string("# ") + "\xed\xa0\x80" + "\n" + text;
    const auto parsed = ParsePreferences(invalid);
    Check(!parsed.preferences && parsed.line == 0 && std::string_view(parsed.error).find("invalid UTF-8") != std::string_view::npos, "malformed comment bytes rejected before field parsing");
    fake.registry[L"SettingsProfile"] = {REG_BINARY, std::vector<char>(invalid.begin(), invalid.end())};
    Check(LoadPreferences().warning && fake.registryWrites == 0, "invalid persisted encoding warns without rewriting settings");
    fake.profileData = invalid;
    Check(ValidateSettingsProfileFile(L"invalid-encoding.txt") == 4 && fake.stdoutText.find("invalid UTF-8") != std::string::npos && !fake.profileHandleOpen,
        "offline validation reports encoding failure as invalid data after checked cleanup");
    Check(fake.magInitializes == 0 && fake.dcRequests == 0 && fake.clipOpens == 0 && fake.hotkeyRegisters == 0, "encoding validation has no display/clipboard/shortcut side effects");
}
void ClipboardEmbeddedNulRejection() {
    fake.clipMemory = {L'p', L'r', L'i', L'o', L'r', L'\0'};
    const auto prior = fake.clipMemory;
    const std::string text("complete\0hidden", 15);
    Check(!CopyUtf8TextToClipboard(nullptr, text) && !CopyUtf8TextToClipboard(nullptr, std::string_view("\0", 1)), "NUL-containing text is never reported as a complete clipboard export");
    Check(fake.clipConversions == 0 && fake.clipAllocations == 0 && fake.clipOpens == 0 && fake.clipEmpties == 0 && fake.clipSets == 0 && fake.clipMemory == prior,
        "rejection occurs before conversion, allocation or clipboard mutation");
}
void SavedProfileExportDefaultsAndMigration() {
    Check(ExportSavedSettingsProfile() == 0 && ParsePreferences(fake.stdoutText).preferences == AppPreferences{} && fake.registryWrites == 0,
        "absent settings export explicit defaults without writing storage");
    Reset();
    PutLegacy(L"RedMode", 0); PutLegacy(L"LumaProfile", 2); PutLegacy(L"SessionLimitMinutes", 17);
    Check(ExportSavedSettingsProfile() == 0 && ParsePreferences(fake.stdoutText).preferences == AppPreferences{RedMode::StrictRed, LumaProfile::EqualDetail, BackendPreference::Automatic, 17} &&
        !fake.registry.contains(L"SettingsProfile") && fake.registryWrites == 0, "legacy settings export canonically without migration writes");
    Reset();
    const std::string v2 = "[RedLightProfile]\nversion=2\nmode=luma\nluma=green\nbackend=auto\nsession_minutes=31\nhotkeys=f10\n";
    fake.registry[L"SettingsProfile"] = {REG_BINARY, std::vector<char>(v2.begin(), v2.end())};
    Check(ExportSavedSettingsProfile() == 0 && fake.stdoutText.find("version=3") != std::string::npos && ParsePreferences(fake.stdoutText).preferences ==
        AppPreferences{RedMode::LumaRed, LumaProfile::GreenDetail, BackendPreference::Automatic, 31, ShortcutProfile::F10, ExpiryWarning::Off} && fake.registryWrites == 0,
        "older saved profiles export current complete schema without changing stored record");
    Check(fake.profileOpens == 0 && fake.clipOpens == 0 && fake.hotkeyRegisters == 0 && fake.magInitializes == 0 && fake.dcRequests == 0 && fake.statusSends == 0,
        "saved export does not read a profile file, use clipboard/shortcuts, start a backend or query a tray");
}
void SavedProfileExportPreservesRecoveryOnFailures() {
    GammaRampFilter filter;
    Check(filter.initialize() && filter.enable(), "active gamma backend");
    const AppPreferences saved{RedMode::LumaRed, LumaProfile::GreenDetail, BackendPreference::Automatic, 57, ShortcutProfile::F10, ExpiryWarning::OneMinute};
    Check(SavePreferences(saved), "seed preferred luma settings");
    const auto reads = fake.gammaReads, writes = fake.gammaWrites, releases = fake.mutexReleases;
    const auto statistics = filter.statistics();
    const int registryWrites = fake.registryWrites;
    Check(ExportSavedSettingsProfile() == 0 && ParsePreferences(fake.stdoutText).preferences == saved && fake.registryWrites == registryWrites,
        "export preserves saved luma choices while running gamma remains strict red");
    fake.stdoutText.clear(); fake.registryRead = false;
    Check(ExportSavedSettingsProfile() == 1 && fake.stdoutText.empty(), "unreadable storage cannot masquerade as a successful default backup");
    fake.registryRead = true;
    const std::string corrupt = "not a settings profile";
    fake.registry[L"SettingsProfile"] = {REG_BINARY, std::vector<char>(corrupt.begin(), corrupt.end())};
    Check(ExportSavedSettingsProfile() == 1 && fake.stdoutText.empty(), "corrupt storage produces no profile output");
    Check(SavePreferences(saved), "restore valid stored data for fault injection");
    const auto beforeFaultWrites = fake.registryWrites;
    {
        TestMemory::FailNextAllocation fail;
        Check(ExportSavedSettingsProfile() == 1 && fake.stdoutText.empty() && fake.registryWrites == beforeFaultWrites,
            "serialization allocation failure is contained before output or storage mutation");
    }
    fake.stdoutChunk = 3; fake.stdoutFailOnCall = fake.stdoutWrites + 2;
    Check(ExportSavedSettingsProfile() == 1 && fake.stdoutText.size() == 3 && !ParsePreferences(fake.stdoutText).preferences,
        "partial output failure returns failure and cannot be parsed as a complete profile");
    Check(filter.isActive() && filter.needsRestore() && fake.mutexOwned && fake.gammaReads == reads && fake.gammaWrites == writes && fake.mutexReleases == releases &&
        filter.statistics() == statistics, "all export outcomes preserve active filtering, calibration snapshot and ownership without backend calls");
    Check(filter.disable(), "ordinary recovery remains available after export failures");
}
void SavedProfileExportCommandPolicy() {
    const std::wstring_view args[] = {L"--export-profile"};
    const auto options = ParseStartupOptions(args);
    Check(options.action == StartupAction::ExportProfile && options.errorOutput == StartupErrorOutput::LogOnly && options.profilePath.empty() && !options.backendOverride,
        "export is a standalone profile-text action");
    const std::wstring_view invalid[] = {L"--export-profile=file"}, conflicting[] = {L"--export-profile", L"--backend=gamma"};
    Check(ParseStartupOptions(invalid).action == StartupAction::Invalid && ParseStartupOptions(invalid).errorOutput == StartupErrorOutput::LogOnly &&
        ParseStartupOptions(conflicting).errorOutput == StartupErrorOutput::LogOnly, "invalid export syntax is noninteractive without adding JSON to profile stdout");
    const std::wstring_view mixed[] = {L"--export-profile", L"--status"}, help[] = {L"--help"};
    Check(ParseStartupOptions(mixed).action == StartupAction::Invalid && ParseStartupOptions(mixed).errorOutput == StartupErrorOutput::Json &&
        ParseStartupOptions(help).errorOutput == StartupErrorOutput::Dialog, "JSON errors take precedence for mixed machine requests while ordinary help stays interactive");
    fake.argumentsCount = 2; fake.arguments[1] = const_cast<LPWSTR>(L"--export-profile");
    Check(GetStartupOptions().action == StartupAction::ExportProfile && fake.argumentFrees == 1 && !fake.argumentsOutstanding,
        "native export argument loading retains no argv allocation");
}
void DetailedMatrixVerification() {
    MagnificationFilter filter;
    Check(filter.initialize() && filter.enable(), "activate matrix backend");
    const auto originalWrites = fake.magWrites;
    auto report = filter.inspectState();
    Check(report.state == BackendVerification::Matches && report.readback && report.readback->values == 25 && report.readback->mismatches == 0 &&
        report.readback->maximumDifference == 0 && !report.readback->gammaRamp, "matrix report compares all coefficients in coefficient units");
    fake.effect.transform[0][0] += 0.125f;
    fake.effect.transform[1][1] = 0.01f;
    fake.effect.transform[1][0] = (std::numeric_limits<float>::quiet_NaN)();
    report = filter.inspectState();
    Check(report.state == BackendVerification::Changed && report.readback && report.readback->values == 25 && report.readback->mismatches == 3 &&
        std::abs(report.readback->maximumDifference - 0.125) < 0.000001 && report.readback->nonFiniteValues && report.readback->unexpectedGreenBlue,
        "all mismatches are counted; nonfinite inputs do not poison the finite maximum");
    Check(fake.magWrites == originalWrites && filter.isActive() && filter.needsRestore() && !filter.lastError()[0], "inspection never writes or changes backend ownership/error state");
    fake.magGet = false; fake.lastError = 4321; fake.diagnosticsClobbersError = true;
    report = filter.inspectState();
    Check(report.state == BackendVerification::Unavailable && !report.readback && report.errorCode == 4321, "read error survives diagnostic logging without stale numerical data");
    fake.magGet = true; fake.diagnosticsClobbersError = false;
    Check(filter.disable() && fake.effect.transform[0][0] == 1 && fake.effect.transform[1][1] == 1, "original matrix remains recoverable after detailed checks");
}
void DetailedGammaVerification() {
    GammaRampFilter filter;
    Check(filter.initialize() && filter.enable(), "activate gamma backend");
    auto report = filter.inspectState();
    Check(report.state == BackendVerification::Matches && report.readback && report.readback->values == 768 && report.readback->gammaRamp && report.readback->maximumDifference == 0,
        "gamma report covers all entries in normalized full-scale units");
    fake.ramp[0][42] ^= 1;
    report = filter.inspectState();
    Check(report.state == BackendVerification::Matches && report.readback->mismatches == 0 && std::abs(report.readback->maximumDifference - 1.0 / 65535) < 1e-12,
        "tolerated red low-byte rounding is quantified without changing significant-byte acceptance");
    fake.ramp[1][100] = 1; fake.ramp[2][101] = 257; fake.ramp[0][10] ^= 0x100;
    const auto writes = fake.gammaWrites, releases = fake.mutexReleases;
    report = filter.inspectState();
    Check(report.state == BackendVerification::Changed && report.readback->values == 768 && report.readback->mismatches == 3 && report.readback->unexpectedGreenBlue &&
        !report.readback->nonFiniteValues && std::abs(report.readback->maximumDifference - 257.0 / 65535) < 1e-12, "low-byte green leakage and all other mismatches are counted exactly");
    Check(fake.gammaWrites == writes && fake.mutexReleases == releases && fake.mutexOwned && filter.isActive() && filter.needsRestore(), "detailed gamma verification preserves snapshot and exclusive ownership");
    fake.gammaGet = false; fake.lastError = 987; fake.diagnosticsClobbersError = true;
    report = filter.inspectState();
    Check(report.state == BackendVerification::Unavailable && !report.readback && report.errorCode == 987, "gamma read failure has checked error detail without stale comparison");
    fake.gammaGet = true; fake.diagnosticsClobbersError = false;
    Check(filter.disable() && fake.ramp[1][100] == 100 * 257, "restores original calibration after mismatches");
    const auto reads = fake.gammaReads;
    report = filter.inspectState();
    Check(report.state == BackendVerification::Inactive && !report.readback && report.errorCode == 0 && fake.gammaReads == reads, "inactive report never samples another tool's state");
}
void ComparisonToleranceAndFiniteRange() {
    float expected[5][5] = {}, actual[5][5] = {};
    expected[0][0] = actual[0][0] = 1;
    actual[0][0] = std::nextafter(1.0f, 2.0f);
    auto matrix = BackendComparison::Matrix(actual, expected);
    Check(matrix.mismatches == 0 && matrix.maximumDifference > 0, "nonzero matrix coefficients retain established float tolerance");
    actual[0][1] = (std::numeric_limits<float>::denorm_min)();
    matrix = BackendComparison::Matrix(actual, expected);
    Check(matrix.mismatches == 1 && matrix.unexpectedGreenBlue, "every nonzero expected-zero coefficient is a mismatch, including subnormal leakage");
    actual[0][1] = -0.0f;
    expected[0][0] = -(std::numeric_limits<float>::max)(); actual[0][0] = (std::numeric_limits<float>::max)();
    Check(BackendComparison::FiniteMatrix(actual) && BackendComparison::FiniteMatrix(expected), "finite snapshot validation does not reject large or signed-zero coefficients");
    matrix = BackendComparison::Matrix(actual, expected);
    Check(matrix.mismatches == 1 && std::isfinite(matrix.maximumDifference) && !matrix.nonFiniteValues && !matrix.unexpectedGreenBlue,
        "finite extreme values retain a finite double-precision maximum; signed zero stays zero");
    WORD gammaExpected[3][256] = {}, gammaActual[3][256] = {};
    gammaActual[1][0] = 10;
    Check(BackendComparison::Gamma(gammaActual, gammaExpected, true).mismatches == 1 && BackendComparison::Gamma(gammaActual, gammaExpected, false).mismatches == 0,
        "strict red zero checks and ordinary restoration significant-byte checks retain distinct policies");
    gammaActual[0][0] = 65535;
    Check(BackendComparison::Gamma(gammaActual, gammaExpected, true).maximumDifference == 1, "normalized gamma full-scale difference is exactly one");
}
void DetailedVerificationReportFormatting() {
    SupportReportState state;
    state.verification = BackendVerification::Changed;
    state.readback = BackendReadbackDetails{25, 3, 0.125, true, true, false};
    auto text = BuildSupportReport(state);
    Check(text.find("Read-back values/mismatches: 25 / 3") != std::string::npos && text.find("Largest finite difference: 0.125 (matrix coefficient)") != std::string::npos &&
        text.find("Nonfinite values: yes") != std::string::npos && text.find("Unexpected green/blue entries: yes") != std::string::npos, "cached report contains numerical interpretation and flags");
    state.verification = BackendVerification::Unavailable; state.verificationError = 4321; state.readback.reset();
    text = BuildSupportReport(state);
    Check(text.find("Verification error code: 4321") != std::string::npos && text.find("Read-back values/mismatches:") == std::string::npos,
        "unavailable reports cannot retain prior comparison values");
    BackendVerificationReport cached{BackendVerification::Changed, BackendReadbackDetails{25, 1}, 0};
    cached = {};
    Check(cached.state == BackendVerification::NotChecked && !cached.readback && cached.errorCode == 0, "filter-operation cache reset discards all numerical/error details");
    Check(fake.magReads == 0 && fake.gammaReads == 0 && fake.magWrites == 0 && fake.gammaWrites == 0, "formatting cached detail performs no backend operation");
}
void SessionNotificationOwnership() {
    SessionNotifications notifications;
    const auto first = reinterpret_cast<HWND>(std::uintptr_t{0x1234});
    const auto other = reinterpret_cast<HWND>(std::uintptr_t{0x5678});
    Check(!notifications.ensure(nullptr) && !notifications.isRegistered() && notifications.lastError() == ERROR_INVALID_PARAMETER && fake.sessionRegisters == 0,
        "missing owner rejected before native registration");
    Check(notifications.ensure(first) && notifications.isRegistered() && notifications.lastError() == 0 && fake.sessionRegisteredOwner == first && fake.sessionFlags == 0,
        "successful current-session registration owns exact window");
    Check(notifications.ensure(first) && fake.sessionRegisters == 1, "healthy retry does not duplicate native registration");
    Check(!notifications.ensure(other) && notifications.isRegistered() && fake.sessionRegisteredOwner == first && fake.sessionRegisters == 1,
        "different window cannot silently replace a live registration");
    Check(notifications.release() && !notifications.isRegistered() && fake.sessionUnregisterOwner == first && notifications.lastError() == 0,
        "release targets original owner and clears state only after success");
    Check(notifications.release() && fake.sessionUnregisters == 1, "inactive release is idempotent");
    Check(notifications.ensure(other) && notifications.release() && fake.sessionRegisters == 2 && fake.sessionUnregisters == 2, "owner can change after successful release");
}
void SessionNotificationFailureRecovery() {
    MagnificationFilter filter;
    Check(filter.initialize() && filter.enable(), "active filtering");
    const auto writes = fake.magWrites, reads = fake.magReads;
    SessionNotifications notifications;
    const auto owner = reinterpret_cast<HWND>(std::uintptr_t{0x1234});
    fake.sessionRegister = false; fake.diagnosticsClobbersError = true;
    Check(!notifications.ensure(owner) && !notifications.isRegistered() && notifications.lastError() == 1702 && !fake.sessionRegisteredOwner,
        "service-not-ready failure remains unavailable and preserves API error despite logging");
    fake.sessionRegister = true;
    Check(notifications.ensure(owner) && notifications.isRegistered() && notifications.lastError() == 0, "later retry recovers registration without app restart");
    fake.sessionUnregister = false; fake.sessionError = ERROR_ACCESS_DENIED;
    Check(!notifications.release() && notifications.isRegistered() && notifications.lastError() == ERROR_ACCESS_DENIED && fake.sessionRegisteredOwner == owner,
        "failed unregister preserves owner/state and actual API error");
    fake.sessionUnregister = true;
    Check(notifications.release() && !notifications.isRegistered() && notifications.lastError() == 0 && !fake.sessionRegisteredOwner, "unregister failure can retry safely");
    Check(filter.isActive() && filter.needsRestore() && fake.magWrites == writes && fake.magReads == reads && fake.gammaWrites == 0 && fake.registryWrites == 0 && fake.hotkeyRegisters == 0,
        "registration/cleanup outcomes never read/write display state or preferences/shortcuts");
}
void SessionNotificationReentrancyAndUnwind() {
    SessionNotifications notifications;
    const auto owner = reinterpret_cast<HWND>(std::uintptr_t{0x1234});
    fake.sessionCallbackTarget = &notifications;
    Check(notifications.ensure(owner) && !fake.nestedSessionEnsure && !fake.nestedSessionRelease && fake.sessionRegisters == 1 && fake.sessionUnregisters == 0 && notifications.lastError() == 0,
        "nested registration/release requests cannot corrupt in-progress ownership");
    Check(notifications.release(), "release outer registration");
    fake.sessionRegisterThrows = true;
    bool caught = false;
    try { notifications.ensure(owner); } catch (const std::exception&) { caught = true; }
    Check(caught && !notifications.isRegistered() && !fake.sessionRegisteredOwner, "injected pre-registration exception does not claim success");
    fake.sessionRegisterThrows = false;
    Check(notifications.ensure(owner) && notifications.release(), "scoped guard unwinds so later registration and release remain possible");
}
void SessionNotificationReport() {
    SupportReportState state; state.sessionNotificationError = 1702;
    auto report = BuildSupportReport(state);
    Check(report.find("Session notifications: unavailable") != std::string::npos && report.find("Last session-notification error: 1702") != std::string::npos,
        "cached registration failure is visible without a log file");
    state.sessionNotifications = true; state.sessionNotificationError = 0;
    report = BuildSupportReport(state);
    Check(report.find("Session notifications: registered") != std::string::npos && report.find("Last session-notification error: 0") != std::string::npos && !CommandMayChangeFilter(ID_TRAY_RETRY_SESSION),
        "registration recovery and display-read-only failure policy are explicit");
    Check(fake.sessionRegisters == 0 && fake.magReads == 0 && fake.gammaReads == 0, "reporting cached status does not register or inspect backends");
}
void ReadOnlyFailureHonorsQueuedRestore() {
    MagnificationFilter filter;
    Check(filter.initialize(), "initialize");
    const auto original = fake.effect;
    Check(filter.enable(), "activate");
    bool busy = false, requested = false, attempted = false;
    GuardUiCall<int>([&]() -> int {
        ScopedFlag call(busy);
        requested = true; // A restore request arrived while the read-only operation was busy.
        throw std::runtime_error("read-only tool failure");
    }, [&](const char*) {
        if (RecoveryAfterUiFailure(CommandMayChangeFilter(ID_TRAY_RETRY_SESSION), requested, busy) == UiFailureRecovery::Restore) {
            attempted = true;
            if (filter.disable()) requested = false;
        }
        return 1;
    });
    Check(!busy && attempted && !requested && !filter.isActive() && !filter.needsRestore() && Same(fake.effect, original),
        "read-only failure preserves and honors the independently requested restoration after busy scope unwinds");
}
void QueuedRestoreRetainsOuterBusyOwnership() {
    MagnificationFilter filter;
    Check(filter.initialize() && filter.enable(), "activate");
    const auto writes = fake.magWrites;
    bool busy = false, requested = false, deferred = false;
    {
        ScopedFlag outer(busy);
        GuardUiCall<int>([&]() -> int {
            ScopedFlag inner(busy); requested = true; throw 7;
        }, [&](const char*) {
            deferred = RecoveryAfterUiFailure(false, requested, busy) == UiFailureRecovery::Defer;
            if (deferred) requested = true;
            return 1;
        });
        Check(deferred && busy && requested && filter.isActive() && fake.magWrites == writes, "inner failure cannot restore over an outer in-progress operation");
        Check(RecoveryAfterUiFailure(false, false, busy) == UiFailureRecovery::None, "read-only failure without a request never queues restoration on its own");
    }
    Check(!busy && requested && RecoveryAfterUiFailure(false, requested, busy) == UiFailureRecovery::Restore && filter.disable(), "queued restoration remains actionable after outer scope ends");
}
void GammaPreferredLumaWithoutBackendChanges() {
    GammaRampFilter filter;
    Check(filter.initialize() && filter.enable(), "active strict gamma backend");
    Check(!filter.setLumaProfile(LumaProfile::GreenDetail), "seed an existing unsupported-operation diagnostic");
    const auto cached = filter.inspectState();
    const auto statistics = filter.statistics();
    const auto lastError = std::string(filter.lastError());
    const auto reads = fake.gammaReads, writes = fake.gammaWrites, releases = fake.mutexReleases;
    WORD before[3][256]; std::memcpy(before, fake.ramp, sizeof(before));
    AppPreferences preferences;
    Check(UpdateLumaPreference(filter, preferences, LumaProfile::EqualDetail) == LumaPreferenceResult::PreferredOnly && SavePreferences(preferences),
        "gamma stores a preferred Magnification preset through the normal complete-record save");
    Check(preferences.luma == LumaProfile::EqualDetail && LoadPreferences().preferences.luma == LumaProfile::EqualDetail && filter.redMode() == RedMode::StrictRed &&
        filter.lumaProfile() == LumaProfile::Classic && filter.isActive() && filter.needsRestore() && fake.mutexOwned &&
        fake.gammaReads == reads && fake.gammaWrites == writes && fake.mutexReleases == releases && std::memcmp(before, fake.ramp, sizeof(before)) == 0 &&
        lastError == filter.lastError() && filter.statistics() == statistics && cached.state == BackendVerification::Matches,
        "preferred-only change leaves exact output, mode, API errors, verification, statistics, ownership and snapshots untouched");
    TrayStatus status; status.backend = StatusBackend::Gamma; status.mode = RedMode::StrictRed; status.supportsLuma = false; status.luma = preferences.luma;
    const auto json = *TrayStatusJson(status);
    Check(json.find("\"luma_profile\":\"equal\"") != std::string::npos && json.find("\"luma_profile_scope\":\"magnification-preference\"") != std::string::npos,
        "status makes the gamma preference scope explicit");
    SupportReportState report; report.redMode = "Strict Red"; report.lumaProfile = "Unavailable on gamma";
    report.preferredRedMode = RedModeDisplayName(preferences.mode); report.preferredLumaProfile = LumaProfileName(preferences.luma);
    Check(BuildSupportReport(report).find("Preferred red mode: Luma Red") != std::string::npos && BuildSupportReport(report).find("Preferred Luma detail: Equal color detail") != std::string::npos,
        "reports distinguish stored preference from effective gamma mode");
    Check(!CommandMayChangeFilter(ID_LUMA_PROFILE_BASE + 2, false) && CommandMayChangeFilter(ID_LUMA_PROFILE_BASE + 2, true),
        "read-only display-failure policy follows current backend capability");
    Check(filter.disable() && fake.ramp[1][100] == 100 * 257, "original calibration restores after preferred-only edits");
}
void MagnificationLumaPreferenceCommitRules() {
    MagnificationFilter filter;
    Check(filter.initialize() && filter.enable(), "active Magnification backend");
    AppPreferences preferences;
    fake.magSet = false;
    Check(UpdateLumaPreference(filter, preferences, LumaProfile::GreenDetail) == LumaPreferenceResult::BackendFailed && preferences.luma == LumaProfile::Classic &&
        filter.lumaProfile() == LumaProfile::Classic && filter.isActive() && filter.needsRestore(), "failed live application never commits the preferred preset");
    fake.magSet = true;
    Check(UpdateLumaPreference(filter, preferences, LumaProfile::GreenDetail) == LumaPreferenceResult::Applied && preferences.luma == LumaProfile::GreenDetail && filter.lumaProfile() == LumaProfile::GreenDetail,
        "confirmed live application commits matching preference");
    const auto writes = fake.magWrites;
    Check(UpdateLumaPreference(filter, preferences, static_cast<LumaProfile>(99)) == LumaPreferenceResult::Rejected && preferences.luma == LumaProfile::GreenDetail && fake.magWrites == writes,
        "invalid preference cannot reach the backend");
    Check(filter.disable() && fake.effect.transform[0][0] == 1 && fake.effect.transform[1][1] == 1, "original snapshot survives preference updates/failures");
}
void GammaPreferredLumaDuringRecoveryAndSaveFailure() {
    GammaRampFilter filter;
    Check(filter.initialize(), "initialize");
    fake.writes = {Write::FailAfterApply, Write::Fail};
    Check(!filter.enable() && filter.needsRestore() && !filter.isActive(), "partial-write recovery is pending");
    const auto reads = fake.gammaReads, writes = fake.gammaWrites;
    const auto error = std::string(filter.lastError());
    AppPreferences preferences;
    Check(UpdateLumaPreference(filter, preferences, LumaProfile::GreenDetail) == LumaPreferenceResult::PreferredOnly, "future preset remains editable during gamma recovery");
    fake.registryWrite = false;
    Check(!SavePreferences(preferences) && preferences.luma == LumaProfile::GreenDetail && filter.needsRestore() && !filter.isActive() && fake.mutexOwned &&
        fake.gammaReads == reads && fake.gammaWrites == writes && error == filter.lastError(), "save failure preserves current preference and pending recovery without new backend calls");
    fake.registryWrite = true;
    Check(SavePreferences(preferences) && filter.disable(), "preference persistence and display restoration can retry independently");
}
void CountdownRestartResetsDeadlineAndWarning() {
    SessionTimer timer;
    Check(timer.configure(15, 1000, true) && timer.warningDue(841000, 60), "warning threshold on initial countdown");
    timer.noteWarningAttempt();
    Check(timer.restart(850000, true) && timer.minutes() == 15 && timer.remainingMilliseconds(850000) == 900000,
        "explicit restart reuses configured duration from the supplied time");
    Check(!timer.restoreDue(1749999) && timer.restoreDue(1750000) && timer.warningDue(1690000, 60), "new deadline is exact and one warning is rearmed");
    Check(fake.registryWrites == 0 && fake.magWrites == 0 && fake.gammaWrites == 0, "restart policy has no persistence or display side effects");
}
void CountdownRestartRefusesExpiredInactiveAndUnlimited() {
    SessionTimer timer;
    Check(!timer.restart(0, true) && !timer.armed(), "unlimited default cannot restart");
    Check(timer.configure(1, 1000, false) && !timer.restart(1000, false) && !timer.armed(), "inactive configuration cannot start a countdown through restart");
    timer.start(1000);
    Check(!timer.restart(2000, false) && timer.remainingMilliseconds(2000) == 59000, "inactive restart refusal preserves existing deadline");
    Check(!timer.restart(61000, true) && timer.restoreDue(61000) && timer.remainingMilliseconds(61000) == 0, "deadline reached cannot be postponed by restart");
    timer.noteRestoreAttempt(61000);
    Check(!timer.restart(62000, true) && !timer.restoreDue(65999) && timer.restoreDue(66000), "failed-recovery retry cadence survives refused restarts");
    const auto start = (std::numeric_limits<std::uint64_t>::max)() - 100;
    Check(timer.configure(1, start, true) && timer.restart(start + 200, true) && timer.remainingMilliseconds(start + 200) == 60000, "restart tolerates monotonic clock wrap");
}
void CountdownControlBackendEligibility() {
    MagnificationFilter filter;
    Check(filter.initialize() && filter.enable(), "activate");
    SessionTimer timer; Check(timer.configure(1, 0, true), "one-minute countdown");
    const auto reads = fake.magReads, writes = fake.magWrites;
    Check(CanChangeSessionLimit(timer, 1000, filter, BackendVerification::NotChecked, false) && CanRestartCountdown(timer, 1000, filter, BackendVerification::Matches, false), "healthy active countdown can change or restart");
    for (const auto verification : {BackendVerification::Changed, BackendVerification::Unavailable, static_cast<BackendVerification>(99)}) {
        Check(!CanChangeSessionLimit(timer, 1000, filter, verification, false) && !CanRestartCountdown(timer, 1000, filter, verification, false), "unconfirmed backend state cannot extend timing");
    }
    Check(!CanChangeSessionLimit(timer, 1000, filter, BackendVerification::Matches, true) && !CanRestartCountdown(timer, 1000, filter, BackendVerification::Matches, true), "queued restoration blocks deadline edits");
    Check(!CanChangeSessionLimit(timer, 60000, filter, BackendVerification::Matches, false) && !CanRestartCountdown(timer, 60000, filter, BackendVerification::Matches, false), "due restoration wins over new duration or restart");
    Check(fake.magReads == reads && fake.magWrites == writes, "eligibility checks read only cached backend getters");
    fake.magSet = false;
    Check(!filter.setLumaProfile(LumaProfile::GreenDetail) && !CanChangeSessionLimit(timer, 1000, filter, BackendVerification::NotChecked, false), "backend API errors block timing changes while active");
    fake.magSet = true;
    Check(filter.disable() && CanChangeSessionLimit(timer, 60000, filter, BackendVerification::NotChecked, false) && !CanRestartCountdown(timer, 60000, filter, BackendVerification::NotChecked, false), "off/restored state permits configuration but cannot restart");
}
void CountdownControlsPreservePartialRecovery() {
    GammaRampFilter filter;
    Check(filter.initialize(), "initialize");
    fake.writes = {Write::FailAfterApply, Write::Fail};
    Check(!filter.enable() && !filter.isActive() && filter.needsRestore(), "unconfirmed partial-write recovery");
    SessionTimer timer; Check(timer.configure(1, 0, true), "existing timing policy");
    const auto reads = fake.gammaReads, writes = fake.gammaWrites;
    Check(!CanChangeSessionLimit(timer, 1000, filter, BackendVerification::NotChecked, false) && !CanRestartCountdown(timer, 1000, filter, BackendVerification::NotChecked, false) &&
        filter.needsRestore() && fake.mutexOwned && fake.gammaReads == reads && fake.gammaWrites == writes, "timing controls cannot cancel or postpone pending partial recovery");
    Check(filter.disable() && CanChangeSessionLimit(timer, 1000, filter, BackendVerification::Inactive, false), "configuration becomes available after recovery succeeds");
}
void MagnificationRejectsNonFiniteInitialSnapshots() {
    for (const float invalid : {(std::numeric_limits<float>::quiet_NaN)(), (std::numeric_limits<float>::infinity)(), -(std::numeric_limits<float>::infinity)()}) {
        for (int row = 0; row < 5; ++row) for (int column = 0; column < 5; ++column) {
            Reset(); fake.effect.transform[row][column] = invalid;
            MagnificationFilter filter;
            Check(!filter.initialize() && !filter.isActive() && !filter.needsRestore() && fake.magWrites == 0 && fake.uninitializes == 1,
                "every nonfinite original coefficient is rejected with checked initialization cleanup and no color write");
            Check(std::string_view(filter.lastError()).find("non-finite") != std::string_view::npos && filter.statistics().initialization.failures == 1,
                "invalid successful API capture is an explicit diagnosed initialization failure");
        }
    }
}
void MagnificationRejectsNonFiniteActivationSnapshot() {
    MagnificationFilter filter;
    Check(filter.initialize(), "initialize with valid snapshot");
    fake.effect.transform[4][0] = (std::numeric_limits<float>::quiet_NaN)();
    Check(!filter.enable() && !filter.isActive() && !filter.needsRestore() && fake.magWrites == 0,
        "invalid pre-activation capture refuses activation without changing current display state");
    const auto reads = fake.magReads;
    Check(filter.verifyState() == BackendVerification::Inactive && fake.magReads == reads && fake.magWrites == 0,
        "rejected capture does not claim another tool's matrix as owned recovery data");
    fake.effect.transform[4][0] = 0; fake.effect.transform[0][0] = 0.67f;
    const auto valid = fake.effect;
    Check(filter.enable() && filter.disable() && Same(fake.effect, valid), "later valid capture can activate and restores the latest usable original matrix");
}
void NonFiniteCaptureCleanupBlocksFallback() {
    MagnificationFilter magnification;
    GammaRampFilter gamma;
    fake.effect.transform[3][3] = (std::numeric_limits<float>::infinity)(); fake.magUninit = false;
    Check(InitializePreferredBackend(BackendPreference::Automatic, magnification, gamma) == nullptr && fake.dcRequests == 0 && fake.magWrites == 0 && fake.gammaWrites == 0,
        "invalid capture with failed cleanup cannot fall through to another display backend");
    Check(fake.uninitializes >= 2 && !magnification.isActive() && !magnification.needsRestore(), "backend resources remain available for cleanup without claiming display ownership");
    fake.magUninit = true;
    Check(magnification.shutdown(), "cleanup can retry after invalid capture");
}
void SelectedShortcutRetryRecoversConflictWithoutSaving() {
    MagnificationFilter filter;
    Check(filter.initialize() && filter.enable() && SavePreferences(AppPreferences{}), "active filter with saved preferences");
    HotkeyManager manager;
    fake.hotkeyRegisterResults = {false, true};
    Check(!manager.initialize(nullptr, ShortcutProfile::Default) && !manager.matches(ShortcutProfile::Default) && manager.restore(), "startup conflict retains restore binding and incomplete health");
    const auto writes = fake.magWrites, reads = fake.magReads, registryWrites = fake.registryWrites;
    Check(manager.change(ShortcutProfile::Default) && manager.matches(ShortcutProfile::Default) && manager.toggle() == ShortcutBinding{3, 'R'} && manager.restore() == ShortcutBinding{7, 'R'},
        "retry fills missing registration after conflict clears");
    const auto registrations = fake.hotkeyRegisters, unregistrations = fake.hotkeyUnregisters;
    Check(manager.change(ShortcutProfile::Default) && manager.matches(ShortcutProfile::Default) && fake.hotkeyRegisters == registrations && fake.hotkeyUnregisters == unregistrations,
        "already healthy retry does not duplicate or disturb registrations");
    Check(fake.registryWrites == registryWrites && fake.magWrites == writes && fake.magReads == reads && filter.isActive() && filter.needsRestore(), "retry has no settings or display API effects");
    Check(manager.release(), "release test registrations");
}
void SelectedShortcutRetryFailureAndDisabledHealth() {
    HotkeyManager manager;
    fake.hotkeyRegisterResults = {false, true};
    Check(!manager.initialize(nullptr, ShortcutProfile::Default), "partial startup");
    fake.hotkeyRegisterResults = {false};
    Check(!manager.change(ShortcutProfile::Default) && !manager.matches(ShortcutProfile::Default) && !manager.toggle() && manager.restore() == ShortcutBinding{7, 'R'} && fake.hotkeyUnregisters == 0,
        "failed retry preserves existing restore registration and actual partial state");
    Check(manager.change(ShortcutProfile::Default) && manager.matches(ShortcutProfile::Default), "later retry recovers the exact desired pair");
    Check(manager.change(ShortcutProfile::Disabled) && manager.matches(ShortcutProfile::Disabled) && !manager.matches(ShortcutProfile::Default) &&
        !manager.matches(static_cast<ShortcutProfile>(99)) && !manager.accepts(ID_HOTKEY_TOGGLE, 3, 'R'), "disabled profile is applied, invalid profiles are not, and old queued keys remain rejected");
    Check(fake.registryWrites == 0 && fake.magWrites == 0 && fake.gammaWrites == 0 && !CommandMayChangeFilter(ID_TRAY_RETRY_SHORTCUTS), "retry failure policy is read-only for display state");
    Check(manager.release(), "release disabled manager");
}
void ShortcutRegistrationHealthReports() {
    constexpr StatusShortcut expected[] = {StatusShortcut::R, StatusShortcut::F9, StatusShortcut::F10, StatusShortcut::None};
    for (unsigned profile = 0; profile < 4; ++profile) for (unsigned toggle = 0; toggle < 4; ++toggle) for (unsigned restore = 0; restore < 4; ++restore) {
        TrayStatus status; status.shortcutProfile = static_cast<ShortcutProfile>(profile);
        status.toggle = static_cast<StatusShortcut>(toggle); status.restore = static_cast<StatusShortcut>(restore);
        const bool applied = status.toggle == expected[profile] && status.restore == expected[profile];
        Check(StatusShortcutProfileApplied(status) == applied, "registration health distinguishes complete, absent, mixed and disabled combinations");
        Check(TrayStatusJson(status)->find(applied ? "\"shortcut_profile_applied\":true" : "\"shortcut_profile_applied\":false") != std::string::npos,
            "JSON reports registration health from existing cached protocol fields");
    }
    SupportReportState state; state.shortcutProfileApplied = true;
    Check(BuildSupportReport(state).find("Shortcut preference applied: yes") != std::string::npos, "support report carries selected-profile registration health");
    Check(fake.hotkeyRegisters == 0 && fake.hotkeyUnregisters == 0 && fake.magReads == 0 && fake.gammaReads == 0, "health reporting performs no native registration or display read");
}
void PreferenceSerializationSingleAllocation() {
    const AppPreferences preferences{RedMode::StrictRed, LumaProfile::EqualDetail, BackendPreference::Magnification, 240, ShortcutProfile::Disabled, ExpiryWarning::FiveMinutes};
    std::optional<std::string> text;
    bool spareAllocation = false;
    {
        TestMemory::FailNextAllocation fail(1);
        text = SerializePreferences(preferences);
        spareAllocation = TestMemory::AllocationFailurePending();
    }
    Check(text && spareAllocation && text->size() < 256 && ParsePreferences(*text).preferences == preferences,
        "complete maximum-field profile requires only its owned output allocation");
    const auto versionLine = std::string("version=") + std::to_string(PreferencesProfileVersion) + "\r\n";
    Check(text->find(versionLine) != std::string::npos && text->ends_with("expiry_warning=5m\r\n"), "schema metadata and canonical output remain consistent");
}
void BuildInfoJsonAndIsolation() {
    fake.stdoutChunk = 7;
    Check(WriteBuildInfo("0.6.0-rc.1", "0123456789ab", "clean") == 0, "build info handles partial output writes");
    const auto& text = fake.stdoutText;
    Check(text.starts_with("{\"protocol\":1,\"product\":\"RedLight\",\"version\":\"0.6.0-rc.1\"") &&
        text.find("\"build_revision\":\"0123456789ab\",\"source_state\":\"clean\",\"platform\":\"windows-x64\"") != std::string::npos &&
        text.ends_with("\"profile_schema\":3,\"status_protocol\":1}\n"), "build info carries a complete stable machine-readable identity");
    Check(fake.magInitializes == 0 && fake.dcRequests == 0 && fake.magReads == 0 && fake.gammaReads == 0 &&
        fake.magWrites == 0 && fake.gammaWrites == 0 && fake.mutexCreates == 0 && fake.statusSends == 0 && fake.profileOpens == 0 &&
        fake.registryReads == 0 && fake.registryWrites == 0 && fake.clipOpens == 0 && fake.hotkeyRegisters == 0 && fake.sessionRegisters == 0,
        "metadata output needs no instance, preferences, clipboard, registration, profile file, or display operation");
}
void BuildInfoEscapingAndBounds() {
    Check(WriteBuildInfo("v\"\\\n", "\xc3\xa9", std::string_view("a\0b", 3)) == 0 &&
        fake.stdoutText.find("\"version\":\"v\\\"\\\\\\u000a\"") != std::string::npos &&
        fake.stdoutText.find("\"build_revision\":\"\xc3\xa9\",\"source_state\":\"a\\u0000b\"") != std::string::npos,
        "identity fields preserve valid Unicode and escape quotes, slashes, and controls");
    Reset();
    const std::string maximum(254, 'x');
    Check(WriteBuildInfo(maximum, maximum, maximum) == 0 && fake.stdoutText.size() < 1024 && fake.stdoutText.ends_with("}\n"),
        "three largest accepted encoded identity fields still produce a complete record");
    for (const auto& invalid : {std::string{}, std::string(255, 'x'), std::string(43, '\n'), std::string("\xff")}) {
        for (unsigned field = 0; field < 3; ++field) {
            Reset();
            Check(WriteBuildInfo(field == 0 ? invalid : "version", field == 1 ? invalid : "revision", field == 2 ? invalid : "state") == 1 &&
                fake.stdoutText.empty() && fake.stdoutLookups == 0, "invalid, empty, or oversized field fails before any output lookup/write");
        }
    }
}
void BuildInfoNoCppAllocation() {
    fake.stdoutText.reserve(1024); // The fake sink must not consume the allocation fault.
    int result = 1;
    bool allocationUnused = false;
    {
        TestMemory::FailNextAllocation fail;
        result = WriteBuildInfo("0.6.0-rc.1", "unknown", "archive");
        allocationUnused = TestMemory::AllocationFailurePending();
    }
    Check(result == 0 && allocationUnused && fake.stdoutText.ends_with("}\n"), "build metadata preparation and output do not allocate C++ heap storage");
}
void BuildInfoOutputFailures() {
    fake.stdoutWrite = false;
    Check(WriteBuildInfo("v", "r", "s") == 1 && fake.stdoutText.empty(), "output failure cannot report successful build metadata");
    Reset(); fake.stdoutChunk = 5; fake.stdoutFailOnCall = 2;
    Check(WriteBuildInfo("v", "r", "s") == 1 && fake.stdoutText.size() == 5, "interrupted output reports failure and leaves only the failed prefix");
    Reset(); fake.stdoutValid = false; fake.consoleAttach = false;
    Check(WriteBuildInfo("v", "r", "s") == 1 && fake.stdoutText.empty(), "no inherited output or parent console is a noninteractive failure");
}
void BuildInfoArguments() {
    const std::wstring_view args[] = {L"--build-info"};
    const auto options = ParseStartupOptions(args);
    Check(options.action == StartupAction::BuildInfo && options.errorOutput == StartupErrorOutput::Json && options.profilePath.empty() && !options.backendOverride,
        "build info is a standalone read-only JSON action");
    for (const auto malformed : {L"--build-info=extra", L"--build-info-extra"}) {
        const std::wstring_view bad[] = {malformed};
        Check(ParseStartupOptions(bad).action == StartupAction::Invalid && ParseStartupOptions(bad).errorOutput == StartupErrorOutput::Json,
            "malformed metadata requests keep noninteractive JSON error policy");
    }
    const std::wstring_view combined[] = {L"--export-profile", L"--build-info"};
    Check(ParseStartupOptions(combined).action == StartupAction::Invalid && ParseStartupOptions(combined).errorOutput == StartupErrorOutput::Json,
        "conflicting metadata action takes JSON error precedence");
    fake.argumentsCount = 2; fake.arguments[1] = const_cast<LPWSTR>(L"--build-info");
    Check(GetStartupOptions().action == StartupAction::BuildInfo && fake.argumentFrees == 1 && !fake.argumentsOutstanding,
        "native argument ownership is released for build metadata requests");
}
void VerificationTimingStatesAndPrecision() {
    BackendVerificationCache cache;
    Check(cache.result().state == BackendVerification::NotChecked && !cache.ageSeconds(5000) && !cache.durationMilliseconds(),
        "an unchecked cache has no invented check time or duration");
    for (const auto state : {BackendVerification::Inactive, BackendVerification::Matches, BackendVerification::Changed, BackendVerification::Unavailable}) {
        BackendVerificationReport report{state, BackendReadbackDetails{25, 2, 0.125, true, true, false}, 5};
        cache.record(report, 1000, 1075);
        Check(cache.result().state == state && cache.result().errorCode == 5 && cache.result().readback->mismatches == 2 &&
            cache.result().readback->nonFiniteValues && cache.durationMilliseconds() == 75 && cache.ageSeconds(1075) == 0 &&
            cache.ageSeconds(2074) == 0 && cache.ageSeconds(2075) == 1,
            "every completed outcome preserves its result with duration and whole-second age since completion");
    }
    cache.record({BackendVerification::Inactive}, 0, 0);
    Check(cache.ageSeconds(0) == 0 && cache.durationMilliseconds() == 0, "a check at monotonic zero is recorded, not confused with absence");
}
void VerificationTimingWrapAndInvalidation() {
    BackendVerificationCache cache;
    const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
    cache.record({BackendVerification::Changed, BackendReadbackDetails{768, 1, 1, false, true, true}, 0}, maximum - 4, 3);
    Check(cache.durationMilliseconds() == 8 && cache.ageSeconds(2003) == 2, "duration and age tolerate unsigned monotonic-clock wrap");
    cache = {};
    Check(cache.result().state == BackendVerification::NotChecked && !cache.result().readback && cache.result().errorCode == 0 &&
        !cache.ageSeconds(99999) && !cache.durationMilliseconds(), "filter-operation invalidation clears the result and all timing together");
    cache.record({BackendVerification::Unavailable, std::nullopt, 123}, 10, 20);
    cache.record({}, 30, 40);
    Check(cache.result().state == BackendVerification::NotChecked && cache.result().errorCode == 0 && !cache.ageSeconds(50),
        "an unchecked replacement cannot retain an earlier failure timestamp");
}
void VerificationTimingReportingPreservesBackend() {
    MagnificationFilter filter;
    Check(filter.initialize() && filter.enable(), "prepare active backend for explicit verification");
    BackendVerificationCache cache;
    cache.record(filter.inspectState(), 100, 115);
    const int reads = fake.magReads, writes = fake.magWrites;
    const auto statistics = filter.statistics();
    SupportReportState state;
    state.active = filter.isActive(); state.needsRestore = filter.needsRestore();
    state.verification = cache.result().state; state.readback = cache.result().readback;
    state.verificationAgeSeconds = cache.ageSeconds(6115); state.verificationDurationMilliseconds = cache.durationMilliseconds();
    const auto report = BuildSupportReport(state);
    Check(report.find("Verification age: 6 seconds since check completed") != std::string::npos &&
        report.find("Verification duration: 15 milliseconds") != std::string::npos && report.find("State: ON") != std::string::npos,
        "support report distinguishes cached age and call duration from active backend state");
    Check(fake.magReads == reads && fake.magWrites == writes && filter.statistics() == statistics && filter.isActive() && filter.needsRestore(),
        "viewing timing makes no display operation or counter/ownership change");
    cache = {};
    Check(filter.isActive() && filter.needsRestore() && filter.disable(), "invalidating only the cache preserves the original restoration snapshot");
}
void VerificationTimingUnknownAndZeroReports() {
    SupportReportState state;
    auto report = BuildSupportReport(state);
    Check(report.find("Verification age: not recorded\r\nVerification duration: not recorded") != std::string::npos,
        "reports do not present an unmeasured check as zero milliseconds old");
    state.verification = BackendVerification::Unavailable;
    state.verificationError = 123;
    state.verificationAgeSeconds = 0; state.verificationDurationMilliseconds = 0;
    report = BuildSupportReport(state);
    Check(report.find("Verification age: 0 seconds since check completed\r\nVerification duration: 0 milliseconds") != std::string::npos &&
        report.find("Verification error code: 123") != std::string::npos,
        "an immediate failed check retains genuine zero timing and its API error");
}
} // namespace

DWORD GetLastError() { return fake.lastError; }
void SetLastError(DWORD value) { fake.lastError = value; }
BOOL WTSRegisterSessionNotification(HWND owner, DWORD flags) {
    ++fake.sessionRegisters; fake.sessionFlags = flags;
    if (fake.sessionCallbackTarget) {
        auto* target = fake.sessionCallbackTarget; fake.sessionCallbackTarget = nullptr;
        fake.nestedSessionEnsure = target->ensure(owner);
        fake.nestedSessionRelease = target->release();
    }
    if (fake.sessionRegisterThrows) throw std::runtime_error("injected pre-registration failure");
    if (!fake.sessionRegister) { fake.lastError = fake.sessionError; return FALSE; }
    fake.sessionRegisteredOwner = owner;
    return 1;
}
BOOL WTSUnRegisterSessionNotification(HWND owner) {
    ++fake.sessionUnregisters; fake.sessionUnregisterOwner = owner;
    if (!fake.sessionUnregister || owner != fake.sessionRegisteredOwner) { fake.lastError = fake.sessionError; return FALSE; }
    fake.sessionRegisteredOwner = nullptr;
    return 1;
}
LPWSTR GetCommandLineW() { return fake.arguments[0]; }
LPWSTR* CommandLineToArgvW(const wchar_t*, int* count) {
    ++fake.argumentReads; *count = fake.argumentsCount;
    if (!fake.argumentsRead) return nullptr;
    fake.argumentsOutstanding = true;
    return fake.arguments.data();
}
HLOCAL LocalFree(HLOCAL block) {
    ++fake.argumentFrees;
    if (!fake.argumentsFree) return block;
    fake.argumentsOutstanding = false;
    return nullptr;
}
HANDLE CreateFileW(const wchar_t* path, DWORD access, DWORD share, void*, DWORD disposition, DWORD flags, HANDLE) {
    ++fake.profileOpens; fake.profilePath = path; fake.profileAccess = access; fake.profileShare = share;
    fake.profileDisposition = disposition; fake.profileFlags = flags; fake.profileModeAtOpen = fake.errorMode;
    if (!fake.profileOpen) return INVALID_HANDLE_VALUE;
    fake.profileHandleOpen = true;
    return &fake.profileHandleOpen;
}
DWORD GetFileType(HANDLE) { fake.lastError = fake.profileTypeError; return fake.profileType; }
BOOL ReadFile(HANDLE, void* data, DWORD size, DWORD* received, void*) {
    ++fake.profileReads; *received = 0;
    if (!fake.profileRead) { fake.lastError = ERROR_ACCESS_DENIED; return FALSE; }
    if (fake.profileOverRead) { *received = size + 1; return 1; }
    if (fake.profilePosition == fake.profileData.size() && fake.profileEofError) { fake.lastError = ERROR_HANDLE_EOF; return FALSE; }
    *received = static_cast<DWORD>((std::min)((std::min)(fake.profileData.size() - fake.profilePosition, static_cast<std::size_t>(size)), static_cast<std::size_t>(fake.profileChunk)));
    std::memcpy(data, fake.profileData.data() + fake.profilePosition, *received);
    fake.profilePosition += *received;
    return 1;
}
DWORD GetThreadErrorMode() { return fake.errorMode; }
BOOL SetThreadErrorMode(DWORD mode, DWORD* previous) {
    ++fake.errorModeCalls;
    if (!(previous ? fake.errorModeSet : fake.errorModeRestore)) { fake.lastError = ERROR_ACCESS_DENIED; return FALSE; }
    if (previous) *previous = fake.errorMode;
    fake.errorMode = mode;
    return 1;
}
HANDLE CreateMutexW(void*, BOOL initialOwner, const wchar_t* name) {
    ++fake.mutexCreates; fake.mutexName = name; fake.mutexInitialOwner = initialOwner;
    if (!fake.mutexCreate) return nullptr;
    fake.mutexOpen = true;
    return &fake.mutexOpen;
}
DWORD WaitForSingleObject(HANDLE, DWORD milliseconds) {
    ++fake.mutexWaits; fake.mutexWaitMilliseconds = milliseconds;
    if (fake.mutexWaitResult == WAIT_OBJECT_0 || fake.mutexWaitResult == WAIT_ABANDONED) fake.mutexOwned = true;
    return fake.mutexWaitResult;
}
BOOL ReleaseMutex(HANDLE) {
    ++fake.mutexReleases; fake.gammaWritesAtMutexRelease = fake.gammaWrites;
    if (!fake.mutexRelease || !fake.mutexOwned) return FALSE;
    fake.mutexOwned = false;
    return 1;
}
BOOL CloseHandle(HANDLE handle) {
    if (handle == &fake.profileHandleOpen) {
        ++fake.profileCloses;
        if (!fake.profileClose) { fake.lastError = ERROR_ACCESS_DENIED; return FALSE; }
        fake.profileHandleOpen = false;
        return 1;
    }
    ++fake.mutexCloses;
    if (fake.mutexOwned) ++fake.mutexClosesWhileOwned;
    if (!fake.mutexClose) return FALSE;
    fake.mutexOpen = false;
    return 1;
}
HANDLE GetStdHandle(DWORD) {
    ++fake.stdoutLookups;
    const bool valid = fake.consoleAttaches ? fake.attachedOutputValid : fake.stdoutValid;
    return valid ? &fake : (fake.stdoutInvalidHandle ? INVALID_HANDLE_VALUE : nullptr);
}
BOOL AttachConsole(DWORD) { ++fake.consoleAttaches; return fake.consoleAttach; }
BOOL FreeConsole() { ++fake.consoleFrees; return fake.consoleFree; }
BOOL WriteFile(HANDLE, const void* data, DWORD size, DWORD* written, void*) {
    ++fake.stdoutWrites;
    if (fake.profileHandleOpen) ++fake.stdoutWhileProfileOpen;
    fake.stdoutErrorMode = fake.errorMode;
    if (!fake.stdoutWrite || fake.stdoutFailOnCall == fake.stdoutWrites) { *written = 0; return FALSE; }
    if (fake.stdoutOverWrite) { *written = size + 1; return 1; }
    *written = fake.stdoutZeroWrite ? 0 : (std::min)(size, fake.stdoutChunk);
    fake.stdoutText.append(static_cast<const char*>(data), *written);
    return 1;
}
HWND FindWindowA(const char*, const char*) { return fake.trayExists ? &fake : nullptr; }
LRESULT SendMessageTimeoutA(HWND, unsigned message, WPARAM wparam, LPARAM lparam, unsigned flags, unsigned timeout, DWORD_PTR* reply) {
    ++fake.statusSends; fake.statusMessageId = message; fake.statusWparam = wparam; fake.statusLparam = lparam;
    fake.statusFlags = flags; fake.statusTimeout = timeout; *reply = fake.statusReply;
    return fake.statusMessage;
}
HDC GetDC(HWND) { ++fake.dcRequests; return fake.dc ? &fake : nullptr; }
int ReleaseDC(HWND, HDC) { ++fake.releases; return fake.release ? 1 : 0; }
BOOL GetDeviceGammaRamp(HDC, void* ramp) {
    ++fake.gammaReads;
    if (!fake.gammaGet) return 0;
    std::memcpy(ramp, fake.ramp, sizeof(fake.ramp)); return 1;
}
BOOL SetDeviceGammaRamp(HDC, void* ramp) {
    ++fake.gammaWrites;
    if (!fake.mutexOwned) ++fake.gammaUnownedWrites;
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
    ++fake.magReads;
    if (!fake.magGet) return 0;
    *effect = fake.effect; return 1;
}
BOOL MagSetFullscreenColorEffect(MAGCOLOREFFECT* effect) {
    ++fake.magWrites;
    if (!fake.magSet) return 0;
    fake.effect = *effect; return 1;
}
int MultiByteToWideChar(unsigned, DWORD, const char* text, int length, wchar_t* output, int) {
    ++fake.clipConversions;
    if (!fake.clipConvert || (output && !fake.clipSecondConvert)) return 0;
    if (output) for (int i = 0; i < length; ++i) output[i] = static_cast<unsigned char>(text[i]);
    return length;
}
HGLOBAL GlobalAlloc(unsigned, std::size_t bytes) {
    ++fake.clipAllocations;
    if (!fake.clipAllocate) return nullptr;
    fake.clipMemory.resize(bytes / sizeof(wchar_t));
    return &fake.clipMemory;
}
void* GlobalLock(HGLOBAL) { return fake.clipLock ? fake.clipMemory.data() : nullptr; }
BOOL GlobalUnlock(HGLOBAL) { ++fake.clipUnlocks; return 0; }
HGLOBAL GlobalFree(HGLOBAL) { ++fake.clipFrees; return nullptr; }
BOOL OpenClipboard(HWND) { ++fake.clipOpens; return fake.clipOpen; }
BOOL EmptyClipboard() { ++fake.clipEmpties; return fake.clipEmpty; }
HANDLE SetClipboardData(unsigned, HANDLE handle) { ++fake.clipSets; return fake.clipSet ? handle : nullptr; }
BOOL CloseClipboard() { ++fake.clipCloses; return fake.clipClose; }
HANDLE GetClipboardData(unsigned) { ++fake.clipReads; return fake.clipRead ? &fake.clipMemory : nullptr; }
std::size_t GlobalSize(HGLOBAL) { return fake.clipMemory.size() * sizeof(wchar_t); }
int WideCharToMultiByte(unsigned, DWORD, const wchar_t* input, int length, char* output, int capacity, const char*, BOOL*) {
    if (!fake.clipWideConvert || length > capacity) return 0;
    for (int i = 0; i < length; ++i) output[i] = static_cast<char>(input[i]);
    return length;
}
LSTATUS RegGetValueW(HKEY, const wchar_t*, const wchar_t* name, DWORD flags, DWORD* type, void* output, DWORD* size) {
    ++fake.registryReads;
    if (!fake.registryRead) return ERROR_ACCESS_DENIED;
    const auto found = fake.registry.find(name);
    if (found == fake.registry.end()) return ERROR_FILE_NOT_FOUND;
    const auto& value = found->second;
    if ((flags == RRF_RT_REG_BINARY && value.type != REG_BINARY) ||
        (flags == RRF_RT_REG_DWORD && value.type != REG_DWORD)) return ERROR_UNSUPPORTED_TYPE;
    if (type) *type = value.type;
    const DWORD needed = static_cast<DWORD>(value.bytes.size());
    if (*size < needed) { *size = needed; return ERROR_MORE_DATA; }
    if (output) std::memcpy(output, value.bytes.data(), needed);
    *size = needed;
    return ERROR_SUCCESS;
}
LSTATUS RegSetKeyValueW(HKEY, const wchar_t*, const wchar_t* name, DWORD type, const void* data, DWORD bytes) {
    ++fake.registryWrites;
    if (!fake.registryWrite) return ERROR_ACCESS_DENIED;
    const auto* begin = static_cast<const char*>(data);
    fake.registry[name] = {type, std::vector<char>(begin, begin + bytes)};
    return ERROR_SUCCESS;
}
BOOL RegisterHotKey(HWND, int id, unsigned modifiers, unsigned key) {
    ++fake.hotkeyRegisters;
    bool success = true;
    if (!fake.hotkeyRegisterResults.empty()) { success = fake.hotkeyRegisterResults.front(); fake.hotkeyRegisterResults.pop_front(); }
    if (!success || fake.registeredHotkeys.contains(id)) return 0;
    fake.registeredHotkeys[id] = {modifiers & 0xf, key};
    return 1;
}
BOOL UnregisterHotKey(HWND, int id) {
    ++fake.hotkeyUnregisters;
    bool success = true;
    if (!fake.hotkeyUnregisterResults.empty()) { success = fake.hotkeyUnregisterResults.front(); fake.hotkeyUnregisterResults.pop_front(); }
    if (!success) return 0;
    return fake.registeredHotkeys.erase(id) != 0;
}
namespace Diagnostics {
void Log(const char*) noexcept {}
void LogFormat(const char*, ...) noexcept { if (fake.diagnosticsClobbersError) fake.lastError = ERROR_DISK_FULL; }
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
        {"Gamma ownership lifecycle", GammaOwnershipLifecycle}, {"Gamma ownership conflict/wait failure", GammaOwnershipConflictAndWaitFailure},
        {"Gamma ownership creation failure", GammaOwnershipCreationFailure}, {"Gamma ownership capture cleanup", GammaOwnershipCaptureFailuresReleaseGuard},
        {"Gamma ownership failed restoration", GammaOwnershipRestoreFailureRetainsGuard}, {"Gamma ownership cleanup retry", GammaOwnershipCleanupRetryDoesNotRewrite},
        {"Gamma rejected-wait cleanup", GammaOwnershipRejectedWaitCleanupRetry}, {"Gamma abandoned ownership", GammaOwnershipAbandonmentRefusesCapture},
        {"Gamma abandoned-owner cleanup", GammaOwnershipAbandonmentCleanupFailure},
        {"Session timer boundaries", SessionTimerBoundaries},
        {"Session timer retries", SessionTimerRetryAndCancellation}, {"Session timer clock", SessionTimerChangesAndClockWrap},
        {"Backend default selection", BackendSelectionPolicies}, {"Backend strict/fallback selection", BackendSelectionStrictAndFallback},
        {"Backend cleanup failure", BackendSelectionCleanupFailure}, {"Backend command line", BackendCommandLine},
        {"Luma preset output", LumaPresetOutput}, {"Luma preset failure/restoration", LumaPresetFailureAndRestore},
        {"Luma preset strict/gamma behavior", LumaPresetInStrictModeAndGamma},
        {"Support report states", SupportReportStates}, {"Clipboard allocation/open failures", ClipboardAllocationAndOpenFailures},
        {"Clipboard ownership transfer", ClipboardTransferOwnership}, {"Clipboard conversion failure", ClipboardConversionFailure},
        {"Preference profile round trip", PreferenceProfileRoundTrip}, {"Preference profile validation", PreferenceProfileStrictValidation},
        {"Preference profile text conventions", PreferenceProfileWhitespaceAndOrder},
        {"Preferences migration", PreferencesStoreDefaultsAndMigration}, {"Preferences single-record save", PreferencesStoreFailureAtomicity},
        {"Preferences corruption", PreferencesStoreCorruption}, {"Invalid legacy preferences", PreferencesStoreInvalidLegacy},
        {"Clipboard profile read", ClipboardProfileRead}, {"Clipboard read bounds", ClipboardReadBoundsAndFailures},
        {"Preference import recovery gate", PreferencesImportRecoveryGate}, {"Preference import validation gate", PreferencesImportValidationGate},
        {"Magnification read-only verification", MagnificationVerificationReadOnly}, {"Matrix verification nonfinite values", MagnificationVerificationFiniteValues},
        {"Gamma read-only verification", GammaVerificationReadOnly}, {"Verification status priority", VerificationStatusPriority},
        {"Custom session-limit input", CustomSessionLimitParsing},
        {"Shortcut registration and stale messages", ShortcutRegistrationAndStaleMessages}, {"Shortcut rollback", ShortcutRegistrationRollback},
        {"Shortcut partial rollback/retry", ShortcutPartialRollbackAndRetry}, {"Shortcut startup and schema migration", ShortcutStartupPartialAndSchemaMigration},
        {"Default preference restoration", DefaultPreferencesRestoreActiveGamma},
        {"Preference save allocation failure", PreferenceSaveAllocationFailure}, {"Preference import allocation failure", PreferenceImportAllocationFailure},
        {"Read-only UI allocation failure", ReadOnlyUiFailureRetainsFilter}, {"Mutating UI exception recovery", MutatingUiFailureRestoresAfterUnwind},
        {"UI restore retry after failure", UiRecoveryFailureKeepsSnapshot}, {"Nested UI flag ownership", NestedUiFlagsAndFailureHandler},
        {"Complete reports under allocation failure", SupportReportAllocationFailuresNeverReturnPartial},
        {"Status protocol round trip", StatusProtocolRoundTrip}, {"Status protocol validation", StatusProtocolRejectsInvalid},
        {"Conservative status JSON", StatusJsonConservativeStates}, {"Standard output ownership", StandardOutputOwnership},
        {"Standard output failures", StandardOutputFailures}, {"Existing-instance status query", StatusQueryExistingInstanceOnly},
        {"Status query failures", StatusQueryFailures},
        {"Expiry warning deadline policy", ExpiryWarningBoundariesAndDeadline}, {"Expiry warning short sessions/clock", ExpiryWarningShortSessionsAndClockWrap},
        {"Expiry warning schema migration", ExpiryWarningSchemaMigrationAndValidation},
        {"Magnification operation counters", MagnificationStatisticsAndSafeReset}, {"Gamma operation counters", GammaStatisticsAndCleanupReset},
        {"Backend counters saturation/reset guard", BackendStatisticsSaturationAndInProgress}, {"Backend counters support report", BackendStatisticsSupportReport},
        {"Native startup argument ownership", NativeStartupArgumentOwnership}, {"Native startup argument failures", NativeStartupArgumentFailures},
        {"Profile validation owned path", ProfileValidationOwnedPathArguments}, {"Profile validation read-only file", ProfileFileValidationReadOnly},
        {"Profile validation file bounds/encoding", ProfileFileValidationBoundsAndEncoding}, {"Profile validation I/O failures", ProfileFileValidationIoFailures},
        {"Profile validation output failures", ProfileFileValidationOutputFailures}, {"Bounded JSON string encoding", BoundedJsonStringEncoding},
        {"UTF-8 scalar boundaries", Utf8ScalarBoundaries}, {"Strict profile encoding", ProfileUtf8Validation}, {"Clipboard embedded NUL rejection", ClipboardEmbeddedNulRejection},
        {"Saved profile export defaults/migration", SavedProfileExportDefaultsAndMigration}, {"Saved profile export failure isolation", SavedProfileExportPreservesRecoveryOnFailures},
        {"Saved profile export command policy", SavedProfileExportCommandPolicy},
        {"Detailed matrix read-back", DetailedMatrixVerification}, {"Detailed gamma read-back", DetailedGammaVerification},
        {"Comparison tolerance/finite range", ComparisonToleranceAndFiniteRange}, {"Detailed verification support report", DetailedVerificationReportFormatting},
        {"Session notification ownership", SessionNotificationOwnership}, {"Session notification failure/retry", SessionNotificationFailureRecovery},
        {"Session notification reentrancy/unwind", SessionNotificationReentrancyAndUnwind}, {"Session notification support report", SessionNotificationReport},
        {"Read-only exception with queued restore", ReadOnlyFailureHonorsQueuedRestore}, {"Queued restore under outer busy scope", QueuedRestoreRetainsOuterBusyOwnership},
        {"Gamma preferred Luma without output changes", GammaPreferredLumaWithoutBackendChanges}, {"Live Luma preference commit rules", MagnificationLumaPreferenceCommitRules},
        {"Gamma preferred Luma during recovery", GammaPreferredLumaDuringRecoveryAndSaveFailure},
        {"Countdown restart deadline/warning", CountdownRestartResetsDeadlineAndWarning}, {"Countdown restart refusal/clock", CountdownRestartRefusesExpiredInactiveAndUnlimited},
        {"Countdown backend eligibility", CountdownControlBackendEligibility}, {"Countdown partial-recovery guard", CountdownControlsPreservePartialRecovery},
        {"Nonfinite initial restoration snapshots", MagnificationRejectsNonFiniteInitialSnapshots}, {"Nonfinite activation restoration snapshot", MagnificationRejectsNonFiniteActivationSnapshot},
        {"Nonfinite snapshot cleanup/fallback gate", NonFiniteCaptureCleanupBlocksFallback},
        {"Selected shortcut retry without persistence", SelectedShortcutRetryRecoversConflictWithoutSaving}, {"Selected shortcut retry failure/disabled", SelectedShortcutRetryFailureAndDisabledHealth},
        {"Shortcut registration health reports", ShortcutRegistrationHealthReports},
        {"Single-allocation preference serialization", PreferenceSerializationSingleAllocation},
        {"Build metadata JSON and isolation", BuildInfoJsonAndIsolation}, {"Build metadata escaping and bounds", BuildInfoEscapingAndBounds},
        {"Build metadata without C++ allocation", BuildInfoNoCppAllocation}, {"Build metadata output failures", BuildInfoOutputFailures},
        {"Build metadata arguments", BuildInfoArguments},
        {"Verification timing outcomes and precision", VerificationTimingStatesAndPrecision}, {"Verification timing wrap/invalidation", VerificationTimingWrapAndInvalidation},
        {"Verification timing read-only reporting", VerificationTimingReportingPreservesBackend}, {"Verification timing missing/zero reports", VerificationTimingUnknownAndZeroReports},
        {"Command line", Arguments}
    };
    int failed = 0;
    for (const auto& [name, run] : tests) {
        Reset();
        try {
            run();
            Check(fake.gammaUnownedWrites == 0 && fake.mutexClosesWhileOwned == 0, "all gamma writes hold ownership; owned mutex handles are never closed");
            Check(!fake.sessionRegisteredOwner, "session registration is released while its fake owner remains valid");
            std::printf("PASS %s\n", name);
        }
        catch (const std::exception& e) { ++failed; std::printf("FAIL %s: %s\n", name, e.what()); }
    }
    return failed ? 1 : 0;
}
