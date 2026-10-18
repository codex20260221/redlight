# RedLight 0.6.0-rc.1 release validation

This is a release candidate for review and manual qualification, not a claim of completed hardware testing. The authorized scope for this preparation excludes launching RedLight, running MagRedProbe, or performing display/monitor tests. None of those actions were performed. Historical beta screenshots and changelog tests are not evidence for this candidate.

## Candidate scope

Preserve the native red-only filter, both established modes, Magnification-first selection, and legacy Strict Red gamma fallback. Add recoverable failure handling, usable tray/keyboard controls, remembered mode, diagnostics, and portable release delivery. No translucent overlay, autostart, service, or automatic filter activation is introduced.

## Automated evidence

Run from a Visual Studio 2022 x64 C++ environment with CMake 3.20 or later:

```powershell
cmake -S . -B build
cmake --build build --config Release
./tools/Test-CodeOnlyBinary.ps1
ctest --test-dir build -C Release --output-on-failure
./tools/Verify-Release.ps1
cpack --config build/CPackConfig.cmake -C Release -B build/packages
./tools/Verify-Package.ps1
```

Code tests compile the actual MagnificationFilter.cpp and GammaRampFilter.cpp against fake Windows/Magnification headers. They link no display libraries; the import checker rejects any DLL except KERNEL32.dll. Transform checks cover black, white, primary colors, intermediate RGB combinations, and preserved alpha. Failure checks cover initialization/capture/write/read-back/release failures, silent gamma rejection, partial-write rollback failure, retained restore state, retries, and mode state after failed calls. Command parsing is tested without starting the tray application.

Local validation on 2026-10-18 passed: MSVC 19.44.35228, Windows SDK 10.0.26100.0, CMake 4.4.4, Release build, all 17 code-test scenarios, and warning-free MSVC /analyze. The import-isolation gate also correctly rejected the real application binary without executing it.

Static artifact checks cover version 0.6.0-rc.1, prerelease metadata, PE x64/GUI, ASLR/DEP, the embedded asInvoker manifest, and executable SHA-256 generation. Package verification compares all 11 packaged files with the current binary/documentation and checks the internal executable checksum. The release uses the static MSVC runtime. CPack includes only the app and documentation, not tests or the optional manual probe.

These tests cannot prove physical panel output, driver correctness, Explorer behavior, or session/power behavior. The CI definition is updated locally; no remote CI run or publication is claimed.

## Manual Windows test notes / proposed PR summary

Changes: capture state before activation, preserve failed restoration for retry, read back gamma writes, restore on environment transitions, and keep tray controls recoverable. Strict Red and Luma Red retain their existing transforms.

Validation performed: Windows x64 build, isolated backend/command-line tests, static executable inspection, and portable package inspection. **All manual items below are pending and were deliberately not executed.** Record OS build, GPU/driver, backend, SDR/HDR setting, monitor topology, expected/observed result, and relevant logs for each manual run.

| Manual check | Expected result | Status |
| --- | --- | --- |
| First start / repeat start | One instance per session; filter starts off; tray is usable; no administrator prompt | Pending |
| Tray click and keyboard activation | Mouse, Enter/Space, context menu, Escape, and menu accelerators work | Pending |
| Shortcuts and conflicts | Toggle and restore work; conflicting shortcuts shown as unavailable; tray remains usable | Pending |
| Strict Red | Black stays black; white is maximum red; pure green/blue inputs go black; no output green/blue | Pending |
| Luma Red | Black stays black; white is maximum red; red output uses 0.299R + 0.587G + 0.114B; no output green/blue | Pending |
| Mode switching and persistence | Active/inactive switching works; restart remembers mode but starts off; fallback offers Strict Red only | Pending |
| Repeated enable/disable | Each disable restores state captured immediately before that activation | Pending |
| Exit, close, and logoff | Normal exit restores state; failed restore stays available for retry; OS shutdown restoration is best effort | Pending |
| Failure/recovery | Backend failures visible and logged; failed enable never shows ON; partial-write recovery remains retryable | Pending |
| Restore commands | Running instance restores saved state; unavailable owner is not bypassed; standalone Magnification reset uses identity | Pending |
| Explorer restart | Filter restores; icon and keyboard menu return; failed icon registration retries; no automatic re-enable | Pending |
| Multiple monitors/adapters | Check every attached panel; record gamma limitations separately; no claim of gamma coverage across adapters | Pending |
| Sleep/wake and lock/unlock | Restoration attempted and filter stays off; user can re-enable; failure retains recovery data | Pending |
| Hotplug, resolution/orientation changes | Restoration attempted; filter stays off; next enable captures current state | Pending |
| Unicode Windows profile / clean PC | Diagnostics folder opens, log rotation works, portable app needs no VC++ redistributable | Pending |

