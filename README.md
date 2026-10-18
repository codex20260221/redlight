# RedLight
RedLight is a lightweight Windows tray application that applies a red-only display filter. It is not a translucent tint. The app runs from the system tray, allowing users to easily toggle the filter on and off with one click.

## Release candidate

**0.6.0-rc.1** is a portable Windows x64 release candidate. The filter starts **off**. This candidate has code-only validation; live display and tray acceptance checks are listed in [release validation](docs/release-validation.md) and have not been run for this candidate.

## Features

- Strict Red and Luma Red modes, with your selection remembered between starts.
- Tray toggle and keyboard shortcuts; a separate Restore display command.
- Visible errors, recovery retries, and diagnostics available from the tray menu.
- Tray recovery after Explorer restarts.
- Optional auto-restore session limits with a remaining-time countdown.
- Restoration attempts on sleep/wake, lock/unlock, session disconnect, and display configuration changes. Re-enable manually afterward.
- Native C++/Win32, with no installer or separate Visual C++ runtime required.

## Red Modes
Strict Red is the original red-channel-only mode and transforms display output as follows:
- Black stays black.
- White becomes pure red.
- Red stays red.
- Green and blue output are eliminated.

Luma Red is the default mode when the Magnification API backend is active. It converts visible colors into red luma using BT.601-style luma weights:
- Output red = `0.299R + 0.587G + 0.114B`.
- Output green = `0`.
- Output blue = `0`.

Luma Red keeps the display red-only while making blue, green, and cyan UI elements visible as shades of red. It requires the Windows Magnification API backend because gamma ramps do not support cross-channel color mixing. If RedLight falls back to the gamma-ramp backend, RedLight starts in Strict Red and only Strict Red is available.

### Luma color detail

The **Luma color detail** menu offers three red-only mappings on Magnification:

| Preset | Red input weight | Green input weight | Blue input weight | Use |
| --- | --- | --- | --- | --- |
| Classic (default) | 0.299 | 0.587 | 0.114 | Original Luma Red appearance |
| Green detail | 0.2126 | 0.7152 | 0.0722 | Emphasize green content using BT.709-style weights |
| Equal color detail | 1/3 | 1/3 | 1/3 | Make equally bright red, green, and blue inputs equally visible |

Every preset keeps black black, maps white to pure red, preserves alpha, and produces no green/blue output. The choice is saved and can be changed while filtering. In Strict Red, it only selects the preset for a later switch to Luma Red. Gamma fallback does not support these cross-channel mappings; it retains the saved choice for a future Magnification session. Failed application keeps the previously confirmed preset and the original restoration snapshot.

## Screenshots

### Desktop example

| Normal display | Luma Red mode |
| --- | --- |
| ![Normal Windows desktop screenshot](docs/images/normal-desktop.webp) | ![Windows desktop with RedLight Luma Red mode enabled](docs/images/luma-red-desktop.webp) |

### Color test example

| Normal color bars | Luma Red mode |
| --- | --- |
| ![Color bars test image](docs/images/color-bars.webp) | ![Color bars shown through RedLight Luma Red mode](docs/images/luma-red-color-bars.webp) |

## Architecture
RedLight uses the Windows Magnification API full-screen color transform as the preferred backend, with Luma Red as the default mode.
The legacy gamma-ramp backend is retained as a Strict Red fallback.

