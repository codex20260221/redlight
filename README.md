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

During gamma fallback, the menu becomes **Preferred Luma detail (Magnification)**. You can edit this saved choice while gamma is off, active, or awaiting recovery. It affects a future Magnification Luma Red session; gamma output, mode, last error, verification cache, and recovery data stay intact. About and support reports show effective mode/detail and preferred mode/detail separately. A failed save remains marked unsaved and can be retried through Save current preferences.

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

For terminal help, use `RedLight.exe --help-text`. It writes the same versioned help body as the native `--help` dialog to standard output, with no dialog or tray/backend/settings initialization. In PowerShell, `& .\RedLight.exe --help-text | Out-String` waits for this GUI executable to finish. Exit codes are `0` for complete help, `1` for formatting/output failure, and `2` for invalid arguments. Malformed text-help requests log errors without opening a dialog; discard partial output after failure. `--help`, `-h`, and `/?` keep their native help dialog, and `--version` keeps its version dialog.

| Control | Action |
| --- | --- |
| Left-click, Enter, or Space on the tray icon | Toggle the filter |
| Ctrl+Alt+R (default profile) | Toggle the filter |
| Ctrl+Alt+Shift+R (default profile) | Restore the display |
| Right-click or the keyboard context menu | Open the tray menu |

The menu provides **Turn on / Turn off**, **Restore display**, **Strict Red**, **Luma Red**, **Open logs folder**, **About**, and **Exit**. About shows the backend, mode, filter status, and shortcut/session registration status. If another program has registered a shortcut, use the tray controls.

When inactive recovery data or a queued restore needs attention, the primary menu action becomes **Retry recovery**. Busy/preference transactions disable the primary toggle while **Restore display** stays available. A menu selection retains its displayed Turn on/Turn off intent if state changes while the menu is open: Turn off remains a restore, and Turn on is harmless when already active. Queued recovery takes priority over activation, and an already-active Turn on does not restart the countdown. Tray clicks and toggle shortcuts retain ordinary current-state toggle behavior.

**Auto-restore time limit** offers Unlimited (default), 15, 30, 60, 120, or 240 minutes. **Custom** accepts any whole number from 0 to 240, with 0 meaning Unlimited; Cancel leaves the existing limit unchanged. A successful activation starts the countdown; changing the limit during a session restarts it from that moment. The tray tooltip shows remaining time. When it expires, RedLight restores the display and retries failures every five seconds while retaining recovery data. Manual restoration or environment restoration cancels the countdown. The limit is saved; an active countdown is never saved or resumed across app starts. If Windows cannot create the timer, RedLight attempts restoration and reports the failure. This is a best-effort message-queue timer, so a busy or blocked process may restore later than the requested deadline.

**Auto-restore time limit → Restart current countdown** starts the configured duration again for a healthy active filter before expiry. It re-arms one expiry warning and preserves preferences, display state, and recovery snapshots. Selecting the already checked limit leaves the countdown unchanged. Once expiry or restoration is pending, restore first; timing controls cannot postpone recovery. Backend errors or unconfirmed verification also block timing edits until resolved. Restart is disabled for off, unlimited, expired, or recovery-pending sessions. A native timer-arming failure still attempts immediate restoration.

**Auto-restore time limit → Expiry warning** offers Off (default), 30 seconds, 1 minute, or 5 minutes. During an active countdown, RedLight attempts one tray notification when the remaining time reaches that period. Shorter sessions qualify on the first timer tick. Windows may suppress or delay notifications. Changing the warning does not restart, extend, or activate the countdown; an attempted warning is not repeated until a new countdown starts. Expiry takes priority over a missed warning. About, support reports, and settings profiles include the saved choice.

Support reports distinguish the pure countdown from tracked native timer registration: not armed, confirmed armed, unconfirmed cleanup, or armed without confirmed registration. They also show whether warning/restoration attempts were recorded for the current countdown and milliseconds until an expired countdown may retry. A zero wait means the policy allows retry; busy message processing may delay the call. An attempted warning does not prove Windows displayed it. These diagnostics read cached state, use one report timestamp, and do not change deadlines, retry throttling, timer registration, or display state.

Preferences are saved as one validated SettingsProfile record under `HKCU\Software\RedLight`. Prior individual settings are loaded when this record is absent and migrated on the next save. Corrupt or unreadable settings are reported and use safe defaults. About and support reports identify unsaved changes after a failed save. An unavailable Luma Red preference is retained for a future Magnification session, while gamma fallback uses Strict Red. The enabled state is never saved. Exit first restores display state; if restoration fails, RedLight stays available for another attempt.

