#include "TrayMenu.h"
#include "CommandIds.h"
#include "Diagnostics.h"
#include <cstdio>

namespace {
class Menu {
public:
    Menu() : handle_(CreatePopupMenu()) {
        if (!handle_) Diagnostics::LogFormat("CreatePopupMenu failed (error %lu).", GetLastError());
    }
    ~Menu() {
        if (handle_ && !DestroyMenu(handle_)) Diagnostics::LogFormat("DestroyMenu failed (error %lu).", GetLastError());
    }
    Menu(const Menu&) = delete;
    Menu& operator=(const Menu&) = delete;
    bool add(unsigned id, const char* label, UINT flags = MF_STRING) {
        if (!handle_) return false;
        if (AppendMenuA(handle_, flags, id, label)) return true;
        Diagnostics::LogFormat("AppendMenu failed (error %lu).", GetLastError());
        return false;
    }
    bool child(Menu& child, const char* label, UINT flags = MF_ENABLED) {
        if (!handle_ || !child.handle_) return false;
        if (!AppendMenuA(handle_, MF_POPUP | flags, reinterpret_cast<UINT_PTR>(child.handle_), label)) {
            Diagnostics::LogFormat("AppendMenu(submenu) failed (error %lu).", GetLastError());
            return false;
        }
        child.handle_ = NULL; // Parent now owns the entire child tree.
        return true;
    }
    HMENU release() { const auto handle = handle_; handle_ = NULL; return handle; }
private:
    HMENU handle_;
};
UINT Checked(bool value) { return MF_STRING | (value ? MF_CHECKED : MF_UNCHECKED); }
}

