#include <windows.h>
#include <shellapi.h>
#include <wtsapi32.h>
#include <magnification.h>
#include "WindowedProbeSession.h"
#include "WindowedTopology.h"
#include "WindowedRedControl.h"
#include "CountdownTimerRegistration.h"
#include "SessionNotifications.h"
#include "ScheduleSession.h"
#include "InstanceName.h"
#include <array>
#include <cwchar>

namespace {
using namespace MonitorPrototype;
constexpr wchar_t PanelClass[] = L"RedLight.WindowedProbe.Controls.v1", HostClass[] = L"RedLight.WindowedProbe.Host.v1";
constexpr UINT_PTR TimerId = 1;
constexpr int StopId = 1, HotkeyId = 1;
constexpr wchar_t Help[] = L"Build-only experimental per-monitor probe. Hardware behavior is unqualified.\n\n"
    L"Required: --monitor N --seconds S\nN: 1-8 (entry in the reviewed snapshot, not Windows' display number).\nS: 1-30 seconds.\n"
    L"Optional: --mode strict|luma --luma classic|green|equal --brightness 1-100\n\n"
    L"Every run requires review. Stop/close, Ctrl+Alt+Shift+F12, deadline, or environment changes request removal; failed cleanup remains retryable. "
    L"No fullscreen/gamma/cursor/input transform is written. Do not run without separate authorization for native display testing.";

LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM) noexcept;
bool Notice(HWND owner, const wchar_t* text, UINT flags) noexcept {
    SetLastError(ERROR_SUCCESS);
    if (MessageBoxW(owner, text, L"Windowed probe", flags)) return true;
    OutputDebugStringW(L"Probe notice dialog failed.\n"); return false;
}
class NativeApi {
public:
    HINSTANCE instance = nullptr;
    HWND panel = nullptr, status = nullptr, button = nullptr;
    bool placingPanel = false;
    const wchar_t* failure = nullptr;
    DWORD error = 0;
    const wchar_t* cleanupFailure = nullptr;
    DWORD cleanupError = 0;
    std::uint64_t now() const noexcept { return GetTickCount64(); }
    bool acquireOwnership() noexcept {
        SetLastError(ERROR_SUCCESS);
        sessionMutex_ = CreateMutexW(nullptr, TRUE, RedLightInstanceMutexName);
        const DWORD sessionError = GetLastError();
        if (!sessionMutex_) return fail(L"Same-session exclusion could not be acquired", sessionError);
        if (sessionError == ERROR_ALREADY_EXISTS) return fail(L"Another RedLight instance/startup guard exists");
        sessionOwned_ = true;
        return true;
    }
    bool review(const ProbeOptions& options) noexcept {
        const auto captured = ReadWindowedTopology();
        if (!captured.snapshot) return fail(L"A complete PMv2 topology snapshot could not be confirmed", captured.nativeError);
        if (captured.snapshot->count < 2 || options.monitor > captured.snapshot->count) return fail(L"This run requires a selected subset and an unselected recovery monitor");
        reviewed_ = *captured.snapshot; selected_ = options.monitor - 1; recovery_ = selected_ == 0 ? 1u : 0u;
        if (!positionPanel()) return false;
        wchar_t text[2400] = {}; std::size_t used = 0;
        const auto append = [&](const wchar_t* format, auto... values) {
            const int length = std::swprintf(text + used, std::size(text) - used, format, values...);
            if (length < 0 || static_cast<std::size_t>(length) >= std::size(text) - used) return false;
            used += static_cast<std::size_t>(length); return true;
        };
        if (!append(L"Experimental windowed copy; output, cursor and input behavior are unqualified.\n\nSnapshot entries:\n")) return fail(L"Review could not be formatted completely");
        for (unsigned index = 0; index < reviewed_.count; ++index) {
            const auto& output = reviewed_.outputs[index]; const auto& bounds = output.region.bounds;
            if (!append(L"%u: %ls [%ld,%ld to %ld,%ld]%ls\n", index + 1, output.gdiName.data(), static_cast<long>(bounds.left), static_cast<long>(bounds.top),
                static_cast<long>(bounds.right), static_cast<long>(bounds.bottom), index == selected_ ? L"  SELECTED" : L"")) return fail(L"Review could not be formatted completely");
        }
        if (!append(L"\nRequest entry %u for %u seconds (best-effort timer)?\nMode: %ls / %ls; red brightness: %u%%.\n\n"
            L"Stop/close or Ctrl+Alt+Shift+F12 cancels. The recovery panel stays on an unselected monitor. "
            L"Topology/session/power/DPI changes cancel; no restart is queued.\n"
            L"Only a checked opaque copy is requested. No global color, gamma, cursor or input setter is called. "
            L"External tools and hardware may still affect either output. Continue only for separately authorized native testing.",
            options.monitor, options.seconds, options.mode == RedMode::StrictRed ? L"Strict Red" : L"Luma Red",
            options.profile == LumaProfile::Classic ? L"Classic" : (options.profile == LumaProfile::GreenDetail ? L"Green detail" : L"Equal detail"), options.brightness))
            return fail(L"Review could not be formatted completely");
        if (used >= std::size(text)) return fail(L"Review could not be formatted completely");
        text[used] = L'\0'; text[std::size(text) - 1] = L'\0'; // Explicit native string boundary after complete formatting.
        SetLastError(ERROR_SUCCESS);
        const int answer = MessageBoxW(panel, text, L"Review isolated display experiment", MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING);
        if (!answer) return fail(L"Review dialog failed", GetLastError());
        return answer == IDYES; // No/close is cancellation, not consent.
    }
    bool createHidden(const ProbeOptions&) noexcept {
        if (!acquireGamma()) return false; // Only after explicit Yes; review does not take global Gamma ownership.
        const auto fresh = ReadWindowedTopology();
        if (!fresh.snapshot || *fresh.snapshot != reviewed_) return fail(L"The reviewed topology changed or is unavailable", fresh.nativeError);
        const auto& bounds = reviewed_.outputs[selected_].region.bounds;
        SetLastError(ERROR_SUCCESS);
        host_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST,
            HostClass, L"Isolated red copy", WS_POPUP | WS_CLIPCHILDREN, bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top,
            nullptr, nullptr, instance, nullptr);
        return host_ ? true : fail(L"Hidden capture host could not be created", GetLastError());
    }
    bool prepareControl(const ProbeOptions& options) noexcept {
        SetLastError(ERROR_SUCCESS);
        if (!SetLayeredWindowAttributes(host_, 0, 255, LWA_ALPHA)) return fail(L"Full host opacity could not be set", GetLastError());
        if (!confirmHost(false)) return false;
        std::array<Region, MaxTopologyOutputs> regions{};
        for (unsigned index = 0; index < reviewed_.count; ++index) { regions[index] = reviewed_.outputs[index].region; regions[index].selected = index == selected_; }
        const std::array hosts{host_}; // The panel is outside the source; movement cancels the run.
        if (!control_.prepare(host_, instance, std::span<const Region>(regions.data(), reviewed_.count), {1, 1, sessionOwned_ && gammaOwned_, true, true},
            hosts, options.mode, options.profile, options.brightness)) return fail(L"Hidden red-only control configuration/read-back failed");
        return neutralGlobalState();
    }
    bool installRecovery() noexcept {
        if (!hooks_.ensure(panel)) return fail(L"Session recovery notification could not be registered", hooks_.lastError());
        SetLastError(ERROR_SUCCESS);
        if (!RegisterHotKey(panel, HotkeyId, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, VK_F12)) return fail(L"The mandatory stop shortcut is unavailable", GetLastError());
        hotkey_ = true;
        if (!timer_.install(panel, TimerId, 100)) return fail(L"The bounded probe timer could not be confirmed", timer_.lastFailure() ? timer_.lastFailure()->errorCode : 0);
        return true;
    }
    bool confirmReviewed() noexcept {
        const auto began = now();
        const auto fresh = ReadWindowedTopology();
        if (!fresh.snapshot || *fresh.snapshot != reviewed_) return fail(L"Topology changed before visibility; no retargeting is allowed", fresh.nativeError);
        const auto session = ReadScheduleSession();
        if (session.state != ScheduleSessionState::Ready) return fail(L"The current session is not confirmed active/unlocked", session.nativeError);
        RECT bounds = {};
        SetLastError(ERROR_SUCCESS);
        if (!GetWindowRect(panel, &bounds)) return fail(L"Recovery panel position could not be read", GetLastError());
        const auto& safe = reviewed_.outputs[recovery_].region.bounds;
        if (!IsWindowVisible(panel) || !IsWindowEnabled(panel) || bounds.left < safe.left || bounds.top < safe.top || bounds.right > safe.right || bounds.bottom > safe.bottom ||
            !timer_.installed() || !hotkey_ || !hooks_.isRegistered()) return fail(L"Recovery controls are not confirmed on the unselected monitor");
        if (!neutralGlobalState() || !confirmHost(false)) return false;
        return now() - began <= 2000 ? true : fail(L"Final topology/session confirmation became stale");
    }
    bool revealChild() noexcept {
        SetLastError(ERROR_SUCCESS);
        if (!SetWindowPos(control_.child(), nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW))
            return fail(L"The prepared child could not be shown", GetLastError());
        return (GetWindowLongPtrW(control_.child(), GWL_STYLE) & WS_VISIBLE) && !IsWindowVisible(host_) ? true : fail(L"Hidden-parent child visibility was not confirmed");
    }
    bool revealHost() noexcept {
        if (!confirmHost(false)) return false;
        SetLastError(ERROR_SUCCESS);
        if (!SetWindowPos(host_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW)) return fail(L"The prepared host could not be shown", GetLastError());
        return confirmHost(true) && IsWindowVisible(control_.child()) ? true : fail(L"Host/child visibility was not confirmed");
    }
    bool cleanup() noexcept {
        if (host_) {
            SetLastError(ERROR_SUCCESS);
            if (!SetWindowPos(host_, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_HIDEWINDOW)) cleanupProblem(L"Hide host", GetLastError());
            if (IsWindowVisible(host_)) cleanupProblem(L"Host remains visible", 0);
        }
        if (!control_.shutdown()) { cleanupProblem(L"Child/runtime cleanup", 0); return false; }
        if (host_) {
            SetLastError(ERROR_SUCCESS);
            if (!DestroyWindow(host_)) { cleanupProblem(L"Destroy host", GetLastError()); return false; }
            host_ = nullptr;
        }
        if (!hooks_.release()) { cleanupProblem(L"Session registration cleanup", hooks_.lastError()); return false; }
        if (hotkey_) {
            SetLastError(ERROR_SUCCESS);
            if (!UnregisterHotKey(panel, HotkeyId)) { cleanupProblem(L"Shortcut cleanup", GetLastError()); return false; }
            hotkey_ = false;
        }
        if (!closeLease(gammaMutex_, gammaOwned_) || !closeLease(sessionMutex_, sessionOwned_)) return false;
        if (!timer_.remove()) { cleanupProblem(L"Timer cleanup", timer_.lastFailure() ? timer_.lastFailure()->errorCode : 0); return false; }
        return true;
    }
    bool hasHost() const noexcept { return host_ != nullptr; }
