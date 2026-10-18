#include <windows.h>
#include <shellapi.h>
#include <wtsapi32.h>
#include <vector>
#include "CommandLine.h"
#include "Version.h"
#include "SessionTimer.h"
#include "BackendSelection.h"
#include <cstdio>

#include "Diagnostics.h"
#include "GammaRampFilter.h"
#include "MagnificationFilter.h"

#define WM_TRAYICON (WM_USER + 1)
#define WM_REDLIGHT_RESTORE (WM_APP + 1)
#define IDI_REDLIGHT_ICON 100
#define ID_TRAY_TOGGLE 200
#define ID_TRAY_ABOUT 201
#define ID_TRAY_EXIT 202
#define ID_TRAY_MODE_STRICT_RED 203
#define ID_TRAY_MODE_LUMA_RED 204
#define ID_TRAY_RESTORE 205
#define ID_TRAY_LOGS 206
#define ID_HOTKEY_TOGGLE 1
#define ID_HOTKEY_RESTORE 2
#define ID_TRAY_RETRY_TIMER 1
#define ID_SESSION_TIMER 2
#define ID_SESSION_LIMIT_BASE 1000
#define ID_BACKEND_PREFERENCE_BASE 1300

namespace {

MagnificationFilter g_magnificationFilter;
GammaRampFilter g_gammaRampFilter;
DisplayFilter* g_displayFilter = &g_gammaRampFilter;
NOTIFYICONDATAA nid = {};
UINT g_taskbarCreated = 0;
bool g_trayAdded = false;
bool g_trayVersion4 = false;
bool g_sessionNotifications = false;
bool g_toggleHotkey = false;
bool g_restoreHotkey = false;
bool g_busy = false;
bool g_pendingRestore = false;
SessionTimer g_sessionTimer;
bool g_sessionTimerInstalled = false;
bool g_autoRestoreFailureNotified = false;
BackendPreference g_backendPreference = BackendPreference::Automatic;
BackendPreference g_startupBackendPreference = BackendPreference::Automatic;
bool g_backendOverride = false;

}