## Installation
To install RedLight, follow these steps:
1. Download the latest release from the [Releases](https://github.com/michaelmawhinney/redlight/releases) page.
2. Extract the portable ZIP to a folder you can keep, then run `RedLight.exe`. The standalone executable is also available. Administrator privileges are not required.
3. If the icon is hidden, look in the taskbar notification overflow. Windows controls its placement.

## Usage
After starting RedLight, an icon appears in the system tray with the filter off.

| Control | Action |
| --- | --- |
| Left-click, Enter, or Space on the tray icon | Toggle the filter |
| Ctrl+Alt+R (default profile) | Toggle the filter |
| Ctrl+Alt+Shift+R (default profile) | Restore the display |
| Right-click or the keyboard context menu | Open the tray menu |

The menu provides **Turn on / Turn off**, **Restore display**, **Strict Red**, **Luma Red**, **Open logs folder**, **About**, and **Exit**. About shows the backend, mode, filter status, and shortcut/session registration status. If another program has registered a shortcut, use the tray controls.

**Auto-restore time limit** offers Unlimited (default), 15, 30, 60, 120, or 240 minutes. **Custom** accepts any whole number from 0 to 240, with 0 meaning Unlimited; Cancel leaves the existing limit unchanged. A successful activation starts the countdown; changing the limit during a session restarts it from that moment. The tray tooltip shows remaining time. When it expires, RedLight restores the display and retries failures every five seconds while retaining recovery data. Manual restoration or environment restoration cancels the countdown. The limit is saved; an active countdown is never saved or resumed across app starts. If Windows cannot create the timer, RedLight attempts restoration and reports the failure. This is a best-effort message-queue timer, so a busy or blocked process may restore later than the requested deadline.

Preferences are saved as one validated SettingsProfile record under `HKCU\Software\RedLight`. Prior individual settings are loaded when this record is absent and migrated on the next save. Corrupt or unreadable settings are reported and use safe defaults. About and support reports identify unsaved changes after a failed save. An unavailable Luma Red preference is retained for a future Magnification session, while gamma fallback uses Strict Red. The enabled state is never saved. Exit first restores display state; if restoration fails, RedLight stays available for another attempt.

On an Explorer restart, display change, sleep/wake, or session change, RedLight attempts to restore the saved state and leaves the filter off. If restoration fails, an error is logged and recovery data is kept. Use **Restore display** to retry. No automatic re-enabling occurs.

### Shortcut profiles

**Expert controls → Shortcut profile** offers Ctrl+Alt+R (default), Ctrl+Alt+F9, Ctrl+Alt+F10, or Disabled. Toggle uses the chosen combination; Restore adds Shift. Tray captions, About, and support reports show the actual registered combinations. Disabling shortcuts leaves tray controls and the restore command available.

A rejected change attempts to restore the previous registrations and keeps the prior preference. If rollback also fails, the actual remaining bindings are shown rather than assuming success. Retry the desired profile to recover. Stale queued messages from old or disabled combinations are ignored. A successful change is saved; storage failures are shown as unsaved changes. Imported profiles apply shortcut choices after restoring the display and keep the filter off.

### Expert backend control

**Expert controls → Backend preference (next start)** offers **Automatic** (Magnification first, gamma fallback), **Magnification only**, and **Legacy gamma only**. The saved selection applies after restarting RedLight and never enables the filter by itself. Explicit backend choices fail visibly if unavailable; they do not silently fall back. About distinguishes the active backend, current startup policy, and saved next-start choice.

Start with one of these options to override the saved preference for that launch only:

    RedLight.exe --backend=auto
    RedLight.exe --backend=magnification
    RedLight.exe --backend=gamma

Use the Automatic override if an unavailable saved backend prevents startup. Gamma is a legacy diagnostic/compatibility option with driver and multi-monitor limitations. Close the running instance before using a startup override. The override is not saved.

### Portable settings profiles

**Expert controls → Export settings profile to clipboard** copies a small versioned text profile containing preferred red mode, luma detail, next-start backend preference, and session limit. Paste it into a text file to keep or share the configuration. It preserves preferred Luma Red settings even while gamma fallback is running, and includes no enabled state, countdown, recovery snapshot, account identity, or file path. A command-line backend override does not replace the saved next-start preference in the export.

**Expert controls → Import settings profile from clipboard** reads a copied profile, validates it, and previews all choices before applying. Cancel or invalid input changes nothing. On confirmation, RedLight first restores any active filter, saves one complete configuration, applies available mode/luma preferences, and leaves the filter off. Backend selection applies on the next start. A restoration failure aborts import with recovery data intact; a storage failure keeps prior preferences selected and leaves the already-restored display off. Clipboard reads are bounded to 4096 bytes of UTF-8 text and never clear or replace clipboard contents.

**Expert controls → Reset preferences to defaults** previews the default Luma Red/Classic, Automatic backend, Unlimited session, and Ctrl+Alt+R shortcut choices. Confirmation uses the same restore-before-save path as import and leaves the display off. It does not delete logs or recovery information, change clipboard contents, or restart the app. Backend selection takes effect on the next start.

The format uses a RedLightProfile section. Version 2 exports explicit mode/luma/backend/session_minutes/hotkeys fields; version 1 profiles are accepted with default shortcuts. The hotkeys tokens are default, f9, f10, or disabled. Blank lines, whole-line comments, whitespace around fields, and UTF-8 BOMs are accepted by the parser; duplicates, unknown fields, missing fields, unsupported versions, and invalid ranges are rejected. Profiles are data and contain no commands to execute.

## Compatibility and limitations

- Target: Windows 10/11 x64 desktop sessions. HDR, remote sessions, exclusive full-screen apps, protected content, and the secure desktop are not qualified for this candidate. RedLight is not a hardware blue-light guarantee.
- Magnification is the preferred full-screen backend. Gamma fallback uses a desktop device context, and may not cover every monitor/adapter. It supports Strict Red only.
- Gamma drivers may silently reject color ramps; RedLight reads back the ramp and attempts rollback on failure. This verifies the reported ramp, not physical panel output. See [Microsoft's gamma API limitations](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-setdevicegammaramp).

* This application may conflict with other apps/features that change display color, such as f.lux, Windows Night Light, HDR/color calibration settings, GPU color controls, or similar display-color tools. If you want to use RedLight, you should close or disable similar apps/features to avoid unexpected behavior.
* RedLight is provided as-is and without any warranty of any kind. Although the code is simple and straightforward, you are still advised to use it at your own risk!

## Windows Security Notice

RedLight builds, including this release candidate, are currently unsigned. Because the executable is not code-signed, Windows or Microsoft Defender SmartScreen may show a warning before running it, especially the first time it is downloaded.

This warning does not necessarily mean RedLight is malicious; it means Windows does not yet recognize the publisher or file reputation. RedLight is open source, so users who are concerned are encouraged to inspect the source code, build the app themselves, or verify the downloaded executable against the SHA-256 checksum published with the release.

Do not run software you do not trust. RedLight is provided as-is and without warranty.

## Troubleshooting
* **Expert controls → Verify backend state** reads the active Magnification matrix or gamma ramp and compares it with the requested transform. It does not reapply, restore, or change modes. A mismatch or failed read is shown in the tray and support report until the next filter operation. Inactive checks do not inspect another tool's display state. Recovery data remains available. A matching read-back describes software state, not physical panel output.
* **Expert controls → Copy support report** copies the version, backend/startup policy, red mode, luma detail preset, session limit/countdown, shortcut/session registration, and last backend error. Paste it into a support request or text editor. It includes no screenshot, account name, or filesystem path, and it reports backend call results rather than measuring the display. Clipboard access happens only when you choose this command; a busy clipboard is reported without clearing its existing contents.
* Log file: `%LOCALAPPDATA%\RedLight\redlight.log`
* Reset commands:
  * `RedLight.exe --restore`
  * `RedLight.exe --reset-display`
* With a running instance, reset mode asks that instance to restore its saved state. A busy or unresponsive instance is not bypassed.
* Without a running instance, reset mode applies an identity Magnification matrix. This clears Magnification color effects, including those from other tools; it cannot recover a lost gamma-ramp snapshot after a crash.
* Exit codes: `0` for success (including an already-running instance), `1` for startup/recovery failure, and `2` for invalid arguments. `--help` and `--version` display informational dialogs. Options are case-sensitive and exactly one option is accepted.
* Logs support Unicode Windows profile paths. Rotation runs at startup and before a write would push the current log past a 1 MiB target, retaining up to three archives. Long records are truncated with a marker. About/support reports show file-logging health, missing/truncated record counts, and the last file error. If rotation or storage fails, affected disk writes are skipped and debugger output continues; **Expert controls → Retry file logging** can retry after the underlying problem is fixed. Earlier missing-record counts remain visible after recovery. Rotation and size checks are best effort when other processes or filesystem restrictions interfere.
* To remove RedLight, exit it, delete its folder, and optionally delete `%LOCALAPPDATA%\RedLight` and the `HKCU\Software\RedLight` registry key. No service or startup entry is installed.

## Building from Source
RedLight is built and tested as a Windows x64 application. Use Visual Studio 2022 Build Tools or Visual Studio 2022 with the C++ toolchain installed.

### Prerequisites
- Windows OS
- CMake 3.20 or higher
- Visual Studio 2022 Build Tools or Visual Studio 2022
- x64 C++ toolchain

### Build Instructions
1. Clone the repository:
   ```
   git clone https://github.com/michaelmawhinney/redlight.git
   cd redlight
   ```
2. Run CMake to generate the build configuration:
   ```
   cmake -S . -B build -A x64
   ```
3. Build the project:
   ```
   cmake --build build --config Release
   ```
4. The release artifact is expected at `build\Release\RedLight.exe`

### Code-only validation and packaging

These commands do not start RedLight or the manual display probe:

```powershell
./tools/Test-CodeOnlyBinary.ps1
ctest --test-dir build -C Release --output-on-failure
./tools/Verify-Release.ps1
cpack --config build/CPackConfig.cmake -C Release -B build/packages
./tools/Verify-Package.ps1
```

About and support reports include the local source revision and clean/modified state captured when built. Source archives without their own Git metadata show an unversioned identity. Configure with `-DREDLIGHT_EMBED_SOURCE_REVISION=OFF` to omit revision inspection. Generated identity contains no repository path.

The code-test targets compile production backends and supporting helpers against fake Win32 functions. The import check permits only KERNEL32.dll for both executables. Tests cover transforms/recovery, preferences, timers, clipboard, shortcuts, and logging; display, registry, clipboard, and diagnostics-file operations use fakes. The diagnostics suite also checks concurrent records and runtime rotation in memory. Build the manual probe only when explicitly wanted with `-DREDLIGHT_BUILD_PROBE=ON`; it is excluded by default and is never a CTest test.

The ZIP includes the executable, SHA-256 checksum, license, README, changelog, and documentation. Verify a downloaded executable with `Get-FileHash .\RedLight.exe -Algorithm SHA256` against its published checksum. SHA-256 identifies the file; it does not establish publisher trust.

[Release validation and manual Windows test notes](docs/release-validation.md) describe candidate acceptance and promotion to a stable release.

## Contributing
Contributions are welcome! Feel free to open pull requests or issues to suggest improvements or add new features.

## License
RedLight is open source software [licensed as GPLv3](LICENSE).

## Acknowledgments
- Windows Magnification API full-screen color transform is used as the preferred display backend.
- Gamma ramp manipulation remains available as a fallback display backend on Windows.
- Icon and design resources were custom made for this project using Adobe Photoshop and ImageMagick.
