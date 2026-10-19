#include "ScheduleDialog.h"
#include "Diagnostics.h"
#include "resource.h"
#include <iterator>
#include <cwchar>

namespace {
void End(HWND dialog, INT_PTR result) {
    if (!EndDialog(dialog, result)) Diagnostics::LogFormat("EndDialog(schedule) failed (error %lu).", GetLastError());
}
std::optional<unsigned> ReadTime(HWND dialog, int id) {
    const HWND edit = GetDlgItem(dialog, id);
    wchar_t wide[32] = {};
    const int length = edit ? GetWindowTextLengthW(edit) : -1;
    if (length != 5 || GetDlgItemTextW(dialog, id, wide, static_cast<int>(std::size(wide))) != 5) return std::nullopt;
    char text[5];
    for (unsigned index = 0; index < 5; ++index) {
        if (wide[index] > 127) return std::nullopt;
        text[index] = static_cast<char>(wide[index]);
    }
    return Scheduling::ParseMinute(std::string_view(text, 5));
}
INT_PTR CALLBACK Proc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_INITDIALOG) {
        auto* times = reinterpret_cast<Scheduling::DailyTimes*>(lParam);
        wchar_t enableAt[6], restoreAt[6];
        if (!times || !times->valid()) { End(dialog, IDCANCEL); return TRUE; }
        swprintf_s(enableAt, L"%02u:%02u", times->enableMinute / 60, times->enableMinute % 60);
        swprintf_s(restoreAt, L"%02u:%02u", times->restoreMinute / 60, times->restoreMinute % 60);
        SetLastError(ERROR_SUCCESS);
        if ((!SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(times)) && GetLastError() != ERROR_SUCCESS) ||
            !CheckDlgButton(dialog, IDC_SCHEDULE_ENABLED, times->enabled ? BST_CHECKED : BST_UNCHECKED) ||
            !SetDlgItemTextW(dialog, IDC_SCHEDULE_ENABLE_AT, enableAt) || !SetDlgItemTextW(dialog, IDC_SCHEDULE_RESTORE_AT, restoreAt)) {
            Diagnostics::Log("Could not initialize the daily schedule dialog."); End(dialog, IDCANCEL);
        }
        return TRUE;
    }
    if (message == WM_CLOSE || (message == WM_COMMAND && LOWORD(wParam) == IDCANCEL)) { End(dialog, IDCANCEL); return TRUE; }
    if (message == WM_COMMAND && LOWORD(wParam) == IDOK) {
        auto* times = reinterpret_cast<Scheduling::DailyTimes*>(GetWindowLongPtrW(dialog, DWLP_USER));
        const UINT checked = IsDlgButtonChecked(dialog, IDC_SCHEDULE_ENABLED);
        const auto enableAt = ReadTime(dialog, IDC_SCHEDULE_ENABLE_AT), restoreAt = ReadTime(dialog, IDC_SCHEDULE_RESTORE_AT);
        const bool valid = times && GetDlgItem(dialog, IDC_SCHEDULE_ENABLED) && (checked == BST_CHECKED || checked == BST_UNCHECKED) &&
            enableAt && restoreAt && (checked != BST_CHECKED || *enableAt != *restoreAt);
        if (!valid) {
            if (!MessageBoxW(dialog, L"Use 24-hour times from 00:00 to 23:59. When scheduling is enabled, the two times must differ.",
                L"Daily schedule", MB_OK | MB_ICONINFORMATION)) Diagnostics::LogFormat("MessageBox(schedule validation) failed (error %lu).", GetLastError());
            const HWND edit = GetDlgItem(dialog, !enableAt || (enableAt && restoreAt && *enableAt == *restoreAt) ? IDC_SCHEDULE_ENABLE_AT : IDC_SCHEDULE_RESTORE_AT);
            if (edit) {
                SetLastError(ERROR_SUCCESS);
                const HWND prior = SetFocus(edit);
                const auto error = prior ? ERROR_SUCCESS : GetLastError();
                if (GetFocus() != edit) Diagnostics::LogFormat("Schedule input focus not confirmed (error %lu).", error);
                else SendMessageW(edit, EM_SETSEL, 0, -1);
            }
            return TRUE;
        }
        *times = {checked == BST_CHECKED, *enableAt, *restoreAt};
        End(dialog, IDOK); return TRUE;
    }
    return FALSE;
}
}
std::optional<Scheduling::DailyTimes> ShowScheduleDialog(HWND owner, Scheduling::DailyTimes current) {
    if (!current.valid()) return std::nullopt;
    const auto instance = GetModuleHandleW(nullptr);
    if (!instance) { Diagnostics::Log("GetModuleHandle(schedule dialog) failed."); return std::nullopt; }
    const auto result = DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_DAILY_SCHEDULE), owner, Proc, reinterpret_cast<LPARAM>(&current));
    if (result == -1) {
        Diagnostics::LogFormat("DialogBoxParam(schedule) failed (error %lu).", GetLastError());
        if (!MessageBoxW(owner, L"The schedule dialog could not be opened. Your previous schedule is unchanged.",
            L"RedLight", MB_OK | MB_ICONERROR)) Diagnostics::LogFormat("MessageBox(schedule failure) failed (error %lu).", GetLastError());
    }
    return result == IDOK ? std::optional<Scheduling::DailyTimes>(current) : std::nullopt;
}