StartupOptions GetStartupOptions();
BackendPreference LoadBackendPreference();
void SetBackendPreference(BackendPreference preference);
bool RestoreFilter(bool showError, bool notifyFailure = true);
void StopSessionTimer();
bool StartSessionTimer();
void SetSessionLimit(unsigned minutes);
void OnSessionTimer();
void CloseInstanceMutex(HANDLE handle);
void PostTrayCommand(HWND hwnd, UINT command);
void DestroyTrayWindow(HWND hwnd);
void ReportError(const char* message, bool modal = true);
bool AddTrayIcon();
void ScheduleTrayRecovery();
void ShowTrayMenu(HWND hwnd);
void SaveRedMode(RedMode mode);
void LoadRedMode();
void LogSystemInfo();
bool ResetDisplay();
bool RequestRunningInstanceRestore(bool* handled);
void ToggleRedlight();
void UpdateTrayIconTip(const char* tip);
void RefreshTrayIconTip();
bool InitializeTrayIcon(HINSTANCE hInstance);
void SetRedModeFromTray(RedMode mode);
void ShowAboutDialog(HWND parent);
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nShowCmd;

    Diagnostics::Initialize();
    Diagnostics::Log("RedLight " REDLIGHT_VERSION " starting.");
    LogSystemInfo();

    const StartupOptions options = GetStartupOptions();
    const StartupAction action = options.action;
    if (action == StartupAction::Help || action == StartupAction::Version || action == StartupAction::Invalid) {
        MessageBoxA(NULL, action == StartupAction::Version ? "RedLight " REDLIGHT_VERSION :
            "RedLight " REDLIGHT_VERSION "\n\nUsage: RedLight.exe [option]\n\n"
            "--restore / --reset-display: restore display colors\n"
            "--help: show this help\n--version: show version\n"
            "--backend=auto|magnification|gamma: one-start backend override\n\n"
            "Without options, start in the tray with the filter off.\n"
            "Ctrl+Alt+R: toggle. Ctrl+Alt+Shift+R: restore.",
            action == StartupAction::Invalid ? "RedLight - invalid command line" : "RedLight",
            MB_OK | (action == StartupAction::Invalid ? MB_ICONERROR : MB_ICONINFORMATION));
        Diagnostics::Shutdown();
        return action == StartupAction::Invalid ? 2 : 0;
    }
    if (action == StartupAction::Restore) {
        const bool resetSucceeded = ResetDisplay();
        Diagnostics::LogFormat("RedLight reset mode completed with %s.", resetSucceeded ? "success" : "failure");
        if (!resetSucceeded) ReportError("Display recovery failed. The running instance may be busy, or no saved gamma ramp is available. See %LOCALAPPDATA%\\RedLight\\redlight.log.");
        Diagnostics::Log("RedLight exiting.");
        Diagnostics::Shutdown();
        return resetSucceeded ? 0 : 1;
    }

    // silently exit multiple instances
    HANDLE hMutex = CreateMutex(NULL, TRUE, "Local\\RedLightApp");
    if (!hMutex) {
        Diagnostics::LogFormat("CreateMutex failed (GetLastError=%lu).", static_cast<unsigned long>(GetLastError()));
        ReportError("RedLight could not acquire its instance guard. See the diagnostics log for details.");
        Diagnostics::Log("RedLight exiting.");
        Diagnostics::Shutdown();
        return 1;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        Diagnostics::Log("Another RedLight instance is already running.");
        if (!CloseHandle(hMutex)) Diagnostics::Log("Closing duplicate instance mutex failed.");
        Diagnostics::Log("RedLight exiting.");
        Diagnostics::Shutdown();
        return 0;
    }

    g_backendPreference = LoadBackendPreference();
    g_backendOverride = options.backendOverride.has_value();
    g_startupBackendPreference = options.backendOverride.value_or(g_backendPreference);
    Diagnostics::LogFormat("Startup backend preference: %s (%s)",
        BackendPreferenceName(g_startupBackendPreference), g_backendOverride ? "command-line override" : "saved/default");
    g_displayFilter = InitializePreferredBackend(g_startupBackendPreference, g_magnificationFilter, g_gammaRampFilter);
    if (!g_displayFilter) {
        ReportError("The selected display backend could not initialize. RedLight remains off.\n"
            "Restart with --backend=auto to override a saved preference. See the diagnostics log for details.");
        CloseInstanceMutex(hMutex);
        Diagnostics::Shutdown();
        return 1;
    }
    Diagnostics::LogFormat("Selected backend: %s; red mode: %s", g_displayFilter->name(), RedModeDisplayName(g_displayFilter->redMode()));

    Diagnostics::LogFormat("Monitor count: %d", GetSystemMetrics(SM_CMONITORS));
    LoadRedMode();
    DWORD sessionMinutes = 0;
    DWORD sessionSize = sizeof(sessionMinutes);
    const LSTATUS sessionLoad = RegGetValueW(HKEY_CURRENT_USER, L"Software\\RedLight", L"SessionLimitMinutes",
        RRF_RT_REG_DWORD, NULL, &sessionMinutes, &sessionSize);
    if (sessionLoad == ERROR_SUCCESS && sessionMinutes <= SessionTimer::maxMinutes) {
        g_sessionTimer.configure(sessionMinutes, 0, false);
    } else if (sessionLoad != ERROR_FILE_NOT_FOUND) {
        Diagnostics::LogFormat("Invalid/unavailable saved session limit (error %ld).", sessionLoad);
    }
    if (!InitializeTrayIcon(hInstance)) {
        ReportError("The tray controls could not be created. RedLight will exit with the filter off.");
        g_displayFilter->shutdown();
        if (IsWindow(nid.hWnd)) DestroyTrayWindow(nid.hWnd);
        CloseInstanceMutex(hMutex);
        Diagnostics::Shutdown();
        return 1;
    }

    MSG msg = {};
    int messageResult = 0;
    while ((messageResult = GetMessageA(&msg, NULL, 0, 0)) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    if (messageResult == -1) {
        Diagnostics::LogFormat("GetMessage failed (error %lu).", GetLastError());
        ReportError("The Windows message loop failed. RedLight is attempting display restoration.");
    }
    const bool restored = g_displayFilter->shutdown();
    if (IsWindow(nid.hWnd)) DestroyTrayWindow(nid.hWnd);
    Diagnostics::Log("RedLight exiting.");
    CloseInstanceMutex(hMutex);
    Diagnostics::Shutdown();
    return messageResult == -1 || !restored ? 1 : 0;
}

void CloseInstanceMutex(HANDLE handle) {
    if (!ReleaseMutex(handle)) Diagnostics::LogFormat("ReleaseMutex failed (error %lu).", GetLastError());
    if (!CloseHandle(handle)) Diagnostics::LogFormat("CloseHandle failed (error %lu).", GetLastError());
}

void PostTrayCommand(HWND hwnd, UINT command) {
    if (!PostMessageA(hwnd, WM_COMMAND, command, 0)) ReportError("The tray command could not be queued. Please retry.");
}

void DestroyTrayWindow(HWND hwnd) {
    if (!DestroyWindow(hwnd)) {
        Diagnostics::LogFormat("DestroyWindow failed (error %lu).", GetLastError());
        ReportError("RedLight could not close its controls. Please retry Exit.");
    }
}

StartupOptions GetStartupOptions() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return {StartupAction::Invalid, std::nullopt};
    std::vector<std::wstring_view> args;
    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
    const StartupOptions options = ParseStartupOptions(args);
    if (LocalFree(argv)) Diagnostics::Log("Freeing command-line arguments failed.");
    return options;
}