private:
    bool acquireGamma() noexcept {
        // Existing cooperating Gamma protocol, also used by GammaRampFilter.
        SetLastError(ERROR_SUCCESS);
        gammaMutex_ = CreateMutexW(nullptr, FALSE, L"Global\\RedLight.GammaRampOwner.v1");
        if (!gammaMutex_) return fail(L"Gamma exclusion could not be opened", GetLastError());
        SetLastError(ERROR_SUCCESS);
        const auto wait = WaitForSingleObject(gammaMutex_, 0);
        if (wait == WAIT_OBJECT_0) { gammaOwned_ = true; return true; }
        if (wait == WAIT_ABANDONED) { gammaOwned_ = true; return fail(L"Abandoned Gamma ownership requires separate recovery; probe refused"); }
        return fail(wait == WAIT_TIMEOUT ? L"A cooperating Gamma owner is present" : L"Gamma exclusion could not be confirmed", wait == WAIT_FAILED ? GetLastError() : 0);
    }
    bool confirmHost(bool visible) noexcept {
        BYTE alpha = 0; COLORREF key = 0; DWORD flags = 0;
        SetLastError(ERROR_SUCCESS);
        if (!GetLayeredWindowAttributes(host_, &key, &alpha, &flags)) return fail(L"Host opacity could not be read", GetLastError());
        if (alpha != 255 || flags != LWA_ALPHA) return fail(L"Full opacity could not be confirmed");
        const auto extended = GetWindowLongPtrW(host_, GWL_EXSTYLE), style = GetWindowLongPtrW(host_, GWL_STYLE);
        constexpr LONG_PTR required = WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOPMOST;
        if ((extended & required) != required || !(style & WS_POPUP) || !(style & WS_CLIPCHILDREN) ||
            ((style & WS_VISIBLE) != 0) != visible || (IsWindowVisible(host_) != FALSE) != visible) return fail(L"Host visibility/styles could not be confirmed");
        RECT bounds = {}, client = {};
        SetLastError(ERROR_SUCCESS);
        if (!GetWindowRect(host_, &bounds)) return fail(L"Host rectangle could not be read", GetLastError());
        SetLastError(ERROR_SUCCESS);
        if (!GetClientRect(host_, &client)) return fail(L"Host client rectangle could not be read", GetLastError());
        const auto& target = reviewed_.outputs[selected_].region.bounds;
        return bounds.left == target.left && bounds.top == target.top && bounds.right == target.right && bounds.bottom == target.bottom &&
            client.left == 0 && client.top == 0 && client.right == target.right - target.left && client.bottom == target.bottom - target.top ? true : fail(L"Host geometry differs from the reviewed source");
    }
    bool positionPanel() noexcept {
        const auto& safe = reviewed_.outputs[recovery_].region.bounds;
        const auto width = static_cast<std::int64_t>(safe.right) - safe.left, height = static_cast<std::int64_t>(safe.bottom) - safe.top;
        if (width < 500 || height < 220) return fail(L"The unselected output is too small for recovery controls");
        placingPanel = true;
        SetLastError(ERROR_SUCCESS);
        const bool positioned = SetWindowPos(panel, HWND_TOPMOST, static_cast<int>(safe.left + (width - 500) / 2), static_cast<int>(safe.top + (height - 220) / 2),
            500, 220, SWP_NOACTIVATE | SWP_SHOWWINDOW) != FALSE;
        const DWORD code = positioned ? ERROR_SUCCESS : GetLastError(); placingPanel = false;
        return positioned ? true : fail(L"The recovery panel could not be positioned", code);
    }
    bool neutralGlobalState() noexcept {
        MAGCOLOREFFECT effect = {}; float scale = 0; int x = 0, y = 0;
        if (!MagGetFullscreenColorEffect(&effect) || !MagGetFullscreenTransform(&scale, &x, &y)) return fail(L"Existing fullscreen state could not be read; no reset will be attempted");
        if (scale != 1.0f || x || y) return fail(L"An existing fullscreen magnifier is active; probe refused");
        for (unsigned row = 0; row < 5; ++row) for (unsigned column = 0; column < 5; ++column)
            if (effect.transform[row][column] != (row == column ? 1.0f : 0.0f)) return fail(L"An existing fullscreen color transform is active; probe refused");
        return true;
    }
    bool closeLease(HANDLE& handle, bool& owned) noexcept {
        if (owned) {
            SetLastError(ERROR_SUCCESS);
            if (!ReleaseMutex(handle)) { cleanupProblem(L"Exclusion ownership release", GetLastError()); return false; }
            owned = false;
        }
        if (handle) {
            SetLastError(ERROR_SUCCESS);
            if (!CloseHandle(handle)) { cleanupProblem(L"Exclusion handle close", GetLastError()); return false; }
            handle = nullptr;
        }
        return true;
    }
    bool fail(const wchar_t* stage, DWORD code = 0) noexcept { if (!failure) { failure = stage; error = code; OutputDebugStringW(stage); } return false; }
    void cleanupProblem(const wchar_t* stage, DWORD code) noexcept { cleanupFailure = stage; cleanupError = code; OutputDebugStringW(stage); }
    HWND host_ = nullptr;
    HANDLE sessionMutex_ = nullptr, gammaMutex_ = nullptr;
    bool sessionOwned_ = false, gammaOwned_ = false, hotkey_ = false;
    unsigned selected_ = 0, recovery_ = 0;
    WindowedTopology reviewed_;
    WindowedRedControl control_;
    SessionNotifications hooks_;
    CountdownTimerRegistration timer_;
};

