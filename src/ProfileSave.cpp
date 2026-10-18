#include "ProfileSave.h"
#include "PreferencesStore.h"
#include "InstanceName.h"
#include "StandardOutput.h"
#include "Diagnostics.h"
#include <windows.h>
#include <cstdio>

namespace {
class InstanceGuard {
public:
    InstanceGuard() noexcept {
        handle_ = CreateMutexW(nullptr, TRUE, RedLightInstanceMutexName);
        const DWORD error = GetLastError();
        if (!handle_) { error_ = error; stage_ = "guard-create"; return; }
        present_ = error == ERROR_ALREADY_EXISTS;
        error_ = present_ ? ERROR_ALREADY_EXISTS : ERROR_SUCCESS;
        owned_ = !present_;
    }
    ~InstanceGuard() { close(); }
    InstanceGuard(const InstanceGuard&) = delete;
    InstanceGuard& operator=(const InstanceGuard&) = delete;
    bool acquired() const noexcept { return owned_; }
    bool instancePresent() const noexcept { return present_; }
    DWORD error() const noexcept { return error_; }
    const char* stage() const noexcept { return stage_; }
    bool close() noexcept {
        if (owned_) {
            if (!ReleaseMutex(handle_)) {
                error_ = GetLastError(); stage_ = "guard-release";
                Diagnostics::LogFormat("Profile save instance-guard release failed (error %lu); retaining ownership for retry.", error_);
                return false;
            }
            owned_ = false;
        }
        if (handle_) {
            if (!CloseHandle(handle_)) {
                error_ = GetLastError(); stage_ = "guard-close";
                Diagnostics::LogFormat("Profile save instance-guard close failed (error %lu); retaining handle for retry.", error_);
                return false;
            }
            handle_ = nullptr;
        }
        return true;
    }
private:
    HANDLE handle_ = nullptr;
    DWORD error_ = ERROR_SUCCESS;
    const char* stage_ = "guard-create";
    bool owned_ = false, present_ = false;
};
int WriteSaveResult(const char* status, bool saved, int result, const char* stage = "none", std::optional<DWORD> error = std::nullopt) noexcept {
    char output[256];
    char errorText[24] = "null";
    if (error) {
        const int errorLength = std::snprintf(errorText, sizeof(errorText), "%lu", *error);
        if (errorLength < 0 || static_cast<std::size_t>(errorLength) >= sizeof(errorText)) {
            Diagnostics::Log("Offline profile save error code could not be formatted.");
            return 1;
        }
    }
    const int length = std::snprintf(output, sizeof(output),
        "{\"protocol\":1,\"status\":\"%s\",\"saved\":%s,\"stage\":\"%s\",\"error_code\":%s}\n",
        status, saved ? "true" : "false", stage, errorText);
    if (length < 0 || static_cast<std::size_t>(length) >= sizeof(output)) {
        Diagnostics::Log("Offline profile save result could not fit its output buffer.");
        return 1;
    }
    if (WriteStandardOutput(std::string_view(output, static_cast<std::size_t>(length)))) return result;
    Diagnostics::Log("Offline profile save could not write standard output; preferences may already be saved. Discard partial output.");
    return 1;
}
int GuardCleanupFailure(InstanceGuard& guard, bool saved, const char* status) noexcept {
    const auto error = guard.error();
    const auto stage = guard.stage();
    guard.close(); // Retry retained ownership/handle before output; the first failure still determines the result.
    return WriteSaveResult(status, saved, 1, stage, error);
}
}
int SaveSettingsProfileForNextStart(const AppPreferences& candidate) noexcept {
    bool saved = false;
    try {
        if (!ValidPreferences(candidate)) return WriteSaveResult("invalid-profile", false, 4, "validation", ERROR_INVALID_DATA);
        InstanceGuard guard;
        if (!guard.acquired()) {
            const auto error = guard.error();
            const bool present = guard.instancePresent();
            if (!guard.close()) return GuardCleanupFailure(guard, false, "guard-error");
            Diagnostics::Log(present ? "Offline profile save refused: a same-session instance/startup guard already exists." :
                "Offline profile save could not create the same-session instance guard.");
            return WriteSaveResult(present ? "instance-present" : "guard-error", false, present ? 6 : 1, "guard-create", error);
        }
        saved = SavePreferences(candidate);
        if (!guard.close()) return GuardCleanupFailure(guard, saved, "cleanup-error");
        Diagnostics::Log(saved ? "Offline profile saved for next start without display/backend initialization." : "Offline profile save failed; no successful preference write reported.");
        return WriteSaveResult(saved ? "profile-saved" : "settings-error", saved, saved ? 0 : 1, saved ? "none" : "settings");
    } catch (...) {
        Diagnostics::Log("Offline profile save encountered an internal error; inspect saved preferences before retrying.");
        return WriteSaveResult("internal-error", saved, 1, "internal");
    }
}