void LogSystemInfo() {
    HMODULE ntdll = LoadLibraryA("ntdll.dll");
    if (!ntdll) {
        Diagnostics::Log("Windows version: unavailable.");
        return;
    }

    struct RTL_OSVERSIONINFOW_LOCAL {
        ULONG dwOSVersionInfoSize;
        ULONG dwMajorVersion;
        ULONG dwMinorVersion;
        ULONG dwBuildNumber;
        ULONG dwPlatformId;
        WCHAR szCSDVersion[128];
    } version = {};

    using RtlGetVersionFn = LONG (WINAPI*)(RTL_OSVERSIONINFOW_LOCAL*);
    auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"));
    if (!rtlGetVersion) {
        if (!FreeLibrary(ntdll)) Diagnostics::Log("FreeLibrary(ntdll) failed.");
        Diagnostics::Log("Windows version: unavailable.");
        return;
    }

    version.dwOSVersionInfoSize = sizeof(version);
    if (rtlGetVersion(&version) == 0) {
        Diagnostics::LogFormat("Windows version: %lu.%lu build %lu",
            static_cast<unsigned long>(version.dwMajorVersion),
            static_cast<unsigned long>(version.dwMinorVersion),
            static_cast<unsigned long>(version.dwBuildNumber));
    } else {
        Diagnostics::Log("Windows version: unavailable.");
    }

    if (!FreeLibrary(ntdll)) Diagnostics::Log("FreeLibrary(ntdll) failed.");
}

