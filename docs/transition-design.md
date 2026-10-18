# Red-only fade-in contract (issue #3)

This defines the locally implemented activation-only behavior. Production tray controls, preferences/profiles, cached progress reporting, and independent timer routing are integrated and covered by builds/static analysis and isolated fake-API tests. Native UI/timer/display behavior is not hardware-qualified. Brightness and optional startup are implemented separately. The [issue discussion](https://github.com/michaelmawhinney/redlight/issues/3#issuecomment-4401991857) calls for intermediate red intensity while preserving green/blue elimination and immediate recovery.

## User behavior

- Optional activation-only fade-in, **Off by default**. Choices: Off, 5 seconds, 15 seconds, 30 seconds, and 1 minute. The selected duration affects the next activation; editing it never starts filtering or changes an in-progress fade.
- On Magnification, activation immediately enters a confirmed **red-only 1%** transform, then increases red intensity toward the saved 1–100% brightness. A 1% target needs no animation. At no point interpolate the original RGB matrix into the red-only matrix: doing that would retain green/blue during the transition.
- Every step preserves the selected Strict/Luma mode and Luma weights, exact green/blue/black-offset zeros, and alpha. Percentages scale software red coefficients, not measured/perceived luminance. Integer percentage steps are deliberate; this does not promise perfectly smooth panel output.
- Turning off, Restore, exit, countdown expiry, profile import/reset, and display/power/session/Explorer recovery remain immediate. There is no fade-out to the original display and no delay of restoration to finish an animation.
- Manual brightness edits cancel a running fade and apply immediately through the existing checked live-change path. A mode/preset edit during a fade first completes the configured brightness immediately and safely, then applies the requested mapping. If this completion fails, abort the selection and attempt original restoration.
- Gamma remains fixed at 100%. Its duration choice is only a future Magnification preference, with no ramp, original snapshot, mutex/DC ownership, error, or verification mutation.
- Startup integration still opens the tray with filtering off. Fade-in never introduces automatic activation, scheduling, or startup consent.

## Timing and ownership

1. Configure the initial brightness while off; this writes no color state. Capture the original only through the established enable path, immediately before activation. Start a fade only after the initial red-only transform is confirmed.
2. The existing session countdown begins after successful enable, including time spent fading. Completing or advancing a fade cannot restart a deadline or warning. Expiry outranks another fade step, including when its own timer message is delayed.
3. Use a separate window-bound timer, interval 100 ms and distinct ID 3. Preserve the existing countdown's ID/one-second default. Keep ownership after failed timer removal, ignore stale messages after logical cancellation, and retry cleanup through ordinary Restore/Exit rather than repeatedly logging on every stale tick.
4. Progress comes from supplied monotonic elapsed time, not tick counts or wall-clock time. Delayed/busy ticks skip obsolete levels and request the current level at most once. Unchanged integer levels make no native color call. A complete fade needs at most 99 intermediate/final brightness changes after initial activation.
5. An elapsed target is a request, not confirmed state. Commit progress only after the backend setter, read-back, and red-only comparison all succeed. Coefficient tolerance and exact-zero checks remain unchanged; arbitrary-original restoration keeps its separate comparison purpose.
6. The original preactivation matrix remains immutable throughout. Never recapture a partially applied step or another tool's matrix as the original. Backend objects retain static lifetime and existing close/main-loop recovery protections.
7. A restoration request logically cancels the fade even if a busy scope must defer the actual restore. A step completing after such cancellation cannot reactivate the plan. Stop the plan before shutdown attempts or window destruction.

## Failure rules

| Event | Required behavior |
| --- | --- |
| Initial activation/capture/write fails | Existing activation rollback/recovery rules; no fade starts and saved target remains unchanged |
| Fade timer cannot be installed | Cancel the plan, attempt immediate original restoration, report failure, retain any unconfirmed recovery ownership |
| Step setter/read-back/mismatch fails | Existing live rollback attempts the last confirmed red level; cancel the plan and immediately attempt original restoration; preserve the saved target and original if recovery fails |
| Manual brightness change fails while canceling a fade | Keep the prior preferred target; cancel the plan and attempt immediate restoration rather than silently continuing toward another target |
| Timer removal fails | Keep exact native window/ID ownership for retry, stop logical color updates, and preserve the original; this cannot prevent immediate restoration |
| Explicit verification reports Changed/Unavailable | Stop further fade writes without an automatic color write; verification stays read-only, attention remains visible, and Restore remains available |
| Another display tool changes output | Existing last-writer/race limits apply. Software read-back is not atomic ownership or physical measurement; do not automatically fight a reported mismatch |
| Busy or delayed processing | No overlapping backend calls or replay of missed levels; independent queued restoration wins |
| Forced Windows termination | Best-effort restoration only; no promise that in-memory recovery survives process death |

## Preferences and status

Portable profile schema 5 adds `fade_in=off|5s|15s|30s|1m`; accepted schemas 1–4 and legacy records default to Off without migration writes. Preserve all existing field indices and defaults. Current-preference IPC uses bits 44–46 for the five duration option indices, with exact schema matching and bit 47 reserved; status protocol 1 stays unchanged.

Keep preferred target brightness separate from the backend's currently confirmed intermediate level. Tooltip/support/About may show a running fade and its target; no status should present the target as already applied. A canceled or unconfirmed fade never promotes active health. Copying cached information performs no new display check. Startup settings remain outside profiles/reset.

## Required isolated and later Windows validation

Isolated coverage includes all target levels/durations, exact timing boundaries, repeated/delayed ticks, monotonic wrap, invalid inputs, cancellation, stale callbacks, progress confirmed only after successful writes, retained originals through partial/read/mismatch/rollback/restore failures, timer arming/removal/ownership failures, countdown precedence, cache attention without writes, immediate manual selection behavior, Gamma preference-only behavior, schema migration/packet bounds, and allocation-free progress using actual production backend code with fake APIs.

Native/manual checks remain pending and unauthorized: observe every mode/preset/brightness at multiple durations, confirm black/green/blue safety on all monitors, validate apparent brightness and responsiveness, interrupt through all Restore/Exit/environment paths, inject timer/backend failures, inspect modal/queued/stale-message behavior, verify unchanged countdowns and startup-off behavior, and qualify sleep/wake/hotplug/lock/session end. No actual RedLight, timer, native display, or hardware test is part of design validation.
