#include <windows.h>
#include <shellapi.h>
#include <cstdio>

#define WM_TRAYICON (WM_USER + 1)
#define IDI_REDLIGHT_ICON 100
#define ID_TRAY_TOGGLE 200
#define ID_TRAY_ABOUT 201
#define ID_TRAY_EXIT 202

HDC hDC = nullptr;
WORD originalGammaRamp[3][256] = {};
WORD redGammaRamp[3][256] = {};
WORD fallbackLinearGammaRamp[3][256] = {};
bool isRedlightActive = false;
bool hasOriginalGammaRamp = false;
NOTIFYICONDATA nid = {};
UINT WM_TASKBARCREATED = 0;

void ToggleRedlight();
void UpdateTrayIconTip(const char* tip);
bool InitializeTrayIcon(HINSTANCE hInstance);
void RemoveTrayIcon();
void ShowAboutDialog(HWND parent);
void BuildRedGammaRamp();
void BuildFallbackLinearGammaRamp();
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    // silently exit multiple instances
    HANDLE hMutex = CreateMutex(nullptr, TRUE, "Global\\RedLightApp");
    if (!hMutex) {
        MessageBox(nullptr, "Failed to create single-instance mutex.", "Error", MB_ICONERROR);
        return 1;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        return 0;
    }

    hDC = GetDC(nullptr);
    if (!hDC) {
        MessageBox(nullptr, "Failed to get device context.", "Error", MB_ICONERROR);
        CloseHandle(hMutex);
        return 1;
    }

    hasOriginalGammaRamp = GetDeviceGammaRamp(hDC, originalGammaRamp) == TRUE;
    BuildRedGammaRamp();
    BuildFallbackLinearGammaRamp();

    WM_TASKBARCREATED = RegisterWindowMessage(TEXT("TaskbarCreated"));

    if (!InitializeTrayIcon(hInstance)) {
        ReleaseDC(nullptr, hDC);
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        MessageBox(nullptr, "Failed to initialize tray icon.", "Error", MB_ICONERROR);
        return 1;
    }

    MSG msg = {};
    BOOL result = 0;
    while ((result = GetMessage(&msg, nullptr, 0, 0)) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    RemoveTrayIcon();
    if (isRedlightActive) {
        WORD(*restoreRamp)[256] = hasOriginalGammaRamp ? originalGammaRamp : fallbackLinearGammaRamp;
        SetDeviceGammaRamp(hDC, restoreRamp); // restore original (or neutral fallback) gamma ramp
    }

    ReleaseDC(nullptr, hDC);
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);

    return (result == -1) ? 1 : 0;
}

void BuildRedGammaRamp() {
    for (int i = 0; i < 256; i++) {
        redGammaRamp[0][i] = static_cast<WORD>(i << 8);
        redGammaRamp[1][i] = 0;
        redGammaRamp[2][i] = 0;
    }
}


void BuildFallbackLinearGammaRamp() {
    for (int i = 0; i < 256; i++) {
        const WORD value = static_cast<WORD>(i << 8);
        fallbackLinearGammaRamp[0][i] = value;
        fallbackLinearGammaRamp[1][i] = value;
        fallbackLinearGammaRamp[2][i] = value;
    }
}

void UpdateTrayIconTip(const char* tip) {
    strcpy_s(nid.szTip, sizeof(nid.szTip), tip);
    Shell_NotifyIcon(NIM_MODIFY, &nid);
}

void ToggleRedlight() {
    bool nextStateActive = !isRedlightActive;
    WORD(*targetRamp)[256] = nextStateActive
        ? redGammaRamp
        : (hasOriginalGammaRamp ? originalGammaRamp : fallbackLinearGammaRamp);

    if (!SetDeviceGammaRamp(hDC, targetRamp)) {
        MessageBox(nullptr, "Failed to apply gamma ramp.", "Error", MB_ICONERROR);
        return;
    }

    isRedlightActive = nextStateActive;
    UpdateTrayIconTip(isRedlightActive ? "RedLight ON" : "RedLight off");
}

bool InitializeTrayIcon(HINSTANCE hInstance) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = TEXT("TrayOnlyClass");

    if (!RegisterClass(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    HWND hwnd = CreateWindowEx(0, TEXT("TrayOnlyClass"), TEXT("RedLight"), 0, 0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr);
    if (!hwnd) {
        return false;
    }

    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = IDI_REDLIGHT_ICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_REDLIGHT_ICON));
    if (!nid.hIcon) {
        nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    }
    strcpy_s(nid.szTip, "RedLight off");

    return Shell_NotifyIcon(NIM_ADD, &nid) == TRUE;
}

void RemoveTrayIcon() {
    if (nid.hWnd != nullptr) {
        Shell_NotifyIcon(NIM_DELETE, &nid);
    }
}

void ShowAboutDialog(HWND parent) {
    char aboutText[512];
    sprintf_s(aboutText, sizeof(aboutText), "RedLight v0.4.0-beta\n\ngithub.com/michaelmawhinney/redlight");
    const char* aboutTitle = "About";

    MessageBox(parent, aboutText, aboutTitle, MB_OK | MB_ICONINFORMATION);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (WM_TASKBARCREATED != 0 && uMsg == WM_TASKBARCREATED) {
        Shell_NotifyIcon(NIM_ADD, &nid);
        return 0;
    }

    switch (uMsg) {
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONDOWN) {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            if (hMenu) {
                AppendMenu(hMenu, MF_STRING, ID_TRAY_TOGGLE, TEXT("Toggle ON/off"));
                AppendMenu(hMenu, MF_STRING, ID_TRAY_ABOUT, TEXT("About"));
                AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, TEXT("Exit"));
                SetForegroundWindow(hwnd);
                TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
                DestroyMenu(hMenu);
            }
        } else if (lParam == WM_LBUTTONDOWN) {
            ToggleRedlight();
        }
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_TRAY_TOGGLE) {
            ToggleRedlight();
        } else if (LOWORD(wParam) == ID_TRAY_EXIT) {
            RemoveTrayIcon();
            PostQuitMessage(0);
        } else if (LOWORD(wParam) == ID_TRAY_ABOUT) {
            ShowAboutDialog(hwnd);
        }
        break;

    case WM_DESTROY:
        RemoveTrayIcon();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