bool ResetDisplay() {
    bool restoreHandledByRunningInstance = false;
    if (RequestRunningInstanceRestore(&restoreHandledByRunningInstance)) {
        Diagnostics::Log("Reset mode: running instance restore succeeded.");
        return true;
    }

    if (restoreHandledByRunningInstance) {
        Diagnostics::Log("Reset mode: running instance restore failed.");
        return false;
    }

    constexpr MAGCOLOREFFECT kIdentityEffect = {{
        {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    }};

    // Never bypass a live/starting owner's recovery data with an identity reset.
    HANDLE owner = CreateMutexA(NULL, TRUE, "Local\\RedLightApp");
    const DWORD mutexError = GetLastError();
    if (!owner) {
        Diagnostics::LogFormat("Reset mode: could not acquire instance guard (error %lu).", mutexError);
        return false;
    }
    if (mutexError == ERROR_ALREADY_EXISTS) {
        if (!CloseHandle(owner)) Diagnostics::Log("Reset mode: CloseHandle failed.");
        Diagnostics::Log("Reset mode: an instance exists but its controls are unavailable; refusing direct reset.");
        return false;
    }
    struct ResetGuard {
        HANDLE handle;
        ~ResetGuard() {
            if (!ReleaseMutex(handle)) Diagnostics::Log("Reset mode: ReleaseMutex failed.");
            if (!CloseHandle(handle)) Diagnostics::Log("Reset mode: CloseHandle failed.");
        }
    } resetGuard{owner};
    Diagnostics::Log("Reset mode: attempting Magnification API restore.");

    if (!MagInitialize()) {
        Diagnostics::LogFormat("Reset mode: MagInitialize failed (GetLastError=%lu).", static_cast<unsigned long>(GetLastError()));
        Diagnostics::Log("Reset mode: gamma-ramp reset not available without a saved ramp.");
        return false;
    }

    Diagnostics::Log("Reset mode: MagInitialize succeeded.");

    MAGCOLOREFFECT identity = kIdentityEffect;
    const BOOL colorEffectReset = MagSetFullscreenColorEffect(&identity);
    if (colorEffectReset) {
        Diagnostics::Log("Reset mode: MagSetFullscreenColorEffect(identity) succeeded.");
    } else {
        Diagnostics::LogFormat("Reset mode: MagSetFullscreenColorEffect(identity) failed (GetLastError=%lu).",
            static_cast<unsigned long>(GetLastError()));
    }

    if (!MagUninitialize()) {
        Diagnostics::LogFormat("Reset mode: MagUninitialize failed (GetLastError=%lu).", static_cast<unsigned long>(GetLastError()));
    } else {
        Diagnostics::Log("Reset mode: MagUninitialize succeeded.");
    }

    Diagnostics::Log("Reset mode: gamma-ramp reset not available without a saved ramp.");
    return colorEffectReset != FALSE;
}

bool RequestRunningInstanceRestore(bool* handled) {
    if (handled) {
        *handled = false;
    }

    HWND trayWindow = FindWindowA("RedLight.TrayWindow", "RedLight");
    if (!trayWindow) {
        Diagnostics::Log("Reset mode: running tray window not found.");
        return false;
    }

    if (handled) {
        *handled = true;
    }

    Diagnostics::LogFormat("Reset mode: found running tray window (hwnd=%p).", static_cast<void*>(trayWindow));

    DWORD_PTR restoreResult = FALSE;
    const LRESULT messageResult = SendMessageTimeoutA(
        trayWindow,
        WM_REDLIGHT_RESTORE,
        0,
        0,
        SMTO_ABORTIFHUNG,
        3000,
        &restoreResult);

    if (messageResult == 0) {
        Diagnostics::LogFormat(
            "Reset mode: WM_REDLIGHT_RESTORE send failed or timed out (GetLastError=%lu).",
            static_cast<unsigned long>(GetLastError()));
        return false;
    }

    Diagnostics::LogFormat(
        "Reset mode: WM_REDLIGHT_RESTORE succeeded with result=%s.",
        restoreResult ? "TRUE" : "FALSE");
    return restoreResult != FALSE;
}

void ReportError(const char* message, bool modal) {
    Diagnostics::Log(message);
    if (modal || !g_trayAdded) {
        MessageBoxA(nid.hWnd, message, "RedLight", MB_OK | MB_ICONERROR);
        return;
    }
    NOTIFYICONDATAA notification = nid;
    notification.uFlags = NIF_INFO;
    strcpy_s(notification.szInfoTitle, "RedLight needs attention");
    strncpy_s(notification.szInfo, message, _TRUNCATE);
    notification.dwInfoFlags = NIIF_ERROR;
    if (!Shell_NotifyIconA(NIM_MODIFY, &notification)) {
        Diagnostics::Log("Could not display recovery notification.");
    }
}

bool RestoreFilter(bool showError, bool notifyFailure) {
    if (g_busy) { g_pendingRestore = true; return false; }
    g_pendingRestore = false;
    g_busy = true;
    const bool restored = !g_displayFilter->needsRestore() || g_displayFilter->disable();
    g_busy = false;
    if (restored) {
        g_pendingRestore = false;
        StopSessionTimer();
    }
    RefreshTrayIconTip();
    if (!restored && notifyFailure) {
        char message[768];
        sprintf_s(message, "Display restoration failed. RedLight has kept its recovery data.\n"
            "Use Restore display or Ctrl+Alt+Shift+R to retry.\n\n%s", g_displayFilter->lastError());
        ReportError(message, showError);
    }
    return restored;
}

void UpdateTrayIconTip(const char* tip) {
    strcpy_s(nid.szTip, tip);
    if (g_trayAdded && !Shell_NotifyIconA(NIM_MODIFY, &nid)) {
        Diagnostics::Log("Tray update failed; restoring display before retrying tray registration.");
        g_trayAdded = false;
        RestoreFilter(false);
        if (!AddTrayIcon()) ScheduleTrayRecovery();
    }
}

void RefreshTrayIconTip() {
    char tip[sizeof(nid.szTip)];
    char remaining[32] = {};
    if (g_sessionTimer.armed()) {
        const auto seconds = (g_sessionTimer.remainingMilliseconds(GetTickCount64()) + 999) / 1000;
        sprintf_s(remaining, " - %llu:%02llu left",
            static_cast<unsigned long long>(seconds / 60), static_cast<unsigned long long>(seconds % 60));
    }
    sprintf_s(tip, "RedLight %s - %s - %s%s",
        g_displayFilter->needsRestore() && g_displayFilter->lastError()[0] ? "RECOVERY NEEDED" :
        (g_displayFilter->isActive() ? "ON" : "off"),
        g_displayFilter->name(), RedModeDisplayName(g_displayFilter->redMode()), remaining);
    UpdateTrayIconTip(tip);
}

void ToggleRedlight() {
    if (g_busy) return;
    if (!g_trayAdded && !g_displayFilter->needsRestore()) {
        ReportError("RedLight is waiting for its tray controls to return. The filter will stay off.");
        return;
    }
    if (g_displayFilter->needsRestore()) {
        RestoreFilter(true);
    } else {
        Diagnostics::LogFormat("Enable request: %s (%s)", g_displayFilter->name(), RedModeDisplayName(g_displayFilter->redMode()));
        g_busy = true;
        const bool enabled = g_displayFilter->enable();
        char enableError[256] = {};
        if (!enabled) strcpy_s(enableError, g_displayFilter->lastError());
        g_busy = false;
        if (g_pendingRestore) RestoreFilter(false);
        if (enabled && g_displayFilter->isActive()) StartSessionTimer();
        if (!enabled) {
            char message[768];
            sprintf_s(message, "The red filter could not be enabled.\n\n%s", enableError);
            ReportError(message);
        }
        RefreshTrayIconTip();
    }
    g_busy = false;
}

void StopSessionTimer() {
    g_sessionTimer.cancel();
    g_autoRestoreFailureNotified = false;
    if (g_sessionTimerInstalled) {
        if (KillTimer(nid.hWnd, ID_SESSION_TIMER)) g_sessionTimerInstalled = false;
        else Diagnostics::LogFormat("KillTimer(session) failed (error %lu).", GetLastError());
    }
}

bool StartSessionTimer() {
    g_sessionTimer.start(GetTickCount64());
    g_autoRestoreFailureNotified = false;
    if (!g_sessionTimer.armed()) return true;
    if (!SetTimer(nid.hWnd, ID_SESSION_TIMER, 1000, NULL)) {
        Diagnostics::LogFormat("SetTimer(session) failed (error %lu).", GetLastError());
        g_sessionTimer.cancel();
        const bool restored = RestoreFilter(false);
        ReportError(restored ? "The auto-restore timer could not be started. The display was restored." :
            "The auto-restore timer could not be started, and restoration needs attention. Use Restore display to retry.");
        return false;
    }
    g_sessionTimerInstalled = true;
    Diagnostics::LogFormat("Auto-restore armed for %u minutes.", g_sessionTimer.minutes());
    return true;
}

void SetSessionLimit(unsigned minutes) {
    if (g_busy || minutes > SessionTimer::maxMinutes) return;
    const bool active = g_displayFilter->isActive();
    const unsigned previous = g_sessionTimer.minutes();
    StopSessionTimer();
    if (!g_sessionTimer.configure(minutes, GetTickCount64(), false)) return;
    if (active && !StartSessionTimer()) {
        // The new limit was not armed. Retain the user's prior preference.
        g_sessionTimer.configure(previous, GetTickCount64(), false);
        RefreshTrayIconTip();
        return;
    }
    const DWORD value = minutes;
    const LSTATUS saved = RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\RedLight", L"SessionLimitMinutes",
        REG_DWORD, &value, sizeof(value));
    if (saved != ERROR_SUCCESS) {
        Diagnostics::LogFormat("Saving session limit failed (error %ld).", saved);
        ReportError("The session limit changed, but could not be saved for the next start.");
    }
    Diagnostics::LogFormat("Session limit changed to %u minutes (0 = unlimited).", minutes);
    RefreshTrayIconTip();
}

