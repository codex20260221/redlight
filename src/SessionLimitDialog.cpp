#include "SessionLimitDialog.h"
#include "SessionTimer.h"
#include "Diagnostics.h"
#include "resource.h"
#include <iterator>

namespace {
struct DialogState { unsigned minutes; };
void EndSessionDialog(HWND dialog, INT_PTR result) {
    if (!EndDialog(dialog, result)) Diagnostics::LogFormat("EndDialog(session) failed (error %lu).", GetLastError());
}
INT_PTR CALLBACK SessionDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_INITDIALOG) {
        auto* state = reinterpret_cast<DialogState*>(lParam);
        SetLastError(ERROR_SUCCESS);
        if (!SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(state)) && GetLastError() != ERROR_SUCCESS) {
            Diagnostics::LogFormat("SetWindowLongPtr(session) failed (error %lu).", GetLastError());
            EndSessionDialog(dialog, IDCANCEL);
            return TRUE;
        }
        if (!state || !SetDlgItemInt(dialog, IDC_SESSION_MINUTES, state->minutes, FALSE)) {
            Diagnostics::Log("Could not initialize the custom session-limit dialog.");
            EndSessionDialog(dialog, IDCANCEL);
        }
        return TRUE;
    }
    if (message == WM_COMMAND) {
        if (LOWORD(wParam) == IDCANCEL) { EndSessionDialog(dialog, IDCANCEL); return TRUE; }
        if (LOWORD(wParam) == IDOK) {
            auto* state = reinterpret_cast<DialogState*>(GetWindowLongPtrW(dialog, DWLP_USER));
            wchar_t text[32] = {};
            const HWND edit = GetDlgItem(dialog, IDC_SESSION_MINUTES);
            const int sourceLength = edit ? GetWindowTextLengthW(edit) : -1;
            const UINT length = GetDlgItemTextW(dialog, IDC_SESSION_MINUTES, text, static_cast<int>(std::size(text)));
            const auto minutes = sourceLength >= 0 && sourceLength < static_cast<int>(std::size(text)) &&
                length == static_cast<UINT>(sourceLength) ? ParseSessionMinutes(std::wstring_view(text, length)) : std::nullopt;
            if (!state || !minutes) {
                if (!MessageBoxW(dialog, L"Enter a whole number from 0 to 240 using digits 0-9. Zero means unlimited.",
                    L"Auto-restore time limit", MB_OK | MB_ICONINFORMATION))
                    Diagnostics::LogFormat("MessageBox(session validation) failed (error %lu).", GetLastError());
                return TRUE;
            }
            state->minutes = *minutes;
            EndSessionDialog(dialog, IDOK);
            return TRUE;
        }
    }
    if (message == WM_CLOSE) { EndSessionDialog(dialog, IDCANCEL); return TRUE; }
    return FALSE;
}
}
std::optional<unsigned> ShowSessionLimitDialog(HWND owner, unsigned currentMinutes) {
    if (currentMinutes > SessionTimer::maxMinutes) return std::nullopt;
    HINSTANCE instance = GetModuleHandleW(NULL);
    if (!instance) { Diagnostics::Log("GetModuleHandle(session dialog) failed."); return std::nullopt; }
    DialogState state{currentMinutes};
    const INT_PTR result = DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_CUSTOM_SESSION_LIMIT), owner,
        SessionDialogProc, reinterpret_cast<LPARAM>(&state));
    if (result == -1) {
        Diagnostics::LogFormat("DialogBoxParam(session) failed (error %lu).", GetLastError());
        if (!MessageBoxW(owner, L"The custom time-limit dialog could not be opened. The previous limit is unchanged.",
            L"RedLight", MB_OK | MB_ICONERROR))
            Diagnostics::LogFormat("MessageBox(session dialog failure) failed (error %lu).", GetLastError());
    }
    return result == IDOK ? std::optional<unsigned>(state.minutes) : std::nullopt;
}