struct App;
App* current = nullptr;
struct App {
    NativeApi api;
    WindowedProbeSession<NativeApi> session{api};
    bool osEnding = false, closeRequested = false, allowQuit = false;
    ~App() { current = nullptr; api.cleanup(); } // Revoke callbacks before member lifetimes end.
    void update() noexcept {
        const wchar_t* text = session.phase() == ProbePhase::Visible ? L"Experimental copy visible. Stop/close or Ctrl+Alt+Shift+F12 cancels. Automatic deadline is armed." :
            (session.phase() == ProbePhase::CleanupPending ? L"Cleanup pending; an unconfirmed copy may remain. Keep this panel open; Stop/close retries owned cleanup." :
                (session.phase() == ProbePhase::Preparing ? L"Preparing or confirming the experiment. Display visibility is not yet confirmed." : L"No owned display copy remains."));
        if (api.status && !SetWindowTextW(api.status, text)) OutputDebugStringW(L"Probe status update failed.\n");
        if (allowQuit && closeRequested && session.phase() == ProbePhase::Finished) PostQuitMessage(0);
    }
    void stop() noexcept { closeRequested = true; session.stop(); update(); }
};

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) noexcept {
    if (!current) return DefWindowProcW(window, message, wParam, lParam);
    auto& app = *current;
    switch (message) {
    case WM_CLOSE: app.stop(); return 0;
    case WM_COMMAND: if (LOWORD(wParam) == StopId) { app.stop(); return 0; } break;
    case WM_HOTKEY: if (wParam == HotkeyId) { app.stop(); return 0; } break;
    case WM_TIMER: if (wParam == TimerId) { app.session.tick(GetTickCount64()); app.update(); return 0; } break;
    case WM_QUERYENDSESSION: app.osEnding = true; app.stop(); return TRUE;
    case WM_ENDSESSION: if (wParam) { app.osEnding = true; app.stop(); } return 0;
    case WM_DPICHANGED:
        if (app.api.placingPanel && !app.api.hasHost()) return 0;
        [[fallthrough]];
    case WM_DISPLAYCHANGE: case WM_SETTINGCHANGE: case WM_WTSSESSION_CHANGE: case WM_ENTERSIZEMOVE:
        if (app.session.phase() != ProbePhase::Idle) app.stop(); return 0;
    case WM_POWERBROADCAST:
        if (wParam == PBT_APMSUSPEND || wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) app.stop(); return TRUE;
    case WM_SHOWWINDOW: if (!wParam && app.session.phase() == ProbePhase::Visible) app.stop(); break;
    case WM_DESTROY: if (window == app.api.panel) { app.api.panel = nullptr; app.stop(); PostQuitMessage(1); } return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

bool CreatePanel(App& app, HINSTANCE instance) noexcept {
    app.api.instance = instance;
    WNDCLASSEXW type = {}; type.cbSize = sizeof(type); type.lpfnWndProc = WindowProc; type.hInstance = instance;
    type.hCursor = LoadCursorW(nullptr, IDC_ARROW); type.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1); type.lpszClassName = PanelClass;
    if (!type.hCursor) return false;
    SetLastError(ERROR_SUCCESS);
    if (!RegisterClassExW(&type)) return false;
    type.lpszClassName = HostClass; type.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    if (!type.hbrBackground || !RegisterClassExW(&type)) return false;
    app.api.panel = CreateWindowExW(WS_EX_TOPMOST, PanelClass, L"RedLight isolated display experiment", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 220, nullptr, nullptr, instance, nullptr);
    if (!app.api.panel) return false;
    app.api.status = CreateWindowExW(0, L"STATIC", L"Preparing; no display copy is active.", WS_CHILD | WS_VISIBLE, 16, 16, 448, 80, app.api.panel, nullptr, instance, nullptr);
    app.api.button = CreateWindowExW(0, L"BUTTON", L"Stop / close (Ctrl+Alt+Shift+F12)", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        16, 112, 448, 32, app.api.panel, reinterpret_cast<HMENU>(static_cast<INT_PTR>(StopId)), instance, nullptr);
    if (!app.api.status || !app.api.button) return false;
    return SetWindowPos(app.api.panel, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW) && IsWindowVisible(app.api.panel);
}
}

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int) {
    int count = 0; SetLastError(ERROR_SUCCESS);
    auto** arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments) { Notice(nullptr, L"Command-line parsing failed; no experiment started.", MB_OK | MB_ICONERROR); return 1; }
    std::array<std::wstring_view, 12> views{};
    ProbeParseResult parsed;
    if (count >= 1 && count <= 13) {
        for (int index = 1; index < count; ++index) views[index - 1] = arguments[index];
        parsed = ParseProbeOptions(std::span<const std::wstring_view>(views.data(), static_cast<std::size_t>(count - 1)));
    }
    if (LocalFree(arguments)) { Notice(nullptr, L"Argument cleanup failed; no experiment started.", MB_OK | MB_ICONERROR); return 1; }
    if (parsed.request != ProbeRequest::Run) {
        if (!Notice(nullptr, Help, MB_OK | (parsed.request == ProbeRequest::Help ? MB_ICONINFORMATION : MB_ICONERROR))) return 1;
        return parsed.request == ProbeRequest::Help ? 0 : 2;
    }
    App app; current = &app;
    if (!CreatePanel(app, instance)) { Notice(nullptr, L"Recovery controls could not be created. No experiment started.", MB_OK | MB_ICONERROR); return 1; }
    app.closeRequested = true; // A one-shot run exits after successful cancellation/expiry cleanup.
    const bool started = app.session.start(parsed.options);
    app.update();
    if (!started && app.api.failure && !app.osEnding) {
        wchar_t error[512] = {};
        const int length = std::swprintf(error, std::size(error), L"%ls.\nNative error (0 = unavailable): %lu\n\nAny incomplete cleanup remains owned; Stop/close retries it.", app.api.failure, app.api.error);
        if (length < 0 || static_cast<std::size_t>(length) >= std::size(error)) Notice(app.api.panel, L"Probe setup failed. Keep recovery controls open if cleanup is pending.", MB_OK | MB_ICONERROR);
        else { error[length] = L'\0'; Notice(app.api.panel, error, MB_OK | MB_ICONERROR); }
    }
    app.allowQuit = true; app.update();
    MSG message = {};
    for (;;) {
        SetLastError(ERROR_SUCCESS);
        const BOOL result = GetMessageW(&message, nullptr, 0, 0);
        if (result > 0) { TranslateMessage(&message); DispatchMessageW(&message); continue; }
        if (result < 0 && !app.api.failure) { app.api.failure = L"Message queue failed"; app.api.error = GetLastError(); OutputDebugStringW(L"Probe GetMessage failed; local-copy cleanup requested.\n"); }
        if (app.session.stop()) break;
        if (!app.api.panel || !IsWindow(app.api.panel)) break; // No surviving controls: best effort process teardown only.
        app.update(); // Preserve the live controller and exact cleanup ownership after a quit/queue failure.
    }
    if (started && app.api.failure && !app.osEnding && app.session.phase() == ProbePhase::Finished)
        Notice(app.api.panel, L"The experiment stopped after an error. Its owned display copy was removed.", MB_OK | MB_ICONERROR);
    current = nullptr;
    bool finalCleanup = true;
    if (app.api.panel && !DestroyWindow(app.api.panel)) { finalCleanup = false; OutputDebugStringW(L"Probe panel destruction failed.\n"); }
    if (!UnregisterClassW(HostClass, instance)) { finalCleanup = false; OutputDebugStringW(L"Probe host class cleanup failed.\n"); }
    if (!UnregisterClassW(PanelClass, instance)) { finalCleanup = false; OutputDebugStringW(L"Probe panel class cleanup failed.\n"); }
    return app.api.failure || !finalCleanup || app.session.phase() != ProbePhase::Finished ? 1 : 0;
}