void OnSessionTimer() {
    if (!g_sessionTimer.armed()) { StopSessionTimer(); return; }
    const auto now = GetTickCount64();
    if (!g_busy && g_sessionTimer.restoreDue(now)) {
        if (!g_autoRestoreFailureNotified) Diagnostics::Log("Session time limit reached; requesting display restoration.");
        g_sessionTimer.noteRestoreAttempt(now);
        const bool restored = RestoreFilter(false, !g_autoRestoreFailureNotified);
        g_autoRestoreFailureNotified = !restored;
    } else {
        RefreshTrayIconTip();
    }
}

void ScheduleTrayRecovery() {
    if (!SetTimer(nid.hWnd, ID_TRAY_RETRY_TIMER, 2000, NULL)) {
        Diagnostics::LogFormat("SetTimer for tray recovery failed (error %lu).", GetLastError());
        ReportError("Tray recovery failed. Use Ctrl+Alt+Shift+R to restore colors, then restart RedLight.");
    } else {
        Diagnostics::Log("Tray registration will be retried every two seconds; new activation is blocked until controls return.");
    }
}

bool AddTrayIcon() {
    if (!Shell_NotifyIconA(NIM_ADD, &nid)) {
        Diagnostics::Log("Shell_NotifyIcon(NIM_ADD) failed.");
        return false;
    }
    g_trayAdded = true;
    nid.uVersion = NOTIFYICON_VERSION_4;
    g_trayVersion4 = Shell_NotifyIconA(NIM_SETVERSION, &nid) != FALSE;
    if (!g_trayVersion4) Diagnostics::Log("Tray version 4 unavailable; using legacy callbacks.");
    return true;
}

bool InitializeTrayIcon(HINSTANCE hInstance) {
    g_taskbarCreated = RegisterWindowMessageA("TaskbarCreated");
    if (!g_taskbarCreated) {
        Diagnostics::LogFormat("RegisterWindowMessage failed (error %lu).", GetLastError());
        return false;
    }
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "RedLight.TrayWindow";
    if (!RegisterClassA(&wc)) {
        Diagnostics::LogFormat("RegisterClass failed (error %lu).", GetLastError());
        return false;
    }
    nid.cbSize = sizeof(nid);
    nid.hWnd = CreateWindowExA(0, wc.lpszClassName, "RedLight", WS_OVERLAPPED,
        0, 0, 0, 0, NULL, NULL, hInstance, NULL);
    if (!nid.hWnd) {
        Diagnostics::LogFormat("CreateWindowEx failed (error %lu).", GetLastError());
        return false;
    }
    nid.uID = IDI_REDLIGHT_ICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIconA(hInstance, MAKEINTRESOURCEA(IDI_REDLIGHT_ICON));
    if (!nid.hIcon) {
        Diagnostics::LogFormat("LoadIcon failed (error %lu).", GetLastError());
        return false;
    }
    RefreshTrayIconTip();
    if (!AddTrayIcon()) return false;
    g_toggleHotkey = RegisterHotKey(nid.hWnd, ID_HOTKEY_TOGGLE, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'R') != FALSE;
    g_restoreHotkey = RegisterHotKey(nid.hWnd, ID_HOTKEY_RESTORE, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, 'R') != FALSE;
    g_sessionNotifications = WTSRegisterSessionNotification(nid.hWnd, NOTIFY_FOR_THIS_SESSION) != FALSE;
    if (g_displayFilter == &g_gammaRampFilter) {
        ReportError("Using the legacy Strict Red fallback. Driver and multi-monitor support varies. See About and the README for details.", false);
    }
    if (!g_toggleHotkey || !g_restoreHotkey || !g_sessionNotifications) {
        ReportError("Some keyboard shortcuts or session notifications are unavailable. See About for shortcut status. Tray controls remain available.", false);
    }
    return true;
}

BackendPreference LoadBackendPreference() {
    DWORD value = 0;
    DWORD size = sizeof(value);
    const LSTATUS result = RegGetValueW(HKEY_CURRENT_USER, L"Software\\RedLight", L"BackendPreference",
        RRF_RT_REG_DWORD, NULL, &value, &size);
    if (result == ERROR_SUCCESS && IsValidBackendPreference(value)) return static_cast<BackendPreference>(value);
    if (result != ERROR_FILE_NOT_FOUND) Diagnostics::LogFormat("Invalid/unavailable saved backend preference (error %ld); using Automatic.", result);
    return BackendPreference::Automatic;
}