## Expert-feature follow-up validation

Auto-restore session limits: pure code tests cover disabled/default behavior, exact deadlines, invalid limits retaining prior state, clock wrap, cancellation, changing active limits, and five-second retry throttling. These run against supplied timestamps; no real timer wait or display operation is used. The earlier 17-scenario result above describes the RC checkpoint, not the growing follow-up suite.

Manual notes (pending; not executed): choose each session limit while off/on, verify a confirmed enable starts the countdown, verify expiration restores on each backend, inject a restoration failure to check retry throttling and retained recovery data, cancel through manual restore/exit/power/session/display changes, and verify a failed Win32 timer registration attempts immediate restoration. Reconfirm red-only output on all monitors while a session is active. No automatic activation should occur.

Backend preferences: code tests cover Automatic priority/fallback, strict choices without fallback, no filter activation during selection, failed initialization cleanup blocking fallback, stored value validation, and command-line override rejection/precedence. Manual notes (pending; not executed): restart under each saved preference and override, verify the selected backend and inactive startup state, verify an unavailable strict choice fails visibly, and confirm an Automatic override recovers from a bad saved preference without changing the saved value. Recheck both red modes where supported, restoration, and all attached displays.

Luma detail presets: code tests verify white/black invariants, zero green/blue output, nonnegative weights, equal-primary visibility, live application failure retaining the confirmed preset, preservation of restoration snapshots, Strict Red remaining unchanged, and gamma rejection without writes. Manual notes (pending; not executed): switch all presets off/on and under Strict Red, verify changes in colored UI readability on each monitor, verify saved selection across restart/backend fallback, reconfirm black/white/no-green-blue output, and test restoration on exit and environment changes after several preset switches.

Support reports: pure formatting tests cover off/active/recovery states and absent versus expired countdowns. Clipboard unit tests compile the production helper with fake allocation, Unicode conversion, and clipboard functions; they cover preparation failures, busy clipboard, empty/transfer/close failures, and transfer-of-ownership cleanup. The code-test binary still imports only KERNEL32.dll. Manual notes (pending; not executed): copy/paste a report while off/on/recovery-pending, verify Unicode text, confirm no account/path/screenshot content, test a busy clipboard, and check that clipboard errors never change filter state.

## Known limitations and promotion gate

Target Windows 10/11 x64 SDR desktop sessions first. HDR, remote sessions, secure desktop, exclusive full-screen applications, protected content, gamma multi-adapter coverage, and interaction with other color tools remain unqualified. Do not promise hardware blue-light elimination. Gamma read-back checks the driver-reported ramp and cannot measure panel output. See [Microsoft's documented gamma limitations](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-setdevicegammaramp).

A standalone reset cannot reconstruct a lost gamma snapshot after a forced termination; it applies only an identity Magnification matrix. Normal exit is preferred. Code signing is not configured; builds remain unsigned.

Before a stable release, complete the relevant manual matrix, resolve failures, review the diff and license/package contents, verify published checksums, and choose a signing/distribution policy. Tag workflows create a draft prerelease for human review. The initial RC preparation made no commits or pushes and published nothing. Follow-up expert-feature development is checkpointed only in local commits; nothing is pushed or published.
