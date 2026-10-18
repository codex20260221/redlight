# Issue-driven development checkpoint — 2026-10-18

The user resumed development from `6cc97cb` with a new issue-driven objective, then **cleared the open-ended goal** while the first tray-visibility change was in progress. This checkpoint closes out that focused change on `release/0.6.0-rc1`; stop afterward and do not resume development until the user asks. No feature implementation remains in progress. The earlier Magnification audit remains complete.

## New issue-driven direction

Read the actual open GitHub issue descriptions and every available comment, then compared them with local code. [Issue roadmap](docs/issue-roadmap.md) records priorities, dependencies, and implemented-versus-missing status. Do not resume the old series of specialized diagnostics. Next user-facing priority, if resumed, is #1 adjustable red brightness, followed by #17 optional disabled-by-default startup. Transitions and scheduling require defined semantics first; per-monitor work remains isolated research only.

Issue #27 is addressed locally with four bright monochrome tray symbols (off ring, Strict filled circle, Luma diamond, recovery/unconfirmed triangle), complete shared resource loading, cached state selection, fake-API regressions, and reproducible original pixel assets. Existing tray callbacks, identity, tooltip, menu/shortcuts, recovery priorities, and restore-before-retry behavior remain intact. Native Explorer/DPI/theme/filtered-display acceptance is pending. Brightness, startup, transitions, scheduling, and per-monitor implementations have not been started.

## Restrictions that persist on resume

- Local commits only. No pushes and no GitHub Actions.
- Do not execute RedLight, MagRedProbe, or the real application's CLI, UI, display, clipboard, registry, timer, session, or hardware tests.
- Builds, static analysis, import/PE/hash/ZIP inspection, and isolated fake-API code tests are permitted.
- Read `AGENTS.md`. Preserve black as black, white as pure red, and zero green/blue output. Keep changes focused; update `[Unreleased]` and pending manual Windows notes for behavior/reliability changes.
- Native/hardware qualification remains pending in [release-validation.md](docs/release-validation.md). A build or fake read-back does not qualify physical output.

## Magnification audit completed

