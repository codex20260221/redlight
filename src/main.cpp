#include <windows.h>
#include <shellapi.h>
#include <wtsapi32.h>
#include <vector>
#include "CommandLine.h"
#include "Version.h"
#include "BuildInfo.h"
#include "SessionTimer.h"
#include "BackendSelection.h"
#include "SupportReport.h"
#include "Clipboard.h"
#include "Preferences.h"
#include "PreferencesStore.h"
#include "PreferencesImport.h"
#include "CommandIds.h"
#include "TrayMenu.h"
#include "SessionLimitDialog.h"
#include "HotkeyManager.h"
#include "UiSafety.h"
#include "TrayStatus.h"
#include "StatusQuery.h"
#include <cstdio>

#include "Diagnostics.h"
#include "GammaRampFilter.h"
#include "MagnificationFilter.h"

#define WM_TRAYICON (WM_USER + 1)
#define WM_REDLIGHT_RESTORE (WM_APP + 1)
#define WM_REDLIGHT_UI_ERROR (WM_APP + 2)

namespace {

MagnificationFilter g_magnificationFilter;
GammaRampFilter g_gammaRampFilter;
DisplayFilter* g_displayFilter = &g_gammaRampFilter;
NOTIFYICONDATAA nid = {};
UINT g_taskbarCreated = 0;
bool g_trayAdded = false;
bool g_trayVersion4 = false;
bool g_sessionNotifications = false;
HotkeyManager g_hotkeys;
bool g_busy = false;
bool g_pendingRestore = false;
SessionTimer g_sessionTimer;
bool g_sessionTimerInstalled = false;
bool g_autoRestoreFailureNotified = false;
BackendPreference g_backendPreference = BackendPreference::Automatic;
BackendPreference g_startupBackendPreference = BackendPreference::Automatic;
bool g_backendOverride = false;
AppPreferences g_preferences;
bool g_preferencesUnsaved = false;
bool g_preferencesApplying = false;
bool g_uiErrorVisible = false;
BackendVerification g_verification = BackendVerification::NotChecked;

}

StartupOptions GetStartupOptions();
bool ApplyInactivePreferences(const AppPreferences& preferences);
void SetBackendPreference(BackendPreference preference);
bool RestoreFilter(bool showError, bool notifyFailure = true);
void StopSessionTimer();
bool StartSessionTimer();
void SetSessionLimit(unsigned minutes);
void SetShortcutProfile(ShortcutProfile profile);
void OnSessionTimer();
void CloseInstanceMutex(HANDLE handle);
void PostTrayCommand(HWND hwnd, UINT command);
void DestroyTrayWindow(HWND hwnd);
void ReportError(const char* message, bool modal = true);
bool AddTrayIcon();
void ScheduleTrayRecovery();
void ShowTrayMenu(HWND hwnd);
void SaveRedMode(RedMode mode);