**Expert controls → Save current preferences** retries saving the complete current configuration; after a failed save its label becomes **Retry saving current preferences**. It preserves filtering, recovery data, active countdowns, and actual shortcuts. Failures remain marked unsaved and can be retried after fixing storage access. About and support reports also retain the startup preference source (Defaults, Legacy values, or Versioned record) and load warnings. These describe the initial load, even after a later successful save, so the reason for fallback defaults remains visible. Loading settings alone never overwrites a corrupt record.

On an Explorer restart, display change, sleep/wake, or session change, RedLight attempts to restore the saved state and leaves the filter off. If restoration fails, an error is logged and recovery data is kept. Use **Restore display** to retry. No automatic re-enabling occurs.

**Expert controls → Diagnostics → Retry session recovery** retries registration for lock, unlock, and disconnect notifications. About and support reports show registration availability and the last error. Windows can reject registration while session services are still starting; see [Microsoft's session-notification API notes](https://learn.microsoft.com/en-us/windows/win32/api/wtsapi32/nf-wtsapi32-wtsregistersessionnotification). A healthy retry avoids duplicate registration. Retrying does not activate filtering or alter preferences; restore requests received during the call are honored afterward.

### Shortcut profiles

**Expert controls → Shortcut profile** offers Ctrl+Alt+R (default), Ctrl+Alt+F9, Ctrl+Alt+F10, or Disabled. Toggle uses the chosen combination; Restore adds Shift. Tray captions, About, and support reports show the actual registered combinations. Disabling shortcuts leaves tray controls and the restore command available.

A rejected change attempts to restore the previous registrations and keeps the prior preference. If rollback also fails, the actual remaining bindings are shown rather than assuming success. Retry the desired profile to recover. Stale queued messages from old or disabled combinations are ignored. A successful change is saved; storage failures are shown as unsaved changes. Imported profiles apply shortcut choices after restoring the display and keep the filter off.

**Expert controls → Diagnostics → Retry selected shortcuts** retries incomplete registrations for the existing choice after a conflict or registration error is resolved. It preserves preferences, unsaved-change status, filtering, countdowns, and recovery data; independently queued restore requests are still honored. A healthy retry avoids duplicate native registration. About, support reports, and `shortcut_profile_applied` in status JSON indicate whether tracked toggle/restore bindings match the selected profile. Disabled is applied when both bindings are absent; mixed or missing required bindings are incomplete.

About and support reports cache the most recent failed shortcut API call from the last accepted initialization/change/cleanup attempt: RegisterHotKey or UnregisterHotKey, ID 1 (toggle) or 2 (restore), Windows error code, and whether it occurred during rollback. A successful rollback retains the rejected change's error; a later rollback failure replaces it and marks rollback incomplete. Successful retries clear these last-attempt diagnostics, including an already healthy retry that needs no native calls. Invalid profile requests preserve the previous diagnostics. Actual bindings remain the registration authority; viewing errors performs no registration or display operation. Status JSON continues to report compact registration health without detailed API errors.

### Expert backend control

**Expert controls → Backend preference (next start)** offers **Automatic** (Magnification first, gamma fallback), **Magnification only**, and **Legacy gamma only**. The saved selection applies after restarting RedLight and never enables the filter by itself. Explicit backend choices fail visibly if unavailable; they do not silently fall back. About distinguishes the active backend, current startup policy, and saved next-start choice.

Start with one of these options to override the saved preference for that launch only:

    RedLight.exe --backend=auto
    RedLight.exe --backend=magnification
    RedLight.exe --backend=gamma

Use the Automatic override if an unavailable saved backend prevents startup. Gamma is a legacy diagnostic/compatibility option with driver and multi-monitor limitations. Close the running instance before using a startup override. The override is not saved.

### Portable settings profiles

**Expert controls → Export settings profile to clipboard** copies a small versioned text profile containing preferred red mode, luma detail, next-start backend preference, shortcuts, session limit, and expiry warning. Paste it into a text file to keep or share the configuration. It preserves preferred Luma Red settings even while gamma fallback is running, and includes no enabled state, countdown, recovery snapshot, account identity, or file path. A command-line backend override does not replace the saved next-start preference in the export.

`--export-profile` writes saved preferences as profile text to standard output. It works before tray/backend initialization, converts valid legacy/version 1/2 settings to the current format without storage writes, and exports defaults if no saved settings exist. Unreadable or invalid storage returns failure before output. It exports persisted preferences; use tray export to include unsaved in-session choices. Exit codes are `0` for a complete export, `1` for storage/serialization/output failure, and `2` for invalid arguments. Errors are logged without a help dialog. Discard any partial output after failure.

Save a UTF-8 backup in PowerShell after checking the export exit code:

```powershell
$profile = & .\RedLight.exe --export-profile | Out-String
if ($LASTEXITCODE -ne 0) { throw 'RedLight profile export failed.' }
$profile | Set-Content -LiteralPath .\night.profile -Encoding utf8
```

The explicit encoding works across Windows PowerShell and PowerShell; their default file encodings differ. See [PowerShell character encoding](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.core/about/about_character_encoding).

`--settings` provides the persisted/default preferences as JSON for scripts. A successful protocol-1 result has `status: "saved-settings"`, `source` (`defaults`, `legacy-values`, or `versioned-record`), `profile_schema` (the current export schema, not necessarily the original stored schema), and a `preferences` object using the six profile field names/tokens. It reads legacy/older records without migration writes and reports their defined defaults. It describes saved choices, excluding unsaved tray edits, active countdowns, and one-start overrides.

```powershell
$json = & .\RedLight.exe --settings | Out-String
if ($LASTEXITCODE -ne 0) { throw 'RedLight saved-settings query failed.' }
($json | ConvertFrom-Json).preferences
```

Exit codes are `0` for complete settings JSON, `1` for storage/output/internal failure, and `2` for invalid arguments. Corrupt or unreadable storage produces `settings-error` with `preferences: null` when output is available; it cannot masquerade as a successful default configuration. The command opens no dialogs, starts no tray, initializes no backend, and writes no preferences. Discard partial output after failure. Mode/Luma/backend tokens are shared with canonical profile export so the two formats agree.

**Expert controls → Import settings profile from clipboard** reads a copied profile, validates it, and previews all choices before applying. Cancel or invalid input changes nothing. On confirmation, RedLight first restores any active filter, saves one complete configuration, applies available mode/luma preferences, and leaves the filter off. Backend selection applies on the next start. A restoration failure aborts import with recovery data intact; a storage failure keeps prior preferences selected and leaves the already-restored display off. Clipboard reads are bounded to 4096 bytes of UTF-8 text and never clear or replace clipboard contents.

**Expert controls → Reset preferences to defaults** previews the default Luma Red/Classic, Automatic backend, Unlimited session, warnings Off, and Ctrl+Alt+R shortcut choices. Confirmation uses the same restore-before-save path as import and leaves the display off. It does not delete logs or recovery information, change clipboard contents, or restart the app. Backend selection takes effect on the next start.

Import and reset previews name the fields that differ from current in-session choices while retaining every candidate value for review. If values are identical, the preview says so and still explains that confirmation restores the display and leaves filtering off. An incomplete preview is rejected before confirmation or applying settings.

The format uses a RedLightProfile section. Version 3 exports explicit mode/luma/backend/session_minutes/hotkeys/expiry_warning fields. The expiry_warning tokens are off, 30s, 1m, or 5m; hotkeys tokens are default, f9, f10, or disabled. Version 1/2 profiles are accepted with warnings Off; version 1 also uses default shortcuts. New fields require their corresponding profile version. Blank lines, whole-line comments, whitespace around fields, and UTF-8 BOMs are accepted by the parser; text must be valid UTF-8, including comments. Duplicates, unknown fields, missing fields, unsupported versions, and invalid ranges are rejected. Profiles are data and contain no commands to execute.

Check a saved profile offline with `--validate-profile=<file>`. In PowerShell, quote the entire option when the path contains spaces:

```powershell
$json = & .\RedLight.exe '--validate-profile=C:\Profiles\night mode.txt' | Out-String
$result = $LASTEXITCODE
$json | ConvertFrom-Json
```

Validation is read-only and runs before tray/backend/settings initialization. Exit codes are `0` for valid profile data, `4` for invalid data, `1` for file/output/internal failure, and `2` for invalid arguments. JSON uses protocol 1 with `status` and `valid` fields. Successful validation adds `input_schema`, current canonical `profile_schema`, `schema_upgrade_required`, `defaulted_fields`, and a `preferences` object containing all six resolved settings with canonical profile tokens. Version 1 defaults `hotkeys` and `expiry_warning`; version 2 defaults `expiry_warning`; current profiles default no missing fields. The upgrade flag concerns schema version, so a current-schema file can still have formatting normalized. Scripts should accept additional fields and require a successful exit/valid result before using settings. Invalid data adds a parser `reason` and `line` (0 when no source line applies); file errors add `stage` and Windows `error_code`. Failures expose no unvalidated preference/schema snapshot. Paths, comments, and raw file text are not echoed. Profiles are limited to 4 KiB; the validator reads at most 4,097 bytes to detect oversize input and rejects pipes/character devices before reading. This is a byte limit; disk/network I/O follows Windows timing. Validation does not import or apply the profile or read saved preferences.

`--normalize-profile=<file>` reads the same bounded UTF-8 input and writes canonical current-format profile text. It upgrades valid version 1/2 profiles with their defined defaults, preserves all accepted settings, and removes comments, BOM, padding, and field-order differences. The output uses CRLF line endings and normalizes to identical bytes on repeated use. It closes the input before output and never edits the source file, loads/saves application preferences, or initializes a display backend.

```powershell
$profile = & .\RedLight.exe '--normalize-profile=C:\Profiles\old night.profile' | Out-String
if ($LASTEXITCODE -ne 0) { throw 'RedLight profile normalization failed.' }
$profile | Set-Content -LiteralPath .\night-v3.profile -Encoding utf8
```

Normalization returns `0` for complete output, `4` for invalid profile data, `1` for file/serialization/output failure, and `2` for invalid arguments. Errors go to diagnostics without a dialog or JSON mixed into profile text; invalid data and input-file failures produce no stdout. Discard partial output after any failure. This command prepares a file for comparison or later confirmed import; it does not apply settings.

`--compare-profile=<file>` compares a validated profile with saved preferences, using defaults when no saved settings exist. It compares all six settings by value, so older schemas and formatting differences do not produce false differences. It reads legacy values without migrating them and refuses corrupt/unreadable storage instead of comparing fallback defaults. It does not compare unsaved tray choices, active countdowns, live backend state, or one-start overrides, and it never imports settings.

```powershell
$json = & .\RedLight.exe '--compare-profile=C:\Profiles\night.profile' | Out-String
$result = $LASTEXITCODE
if ($result -ne 0 -and $result -ne 5) { throw 'RedLight profile comparison failed.' }
$json | ConvertFrom-Json
```

Comparison JSON uses protocol 1. A complete result has `status: "profiles-match"` or `"profiles-differ"`, a Boolean `match`, and `differences` listing changed field names (`mode`, `luma`, `backend`, `session_minutes`, `hotkeys`, `expiry_warning`). Additive fields include `saved_source` (`defaults`, `versioned-record`, or `legacy-values`) and `changes`, an object with exactly the same changed keys and a `{ "saved": ..., "candidate": ... }` pair for each. Session minutes are JSON numbers; other values use canonical profile string tokens. For example, `"session_minutes":{"saved":15,"candidate":30}` describes a proposed increase. A match has `differences: []` and `changes: {}`. These are resolved next-start preferences; older-schema defaults are included by value. Exit codes are `0` for a match, `5` for valid differences, `4` for invalid candidate data, `1` for file/settings/output/internal failure, and `2` for invalid arguments. Scripts should accept additional protocol-1 fields. Candidate/file errors use the validator's JSON; unusable saved settings produce `settings-error` with `match: null` and no value pairs. Output failure takes precedence over a match/difference result; discard partial output after failure. The same bounded read-only disk-file rules apply, and the input closes before saved settings are read or output begins.

`--save-profile=<file>` explicitly replaces all saved preferences with a validated file for the next start, without a confirmation dialog. Close RedLight normally first so its display restoration completes. The command refuses any existing same-session tray/startup/reset guard, including an instance whose controls are unavailable; it does not wait for or ask that instance to restore. When the guard is available, the input is already validated/closed before one complete current-format record is saved. This can repair corrupt saved settings because the replacement comes entirely from the validated file. Starting RedLight afterward loads the choices with filtering off.

```powershell
$json = & .\RedLight.exe '--save-profile=C:\Profiles\night.profile' | Out-String
$result = $LASTEXITCODE
$json | ConvertFrom-Json
if ($result -ne 0) { throw "Profile save did not complete cleanly (exit $result); inspect saved status." }
```

Exit codes are `0` for a completed save, `6` when an instance guard already exists, `4` for invalid profile data, `1` for file/guard/settings/cleanup/output/internal failure, and `2` for invalid arguments. Save-result JSON uses protocol 1 with `status`, Boolean `saved`, `stage`, and `error_code` (null when no specific native code is available). A cleanup failure after a confirmed write returns `1` with `saved: true`; output failure also returns `1` even when preferences were saved. Inspect `--export-profile` or `--compare-profile` after uncertain results. Input errors use the validator's JSON before any guard or preference write. The command initializes no display backend or tray, registers no shortcuts, and preserves the source file. The shared [Windows named mutex](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createmutexw) guard covers this session; saved preferences remain per user.

## Scriptable status

**Expert controls → Diagnostics → Copy status JSON** copies the running tray's cached status as Unicode clipboard text, using the same protocol/field format as `--status`. It includes conservative busy/recovery/verification states, actual shortcuts, timer state, and logging-health flags. It captures cached choices without refreshing backend read-back, reapplying filtering, or querying another instance. Use Copy support report for the fuller counters, history, context, and numerical diagnostics. Inconsistent snapshots or preparation failures are rejected before the clipboard is opened; clipboard access/transfer failures are reported.

`RedLight.exe --status` writes JSON for the existing tray instance. It never starts a tray instance, activates a filter, or reads the display backend. In PowerShell, use a pipeline so the GUI executable completes before reading its output and exit code:

```powershell
$json = & .\RedLight.exe --status | Out-String
$result = $LASTEXITCODE
if ($result -eq 0) { $json | ConvertFrom-Json }
```

Exit codes are `0` for a valid snapshot (including an off filter), `3` when no tray instance exists, and `1` for timeout, incompatible response, or output failure. A missing instance produces `status: "not-running"`; query failures produce `query-failed` or `invalid-response` when output is available. Responses use protocol 1. A request waits up to three seconds for the tray; it opens no dialogs and creates no console window. Redirected standard output is preserved; otherwise it uses an existing parent console if available.

Malformed `--status`, `--settings`, `--validate-profile`, `--compare-profile`, `--save-profile`, or `--build-info` requests produce JSON with `status: "invalid-arguments"` and exit code `2`, keeping script errors noninteractive. Output failure returns `1`.

Snapshots report backend/mode/luma selection, configured session limit and remaining whole seconds, preferred/actual shortcuts, unsaved settings, session-notification registration, and logging health flags. `remaining_seconds` is null when the countdown is unarmed. `restore_pending` means restoration data or ownership cleanup is pending, including during a normal active session. `backend_active` is the last confirmed backend activation flag; `active` is true only when `status` is `on`, and becomes false for busy, recovery-needed, backend-changed, or verification-unavailable states. `verification` describes the last explicit check, which may be `not-checked`. A status query does not refresh verification or measure physical output. Full diagnostic counters and build identity remain available in the support report.

The `busy` status also covers a queued control restore request. About/support status gives recovery priority to pending restoration or inactive ownership cleanup; support reports show control restore requests separately from owned recovery data.

`luma_profile_scope` is `backend-selection` on Magnification or `magnification-preference` on gamma. `luma_profile` reports that configured choice; gamma remains Strict Red, and a Magnification selection in Strict Red applies only after switching to Luma Red.

## Scriptable build identity

`--build-info` writes JSON describing the invoked binary before tray/backend/preferences initialization. It includes `protocol` (1), `product`, `version`, `build_revision`, `source_state`, `platform` (`windows-x64`), `profile_schema` (the exported settings format, currently 3), and `status_protocol` (currently 1). It uses inherited output or an existing parent console and opens no dialogs. Exit codes are `0` for complete output, `1` for metadata/output failure, and `2` for invalid arguments; discard partial output after failure.

```powershell
$json = & .\RedLight.exe --build-info | Out-String
if ($LASTEXITCODE -ne 0) { throw 'RedLight build-info request failed.' }
$json | ConvertFrom-Json
```

This identity belongs to the executable you invoked. A tray instance may be running a different build; its About dialog and support report carry its identity. Build metadata does not probe backend availability or display state. Source archives and builds with revision recording disabled use the same fallback identity values as support reports.

## Compatibility and limitations

- Target: Windows 10/11 x64 desktop sessions. HDR, remote sessions, exclusive full-screen apps, protected content, and the secure desktop are not qualified for this candidate. RedLight is not a hardware blue-light guarantee.
- Magnification is the preferred full-screen backend. Gamma fallback uses a desktop device context, and may not cover every monitor/adapter. It supports Strict Red only.
- Gamma drivers may silently reject color ramps; RedLight reads back the ramp and attempts rollback on failure. This verifies the reported ramp, not physical panel output. See [Microsoft's gamma API limitations](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-setdevicegammaramp).
- Legacy gamma activation acquires a nonblocking global ownership mutex before capturing calibration, and holds it until restoration succeeds. Another cooperating RedLight session, denied access, or a guard failure leaves activation off with an error. Failed restoration retains the guard; failed release/close is retried without rewriting calibration that was already restored. This coordinates builds using the same guard, not older builds or other color tools. It does not persist gamma snapshots or recover calibration lost after a crash. If Windows reports an abandoned owner, activation stays blocked for that process lifetime: recover the display's calibration separately before restarting RedLight. The guard follows [Windows global namespaces](https://learn.microsoft.com/en-us/windows/win32/termserv/kernel-object-namespaces) and [mutex abandonment semantics](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject).

* This application may conflict with other apps/features that change display color, such as f.lux, Windows Night Light, HDR/color calibration settings, GPU color controls, or similar display-color tools. If you want to use RedLight, you should close or disable similar apps/features to avoid unexpected behavior.
* RedLight is provided as-is and without any warranty of any kind. Although the code is simple and straightforward, you are still advised to use it at your own risk!

## Windows Security Notice

RedLight builds, including this release candidate, are currently unsigned. Because the executable is not code-signed, Windows or Microsoft Defender SmartScreen may show a warning before running it, especially the first time it is downloaded.

This warning does not necessarily mean RedLight is malicious; it means Windows does not yet recognize the publisher or file reputation. RedLight is open source, so users who are concerned are encouraged to inspect the source code, build the app themselves, or verify the downloaded executable against the SHA-256 checksum published with the release.

Do not run software you do not trust. RedLight is provided as-is and without warranty.

## Troubleshooting
* **Expert controls → Diagnostics → Refresh display context** updates the logical Windows metrics in the support report without a color-state verification. Successful captures are labeled Manual refresh with a completion timestamp; unavailable metrics retain prior successful values and their original age. Filtering, recovery ownership, verification results, preferences, and countdowns are preserved. The action reports success/unavailability through a tray notification and diagnostics.
* **Expert controls → Diagnostics** groups backend verification, support reports, logging/session/shortcut retries, backend counters, and restoration history. **Clear restoration history** starts a fresh in-memory event history and clears its overwritten count while preserving filtering, recovery data, countdowns, counters, settings, and log files. An unfinished restoration scope blocks clearing, including nested or post-backend callback work; retry when idle. Support reports also show the number of request scopes still finishing.
* **Expert controls → Diagnostics → Verify backend state** reads the active Magnification matrix or gamma ramp and compares it with the requested transform. It does not reapply, restore, or change modes. A mismatch or failed read is shown in the tray and support report until the next filter operation. Inactive checks do not inspect another tool's display state. Recovery data remains available. A matching read-back describes software state, not physical panel output.
* Verification compares all 25 matrix coefficients or 768 gamma values. The dialog, logs, and support report show mismatches, largest finite difference, nonfinite values, unexpected green/blue entries, gamma red-endpoint flags, and read-error codes. Differences use matrix-coefficient units or gamma fractions of full scale. Gamma red-channel middle values retain low-byte tolerance and may show a nonzero difference with zero mismatches; filter confirmation requires exact red black/white endpoints (0/65535) and exact green/blue zeros. A driver that reports quantized endpoints is refused on activation and rollback is attempted, even if the high bytes match. Original calibration restoration retains its prior significant-byte policy. Nonfinite values are flagged separately from the finite maximum. Support reports use the last explicit check; filter operations clear its cached details.
* About and support reports show the age and duration of the last explicit backend verification. Age uses whole seconds since the check completed; duration is elapsed milliseconds around the backend inspection, with Windows tick-clock resolution. Failed and inactive checks are timed too. Output-changing operations clear the result and its timing together, including failed live transform attempts. Selecting the current mode/preset, editing a future Luma preset in Strict Red or gamma, and inactive preference edits preserve the check and its original age. Viewing reports does not perform another check, and elapsed age alone never changes filter state. Status JSON retains its compact cached result without timing fields.
* Magnification rejects an original matrix containing NaN or infinity at initialization or pre-activation capture. It records the invalid snapshot and makes no color write from that rejected capture. A later usable capture can retry; failed initialization cleanup continues to block backend fallback. Finite snapshots retain their original coefficients for restoration.
* A failed initial Magnification filter write immediately attempts restoration of the matrix captured just before activation. Successful rollback leaves RedLight off and preserves the activation error; failed or unconfirmed rollback retains the snapshot/API resources with recovery needed, even while inactive. Restore must succeed before another activation can recapture display state. Shutdown also retries this pending recovery before releasing Magnification resources. This is conservative failure handling; a failed API return is not treated as proof of an unchanged display. Explicit verification remains read-only and cannot clear that obligation.
* A failed live Magnification mode or Luma-detail write attempts to reapply the prior confirmed red transform. Confirmed rollback keeps the prior active mode/preset and reports the failed change. Failed or unconfirmed rollback drops active confirmation, retains the original display snapshot, and requires restoration before re-enabling or changing mode/detail. Neither path saves an unconfirmed choice. Read-back that happens to match does not erase pending recovery; the Restore command and shutdown retry the original snapshot.
* A read-only tool failure preserves filtering when no restoration was requested. Queued restore requests are honored after busy operations unwind. An interrupted filter-changing action requests restoration; failed restoration retains recovery data and Restore remains available.
* If an unexpected ordinary quit ends the message loop while tray controls still exist, failed backend shutdown keeps those controls, the instance guard, and recovery resources available for another Restore/Exit attempt. Shutdown is guarded against nested changes, actual attempts invalidate cached verification, and an already restored/off display cancels its countdown while controls remain open. Native message-queue errors, destroyed controls, and forced session termination retain their existing failure/best-effort exit paths.
* If Windows cannot queue a tray command, captured Restore intent falls back to the normal restoration coordinator directly. Toggle/explicit-enable recovery choices share the same recovery-first policy; an idle off-state toggle cannot activate through this fallback. Busy restoration stays queued, failed restoration keeps its snapshot, and an interrupted fallback retains a request for the callback guard. Other command-post failures report a retry error. Diagnostics preserve the posting error before any logging or recovery call. [Microsoft documents posting failure and queue limits](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postmessagea); this fallback does not change queue limits or assume a driver failure.
* **Expert controls → Diagnostics → Copy support report** copies the version, backend/startup policy, red mode, luma detail preset, session limit/countdown, shortcut/session registration, and last backend error. Paste it into a support request or text editor. It includes no screenshot, account name, or filesystem path, and it reports backend call results rather than measuring the display. Clipboard access happens only when you choose this command; a busy clipboard is reported without clearing its existing contents.
* Support reports include selected-backend request/failure counters for initialization, activation, restoration/ownership cleanup, verification, and shutdown. Requests include successful no-ops and nested recovery calls; verification mismatches or unavailable results count as failures, while inactive checks succeed without display reads. Counters start at zero on app start and saturate instead of wrapping. **Expert controls → Diagnostics → Reset backend counters** restarts these counters while preserving filtering, recovery data, last errors, preferences, and logs. An in-progress operation blocks reset; retry when idle. Counters are diagnostic bookkeeping and do not trigger backend operations or polling.
* Each backend counter category also records completed timed samples, last duration, peak duration, and cumulative inclusive milliseconds. Timing uses the monotonic Windows tick clock around the complete method, including failures, no-ops, logging, and nested rollback/recovery. Nested durations overlap, so totals across categories should not be added as independent time. Zero milliseconds is a genuine coarse-clock sample; no completed sample is shown as timing not recorded. Counts/totals saturate, and Reset backend counters clears timing too. Reading or resetting stored backend metrics starts no measured operation, performs no display call, and cannot discard recovery data. These are elapsed diagnostics, not CPU-time measurements or hardware benchmarks.
* Support reports include cached [Windows display-context metrics](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getsystemmetrics): visible monitor count, virtual desktop width/height in Windows pixels, and whether the remote-session metric is set. Captures occur at tray startup, manual context refresh, explicit backend verification, and after restoration handling for display/power/session/Explorer events. A failed refresh is labeled separately while prior successful values retain their original age and capture reason; no count/geometry is invented when unavailable. Copying a report uses the cache. These logical metrics include no monitor identity, coordinates, screenshots, or paths, and do not establish physical resolution, HDR qualification, remote compatibility, or per-monitor backend coverage. The Boolean remote flag is reported as set/not set rather than claiming a qualified local/remote environment.
* Support reports retain the eight most recent tray restoration-request outcomes in memory, including manual controls, countdown expiry/retries, timer/tray failures, display/power/session/Explorer changes, external requests, confirmed preference import/reset, exit, callback recovery, and deferred replay. Each records its trigger, whole-second age since request, and outcome: deferred, no owned restore needed, backend restored, backend failed, or interrupted before a result. Entries appear by completed request scope, newest first; nested requests can finish before an older outer request. Older overwritten records are counted. History records control activity, includes no paths or images, resets on app restart, and does not determine restoration success or measure display output. Backend-internal rollback/shutdown calls remain in backend counters/logs.
* Completed restoration scopes also retain optional elapsed milliseconds from the monotonic Windows tick clock. Reports distinguish an untimed event from a genuine zero sample. Timing covers the complete control scope, including logging, notifications/dialog wait, and nested recovery; nested times overlap. A deferred attempt measures that attempt, while its later replay has a separate entry. Direct callback observations can be untimed. Clock wrap and interrupted scopes retain timing without altering restore decisions, and viewing/clearing stored history performs no new timing or backend operation.
* Log file: `%LOCALAPPDATA%\RedLight\redlight.log`
* Reset commands:
  * `RedLight.exe --restore`
  * `RedLight.exe --reset-display`
* With a running instance, reset mode asks that instance to restore its saved state. A busy or unresponsive instance is not bypassed.
* Without a running instance, reset mode applies an identity Magnification matrix. This clears Magnification color effects, including those from other tools; it cannot recover a lost gamma-ramp snapshot after a crash.
* Exit codes: `0` for success (including an already-running instance), `1` for startup/recovery/query/file/settings/cleanup/output failure, `2` for invalid arguments, `3` when `--status` finds no tray instance, `4` when an offline profile command finds invalid candidate data, `5` when `--compare-profile` finds valid differences, and `6` when `--save-profile` refuses an existing instance guard. `--help` and `--version` display informational dialogs. Options are case-sensitive and exactly one option is accepted.
* Logs support Unicode Windows profile paths. Rotation runs at startup and before a write would push the current log past a 1 MiB target, retaining up to three archives. Long records are truncated with a marker. About/support reports show file-logging health, missing/truncated record counts, and the last file error. If rotation or storage fails, affected disk writes are skipped while recent memory records and debugger output continue; **Expert controls → Diagnostics → Retry file logging** can retry after the underlying problem is fixed. Earlier missing-record counts remain visible after recovery. Rotation and size checks are best effort when other processes or filesystem restrictions interfere.
* **Expert controls → Diagnostics → Copy recent diagnostics** copies the last 32 retained records, including file-logging failures, oldest first with an overwritten-record count. Recording uses a fixed memory buffer; copying reads no log file and changes no backend, verification, countdown, or preference state. Each formatted record is at most 2,047 bytes; export is bounded below 257 KiB even when malformed UTF-8 bytes need `\xHH` escapes. Valid Unicode stays readable. The buffer starts fresh with logging initialization, survives file retries/shutdown, and is lost when the process ends. File logs may contain more history. Unlike the path-free support report, diagnostic messages can contain paths or other troubleshooting context. Clipboard preparation completes before opening/emptying it; failed preparation or busy access preserves existing content.
* **Expert controls → Diagnostics → Clear recent diagnostic memory** clears retained records and the overwritten-record count while preserving log files, file-logging health/error counts, restoration history, backend state, verification, countdowns, and preferences. It uses no file I/O or allocation and is disabled during filter/preference operations. New diagnostic records, including later notification failures, can appear immediately after clearing.
* Recent diagnostic snapshots/clearing use a separate memory lock from file logging. They can finish while a file write is pending, and formatted records reach memory before waiting for file serialization. The file log can therefore lag behind the memory history; a copied memory record does not confirm its disk write succeeded. Snapshots still allocate bounded output storage and share the memory lock with recording/clearing.
* Support reports also show retained/capacity/overwritten diagnostic-memory counts from an allocation-free memory snapshot. Empty history reports `0 / 32`; unavailable snapshot metadata is labeled not reported. Memory rollover and missing file records are separate counters: older memory records can be overwritten while file logging stays healthy, and file failures can still be retained in memory. Copy recent diagnostics can therefore provide useful context alongside the path-free support report.
* Native OS-version diagnostics run for actual tray startup and display recovery. Help, machine commands, invalid arguments, parser failures, and duplicate launches skip that lookup. Startup build identity and command/error diagnostics still use the logger; command-line operations can write diagnostic files.
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
