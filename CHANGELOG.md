# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Extend offline profile comparison with saved preference source and exact typed saved/candidate pairs for changed settings, retaining the original changed-field list/statuses/exit codes and rejecting unusable input/storage before exposing values.
- Added guarded Clear recent diagnostic memory under Expert diagnostics, resetting retained records/overwrite counts without file I/O, logging-health changes, or display/preference effects; later records begin a fresh bounded history.
- Extend offline profile-validation JSON with validated input/current schema, schema-upgrade requirements, exact defaulted fields, and all resolved preference values, preserving read-only behavior, existing statuses/exit codes, and failure output without unvalidated metadata.
- Added Copy recent diagnostics with a synchronized 32-record memory buffer independent of file availability, complete chronological snapshots and overwrite counts, allocation-free recording, Unicode-safe escaped export, and checked clipboard ownership without display/backend changes.
- Added manual Refresh display context under Expert diagnostics, refreshing only logical Windows metrics with a completion timestamp while retaining prior context on failure and preserving verification, filtering, recovery ownership, timers, and preferences.
- Added Copy status JSON under Expert diagnostics, sharing the cached snapshot factory with IPC status and checked Unicode clipboard export; invalid/preparation failures occur before clipboard mutation, and export preserves backend/recovery state without fresh read-back.
- Added guarded Clear restoration history and in-progress request-scope reporting, with allocation-free reset that preserves display recovery/counters/settings and refuses unfinished nested scopes; production menu ownership and state flags now have isolated fake-menu failure coverage at every creation/append boundary.
- Added noninteractive `--help-text` standard output sharing a bounded, versioned UTF-8 help body with the native help dialog, with checked termination, partial-output failures, allocation-free C++ preparation, and unchanged native help/version behavior.
- Added read-only `--settings` JSON for persisted/default preferences and storage source, sharing canonical profile tokens, supporting older/legacy records without migration, and refusing corrupt/unreadable storage instead of reporting successful fallback defaults.
- Added cached logical Windows display context to support reports and diagnostics: visible monitor count, virtual desktop metrics, raw remote-session indication, capture reason/age, and explicit refresh failure with prior successful data retained; captures follow restoration handling and report viewing remains read-only.
- Added explicit `--save-profile=<file>` JSON for next-start configuration, validating/closing bounded input before a shared same-session instance guard and one complete settings write, refusing existing owners, preserving saved truth through cleanup/output failures, and supporting corrupt-record repair without backend/tray initialization.
- Added completed-sample, last/peak/cumulative elapsed milliseconds to selected-backend counters for both backends, including failures/no-ops/nested recovery, with monotonic wrap handling, saturating totals, reset protection, and explicit inclusive-overlap semantics in support reports.
- Added cached native countdown-registration/cleanup health, current-countdown warning/restore attempt markers, and exact expiry retry eligibility delay to support reports, sharing the unchanged five-second scheduling policy and one report timestamp without native timer/display work.
- Added exact changed-field summaries to bounded import/reset confirmation previews while retaining complete candidate choices, explicitly explaining restoration even for identical values, and rejecting incomplete previews before any settings/display mutation.
- Added read-only `--compare-profile=<file>` JSON to compare all profile fields against persisted/default preferences, report exact changed field names and a distinct difference exit code, support older schemas/legacy storage without migration, and refuse corrupt/unreadable saved settings before any false comparison.
- Added an eight-record in-memory tray restoration history to support reports, showing explicit triggers, request ages, deferred/no-op/restored/failed/interrupted outcomes, nested completion order, and overwritten-record counts without changing recovery decisions or allocating during recording.
- Added cached shortcut API/error/ID and rollback-failure details to About/support reports, preserving original error codes across logging and successful rollback while clearing diagnostics after healthy retries; actual binding ownership and retry behavior remain unchanged.
- Added offline `--normalize-profile=<file>` to upgrade valid older profiles and standardize current-format output for comparison/backups, reusing checked bounded read-only input with no preference/backend changes and no stdout on invalid input or file failures.
- Added monotonic age and inspection duration for the last explicit backend check in About/support reports and duration diagnostics, with absent-versus-zero timing, wrapped-clock handling, and result/timing invalidation together after filter operations; reporting remains read-only.
- Added read-only `--build-info` JSON for the invoked binary's version, path-free source identity, platform, settings schema, and status protocol, with bounded allocation-free C++ preparation and noninteractive output failures before tray/backend/preferences initialization.
- Added Retry selected shortcuts without preference writes or backend changes, plus explicit selected-profile registration health in About, support reports, and status JSON, including partial/mixed/disabled states.
- Added Restart current countdown for healthy, active, unexpired timed sessions, reusing the configured duration and rearming one warning without saving preferences or changing the display backend.
- Allow preferred Luma-detail edits during gamma fallback without backend calls, output/error/snapshot changes, or verification invalidation; distinguish effective and preferred selections in About/support reports and add explicit Luma-profile scope to status JSON.
- Added an Expert retry for Windows session-recovery notifications, checked registration ownership and cleanup, idempotent/reentrant-call protection, and cached last-error details in About/support reports; deferred restore requests remain honored.
- Added an explicit Save current preferences command (labeled Retry after failed saves) with transaction guarding and unchanged filtering/countdowns, plus persistent startup preference-source/load-warning details in About and support reports.
- Added numerical backend-verification details to the check dialog, logs, and cached support reports: complete value/mismatch counts, largest finite difference with units, nonfinite/unexpected green-blue flags, and preserved read-error codes; comparison tolerances and restore policies remain unchanged.
- Added read-only `--export-profile` for scriptable backups of saved/default preferences, with canonical current-format output, legacy conversion without storage writes, noninteractive failures, and rejection of corrupt storage before output.
- Added offline `--validate-profile=<file>` JSON validation with bounded read-only disk-file access, strict parser reasons/line numbers, checked cleanup, and preserved thread error modes; machine-readable commands report invalid arguments as JSON without help dialogs.
- Added selected-backend request/failure counters to support reports for initialization, activation, restoration/ownership cleanup, verification, and shutdown, with an Expert reset that preserves display/recovery state and refuses in-progress operations.
- Added optional saved expiry warnings (Off, 30 seconds, 1 minute, or 5 minutes) with one notification attempt per countdown and unchanged restoration deadlines; settings profiles export version 3 and accept version 1/2 with warnings off.
- Added read-only `--status` JSON for scripts, querying only cached state from an existing tray instance with a bounded, versioned response; reports countdowns, actual shortcuts, recovery/verification state, and logging health without initializing or reading display backends.
- Added path-free build revision/source-state identity to About, startup diagnostics, and support reports, refreshed on each build; source archives and opt-out builds remain supported.
- Added a confirmed Reset preferences to defaults command that restores the display before saving, resets the complete preference record, and leaves filtering off.
- Added file-logging health, missing/truncated record counts, and last file errors to About/support reports, plus an Expert retry command that confirms recovery with an actual log write.
- Added saved shortcut profiles for Ctrl+Alt+R, Ctrl+Alt+F9, Ctrl+Alt+F10, or disabled shortcuts; restore adds Shift. Track actual bindings through registration/rollback failures and reject stale queued shortcut messages. Introduced version 2 settings profiles for shortcut choices while accepting version 1 with default shortcuts.
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
- Group verification, support reports, retry tools, counters, and history under Expert controls → Diagnostics, retaining distinct submenu accelerators and direct primary display/recovery controls.
- Prepare canonical settings profiles in a checked fixed buffer before one owned-string allocation, reducing allocation pressure during preference saves and exports without changing the format.
- Selecting an unchanged session limit leaves its deadline intact; timing changes/restarts are blocked after expiry or while recovery/backend confirmation needs attention, preserving restoration retry cadence.
- Hold a nonblocking cross-session ownership guard around legacy gamma capture, activation, and restoration; conflicts/access failures stay off, detected abandoned ownership blocks activation until recovery/restart, and failed cleanup remains tracked without rewriting already-restored calibration.
- Rotate logs during long sessions at the 1 MiB target with up to three archives; use fixed record buffers/cached paths and serialize writes, rotation, status reads, and shutdown. Logging failures fall back to debugger output without allowing failed rotation to grow the file indefinitely.
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
- Separate diagnostic memory-history locking from serialized file logging so snapshots/clearing can finish during pending file I/O, retain incoming records before file-lock waits, and avoid lock-order cycles while preserving checked file/error handling.
- Preserve Restore intent after a failed tray-command post by directly requesting normal guarded restoration, including busy deferral and retained requests after C++ failures; never use queue failure to enable filtering or execute unrelated actions, and preserve native posting error codes in diagnostics.
- Preserve cached verification warnings, numerical details, and timing for unchanged mode/preset selections and preference-only edits; invalidate before live transform attempts, retain countdown attention guards, and refresh tray status immediately after failed Luma writes.
- Show Retry recovery for inactive/queued restoration, disable busy primary toggles while keeping Restore available, honor queued restore before activation, and bind displayed menu On/Off choices to explicit intent so stale menus cannot invert current state or restart an already-active countdown.
- Preserve live tray controls and the instance/recovery resources after an unexpected ordinary quit when backend shutdown fails, guard shutdown against nested changes, invalidate attempted verification state, and cancel an off display's stale countdown while allowing Restore/Exit retry.
- Require exact red black/white endpoints during gamma filter confirmation and explicit verification, report endpoint mismatches separately from green/blue leakage, and roll back unconfirmed activation; retain middle-red tolerance, original-calibration restoration policy, and failed-rollback ownership for retry.
- Roll failed live Magnification mode/Luma writes back to the prior confirmed red transform, preserve the original snapshot and preferences, and require inactive recovery after failed/unconfirmed rollback; new changes and recapture remain blocked until restoration succeeds.
- Retain the latest Magnification snapshot across failed initial activation writes, attempt checked rollback, block recapture while inactive recovery is pending, and restore before shutdown cleanup; successful rollback preserves the activation error, while failed rollback stays recoverable without claiming active filtering.
- Log failed startup-information and session-dialog message boxes; startup help/version/argument dialogs return failure when Windows cannot show them instead of reporting success.
- Reject nonfinite Magnification restoration captures at initialization and immediately before activation, without color writes or false ownership; preserve cleanup/fallback gating and allow later usable captures to retry.
- Honor already queued restore requests after read-only callback failures, defer them under outer busy scopes, retain requests after recovery exceptions, and report pending/inactive recovery conservatively; native callback rescue preserves the correct creation/session-ending failure return values.
- Reject malformed UTF-8 throughout profiles (including comments) and JSON strings, and reject embedded NUL bytes before clipboard exports can appear silently truncated.
- Free native command-line arguments during C++ allocation failures, contain option-construction exceptions before display initialization, and distinguish internal startup failures from invalid user arguments.
- Contain C++ exceptions at the native callback boundary, unwind busy/transaction flags safely, preserve active filtering after read-only tool failures, and request recoverable restoration after interrupted mutating actions.
- Reject incomplete support reports on stream/allocation failure instead of copying partial output; log failed error/about/confirmation dialogs.
- Contain preference serialization/allocation failures as normal failed-save results, preserving prior stored settings and import restore-before-save behavior; allocation fault injection is restricted to code-test binaries.
- Guard nested preference changes during confirmed import/reset transactions while keeping display restoration available.
- Retain failed-close log handles for retry, preserve write/flush/rotation error details, count partial/missing records, and reject ordinary files masquerading as the diagnostics directory.
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
