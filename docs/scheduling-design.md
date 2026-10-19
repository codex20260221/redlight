# Daily schedule contract (issue #4)

Optional daily scheduling is implemented locally, with native UI/message routing compiled and analyzed and actual control/backend code exercised only through isolated fake APIs. Native Windows and hardware qualification are pending. The [request](https://github.com/michaelmawhinney/redlight/issues/4) asks for timed triggers and startup; the [maintainer comment](https://github.com/michaelmawhinney/redlight/issues/4#issuecomment-4401992980) asks for persistence, manual-control behavior, and more real-world backend experience. That hardware experience has not been established. No application or display experiment was run.

## User behavior

One daily enable and one daily restore time use strict local `HH:MM` in 24-hour form. They must differ when enabled; evening-to-morning naturally crosses midnight. Scheduling is **Off by default**. The tray dialog has an explicit enable choice; Apply saves and arms future boundaries, while Cancel keeps prior preferences. No weekdays, sunrise calculation, service, wake task, location lookup, or unattended login is added. Windows startup remains a separate consented setting, excluded from profiles/reset.

Launch, enable/edit, wake/unlock, reconnect, restored controls, and profile import arm **future** boundaries without catching up the current interval. The tray starts with filtering off. The times are events, not continuously enforced desired state: manual toggle/Turn on/Restore takes effect immediately and lasts until the next eligible future boundary. Editing brightness/mode/fade uses existing checked behavior. Disabling scheduling prevents future automatic activation without changing current filtering; Restore turns it off immediately.

An enable while already active is idempotent and never restarts countdown or fade. A successful off-to-on activation uses the current backend and starts the existing configured countdown, including fade time. Magnification honors mode/brightness/fade; Gamma keeps Strict Red at 100%. Restore, expiry, exit, and environment recovery remain immediate, with no fade-out. Backend preference still applies on the next app start, not through runtime switching.

## Boundary delivery and overrides

`src/DailySchedule.h` promotes the supplied-time prototype into the integrated policy. Entry into the configured minute may deliver late within that minute, once per action/date in the current observation lifetime. Missed minutes are skipped. A first observation, invalid sample, clock/zone discontinuity, or gap longer than one minute rebases without emitting an enable. Busy/modal/preference work consumes an enable as skipped; closing a dialog never replays it. Restoration bypasses enable gates and enters the existing immediate/deferred restore path.

Manual controls invalidate observation so an already-due queued event cannot undo them. Consumption is recorded before native dispatch; failed, skipped, or nested requests cannot repeat the same boundary. Ordinary invalidation, stop, wake/reconnect rearming, and canceled session-end preserve consumed-date marks for unchanged settings. A process restart or deliberate configuration Apply starts a new lifetime; consumption is not persisted in portable preferences. Date rollback cannot replay an action older than its runtime high-water mark.

The timer is window-bound ID 4 at one second, independent of tray recovery, countdown, and fade. [WM_TIMER](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-timer) is low priority, so delivery is best effort. RedLight must be running; it never launches itself, wakes Windows, or replays missed alarms. Suspend/session/tray failure paths keep their established immediate restoration behavior.

## Integrated clock and session checks

`src/ScheduleClock.cpp` reads one UTC FILETIME, obtains checked dynamic zone rules, and converts that same snapshot through checked UTC/calendar and local conversion. FILETIME is floored to SYSTEMTIME millisecond precision. Independent GetLocalTime/UTC samples are not combined. [System time](https://learn.microsoft.com/en-us/windows/win32/sysinfo/system-time) can move in either direction; [local time](https://learn.microsoft.com/en-us/windows/win32/sysinfo/local-time) follows current Windows rules.

Checked [QueryUnbiasedInterruptTime](https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-queryunbiasedinterrupttime) excludes sleep, providing additional discontinuity detection if notifications are missed. GetTickCount64 brackets capture latency and retains its existing countdown/fade role. Capture must finish within two seconds; policy observation permits at most two seconds of UTC/awake elapsed disagreement and rejects gaps beyond one minute. Valid Gregorian years are 1601–30827. Supplied local/UTC values must agree within a conservative whole-minute ±24-hour offset. These are software bounds, not proof that every tiny jump or lost notification can be detected.

[GetDynamicTimeZoneInformation](https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/nf-timezoneapi-getdynamictimezoneinformation) has three successful statuses including zero; its invalid sentinel is failure. [FileTimeToSystemTime](https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/nf-timezoneapi-filetimetosystemtime) and [local conversion](https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/nf-timezoneapi-systemtimetotzspecificlocaltimeex) are checked. Documented native errors are cleared before calls and captured before cleanup/logging. QueryUnbiasedInterruptTime has no documented extended-error contract, so its GetLastError value is not reported as a cause. No call changes clocks, time zones, or registry settings.

Semantic zone identity compares bounded key/rule/bias fields, ignoring localized display names, padding, and unused transition fields. A changed identity rebases even with the same current offset. WM_TIMECHANGE/setting, power, and session events also invalidate continuity. Spring-forward minutes that do not occur are skipped; repeated local hours cannot duplicate an already-consumed action within the current lifetime.

`src/ScheduleSession.cpp` queries only the current local calling session through [WTSQuerySessionInformationW](https://learn.microsoft.com/en-us/windows/win32/api/wtsapi32/nf-wtsapi32-wtsquerysessioninformationw). Only complete level-1 active/unlocked data is Ready; unavailable, malformed, locked, disconnected, or unknown data cannot authorize automatic activation. Buffers are copied without alignment assumptions, freed on every returned-buffer path, and never exported with account names. Native error capture precedes void WTSFreeMemory. Modern [session lock flags](https://learn.microsoft.com/en-us/windows/win32/api/wtsapi32/ns-wtsapi32-wtsinfoex_level1_w) are used for the supported Windows versions.

Session readiness is checked at arming and immediately before an off-to-on dispatch. The dispatcher rechecks gates after this potentially reentrant query, including pending restoration and clock validity. Total elapsed capture/dispatch must remain within two seconds and inside the original trigger minute. An unlocked-session result is not atomic exclusion against a later Windows transition; notification/deferred recovery still has priority. Missing WTS information blocks automatic enable and leaves manual controls available.

## Transactions and failure states

`ScheduleApply.h` prepares policy, confirms timer ownership, saves the complete record, captures a fresh post-save clock baseline, and only then publishes runtime state. A trigger crossed during persistence is already past and cannot replay from the earlier preparation sample. Replacing an existing timer is checked too; retained ownership after failed cleanup is not treated as proof that the timer is running. Disabling needs no valid clock/session sample. `ScheduleRuntime.h` keeps stop revision separate from observation invalidation so callback/exit stops cannot be overwritten by an earlier prepared plan.

| Event | Behavior |
| --- | --- |
| Busy/modal/preferences, pending recovery, missing tray/hooks, attention, invalid clock, or suspended session | Skip enable; retain manual Restore and original ownership; queue no enable |
| Active backend at enable time | Consume as an idempotent no-op; do not restart countdown/fade |
| Countdown expires in an active interval | Existing immediate restoration/retry; no interval-based re-enable |
| Timer install/replacement failure | Stop logical automatic work, retain prior preference and native cleanup ownership, attempt original restoration, expose not-armed status |
| Preference save failure | Keep prior selected configuration/policy; clean a newly installed timer or retain failed cleanup ownership |
| Stop during installation | Abort before persistence; keep logical work stopped and cleanup retryable |
| Clock unavailable after a successful save | Keep the saved preference, stop automatic work, retain any failed timer cleanup, and report saved-but-not-armed |
| Stop during a successful save or post-save clock capture | Report saved-but-not-armed; retain the stop instead of publishing prepared automatic work |
| Timer removal failure after saved Off | Remain logically Off; retain exact owner/ID for Restore/Exit cleanup; stale callbacks cannot write colors |
| Failed/partial activation or restoration | Existing backend rollback/original/recovery states remain authoritative; consume the event without retrying enable |
| Failed scheduled restore | Retain original/controls; existing countdown retry applies when due, otherwise manual or a later restore request retries |
| Exit, callback failure, message-loop shutdown, or destruction | Stop scheduling before backend shutdown/destruction; failed normal close retains controls and original recovery resources |
| Windows session termination | Best effort; Windows may terminate regardless. A canceled query can rearm future boundaries without losing consumed marks |

Deferred restoration is attempted before error UI. About, validation, input dialogs, and the tray popup block automatic enable while open; restoration still works. Modal/configuration scopes invalidate observations on entry and exit, even if no timer callback ran during the due minute. Only a successful transaction with a fresh post-save baseline retains that baseline explicitly. Other dialog exits wait for a trusted observation and skip boundaries already reached by that observation; they do not replay a trigger passed during a slow close or canceled/failed save. Cached menu/About/support status distinguishes saved preference, armed runtime, owned native timer, and errors. Copying/querying settings does not read clocks/session or activate filtering. Read-back and session queries do not prove physical output or exclusive ownership against Windows/external display tools.

## Portable configuration and IPC

Profile schema 6 appends `schedule=off|daily`, `enable_at=HH:MM`, and `restore_at=HH:MM` after the eight existing fields. Schemas 1–5/legacy records resolve to Off with both times 00:00, without migration writes. Equal times are valid only while Off. Import/reset consent shows every choice, automatic activation, future-only semantics, and immediate restoration before saving. Import never executes a trigger; reset disables scheduling. Explicit offline next-start save persists reviewed data but does not run a tray or arm a timer.

Current-settings protocol 2 keeps the bounded synchronous message and one positive x64 scalar. Tag `0x53` in bits 56–63 binds exactly schema 6 through a compile-time assertion; bit 55 is reserved. This avoids inconsistent multi-query snapshots or new cross-process pointers/shared storage. Old/new protocol or packet families reject each other without fallback activation. Current-settings/clipboard JSON and build discovery advertise protocol 2 together. Status protocol 1 is unchanged; saved-settings/validation remain additive protocol 1 with profile schema 6.

| Payload bits | Value |
| --- | --- |
| 0 / 1–2 / 3–4 | Red mode / Luma / backend |
| 5–12 / 13–14 / 15–16 | Session minutes / shortcuts / expiry option index |
| 17 / 18 / 19–20 / 21 | Unsaved / busy / startup source / historical load warning |
| 22–28 / 29–31 / 32 | Brightness / fade option index / schedule enabled |
| 33–43 / 44–54 | Enable / restore local minutes, each 0–1439 |

Every range, option, reserved bit, and enabled equal-time constraint is validated. Existing comparison field positions remain fixed; the three schedule fields are appended. Maximum complete settings still fit the existing 256-byte preference JSON and 512-byte current snapshot buffers. The full native consent preview grows to 1536 bytes and rejects truncation.

## Evidence and remaining qualification

The suite has 501 isolated scenarios: prior policy/clock coverage plus 28 integrated session/settings/runtime/dispatch/backend cases. Coverage includes every local minute and packet flag, all 2048 field masks (including invalid enabled equal-time combinations), strict old/new schemas, bounded consent/JSON, current-session arguments/free/error paths, consumed-date retention across rearm, clock/zone attention, all enable gates, timer/save/allocation failures, stop during installation/save/refresh, post-save clock failure, persistence crossing a trigger, modal exit without a timer callback, three independent timers, active no-op/countdown priority, queued restore/stale dispatch, ambiguous Magnification rollback/restore, independent Gamma cleanup-only recovery, and cached menu/report/query isolation. No application execution is part of this evidence.

Manual Windows qualification remains pending and unauthorized: daily/cross-midnight timings, startup/wake without catch-up, real DST/time-zone/clock corrections and RPC delays, modal/blocked/stale queues, lock/sleep/hibernate/reconnect and canceled session end, tray loss, failed reads/timers/storage/backends, save/import/reset/rearm consent and mixed-version scripts, unchanged countdown/fade/Gamma behavior, recovery/exit paths and actual black/red-only output on supported displays. External tools may overwrite display state between checks; original restoration may overwrite their intervening edits. Per-monitor controls remain separate research, not a scheduler fallback.
