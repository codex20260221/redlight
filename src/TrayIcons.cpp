#include "TrayIcons.h"
#include "Diagnostics.h"
#include "resource.h"

bool TrayIcons::initialize(HINSTANCE instance) {
    if (icons_[0]) return true;
    if (!instance) {
        Diagnostics::Log("Tray icon resources require the application module.");
        return false;
    }
    const int width = GetSystemMetrics(SM_CXSMICON), height = GetSystemMetrics(SM_CYSMICON);
    if (width <= 0 || height <= 0) {
        Diagnostics::Log("Tray icon size unavailable; tray initialization refused.");
        return false;
    }
    constexpr unsigned resources[] = {IDI_TRAY_OFF, IDI_TRAY_STRICT, IDI_TRAY_LUMA, IDI_TRAY_ATTENTION};
    std::array<HICON, 4> loaded{};
    for (std::size_t index = 0; index < loaded.size(); ++index) {
        SetLastError(ERROR_SUCCESS);
        loaded[index] = static_cast<HICON>(LoadImageA(instance, MAKEINTRESOURCEA(resources[index]),
            IMAGE_ICON, width, height, LR_SHARED));
        if (!loaded[index]) {
            const auto error = GetLastError();
            if (error) Diagnostics::LogFormat("Tray icon resource %u could not load (error %lu).", resources[index], error);
            else Diagnostics::LogFormat("Tray icon resource %u could not load (no native error code).", resources[index]);
            return false;
        }
    }
    icons_ = loaded;
    return true;
}

HICON TrayIcons::get(TraySymbol symbol) const noexcept {
    const auto index = static_cast<unsigned>(symbol);
    return index < icons_.size() ? icons_[index] : nullptr;
}
