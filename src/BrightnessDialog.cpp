#include "BrightnessDialog.h"
#include "RedBrightness.h"
#include "Diagnostics.h"
#include "resource.h"
#include <iterator>

namespace {
void EndBrightnessDialog(HWND dialog, INT_PTR result) {
    if (!EndDialog(dialog, result)) Diagnostics::LogFormat("EndDialog(brightness) failed (error %lu).", GetLastError());
}
INT_PTR CALLBACK BrightnessDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_INITDIALOG) {
        auto* percent = reinterpret_cast<unsigned*>(lParam);
        SetLastError(ERROR_SUCCESS);
        if ((!SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(percent)) && GetLastError() != ERROR_SUCCESS) ||
            !percent || !SetDlgItemInt(dialog, IDC_RED_BRIGHTNESS, *percent, FALSE)) {
            Diagnostics::Log("Could not initialize the red brightness dialog.");
            EndBrightnessDialog(dialog, IDCANCEL);
        }
        return TRUE;
    }
    if (message == WM_COMMAND) {
        if (LOWORD(wParam) == IDCANCEL) { EndBrightnessDialog(dialog, IDCANCEL); return TRUE; }
        if (LOWORD(wParam) == IDOK) {
            auto* percent = reinterpret_cast<unsigned*>(GetWindowLongPtrW(dialog, DWLP_USER));
            wchar_t text[32] = {};
            const HWND edit = GetDlgItem(dialog, IDC_RED_BRIGHTNESS);
            const int sourceLength = edit ? GetWindowTextLengthW(edit) : -1;
            const UINT length = GetDlgItemTextW(dialog, IDC_RED_BRIGHTNESS, text, static_cast<int>(std::size(text)));
            const auto parsed = sourceLength >= 0 && sourceLength < static_cast<int>(std::size(text)) &&
                length == static_cast<UINT>(sourceLength) ? ParseRedBrightness(std::wstring_view(text, length)) : std::nullopt;
            if (!percent || !parsed) {
                if (!MessageBoxW(dialog, L"Enter a whole number from 1 to 100 using digits 0-9.",
                    L"Red brightness", MB_OK | MB_ICONINFORMATION))
                    Diagnostics::LogFormat("MessageBox(brightness validation) failed (error %lu).", GetLastError());
                if (edit) {
                    SetLastError(ERROR_SUCCESS);
                    const HWND previous = SetFocus(edit);
                    const DWORD error = previous ? ERROR_SUCCESS : GetLastError();
                    if (GetFocus() != edit) Diagnostics::LogFormat("Brightness input focus was not confirmed (error %lu).", error);
                    else SendMessageW(edit, EM_SETSEL, 0, -1); // EM_SETSEL has no return value.
                }
                return TRUE;
            }
            *percent = *parsed;
            EndBrightnessDialog(dialog, IDOK);
            return TRUE;
        }
    }
    if (message == WM_CLOSE) { EndBrightnessDialog(dialog, IDCANCEL); return TRUE; }
    return FALSE;
}
}
std::optional<unsigned> ShowRedBrightnessDialog(HWND owner, unsigned currentPercent) {
    if (!ValidRedBrightness(currentPercent)) return std::nullopt;
    const auto instance = GetModuleHandleW(nullptr);
    if (!instance) { Diagnostics::Log("GetModuleHandle(brightness dialog) failed."); return std::nullopt; }
    const auto result = DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_RED_BRIGHTNESS), owner,
        BrightnessDialogProc, reinterpret_cast<LPARAM>(&currentPercent));
    if (result == -1) {
        Diagnostics::LogFormat("DialogBoxParam(brightness) failed (error %lu).", GetLastError());
        if (!MessageBoxW(owner, L"The brightness dialog could not be opened. Your previous brightness is unchanged.",
            L"RedLight", MB_OK | MB_ICONERROR)) Diagnostics::LogFormat("MessageBox(brightness dialog failure) failed (error %lu).", GetLastError());
    }
    return result == IDOK ? std::optional<unsigned>(currentPercent) : std::nullopt;
}
