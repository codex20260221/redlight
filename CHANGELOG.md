# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Added a native Custom auto-restore time-limit dialog for 0–240 whole minutes, with validation, cancellation, and protection against truncated pasted input.
- Added explicit read-only backend-state verification, showing software read-back mismatches/unavailability in the tray and support report without reapplying the filter or discarding recovery data.
- Added bounded settings-profile import from clipboard with strict validation and a readable confirmation preview; restore succeeds before saving, imports stay off, and failed restoration/storage keeps prior preferences.
- Added a versioned, human-readable settings profile export that preserves preferred mode/luma selections even during gamma fallback; strict profile parsing rejects duplicate, unknown, missing, and out-of-range settings.
- Added a copyable support report with backend policy, red/luma selections, countdown/recovery status, and shortcut availability, without screenshots, account names, or file paths; clipboard failure and ownership handling is covered by fake-API code tests.
- Added saved Luma Red color-detail presets: Classic (BT.601), Green detail (BT.709-style weights), and Equal color detail, with safe live switching on Magnification and unchanged Strict Red output.
- Added saved next-start backend preferences (Automatic, Magnification only, and legacy gamma only) with one-start command-line overrides; explicit choices never silently fall back.
- Added optional saved auto-restore session limits (15–240 minutes), a tray countdown, and throttled recovery retries after an expired session. Sessions start only after a confirmed enable and never automatically re-enable the filter.
- Added Ctrl+Alt+R toggle and Ctrl+Alt+Shift+R display restoration shortcuts, with availability shown in About.
- Added a Restore display command, direct access to diagnostics, accessible tray activation, and saved red-mode selection.
- Added isolated code tests for color transforms, command-line parsing, API failure handling, rollback, and restoration retries. Tests use fake display APIs and verify binary import isolation.
- Added native x64 portable ZIP packaging, executable version/manifest metadata, SHA-256 checksums, and release validation notes for 0.6.0-rc.1.
- Added command-line help/version dialogs and explicit rejection of unknown or conflicting options.

### Changed
- Grouped backend policy, verification, reports, and profile transfer under Expert controls while keeping toggle/restore/modes first; added distinct menu accelerators, visible shortcut-conflict labels, and explicit submenu ownership cleanup.
- Persist preferences in one validated, versioned record to avoid partial multi-key saves; migrate prior values read-only until the next save, report invalid storage, and show unsaved changes in About/support reports.
- Capture the most recent display state before every activation; refuse activation when capture fails.
- Keep recovery data and backend resources available after failed restoration. Normal Exit stays open when display restoration fails.
- Attempt restoration and leave the filter off on display, power, and session changes; re-enable manually.
- Recover the tray after Explorer restarts, retrying registration while keeping the filter off.
- Use a per-session instance mutex and refuse direct reset when a live/starting instance's recovery controls are unavailable.
- Build with the static MSVC runtime and make the display probe opt-in. CI runs code-only tests and prepares draft prereleases after all gates pass.
- Support Unicode diagnostics paths and report log-write failures to the debugger.

### Fixed
- Prevent automatic fallback when a failed Magnification initialization still cannot release its backend resources.
- Detect silently rejected gamma writes through significant-byte read-back and attempt rollback without claiming a failed enable is active.
- Preserve gamma restoration data even when a partial write and rollback both fail; avoid overwriting calibration on inactive shutdown.
- Cover the full red gamma range from black to maximum red.
- Surface backend, tray initialization, and restoration failures instead of silently running without usable controls.
- Handle message-loop failures and clean up tray/hotkey/session resources.

## [0.5.1-beta] - 2026-05-18

### Added
- Added an optional Luma Red mode for the Magnification API backend.
- Added tray menu options for switching between Strict Red and Luma Red.

### Changed
- Changed the Magnification API backend default red mode to Luma Red while keeping gamma-ramp fallback on Strict Red only.
- Updated tray tooltip, about text, and diagnostics to show the active backend and red mode.
- Documented Strict Red versus Luma Red behavior and the Magnification API requirement for Luma Red.
- Updated README and changelog terminology to use "Luma Red" and "luma-weighted red" instead of "luminance-based" or "red luminance".

## [0.5.0-beta] - 2026-05-07

### Added
- Added an experimental Magnification API red-filter probe.
- Added a Magnification API display backend as the preferred RedLight backend.
- Added a DisplayFilter abstraction for display backend implementations.
- Kept the legacy gamma-ramp backend as a fallback.
- Added lightweight diagnostics logging to `%LOCALAPPDATA%\\RedLight\\redlight.log`.
- Added a command-line display reset mode for `--restore` and `--reset-display`.
- Added startup diagnostics log rotation for `%LOCALAPPDATA%\\RedLight\\redlight.log` with up to three archived logs.

### Changed
- Refactored the gamma-ramp implementation into GammaRampFilter.
- Updated the tray app to select the Magnification backend first and fall back to gamma ramp if needed.
- Added startup and shutdown diagnostics, backend selection logging, and toggle/reset logging.

### Fixed
- Improved display backend state handling so active state is only changed after successful backend calls.
- Improved restore behavior by preserving and restoring the previous Magnification color matrix.
- Added a best-effort Magnification restore path for display recovery without starting the tray UI.
- Updated reset mode so `--restore` and `--reset-display` can ask a running tray instance to restore the active display filter.

### Tested
- Verified Windows x64 build.
- Verified tray toggle behavior.
- Verified exit restoration.
- Verified repeated enable/disable behavior.
- Verified right-click Toggle ON/off behavior.
- Verified MagRedProbe still builds/runs/restores.
- Verified second-monitor behavior with the Magnification backend.

## [0.4.0-beta] - 2024-04-26

### Added
- Initial beta release of RedLight.
- Added a new README file.
- Created a new GitHub repository to push the local dev files and maintain proper versioning.

### Fixed
- CMake build process refactored for MSVC to improve Windows compatibility.
- GitHub Actions workflow fixed for dev branch and release tags. Compile .exe include in Releases page.

### Known Issues
- Need to test on multiple systems and configurations for broader compatibility.

## [0.3.0-alpha] - 2024-04-25

### Added
- Setup Github Actions workflow to build release versions automatically.

### Known Issues
- Compilation errors in Release builds on Github
- Need to update CMake configuration to use MSVC instead of MinGW.

## [0.2.0-alpha] - 2024-04-15
### Added
- Refactored from a windowed app to a tray-only app.
- System tray icon functionality with right-click menu including options to toggle RedLight, view about dialog, and exit the application.

### Fixed
- Handling of multiple instances to ensure only one instance runs at a time.

### Known Issues
- Need to test on multiple systems and configurations for broader compatibility.


## [0.1.0-alpha] - 2024-04-07

### Added
- Project setup and initial code framework.
- Basic window interaction and gamma control functionality to turn off green and blue channels.

### Known Issues
- Initial implementation, not yet tested on multiple devices.
- Error handling is not fully implemented.