HMENU BuildTrayMenu(const TrayMenuState& state) {
    Menu root, session, warning, luma, backend, shortcuts, expert;
    for (unsigned i = 0; i < ExpiryWarningOptions.size(); ++i) {
        const auto option = ExpiryWarningOptions[i];
        if (!warning.add(ID_EXPIRY_WARNING_BASE + i, ExpiryWarningName(option), Checked(state.expiryWarning == option))) return NULL;
    }
    for (const unsigned minutes : {0u, 15u, 30u, 60u, 120u, 240u}) {
        char label[48];
        if (minutes == 0) strcpy_s(label, "Unlimited");
        else sprintf_s(label, "%u minutes", minutes);
        if (!session.add(ID_SESSION_LIMIT_BASE + minutes, label, Checked(state.sessionMinutes == minutes) | (state.canChangeSessionLimit ? MF_ENABLED : MF_GRAYED))) return NULL;
    }
    const bool customLimit = state.sessionMinutes != 0 && state.sessionMinutes != 15 && state.sessionMinutes != 30 &&
        state.sessionMinutes != 60 && state.sessionMinutes != 120 && state.sessionMinutes != 240;
    char customLabel[64];
    if (customLimit) sprintf_s(customLabel, "&Custom (%u minutes)...", state.sessionMinutes);
    else strcpy_s(customLabel, "&Custom...");
    if (!session.add(0, NULL, MF_SEPARATOR) || !session.add(ID_TRAY_CUSTOM_SESSION, customLabel, Checked(customLimit) | (state.canChangeSessionLimit ? MF_ENABLED : MF_GRAYED)) ||
        !session.add(0, NULL, MF_SEPARATOR) || !session.child(warning, "Expiry &warning") ||
        !session.add(0, NULL, MF_SEPARATOR) || !session.add(ID_TRAY_RESTART_COUNTDOWN, "&Restart current countdown", MF_STRING | (state.canRestartCountdown ? MF_ENABLED : MF_GRAYED))) return NULL;
    if (!luma.add(ID_LUMA_PROFILE_BASE, "&Classic (BT.601)", Checked(state.luma == LumaProfile::Classic)) ||
        !luma.add(ID_LUMA_PROFILE_BASE + 1, "&Green detail (BT.709)", Checked(state.luma == LumaProfile::GreenDetail)) ||
        !luma.add(ID_LUMA_PROFILE_BASE + 2, "&Equal color detail", Checked(state.luma == LumaProfile::EqualDetail))) return NULL;
    if (!backend.add(ID_BACKEND_PREFERENCE_BASE, "&Automatic", Checked(state.backendPreference == BackendPreference::Automatic)) ||
        !backend.add(ID_BACKEND_PREFERENCE_BASE + 1, "&Magnification only", Checked(state.backendPreference == BackendPreference::Magnification)) ||
        !backend.add(ID_BACKEND_PREFERENCE_BASE + 2, "Legacy &gamma only", Checked(state.backendPreference == BackendPreference::Gamma))) return NULL;
    if (!shortcuts.add(ID_SHORTCUT_PROFILE_BASE, "&Default: Ctrl+Alt+R", Checked(state.shortcutProfile == ShortcutProfile::Default)) ||
        !shortcuts.add(ID_SHORTCUT_PROFILE_BASE + 1, "Ctrl+Alt+F&9", Checked(state.shortcutProfile == ShortcutProfile::F9)) ||
        !shortcuts.add(ID_SHORTCUT_PROFILE_BASE + 2, "Ctrl+Alt+F&10", Checked(state.shortcutProfile == ShortcutProfile::F10)) ||
        !shortcuts.add(ID_SHORTCUT_PROFILE_BASE + 3, "D&isabled", Checked(state.shortcutProfile == ShortcutProfile::Disabled))) return NULL;
    if (!expert.child(shortcuts, "&Shortcut profile") || !expert.child(backend, "&Backend preference (next start)") ||
        !expert.add(ID_TRAY_VERIFY_BACKEND, "&Verify backend state") ||
        !expert.add(ID_TRAY_COPY_REPORT, "&Copy support report") ||
        !expert.add(ID_TRAY_RETRY_LOGGING, "&Retry file logging") ||
        !expert.add(ID_TRAY_RETRY_SESSION, "Retry session rec&overy") ||
        !expert.add(ID_TRAY_RETRY_SHORTCUTS, "Retry selected s&hortcuts") ||
        !expert.add(ID_TRAY_RESET_BACKEND_COUNTS, "Reset backend cou&nters") ||
        !expert.add(0, NULL, MF_SEPARATOR) ||
        !expert.add(ID_TRAY_SAVE_PREFERENCES, state.preferencesUnsaved ? "Retry saving current &preferences" : "Save current &preferences") ||
        !expert.add(ID_TRAY_COPY_PROFILE, "&Export settings profile to clipboard") ||
        !expert.add(ID_TRAY_IMPORT_PROFILE, "&Import settings profile from clipboard") ||
        !expert.add(0, NULL, MF_SEPARATOR) || !expert.add(ID_TRAY_RESET_PREFERENCES, "Reset preferences to &defaults...")) return NULL;
    char toggleLabel[128], restoreLabel[128];
    const char* absent = state.shortcutProfile == ShortcutProfile::Disabled ? "Disabled" : "Unavailable";
    sprintf_s(toggleLabel, state.needsRestore ? "Turn o&ff\t%s" : "Turn &on\t%s",
        state.toggleShortcut ? state.toggleShortcut : absent);
    sprintf_s(restoreLabel, "&Restore display\t%s", state.restoreShortcut ? state.restoreShortcut : absent);
    if (!root.add(ID_TRAY_TOGGLE, toggleLabel) || !root.add(ID_TRAY_RESTORE, restoreLabel) ||
        !root.add(0, NULL, MF_SEPARATOR) ||
        !root.add(ID_TRAY_MODE_STRICT_RED, "&Strict Red", Checked(state.mode == RedMode::StrictRed)) ||
        !root.add(ID_TRAY_MODE_LUMA_RED, "L&uma Red", Checked(state.mode == RedMode::LumaRed) | (state.supportsLuma ? MF_ENABLED : MF_GRAYED)) ||
        !root.child(luma, state.supportsLuma ? "Luma color &detail" : "Preferred Luma &detail (Magnification)") ||
        !root.child(session, "Auto-restore &time limit") ||
        !root.add(0, NULL, MF_SEPARATOR) || !root.child(expert, "&Expert controls") ||
        !root.add(ID_TRAY_LOGS, "Open &logs folder") || !root.add(ID_TRAY_ABOUT, "&About") ||
        !root.add(ID_TRAY_EXIT, "E&xit")) return NULL;
    return root.release();
}