void SetBackendPreference(BackendPreference preference) {
    if (!IsValidBackendPreference(static_cast<unsigned>(preference))) return;
    const DWORD value = static_cast<DWORD>(preference);
    const LSTATUS result = RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\RedLight", L"BackendPreference",
        REG_DWORD, &value, sizeof(value));
    if (result != ERROR_SUCCESS) {
        Diagnostics::LogFormat("Saving backend preference failed (error %ld).", result);
        ReportError("The backend preference could not be saved. The previous selection remains in effect.");
        return;
    }
    g_backendPreference = preference;
    Diagnostics::LogFormat("Backend preference saved for next start: %s", BackendPreferenceName(preference));
}

void SaveRedMode(RedMode mode) {
    const DWORD value = mode == RedMode::LumaRed ? 1 : 0;
    const LSTATUS result = RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\RedLight", L"RedMode",
        REG_DWORD, &value, sizeof(value));
    if (result != ERROR_SUCCESS) {
        Diagnostics::LogFormat("Saving red mode failed (error %ld).", result);
        ReportError("The mode was applied, but could not be saved for the next start.");
    }
}

void LoadRedMode() {
    DWORD value = 0;
    DWORD size = sizeof(value);
    const LSTATUS result = RegGetValueW(HKEY_CURRENT_USER, L"Software\\RedLight", L"RedMode",
        RRF_RT_REG_DWORD, NULL, &value, &size);
    if (result == ERROR_FILE_NOT_FOUND) return;
    if (result != ERROR_SUCCESS || value > 1) {
        Diagnostics::LogFormat("Invalid/unavailable saved mode (error %ld); using backend default.", result);
        return;
    }
    const RedMode mode = value == 1 ? RedMode::LumaRed : RedMode::StrictRed;
    if (g_displayFilter->supportsRedMode(mode) && !g_displayFilter->setRedMode(mode)) {
        Diagnostics::LogFormat("Loading saved mode failed: %s", g_displayFilter->lastError());
    }
}

void SetRedModeFromTray(RedMode mode) {
    if (g_busy || (g_displayFilter->needsRestore() && !g_displayFilter->isActive())) return;
    Diagnostics::LogFormat("Red mode request: %s -> %s (%s)",
        RedModeDisplayName(g_displayFilter->redMode()),
        RedModeDisplayName(mode),
        g_displayFilter->name());

    if (!g_displayFilter->supportsRedMode(mode)) {
        Diagnostics::LogFormat("%s does not support %s; keeping %s.",
            g_displayFilter->name(),
            RedModeDisplayName(mode),
            RedModeDisplayName(g_displayFilter->redMode()));
        RefreshTrayIconTip();
        return;
    }

    g_busy = true;
    const bool changed = g_displayFilter->setRedMode(mode);
    g_busy = false;
    if (g_pendingRestore) RestoreFilter(false);
    if (!changed) {
        Diagnostics::LogFormat("Red mode change to %s failed.", RedModeDisplayName(mode));
        ReportError(g_displayFilter->lastError());
    } else {
        Diagnostics::LogFormat("Red mode change to %s succeeded.", RedModeDisplayName(g_displayFilter->redMode()));
        SaveRedMode(g_displayFilter->redMode());
    }

    RefreshTrayIconTip();
}

void ShowAboutDialog(HWND parent) {
    char aboutText[1024];
    sprintf_s(aboutText, "RedLight " REDLIGHT_VERSION "\n\nBackend: %s\nStartup selection: %s%s\nNext start: %s\nMode: %s\nStatus: %s\n\n"
        "Ctrl+Alt+R: %s\nCtrl+Alt+Shift+R: %s\nSession recovery: %s\n\n"
        "Logs: %%LOCALAPPDATA%%\\RedLight\nUnsigned release candidate\n"
        "github.com/michaelmawhinney/redlight",
        g_displayFilter->name(), BackendPreferenceName(g_startupBackendPreference),
        g_backendOverride ? " (command line)" : "", BackendPreferenceName(g_backendPreference),
        RedModeDisplayName(g_displayFilter->redMode()),
        g_displayFilter->needsRestore() && g_displayFilter->lastError()[0] ? "Recovery needed" :
        (g_displayFilter->isActive() ? "ON" : "off"),
        g_toggleHotkey ? "Toggle filter" : "Unavailable (shortcut in use)",
        g_restoreHotkey ? "Restore display" : "Unavailable (shortcut in use)",
        g_sessionNotifications ? "Available" : "Unavailable (registration failed)");
    MessageBoxA(parent, aboutText, "About RedLight", MB_OK | MB_ICONINFORMATION);
}