void SetLumaProfile(LumaProfile profile);
void LogSystemInfo();
bool ResetDisplay();
bool RequestRunningInstanceRestore(bool* handled);
void ToggleRedlight();
void UpdateTrayIconTip(const char* tip);
void RefreshTrayIconTip();
bool InitializeTrayIcon(HINSTANCE hInstance);
void SetRedModeFromTray(RedMode mode);
void ShowAboutDialog(HWND parent);
void CopySupportReport(HWND owner);
void CopySettingsProfile(HWND owner);
void ImportSettingsProfile(HWND owner);
void ConfirmPreferences(HWND owner, const AppPreferences& candidate, const char* title);
void VerifyBackendState(HWND owner);
const char* FilterStatusText();
void ShowTrayInfo(const char* title, const char* message);
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) noexcept;
LRESULT HandleWindowMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nShowCmd;

    Diagnostics::Initialize();
    Diagnostics::Log("RedLight " REDLIGHT_VERSION " starting.");
    Diagnostics::LogFormat("Build revision: %s (%s).", REDLIGHT_BUILD_REVISION, REDLIGHT_SOURCE_STATE);
    LogSystemInfo();

    const StartupOptions options = GetStartupOptions();
    const StartupAction action = options.action;
    if (action == StartupAction::Status) {
        const int result = QueryRunningStatus();
        Diagnostics::Shutdown();
        return result;
    }
    if (action == StartupAction::Help || action == StartupAction::Version || action == StartupAction::Invalid) {
        MessageBoxA(NULL, action == StartupAction::Version ? "RedLight " REDLIGHT_VERSION :
            "RedLight " REDLIGHT_VERSION "\n\nUsage: RedLight.exe [option]\n\n"
            "--restore / --reset-display: restore display colors\n"
            "--help: show this help\n--version: show version\n"
            "--status: report cached tray state as JSON without starting it\n"
            "--backend=auto|magnification|gamma: one-start backend override\n\n"
            "Without options, start in the tray with the filter off.\n"
            "Default shortcuts: Ctrl+Alt+R toggles; Ctrl+Alt+Shift+R restores.\n"
            "Expert controls offers alternative shortcut profiles.",
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

    const LoadedPreferences loadedPreferences = LoadPreferences();
    g_preferences = loadedPreferences.preferences;
    g_backendPreference = g_preferences.backend;
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
    const bool preferencesApplied = ApplyInactivePreferences(g_preferences);
    if (!InitializeTrayIcon(hInstance)) {
        ReportError("The tray controls could not be created. RedLight will exit with the filter off.");
        g_displayFilter->shutdown();
        if (IsWindow(nid.hWnd)) DestroyTrayWindow(nid.hWnd);
        CloseInstanceMutex(hMutex);
        Diagnostics::Shutdown();
        return 1;
    }

    if (loadedPreferences.warning || !preferencesApplied) {
        ReportError("Some saved settings could not be loaded or applied. Defaults were used where needed. See the diagnostics log.", false);
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
        if (!MessageBoxA(nid.hWnd, message, "RedLight", MB_OK | MB_ICONERROR)) {
            Diagnostics::LogFormat("MessageBox(error) failed (error %lu).", GetLastError());
        }
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
    g_verification = BackendVerification::NotChecked;
    bool restored = false;
    {
        ScopedFlag busy(g_busy);
        restored = !g_displayFilter->needsRestore() || g_displayFilter->disable();
    }
    if (restored) {
        g_pendingRestore = false;
        StopSessionTimer();
    }
    RefreshTrayIconTip();
    if (!restored && notifyFailure) {
        char message[768];
        sprintf_s(message, "Display restoration failed. RedLight has kept its recovery data.\n"
            "Use Restore display to retry. About lists available shortcuts.\n\n%s", g_displayFilter->lastError());
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
    char profile[48] = {};
    if (g_displayFilter->redMode() == RedMode::LumaRed) {
        sprintf_s(profile, " [%s]", LumaProfileName(g_displayFilter->lumaProfile()));
    }
    sprintf_s(tip, "RedLight %s - %s - %s%s%s",
        FilterStatusText(),
        g_displayFilter->name(), RedModeDisplayName(g_displayFilter->redMode()), profile, remaining);
    UpdateTrayIconTip(tip);
}

void ToggleRedlight() {
    if (g_busy || g_preferencesApplying) return;
    if (!g_trayAdded && !g_displayFilter->needsRestore()) {
        ReportError("RedLight is waiting for its tray controls to return. The filter will stay off.");
        return;
    }
    if (g_displayFilter->needsRestore()) {
        RestoreFilter(true);
    } else {
        Diagnostics::LogFormat("Enable request: %s (%s)", g_displayFilter->name(), RedModeDisplayName(g_displayFilter->redMode()));
        bool enabled = false;
        char enableError[256] = {};
        {
            ScopedFlag busy(g_busy);
            g_verification = BackendVerification::NotChecked;
            enabled = g_displayFilter->enable();
            if (!enabled) strcpy_s(enableError, g_displayFilter->lastError());
        }
        if (g_pendingRestore) RestoreFilter(false);
        if (enabled && g_displayFilter->isActive()) StartSessionTimer();
        if (!enabled) {
            char message[768];
            sprintf_s(message, "The red filter could not be enabled.\n\n%s", enableError);
            ReportError(message);
        }
        RefreshTrayIconTip();
    }
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
    if (g_busy || g_preferencesApplying || minutes > SessionTimer::maxMinutes) return;
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
    g_preferences.sessionMinutes = minutes;
    g_preferencesUnsaved = !SavePreferences(g_preferences);
    if (g_preferencesUnsaved) {
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
        ReportError("Tray recovery failed. Use an available restore shortcut or RedLight.exe --restore, then restart RedLight.");
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
    const bool shortcutsReady = g_hotkeys.initialize(nid.hWnd, g_preferences.shortcuts);
    g_sessionNotifications = WTSRegisterSessionNotification(nid.hWnd, NOTIFY_FOR_THIS_SESSION) != FALSE;
    if (g_displayFilter == &g_gammaRampFilter) {
        ReportError("Using the legacy Strict Red fallback. Driver and multi-monitor support varies. See About and the README for details.", false);
    }
    if (!shortcutsReady || !g_sessionNotifications) {
        ReportError("Some keyboard shortcuts or session notifications are unavailable. See About for shortcut status. Tray controls remain available.", false);
    }
    return true;
}

void SetShortcutProfile(ShortcutProfile profile) {
    if (g_busy || g_preferencesApplying || !IsValidShortcutProfile(static_cast<unsigned>(profile))) return;
    bool changed = false;
    { ScopedFlag busy(g_busy); changed = g_hotkeys.change(profile); }
    if (g_pendingRestore) RestoreFilter(false);
    if (!changed) {
        ReportError("The shortcut profile could not be applied. The prior preference was kept and rollback was attempted. About lists the actual active bindings.");
        return;
    }
    g_preferences.shortcuts = profile;
    g_preferencesUnsaved = !SavePreferences(g_preferences);
    if (g_preferencesUnsaved) ReportError("The shortcuts changed but could not be saved for the next start.");
    Diagnostics::LogFormat("Shortcut preference: %s", ShortcutProfileName(profile));
}

bool ApplyInactivePreferences(const AppPreferences& preferences) {
    if (!ValidPreferences(preferences) || g_displayFilter->needsRestore() || g_displayFilter->isActive()) return false;
    bool applied = true;
    if (g_displayFilter->supportsRedMode(preferences.mode)) applied = g_displayFilter->setRedMode(preferences.mode);
    if (g_displayFilter->supportsRedMode(RedMode::LumaRed)) applied = g_displayFilter->setLumaProfile(preferences.luma) && applied;
    StopSessionTimer();
    return g_sessionTimer.configure(preferences.sessionMinutes, 0, false) && applied;
}

void SetBackendPreference(BackendPreference preference) {
    if (g_busy || g_preferencesApplying || !IsValidBackendPreference(static_cast<unsigned>(preference))) return;
    auto candidate = g_preferences;
    candidate.backend = preference;
    if (!SavePreferences(candidate)) {
        ReportError("The backend preference could not be saved. The previous selection remains in effect.");
        return;
    }
    g_preferences = candidate;
    g_preferencesUnsaved = false;
    g_backendPreference = preference;
    Diagnostics::LogFormat("Backend preference saved for next start: %s", BackendPreferenceName(preference));
}

void SaveRedMode(RedMode mode) {
    g_preferences.mode = mode;
    g_preferencesUnsaved = !SavePreferences(g_preferences);
    if (g_preferencesUnsaved) ReportError("The mode was applied, but could not be saved for the next start.");
}

void SetLumaProfile(LumaProfile profile) {
    if (g_busy || g_preferencesApplying || !g_displayFilter->supportsRedMode(RedMode::LumaRed) ||
        !IsValidLumaProfile(static_cast<unsigned>(profile))) return;
    bool applied = false;
    char error[256] = {};
    {
        ScopedFlag busy(g_busy);
        g_verification = BackendVerification::NotChecked;
        applied = g_displayFilter->setLumaProfile(profile);
        if (!applied) strcpy_s(error, g_displayFilter->lastError());
    }
    if (g_pendingRestore) RestoreFilter(false);
    if (!applied) {
        ReportError(error);
        return;
    }
    g_preferences.luma = profile;
    g_preferencesUnsaved = !SavePreferences(g_preferences);
    if (g_preferencesUnsaved) {
        ReportError("The luma profile was applied but could not be saved for the next start.");
    }
    RefreshTrayIconTip();
}

void SetRedModeFromTray(RedMode mode) {
    if (g_busy || g_preferencesApplying || (g_displayFilter->needsRestore() && !g_displayFilter->isActive())) return;
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

    bool changed = false;
    char modeError[256] = {};
    {
        ScopedFlag busy(g_busy);
        g_verification = BackendVerification::NotChecked;
        changed = g_displayFilter->setRedMode(mode);
        if (!changed) strcpy_s(modeError, g_displayFilter->lastError());
    }
    if (g_pendingRestore) RestoreFilter(false);
    if (!changed) {
        Diagnostics::LogFormat("Red mode change to %s failed.", RedModeDisplayName(mode));
        ReportError(modeError);
    } else {
        Diagnostics::LogFormat("Red mode change to %s succeeded.", RedModeDisplayName(g_displayFilter->redMode()));
        SaveRedMode(g_displayFilter->redMode());
    }

    RefreshTrayIconTip();
}

void CopySupportReport(HWND owner) {
    SupportReportState state;
    state.version = REDLIGHT_VERSION;
    state.buildRevision = REDLIGHT_BUILD_REVISION;
    state.sourceState = REDLIGHT_SOURCE_STATE;
    state.backend = g_displayFilter->name();
    state.startupPolicy = BackendPreferenceName(g_startupBackendPreference);
    state.nextStartupPolicy = BackendPreferenceName(g_backendPreference);
    state.commandLineOverride = g_backendOverride;
    state.preferencesUnsaved = g_preferencesUnsaved;
    const auto logging = Diagnostics::GetStatus();
    state.fileLogging = Diagnostics::FileStateName(logging.file);
    state.droppedLogLines = logging.droppedLines;
    state.truncatedLogLines = logging.truncatedLines;
    state.lastFileError = logging.lastFileError;
    state.verification = g_verification;
    state.redMode = RedModeDisplayName(g_displayFilter->redMode());
    state.lumaProfile = g_displayFilter->supportsRedMode(RedMode::LumaRed) ?
        LumaProfileName(g_displayFilter->lumaProfile()) : "Unavailable on gamma";
    state.lastError = g_displayFilter->lastError();
    state.active = g_displayFilter->isActive();
    state.needsRestore = g_displayFilter->needsRestore();
    state.toggleHotkey = g_hotkeys.toggle().has_value();
    state.restoreHotkey = g_hotkeys.restore().has_value();
    state.shortcutProfile = ShortcutProfileName(g_preferences.shortcuts);
    if (g_hotkeys.toggle()) state.toggleBinding = ShortcutBindingName(*g_hotkeys.toggle());
    if (g_hotkeys.restore()) state.restoreBinding = ShortcutBindingName(*g_hotkeys.restore());
    state.sessionNotifications = g_sessionNotifications;
    state.sessionMinutes = g_sessionTimer.minutes();
    if (g_sessionTimer.armed()) state.remainingSeconds = (g_sessionTimer.remainingMilliseconds(GetTickCount64()) + 999) / 1000;
    if (!CopyUtf8TextToClipboard(owner, BuildSupportReport(state))) {
        ReportError("The support report could not be copied. Another program may be using the clipboard. Please retry.");
        return;
    }
    Diagnostics::Log("Support report copied to clipboard at user request.");
    ShowTrayInfo("RedLight support report copied", "Paste the report into a support request or save it in a text editor.");
}

void ShowTrayInfo(const char* title, const char* message) {
    NOTIFYICONDATAA notification = nid;
    notification.uFlags = NIF_INFO;
    strncpy_s(notification.szInfoTitle, title, _TRUNCATE);
    strncpy_s(notification.szInfo, message, _TRUNCATE);
    notification.dwInfoFlags = NIIF_INFO;
    if (!Shell_NotifyIconA(NIM_MODIFY, &notification)) Diagnostics::Log("Tray confirmation notification failed.");
}

void CopySettingsProfile(HWND owner) {
    const auto profile = SerializePreferences(g_preferences);
    if (!profile || !CopyUtf8TextToClipboard(owner, *profile)) {
        ReportError("The settings profile could not be copied. Please retry when the clipboard is available.");
        return;
    }
    Diagnostics::Log("Settings profile copied to clipboard at user request.");
    ShowTrayInfo("RedLight settings profile copied", "Paste into a text file to keep or share your red-only filter preferences.");
}

void ImportSettingsProfile(HWND owner) {
    if (g_busy || g_preferencesApplying) return;
    const auto text = ReadUtf8TextFromClipboard(owner);
    if (!text) {
        ReportError("A settings profile could not be read from the clipboard. Copy a RedLight profile as text, then retry.");
        return;
    }
    const auto parsed = ParsePreferences(*text);
    if (!parsed.preferences) {
        char error[512];
        sprintf_s(error, "Settings profile rejected at line %u: %s.\nNo preferences or display state were changed.", parsed.line, parsed.error);
        ReportError(error);
        return;
    }
    ConfirmPreferences(owner, *parsed.preferences, "Import RedLight settings");
}

void ConfirmPreferences(HWND owner, const AppPreferences& candidate, const char* title) {
    if (g_busy || g_preferencesApplying || !ValidPreferences(candidate)) return;
    char preview[768];
    sprintf_s(preview, "Apply these preferences?\n\nMode: %s\nLuma detail: %s\nBackend on next start: %s\n"
        "Session limit: %u minutes (0 = unlimited)\nShortcut profile: %s\n\n"
        "The display will be restored and stay off. Backend selection applies on the next start.\n"
        "Luma preferences are retained for Magnification when the current backend is gamma.",
        RedModeDisplayName(candidate.mode), LumaProfileName(candidate.luma), BackendPreferenceName(candidate.backend), candidate.sessionMinutes, ShortcutProfileName(candidate.shortcuts));
    const int answer = MessageBoxA(owner, preview, title, MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION);
    if (!answer) Diagnostics::LogFormat("MessageBox(preferences) failed (error %lu).", GetLastError());
    if (answer != IDYES) return;
    // Recheck after the modal preview: native callbacks may have run while it was open.
    if (g_busy || g_preferencesApplying) return;
    ScopedFlag transaction(g_preferencesApplying);
    const auto saved = SaveImportedPreferences(candidate, [] { return RestoreFilter(true); });
    if (saved == PreferenceImportResult::Rejected || saved == PreferenceImportResult::RestoreFailed) return;
    if (saved == PreferenceImportResult::SaveFailed) {
        ReportError("The preferences could not be saved. Your previous preferences remain selected; the display is off.");
        return;
    }
    g_preferences = candidate;
    g_backendPreference = candidate.backend;
    g_preferencesUnsaved = false;
    const bool modeApplied = ApplyInactivePreferences(candidate);
    const bool shortcutsApplied = g_hotkeys.change(candidate.shortcuts);
    if (!modeApplied || !shortcutsApplied) {
        ReportError("The preferences were saved, but some choices could not be applied in this session. Restart RedLight to use them.");
        return;
    }
    Diagnostics::LogFormat("%s applied after preview confirmation and restoration; display remains off.", title);
    RefreshTrayIconTip();
    ShowTrayInfo("RedLight preferences saved", "The filter remains off; backend preference applies on the next start.");
}

const char* FilterStatusText() {
    return FilterStatusDescription(g_displayFilter->isActive(), g_displayFilter->needsRestore(),
        g_displayFilter->lastError()[0] != 0, g_verification);
}

void VerifyBackendState(HWND owner) {
    if (g_busy) return;
    { ScopedFlag busy(g_busy); g_verification = g_displayFilter->verifyState(); }
    if (g_pendingRestore) RestoreFilter(false);
    RefreshTrayIconTip();
    const char* message = BackendVerificationName(g_verification);
    if (g_verification == BackendVerification::Matches) {
        message = "Software read-back matches the requested red-only transform. Physical display output was not measured.";
    } else if (g_verification == BackendVerification::Changed) {
        message = "Read-back differs from the requested transform. Another color tool or display change may be involved.\n\n"
            "This check did not reapply the filter. Use Restore display to restore the saved state, then re-enable when ready.";
    } else if (g_verification == BackendVerification::Unavailable) {
        message = "The backend state could not be read. See the diagnostics log and use Restore display if recovery is needed.";
    }
    if (!MessageBoxA(owner, message, "RedLight backend state", MB_OK | MB_ICONINFORMATION))
        Diagnostics::LogFormat("MessageBox(verification) failed (error %lu).", GetLastError());
}

void ShowAboutDialog(HWND parent) {
    char aboutText[1536];
    const auto logging = Diagnostics::GetStatus();
    sprintf_s(aboutText, "RedLight " REDLIGHT_VERSION "\nBuild revision: " REDLIGHT_BUILD_REVISION " (" REDLIGHT_SOURCE_STATE ")\n\nBackend: %s\nStartup selection: %s%s\nNext start: %s\nMode: %s\nLuma detail: %s\nStatus: %s\nSettings: %s\n\n"
        "Toggle shortcut: %s\nRestore shortcut: %s\nShortcut preference: %s\nSession recovery: %s\n\n"
        "File logging: %s\nMissing file records: %llu\nTruncated records: %llu\nLast file error: %lu\n\n"
        "Logs: %%LOCALAPPDATA%%\\RedLight\nUnsigned release candidate\n"
        "github.com/michaelmawhinney/redlight",
        g_displayFilter->name(), BackendPreferenceName(g_startupBackendPreference),
        g_backendOverride ? " (command line)" : "", BackendPreferenceName(g_backendPreference),
        RedModeDisplayName(g_displayFilter->redMode()),
        g_displayFilter->supportsRedMode(RedMode::LumaRed) ? LumaProfileName(g_displayFilter->lumaProfile()) : "Unavailable on gamma",
        FilterStatusText(),
        g_preferencesUnsaved ? "Unsaved changes" : "Saved/default",
        g_hotkeys.toggle() ? ShortcutBindingName(*g_hotkeys.toggle()) : "Unavailable/disabled",
        g_hotkeys.restore() ? ShortcutBindingName(*g_hotkeys.restore()) : "Unavailable/disabled",
        ShortcutProfileName(g_preferences.shortcuts),
        g_sessionNotifications ? "Available" : "Unavailable (registration failed)",
        Diagnostics::FileStateName(logging.file), static_cast<unsigned long long>(logging.droppedLines),
        static_cast<unsigned long long>(logging.truncatedLines), static_cast<unsigned long>(logging.lastFileError));
    if (!MessageBoxA(parent, aboutText, "About RedLight", MB_OK | MB_ICONINFORMATION))
        Diagnostics::LogFormat("MessageBox(about) failed (error %lu).", GetLastError());
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
    const TrayMenuState state{g_displayFilter->needsRestore(), g_displayFilter->redMode(),
        g_displayFilter->supportsRedMode(RedMode::LumaRed), g_displayFilter->lumaProfile(),
        g_backendPreference, g_sessionTimer.minutes(), g_preferences.shortcuts,
        g_hotkeys.toggle() ? ShortcutBindingName(*g_hotkeys.toggle()) : NULL,
        g_hotkeys.restore() ? ShortcutBindingName(*g_hotkeys.restore()) : NULL};
    HMENU menu = BuildTrayMenu(state);
    if (!menu) { ReportError("Could not build the tray menu."); return; }
    if (!SetForegroundWindow(hwnd)) Diagnostics::Log("SetForegroundWindow failed for tray menu.");
    const UINT command = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
        pt.x, pt.y, 0, hwnd, NULL);
    if (command) PostTrayCommand(hwnd, command);
    if (!PostMessageA(hwnd, WM_NULL, 0, 0)) Diagnostics::Log("Posting tray menu dismissal failed.");
    if (!Shell_NotifyIconA(NIM_SETFOCUS, &nid)) Diagnostics::Log("Could not restore tray keyboard focus.");
    if (!DestroyMenu(menu)) Diagnostics::Log("DestroyMenu failed.");
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) noexcept {
    return GuardUiCall<LRESULT>([&] { return HandleWindowMessage(hwnd, uMsg, wParam, lParam); },
        [&](const char* error) -> LRESULT {
            Diagnostics::LogFormat("C++ failure in window message %u: %s", uMsg, error);
            if (uMsg == WM_REDLIGHT_STATUS) return 0; // Read-only automation never restores or opens an error dialog.
            const bool mayChangeFilter = uMsg == WM_COMMAND ? CommandMayChangeFilter(LOWORD(wParam)) :
                uMsg != WM_TRAYICON;
            if (mayChangeFilter) {
                if (g_busy) g_pendingRestore = true;
                else {
                    try { RestoreFilter(false); }
                    catch (...) { Diagnostics::Log("C++ failure during emergency restoration; recovery data retained."); }
                }
            }
            if (uMsg != WM_REDLIGHT_UI_ERROR && !PostMessageA(hwnd, WM_REDLIGHT_UI_ERROR, mayChangeFilter ? 1 : 0, 0))
                Diagnostics::LogFormat("Posting internal-error notification failed (error %lu).", GetLastError());
            if (uMsg == WM_NCCREATE) return FALSE;
            if (uMsg == WM_CREATE) return -1;
            if (uMsg == WM_QUERYENDSESSION) return TRUE;
            return 0;
        });
}

LRESULT HandleWindowMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (g_taskbarCreated && uMsg == g_taskbarCreated) {
        g_trayAdded = false;
        Diagnostics::Log("Explorer restarted; restoring display and tray control.");
        RestoreFilter(false);
        if (!AddTrayIcon()) ScheduleTrayRecovery();
        return 0;
    }
    switch (uMsg) {
    case WM_REDLIGHT_STATUS: {
        if (wParam != REDLIGHT_STATUS_PROTOCOL || lParam != 0) return 0;
        TrayStatus state;
        state.backendActive = g_displayFilter->isActive();
        state.restorePending = g_displayFilter->needsRestore();
        state.backendError = g_displayFilter->lastError()[0] != '\0';
        state.supportsLuma = g_displayFilter->supportsRedMode(RedMode::LumaRed);
        state.backend = g_displayFilter == &g_magnificationFilter ? StatusBackend::Magnification : StatusBackend::Gamma;
        state.mode = g_displayFilter->redMode();
        state.luma = g_displayFilter->lumaProfile();
        state.verification = g_verification;
        state.sessionMinutes = g_sessionTimer.minutes();
        state.timerArmed = g_sessionTimer.armed();
        state.remainingSeconds = static_cast<unsigned>((g_sessionTimer.remainingMilliseconds(GetTickCount64()) + 999) / 1000);
        state.shortcutProfile = g_preferences.shortcuts;
        state.toggle = StatusShortcutKey(g_hotkeys.toggle());
        state.restore = StatusShortcutKey(g_hotkeys.restore());
        state.settingsUnsaved = g_preferencesUnsaved;
        state.sessionNotifications = g_sessionNotifications;
        state.busy = g_busy || g_preferencesApplying;
        const auto logging = Diagnostics::GetStatus();
        state.fileLogging = logging.file;
        state.missingFileRecords = logging.droppedLines != 0;
        state.truncatedRecords = logging.truncatedLines != 0;
        state.fileError = logging.lastFileError != 0;
        const auto reply = PackTrayStatus(state);
        if (!reply) Diagnostics::Log("Cached status snapshot is inconsistent; refusing to report it as valid.");
        return reply ? static_cast<LRESULT>(*reply) : 0;
    }
    case WM_REDLIGHT_UI_ERROR: {
        if (g_uiErrorVisible) return 0;
        ScopedFlag visible(g_uiErrorVisible);
        if (!wParam) ReportError("The action could not complete because of an internal error. Filtering controls remain available. See the diagnostics log.");
        else if (g_displayFilter->needsRestore()) ReportError("An internal error interrupted an action and restoration needs attention. Use Restore display to retry. Recovery data remains available.");
        else ReportError("An internal error interrupted an action. RedLight requested restoration and its filter is off. See the diagnostics log.");
        return 0;
    }
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
        if (!g_hotkeys.accepts(static_cast<unsigned>(wParam), LOWORD(lParam), HIWORD(lParam))) {
            Diagnostics::Log("Ignored stale/unregistered shortcut message.");
            return 0;
        }
        if (wParam == ID_HOTKEY_TOGGLE) ToggleRedlight();
        else if (wParam == ID_HOTKEY_RESTORE) RestoreFilter(true);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) >= ID_SHORTCUT_PROFILE_BASE && LOWORD(wParam) <= ID_SHORTCUT_PROFILE_BASE + 3) {
            SetShortcutProfile(static_cast<ShortcutProfile>(LOWORD(wParam) - ID_SHORTCUT_PROFILE_BASE));
            return 0;
        }
        if (LOWORD(wParam) >= ID_LUMA_PROFILE_BASE && LOWORD(wParam) <= ID_LUMA_PROFILE_BASE + 2) {
            SetLumaProfile(static_cast<LumaProfile>(LOWORD(wParam) - ID_LUMA_PROFILE_BASE));
            return 0;
        }
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
        case ID_TRAY_CUSTOM_SESSION:
            if (!g_busy && !g_preferencesApplying) {
                if (const auto minutes = ShowSessionLimitDialog(hwnd, g_sessionTimer.minutes())) SetSessionLimit(*minutes);
            }
            break;
        case ID_TRAY_RESTORE: RestoreFilter(true); break;
        case ID_TRAY_MODE_STRICT_RED: SetRedModeFromTray(RedMode::StrictRed); break;
        case ID_TRAY_MODE_LUMA_RED: SetRedModeFromTray(RedMode::LumaRed); break;
        case ID_TRAY_ABOUT: ShowAboutDialog(hwnd); break;
        case ID_TRAY_COPY_REPORT: CopySupportReport(hwnd); break;
        case ID_TRAY_RETRY_LOGGING:
            if (Diagnostics::RetryFileLogging()) {
                ShowTrayInfo("RedLight file logging available", "File logging recovered. Earlier missing-record counts remain in the support report.");
            } else ReportError("File logging is still unavailable or degraded. About and support reports show the last file error. Debugger output remains available.");
            break;
        case ID_TRAY_VERIFY_BACKEND: VerifyBackendState(hwnd); break;
        case ID_TRAY_COPY_PROFILE: CopySettingsProfile(hwnd); break;
        case ID_TRAY_IMPORT_PROFILE: ImportSettingsProfile(hwnd); break;
        case ID_TRAY_RESET_PREFERENCES: ConfirmPreferences(hwnd, AppPreferences{}, "Reset RedLight preferences to defaults"); break;
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
        if (!g_hotkeys.release()) Diagnostics::Log("Some shortcut registrations could not be released before window destruction.");
        if (g_sessionNotifications && !WTSUnRegisterSessionNotification(hwnd)) Diagnostics::Log("Unregister session notifications failed.");
        if (g_trayAdded && !Shell_NotifyIconA(NIM_DELETE, &nid)) Diagnostics::Log("Tray icon removal failed.");
        g_trayAdded = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}