The focused audit is committed as `4a1dd71`. The detailed state table, ownership trace, failure coverage, and limitations are in [Magnification lifecycle correctness audit](docs/release-validation.md#magnification-lifecycle-correctness-audit).

- Both backend objects retain static storage duration. Normal `WM_CLOSE` still refuses destruction after failed restoration. No replacement-owner or runtime backend-switching architecture was introduced.
- Magnification native-runtime ownership and completed initialization are separate. Failed initial capture plus failed `MagUninitialize` retains unready resources; cleanup must succeed before fresh initialization/capture. Failed teardown invalidates readiness.
- Owned writes require setter success, successful read-back, and the appropriate comparison. Failed/partial/ignored writes, failed reads, mismatches, and unconfirmed live rollback preserve the preactivation original and retry resources. Failed restoration drops active confirmation before attempting the write; direct enable cannot recapture ambiguous output.
- `MatrixPurpose::RedOnly` enforces exact requested zeros, including green/blue and black offsets, with the established tolerance for nonzero coefficients. `OriginalRestoration` tolerates finite coefficient differences at every position, including zeros, and allows an arbitrary original's green/blue/cross-channel/offset terms.
- Native Magnification calls clear last-error before invocation and capture supplied codes before logging. A failure without a supplied code is explicit. Setter failure is not accepted just because output later matches.
- Incomplete message-loop shutdown preserves live controls after ordinary quit or queue error, including busy scopes. Canceled end-session preserves controls. Session-end restoration suppresses failure prompts/tray-refresh recursion and honors Windows termination intent. Windows can still terminate the process or bypass destructors; recovery cannot survive process destruction in memory.
- External display tools can overwrite a confirmed transform at any time. Read-back is neither atomic ownership nor panel measurement. Restoration writes the saved original and can overwrite another tool's intervening edit; pending recovery never adopts that edit as a new original.

## Subsequent local commits

| Commit | Completed change |
| --- | --- |
| `19904fa` | Cached Magnification lifecycle in support reports |
| `49115f9` | Bounded restoration-history clipboard JSON |
| `2a72156` | Gamma recovery-only state after ambiguous restoration; exclude stale GDI errors |
| `7c60090` | Cached Gamma lifecycle, ownership cleanup, and abandoned-owner block |
| `5df8fb8` | Cached explicit-verification clipboard JSON with numeric flags/locations |
| `d1ed43d` | Shared history/verification schema versions advertised by build discovery |
| `e51d281` | Fixed-buffer status preparation for query/clipboard; unchanged JSON bytes |
| `5a82b57` | Cached timing means and explicit count/total saturation limits |
| `9d8983a` | Retained Magnification startup diagnostic when Automatic falls back to Gamma |
| `dc96a4b` | Countdown overdue duration and one shared support-report capture tick |
| `26e1d99` | Validated UTC support capture stamps, independent of monotonic scheduling |
| `f882ec1` | Cached startup self process/session IDs, without owner detection |
| `f39c0aa` | Session-hook native failure history retained after retry/cleanup; stale-error exclusion |
| This checkpoint | Issue #27 high-contrast tray symbols, fake resource/state regressions, and reviewed issue-driven roadmap |

All commits are local. Each feature's evidence and unexecuted manual notes are recorded in [release validation](docs/release-validation.md#expert-feature-follow-up-validation).

## Verified checkpoint

- Release and MSVC `/analyze` builds passed.
- **401 isolated C++ scenarios passed** across two CTest targets: 378 core/fake-API and 23 fake-filesystem diagnostics scenarios.
- `tools/Test-TrayIcons.ps1` verifies four deterministic original monochrome resources at seven sizes, distinct shapes, transparency, bounded file layout, and Strict/Luma endpoint contrast. The off-screen asset contact sheet was inspected without launching RedLight.
- Both test executables passed the KERNEL32-only import gate before execution. RedLight itself was never executed.
- Static release checks passed: seven reviewed Windows DLLs, x64 GUI executable, ASLR/DEP, version/prerelease metadata, embedded `asInvoker` manifest, and SHA-256.
- The portable ZIP contains 12 files matching the current binary/documentation, including the new issue roadmap. Release/package metadata is refreshed after checkpoint commits as part of closeout.
- Local artifact: `build/packages/RedLight-0.6.0-rc.1-windows-x64.zip`, with SHA-256 sidecars. Generated build artifacts are ignored by Git. This root handoff is internal and excluded by the existing package install rules.

Use the existing build trees when resuming:

```powershell
cmake --build build --config Release
cmake --build build-analysis --config Release
./tools/Test-CodeOnlyBinary.ps1
# Continue only if the import gate succeeds:
ctest --test-dir build -C Release --output-on-failure
./tools/Test-TrayIcons.ps1
./tools/Verify-Release.ps1
cpack --config build/CPackConfig.cmake -C Release -B build/packages
./tools/Verify-Package.ps1
```

`Test-CodeOnlyBinary.ps1` is a PowerShell script: check its success/error rather than relying on a potentially stale `$LASTEXITCODE`. Use `$LASTEXITCODE` for native CMake/CTest/CPack commands. The analysis tree is already configured for `/analyze`. In this environment, ordinary process launch has failed with access error 5; previously approved shell commands used `require_escalated` for the authorized local operations.

## Resume notes

Start with `git status --short` and this handoff. The worktree should be clean. There is no unfinished fix, active build, or required implementation follow-up from this checkpoint. Manual/native qualification remains unauthorized unless the user changes that restriction.

Choose the next meaningful user-facing task from [issue roadmap](docs/issue-roadmap.md) if the user resumes development. Brightness is next; no changes for it are present. The old static backend-mode discovery candidate was not started and is superseded by the issue-driven priorities.

Preserve these integration boundaries:

- Backends are selected at startup; preference edits apply on the next start.
- Gamma's global mutex coordinates cooperating RedLight builds only; failed restore retains calibration/DC/ownership, and cleanup-only retries do not rewrite restored calibration. Abandoned ownership blocks activation for the process lifetime.
- Status uses all 48 payload bits. Current-settings uses its separate scalar packet. Do not repurpose existing wire fields casually.
- Clipboard history and verification schemas are currently protocol 1, with shared version constants and no CLI/IPC query. Exported verification is the last explicit check and cannot clear recovery.
- Profile schema is 3; accepted input schemas are 1/2/3, maximum input is 4096 UTF-8 bytes, and session limits remain 0–240 minutes.
- Keep monotonic countdown/age/timing policy independent of the UTC wall-clock label. Cached diagnostic history must not become a new restoration or ownership state machine.