void ShowTrayMenu(HWND hwnd) {
    POINT pt = {};
    NOTIFYICONIDENTIFIER iconId = {};
    iconId.cbSize = sizeof(iconId);
    iconId.hWnd = hwnd;
    iconId.uID = nid.uID;
    RECT iconRect = {};
    if (SUCCEEDED(Shell_NotifyIconGetRect(&iconId, &iconRect))) {
        pt = {iconRect.left, iconRect.bottom};
    } else if (!GetCursorPos(&pt)) {
        ReportError("Could not locate the tray menu.");
        return;
    }
    HMENU menu = CreatePopupMenu();
    if (!menu) { ReportError("Could not create the tray menu."); return; }
    HMENU sessionMenu = CreatePopupMenu();
    if (!sessionMenu) {
        if (!DestroyMenu(menu)) Diagnostics::Log("DestroyMenu failed after submenu failure.");
        ReportError("Could not create the session menu.");
        return;
    }
    bool sessionBuilt = true;
    for (const unsigned minutes : {0u, 15u, 30u, 60u, 120u, 240u}) {
        char label[48];
        if (minutes == 0) strcpy_s(label, "Unlimited");
        else sprintf_s(label, "%u minutes", minutes);
        sessionBuilt = sessionBuilt && AppendMenuA(sessionMenu,
            MF_STRING | (g_sessionTimer.minutes() == minutes ? MF_CHECKED : MF_UNCHECKED),
            ID_SESSION_LIMIT_BASE + minutes, label);
    }
    const bool sessionAttached = sessionBuilt && AppendMenuA(menu, MF_POPUP,
        reinterpret_cast<UINT_PTR>(sessionMenu), "Auto-restore after");
    if (!sessionAttached) {
        if (!DestroyMenu(sessionMenu)) Diagnostics::Log("DestroyMenu(session) failed.");
        if (!DestroyMenu(menu)) Diagnostics::Log("DestroyMenu failed after submenu failure.");
        ReportError("Could not build the session menu.");
        return;
    }
    HMENU backendMenu = CreatePopupMenu();
    bool backendBuilt = backendMenu != NULL;
    for (const auto preference : {BackendPreference::Automatic, BackendPreference::Magnification, BackendPreference::Gamma}) {
        backendBuilt = backendBuilt && AppendMenuA(backendMenu,
            MF_STRING | (g_backendPreference == preference ? MF_CHECKED : MF_UNCHECKED),
            ID_BACKEND_PREFERENCE_BASE + static_cast<unsigned>(preference), BackendPreferenceName(preference));
    }
    if (!backendBuilt || !AppendMenuA(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(backendMenu), "Backend preference (next start)")) {
        if (backendMenu && !DestroyMenu(backendMenu)) Diagnostics::Log("DestroyMenu(backend) failed.");
        if (!DestroyMenu(menu)) Diagnostics::Log("DestroyMenu failed after backend menu failure.");
        ReportError("Could not build the backend preference menu.");
        return;
    }
    const UINT strictFlags = MF_STRING | (g_displayFilter->redMode() == RedMode::StrictRed ? MF_CHECKED : MF_UNCHECKED);
    const UINT lumaFlags = MF_STRING | (g_displayFilter->redMode() == RedMode::LumaRed ? MF_CHECKED : MF_UNCHECKED)
        | (g_displayFilter->supportsRedMode(RedMode::LumaRed) ? MF_ENABLED : MF_GRAYED);
    const bool built =
        AppendMenuA(menu, MF_STRING, ID_TRAY_TOGGLE, g_displayFilter->needsRestore() ? "Turn &off\tCtrl+Alt+R" : "Turn &on\tCtrl+Alt+R") &&
        AppendMenuA(menu, MF_STRING, ID_TRAY_RESTORE, "&Restore display\tCtrl+Alt+Shift+R") &&
        AppendMenuA(menu, MF_SEPARATOR, 0, NULL) &&
        AppendMenuA(menu, strictFlags, ID_TRAY_MODE_STRICT_RED, "&Strict Red") &&
        AppendMenuA(menu, lumaFlags, ID_TRAY_MODE_LUMA_RED, "&Luma Red") &&
        AppendMenuA(menu, MF_SEPARATOR, 0, NULL) &&
        AppendMenuA(menu, MF_STRING, ID_TRAY_LOGS, "Open &logs folder") &&
        AppendMenuA(menu, MF_STRING, ID_TRAY_ABOUT, "&About") &&
        AppendMenuA(menu, MF_STRING, ID_TRAY_EXIT, "E&xit");
    if (!built) {
        ReportError("Could not build the tray menu.");
    } else {
        if (!SetForegroundWindow(hwnd)) Diagnostics::Log("SetForegroundWindow failed for tray menu.");
        const UINT command = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
            pt.x, pt.y, 0, hwnd, NULL);
        if (command) PostTrayCommand(hwnd, command);
        if (!PostMessageA(hwnd, WM_NULL, 0, 0)) Diagnostics::Log("Posting tray menu dismissal failed.");
        if (!Shell_NotifyIconA(NIM_SETFOCUS, &nid)) Diagnostics::Log("Could not restore tray keyboard focus.");
    }
    if (!DestroyMenu(menu)) Diagnostics::Log("DestroyMenu failed.");
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (g_taskbarCreated && uMsg == g_taskbarCreated) {
        g_trayAdded = false;
        Diagnostics::Log("Explorer restarted; restoring display and tray control.");
        RestoreFilter(false);
        if (!AddTrayIcon()) ScheduleTrayRecovery();
        return 0;
    }
    switch (uMsg) {
    case WM_TIMER:
        if (wParam == ID_SESSION_TIMER) { OnSessionTimer(); return 0; }
        if (wParam == ID_TRAY_RETRY_TIMER && (g_trayAdded || AddTrayIcon())) {
            if (!KillTimer(hwnd, ID_TRAY_RETRY_TIMER)) Diagnostics::Log("Could not stop tray recovery timer.");
        }
        return 0;
    case WM_TRAYICON: {
        const UINT event = g_trayVersion4 ? LOWORD(lParam) : static_cast<UINT>(lParam);
        if (event == WM_CONTEXTMENU || (!g_trayVersion4 && event == WM_RBUTTONUP)) {
            ShowTrayMenu(hwnd);
        } else if ((g_trayVersion4 && (event == NIN_SELECT || event == NIN_KEYSELECT)) ||
            (!g_trayVersion4 && event == WM_LBUTTONUP)) {
            PostTrayCommand(hwnd, ID_TRAY_TOGGLE);
        }
        return 0;
    }
    case WM_HOTKEY:
        if (wParam == ID_HOTKEY_TOGGLE) ToggleRedlight();
        else if (wParam == ID_HOTKEY_RESTORE) RestoreFilter(true);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) >= ID_BACKEND_PREFERENCE_BASE && LOWORD(wParam) <= ID_BACKEND_PREFERENCE_BASE + 2) {
            SetBackendPreference(static_cast<BackendPreference>(LOWORD(wParam) - ID_BACKEND_PREFERENCE_BASE));
            return 0;
        }
        if (LOWORD(wParam) >= ID_SESSION_LIMIT_BASE &&
            LOWORD(wParam) <= ID_SESSION_LIMIT_BASE + SessionTimer::maxMinutes) {
            SetSessionLimit(LOWORD(wParam) - ID_SESSION_LIMIT_BASE);
            return 0;
        }
        switch (LOWORD(wParam)) {
        case ID_TRAY_TOGGLE: ToggleRedlight(); break;
        case ID_TRAY_RESTORE: RestoreFilter(true); break;
        case ID_TRAY_MODE_STRICT_RED: SetRedModeFromTray(RedMode::StrictRed); break;
        case ID_TRAY_MODE_LUMA_RED: SetRedModeFromTray(RedMode::LumaRed); break;
        case ID_TRAY_ABOUT: ShowAboutDialog(hwnd); break;
        case ID_TRAY_EXIT:
            if (!PostMessageA(hwnd, WM_CLOSE, 0, 0)) ReportError("The exit request could not be queued. Please retry.");
            break;
        case ID_TRAY_LOGS: {
            const std::wstring directory = Diagnostics::LogDirectory();
            if (directory.empty() || reinterpret_cast<INT_PTR>(ShellExecuteW(hwnd, L"open", directory.c_str(),
                NULL, NULL, SW_SHOWNORMAL)) <= 32) ReportError("Could not open the diagnostics folder.");
            break;
        }
        }
        return 0;
    case WM_REDLIGHT_RESTORE:
        Diagnostics::Log("External restore request received.");
        return RestoreFilter(false) ? TRUE : FALSE;
    case WM_DISPLAYCHANGE:
        Diagnostics::Log("Display configuration changed; restoring filter. Re-enable manually.");
        RestoreFilter(false);
        return 0;
    case WM_POWERBROADCAST:
        if (wParam == PBT_APMSUSPEND || wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) {
            Diagnostics::Log("Power transition; restoring filter. Re-enable manually.");
            RestoreFilter(false);
        }
        return TRUE;
    case WM_WTSSESSION_CHANGE:
        if (wParam == WTS_SESSION_LOCK || wParam == WTS_SESSION_UNLOCK ||
            wParam == WTS_CONSOLE_DISCONNECT || wParam == WTS_REMOTE_DISCONNECT) {
            Diagnostics::Log("Session transition; restoring filter. Re-enable manually.");
            RestoreFilter(false);
        }
        return 0;
    case WM_QUERYENDSESSION:
        RestoreFilter(false);
        return TRUE;
    case WM_ENDSESSION:
        if (wParam) {
            RestoreFilter(false);
            DestroyTrayWindow(hwnd);
        }
        return 0;
    case WM_CLOSE:
        if (RestoreFilter(true)) DestroyTrayWindow(hwnd);
        return 0;
    case WM_DESTROY:
        StopSessionTimer();
        if (g_toggleHotkey && !UnregisterHotKey(hwnd, ID_HOTKEY_TOGGLE)) Diagnostics::Log("Unregister toggle hotkey failed.");
        if (g_restoreHotkey && !UnregisterHotKey(hwnd, ID_HOTKEY_RESTORE)) Diagnostics::Log("Unregister restore hotkey failed.");
        if (g_sessionNotifications && !WTSUnRegisterSessionNotification(hwnd)) Diagnostics::Log("Unregister session notifications failed.");
        if (g_trayAdded && !Shell_NotifyIconA(NIM_DELETE, &nid)) Diagnostics::Log("Tray icon removal failed.");
        g_trayAdded = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}
