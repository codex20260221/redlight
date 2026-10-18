#include "StartupRegistration.h"
#include "Diagnostics.h"
#include <algorithm>

namespace {
bool SafeCommandText(std::wstring_view value) noexcept {
    for (const auto ch : value) if (ch < 32 || ch == 127) return false; // Includes embedded NUL; preview must show the whole value.
    return true;
}
}
bool StartupRegistration::initialize() noexcept {
    command_ = {};
    std::array<wchar_t, StartupCommandMaxCharacters - 1> path{}; // At most 258 path characters plus two quotes.
    SetLastError(ERROR_SUCCESS);
    const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    pathError_ = ERROR_SUCCESS;
    if (!length) {
        pathError_ = GetLastError();
        Diagnostics::LogFormat("Startup executable lookup failed (native error %lu; zero means no code supplied).", pathError_);
    } else if (length >= path.size()) {
        pathError_ = ERROR_FILENAME_EXCED_RANGE;
        Diagnostics::Log("Startup entry refused: quoted executable path exceeds the Run command bound.");
    } else {
        const std::wstring_view name(path.data(), length);
        const bool drive = name.size() >= 3 && ((name[0] >= L'A' && name[0] <= L'Z') || (name[0] >= L'a' && name[0] <= L'z')) && name[1] == L':' && name[2] == L'\\';
        const auto serverEnd = name.find(L'\\', 2), shareEnd = serverEnd == std::wstring_view::npos ? serverEnd : name.find(L'\\', serverEnd + 1);
        const bool unc = name.starts_with(L"\\\\") && !name.starts_with(L"\\\\?\\") && !name.starts_with(L"\\\\.\\") &&
            serverEnd != std::wstring_view::npos && serverEnd > 2 && shareEnd != std::wstring_view::npos && shareEnd > serverEnd + 1 && shareEnd + 1 < name.size();
        if ((!drive && !unc) || !SafeCommandText(name) || name.find_first_of(L"\"%") != std::wstring_view::npos || name.back() == L'\\') {
            pathError_ = ERROR_INVALID_DATA;
            Diagnostics::Log("Startup entry refused: executable path is not a supported absolute literal path.");
        } else {
            command_[0] = L'"'; std::copy(name.begin(), name.end(), command_.begin() + 1); command_[length + 1] = L'"';
        }
    }
    refresh();
    return !command().empty();
}
void StartupRegistration::refresh() noexcept {
    StartupEntrySnapshot current;
    DWORD type = 0, bytes = static_cast<DWORD>(sizeof(current.command));
    const auto status = RegGetValueW(HKEY_CURRENT_USER, RedLightRunKey, RedLightRunValue, RRF_RT_REG_SZ, &type, current.command.data(), &bytes);
    if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND) {
        current = {}; current.state = StartupEntryState::Off;
    } else if (status != ERROR_SUCCESS) {
        current = {}; current.error = static_cast<std::uint32_t>(status);
    } else if (type != REG_SZ || bytes < sizeof(wchar_t) || bytes > sizeof(current.command) || bytes % sizeof(wchar_t) != 0 ||
        current.command[bytes / sizeof(wchar_t) - 1] != L'\0' ||
        !SafeCommandText(std::wstring_view(current.command.data(), bytes / sizeof(wchar_t) - 1))) {
        current = {}; current.error = ERROR_INVALID_DATA;
    } else {
        current.state = !command().empty() && std::wstring_view(current.command.data()) == command() ?
            StartupEntryState::ThisExecutable : StartupEntryState::DifferentEntry;
    }
    current.proposedCommand = command_;
    snapshot_ = current;
    if (!current.readable()) Diagnostics::LogFormat("Startup entry could not be inspected (registry error %lu); no mutation allowed.", static_cast<unsigned long>(current.error));
}
StartupChangeResult StartupRegistration::change(bool enable, StartupEntrySnapshot reviewed, bool userConfirmed) noexcept {
    if (!userConfirmed) return StartupChangeResult::NotConfirmed;
    lastChangeError_ = 0;
    if (!reviewed.readable() || (enable && command().empty())) return StartupChangeResult::Unavailable;
    refresh();
    if (!snapshot_.readable()) return StartupChangeResult::Unavailable;
    if (snapshot_ != reviewed) return StartupChangeResult::ChangedSinceReview;
    if ((enable && snapshot_.state == StartupEntryState::ThisExecutable) || (!enable && snapshot_.state == StartupEntryState::Off))
        return StartupChangeResult::Confirmed; // Already verified; no native write or delete.
    const auto status = enable ? RegSetKeyValueW(HKEY_CURRENT_USER, RedLightRunKey, RedLightRunValue, REG_SZ,
        command_.data(), static_cast<DWORD>((command().size() + 1) * sizeof(wchar_t))) :
        RegDeleteKeyValueW(HKEY_CURRENT_USER, RedLightRunKey, RedLightRunValue);
    lastChangeError_ = static_cast<std::uint32_t>(status); // Registry APIs return their error directly.
    refresh(); // A failed operation may still have changed data; never invent its resulting state.
    if (status != ERROR_SUCCESS) {
        Diagnostics::LogFormat("Startup entry change failed (registry error %ld); observed state refreshed, no rollback overwrite attempted.", status);
        return StartupChangeResult::NativeFailed;
    }
    const bool confirmed = snapshot_.state == (enable ? StartupEntryState::ThisExecutable : StartupEntryState::Off);
    if (!confirmed) {
        lastChangeError_ = snapshot_.error ? snapshot_.error : ERROR_INVALID_DATA;
        Diagnostics::Log("Startup entry change could not be verified; refresh/review before retrying.");
        return StartupChangeResult::VerificationFailed;
    }
    Diagnostics::Log(enable ? "Per-user startup entry confirmed after explicit consent; filtering will start off." : "Per-user startup entry removal confirmed after explicit consent.");
    return StartupChangeResult::Confirmed;
}
