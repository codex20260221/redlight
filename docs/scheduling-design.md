# Daily schedule contract (issue #4)

Scheduling is not implemented in the application. This contract defines a first, optional daily schedule before production changes. The [request](https://github.com/michaelmawhinney/redlight/issues/4) asks for scheduled triggers and startup; the [maintainer comment](https://github.com/michaelmawhinney/redlight/issues/4#issuecomment-4401992980) asks for persistence, manual-control behavior, and more real-world backend experience. Local builds/fake APIs have not supplied that hardware experience. The supplied-time policy in `tests/Prototypes/DailySchedule.h` is isolated, without timers, registry changes, or display activation in the application.

## User behavior

One daily enable time and one daily restore time, entered as local `HH:MM` in 24-hour time. They must differ when scheduling is enabled. An evening enable and morning restore naturally cross midnight. No weekdays, sunrise calculation, service, wake timer, network location, or unattended login is included. Scheduling is **disabled by default** and requires a deliberate user action to enable. Optional Windows startup stays a separate consented setting; scheduling must never create or repair a Run entry.

Initial policy: launch, schedule enable/edit, wake/unlock, and restored tray availability only arm **future** boundaries. They do not catch up the current interval or immediately enable filtering. The tray starts off even when the saved schedule is enabled. An optional user question offered startup/wake catch-up; absent steering, use this conservative choice. It can be revised before integration.

The daily times are trigger events, not a continuously enforced desired state. A manual toggle, explicit Turn on, or Restore takes effect immediately and lasts until the next eligible future scheduled boundary. There is no background loop that repeatedly reapplies an interval's state. Editing brightness/mode/fade-in changes the existing preferences; it does not opt into scheduling or restart an elapsed deadline. Turning scheduling off prevents future automatic activation without changing a manually selected display state; **Restore display** remains the direct way to turn filtering off.

## Boundary delivery and missed events

Use the current Windows local date/time and a separate supplied monotonic elapsed clock. Never use wall-clock subtraction for countdown or fade progress. [Windows system time](https://learn.microsoft.com/en-us/windows/win32/sysinfo/system-time) can move in either direction; [local time](https://learn.microsoft.com/en-us/windows/win32/sysinfo/local-time) follows Windows time-zone rules.

The initial trigger policy observes entry into the configured minute, once per local calendar date and action. Ordinary message delay within that minute may deliver the action late. A missed minute is skipped; never replay a backlog. A first observation, invalid sample, suspended session, unavailable controls, clock/time-zone discontinuity, or a gap longer than one minute rebases observation without emitting an enable event. A busy operation consumes the boundary as skipped, rather than queueing a surprise activation when a dialog closes. A later tick in that same minute cannot replay it. Record the skip reason in ordinary diagnostics.

At midnight, calendar progression must be validated rather than comparing only hour/minute. Malformed dates/times and supplied-time inconsistencies must never become trusted observations or erase consumption records; invalidate continuity so the next valid sample cannot catch up. A repeated sample emits nothing. Consume a boundary before handing an intent to native code, so a failed backend call, nested callback, or retry cannot run it twice.

## Restoration, overrides, and failures

| Event | Required behavior |
| --- | --- |
| Eligible scheduled enable, fully off and healthy | Use the existing checked enable path, preferred mode/brightness and optional fade-in; start a fresh configured countdown only on successful activation |
| Enable while already active | Idempotent; never restart countdown/fade or recapture the original |
| Enable while recovery/restore/busy/attention/tray/session-hook/timer trouble is pending | Skip it, retain recovery ownership and manual controls, log the reason; no deferred activation or fallback force-reset |
| Scheduled restore | Use immediate original restoration, cancel fade/countdown, and retain existing failed-restoration retry rules; do not fade out |
| Manual Restore/toggle or standalone reset request | Immediate existing behavior; rebase observation to skip already-due queued boundaries, consume no future event, and never auto-enable to undo the request |
| Countdown expires inside an active interval | Immediate existing restore/retry; remain off until another eligible future enable event or explicit user action |
| Suspend, lock, display/session/Explorer recovery | Existing immediate restoration wins; suspend/rebase schedule observation, with no catch-up activation on recovery |
| Backend write/read-back/restore failure | Existing original, error, and retry state remains authoritative; consume the event and do not automatically retry enable |
| Schedule timer installation failure | Disable runtime automatic activation, expose the failure, and restore immediately if activation was owned by the scheduling transaction; keep manual controls and native cleanup ownership |
| Schedule timer removal failure | Logical disable prevents activation; preserve exact native owner/ID for retry and ignore stale callbacks |
| Exit/session end/process termination | No launch/wake task is created. Existing best-effort restore and close refusal rules remain; Windows may terminate regardless |

Scheduled restoration must not be blocked by an enable gate or by a manual override. While busy, it enters the existing deferred **restoration** path and logically cancels fade immediately. Use a distinct native timer from tray recovery, countdown, and fade-in. Do not add worker-thread color writes or a second display owner.

## Clock and daylight-saving rules

- Observe UTC plus a consistent local conversion/zone snapshot and monotonic tick; validate all three before activation. The future native adapter must detect unexpected UTC-versus-monotonic jumps even if no notification arrives. [WM_TIMECHANGE](https://learn.microsoft.com/en-us/windows/win32/sysinfo/wm-timechange) is an additional rebase signal, not proof of continuous time. A time-zone/rule change also rebases without catch-up.
- Spring-forward minutes that do not occur are skipped. At fall-back, each action may run at most once on that local date; repeated local minutes cannot repeat enable or restoration.
- Clock/date rollback must not forget consumed events. Keep a per-action maximum consumed date for the runtime observation lifetime. Dates older than that mark cannot replay an action; a deliberate schedule rearm may start a new observation lifetime after a clock correction. Rebase alone does not clear consumption marks.
- Local midnight is the next valid calendar date, including leap days/year changes. An arbitrary date jump is a discontinuity, not midnight progression.
- Native time-zone conversion must use checked APIs such as [SystemTimeToTzSpecificLocalTimeEx](https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/nf-timezoneapi-systemtimetotzspecificlocaltimeex), preserving supplied/absent errors before logging. No call changes the system time or time zone.
- A window timer cannot guarantee an exact time: [WM_TIMER](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-timer) is low priority. User text must disclose best-effort delivery, skipped missed times, and the requirement for RedLight to be running with usable controls.

## Persistence and integration boundaries

Save the enabled flag and two local minutes together in the established validated per-user record, with a future profile schema that defaults older inputs to scheduling Off. Export/import/reset review must disclose automatic-activation consent and restore before storage/application; importing a schedule must not execute a trigger. Reset uses disabled scheduling. Rejected/failed storage retains the prior schedule and runtime policy; no partially armed UI claim. Activation is allowed only after the native timer and schedule configuration are both confirmed.

Do not append schedule times to the current packet: schema 5 uses bits 0–46 and only bit 47 remains free. The proposed replacement below carries a complete single-response snapshot. Preserve old protocol rejection and read-only automation. Runtime observation/consumption, countdown progress, and override state are not portable preferences.

The isolated supplied-time boundary/consumption policy now exists outside the application target and native dispatch. It validates Gregorian dates in the Windows 1601–30827 range, supplied UTC/local agreement within a conservative whole-minute ±24-hour offset, monotonic gaps up to 60 seconds, and UTC/elapsed disagreement up to 2 seconds. These prototype bounds still require review against the future native adapter; a zone/rule change with an unchanged offset requires an explicit continuity signal. Rebase skips already-past events and preserves consumed-date marks; only deliberate configuration starts a new observation lifetime. Next review native clock conversion, control/session readiness, the settings UI/consent transaction, full-preferences IPC, timer lifecycle, and static callback paths as one focused integration plan. No production schedule or hardware support claim follows from the prototype.

## Production integration plan (not implemented)

### Complete preference snapshot

Proposed profile schema 6 appends `schedule=off|daily`, `enable_at=HH:MM`, and `restore_at=HH:MM` after all eight existing fields. Older schemas and legacy records resolve to Off with both times `00:00`; equal times are allowed only while Off. Strict 24-hour tokens preserve midnight and are portable local times, not UTC offsets. Import preview must explicitly disclose automatic activation, both times, future-only arming, and immediate restoration of the current display before import. Reset disables scheduling. Neither operation changes Windows startup consent.

Current-settings protocol 2 can keep the existing bounded synchronous message and one positive x64 scalar by using an eight-bit packet tag and option indices. This avoids a mixed snapshot from multiple queries, cross-process pointers, shared memory, or another server. Proposed tag `0x53` occupies bits 56–63 and binds **exactly schema 6**; enforce this binding with a compile-time assertion rather than silently accepting later schemas. Bit 55 is reserved and must be zero. No production version/format changes occur until the schedule feature is integrated.

| Payload bits | Value |
| --- | --- |
| 0 | Red mode |
| 1–2 / 3–4 | Luma / backend option |
| 5–12 / 13–14 | Session minutes / shortcut option |
| 15–16 | Expiry warning option index (Off, 30s, 1m, 5m), preserving exact existing values |
| 17 / 18 | Unsaved / busy flags |
| 19–20 / 21 | Startup preference source / historical load warning |
| 22–28 / 29–31 | Brightness 1–100 / fade option index |
| 32 | Daily scheduling enabled |
| 33–43 / 44–54 | Enable / restore local minutes, each 0–1439 |

Packing/unpacking must validate every option, range, state, reserved bit, and the equal-time rule. Old clients already reject different canonical profile schemas; protocol negotiation must also reject old/new mismatches with zero/invalid response, without activating the app or changing output. Update current-settings JSON (including errors) and build discovery to protocol 2 together; keep status protocol 1 unchanged. Saved-settings/validation JSON can remain additive protocol 1 with profile schema 6. Recompute bounded formatter/preview sizes from complete maximum values and test exact terminators. Existing comparison field positions remain fixed; append the three new fields.

### Checked native clock adapter

Read one UTC FILETIME, obtain checked dynamic time-zone settings, convert that same UTC snapshot through checked `FileTimeToSystemTime` and `SystemTimeToTzSpecificLocalTimeEx`, and bracket capture with monotonic ticks. Floor FILETIME to the same millisecond precision used in SYSTEMTIME. Never combine independently sampled GetLocalTime and UTC readings around a boundary. Scheduling observations should use checked [QueryUnbiasedInterruptTime](https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-queryunbiasedinterrupttime), which excludes sleep, so UTC/elapsed disagreement can additionally detect missed sleep notifications. Keep GetTickCount64 for bounded capture latency and the existing countdown/fade policy; do not change their clock basis. Power/session notifications still invalidate schedule continuity immediately. Capture and tolerance are best effort, not proof that every tiny discontinuity or notification loss can be identified.

[GetDynamicTimeZoneInformation](https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/nf-timezoneapi-getdynamictimezoneinformation) has three successful status values including zero; only its documented invalid sentinel is failure. [FileTimeToSystemTime](https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/nf-timezoneapi-filetimetosystemtime) and local conversion return checked Boolean success. Clear last-error before calls that document it and capture supplied/absent errors immediately. QueryUnbiasedInterruptTime does not document an extended-error contract: check failure but do not report its GetLastError value as a native cause. Failed/malformed/slow capture produces no usable observation and invalidates continuity. A native time-zone/rule identity must compare semantic fields and a bounded key name, not structure padding or localized display names; compare it even when the current UTC offset has not changed. No operation changes clocks, time zones, or registry settings.

Keep this adapter free of activation, timers, persistence, and dialogs. Fake-API tests must cover success statuses, exact arguments, non-hour offsets, capture near midnight, submillisecond truncation, unsigned tick wrap, each failed conversion and stale-error provenance, malformed output, slow capture, zone/rule identity changes, and allocation/backend isolation. Build it against real headers for ABI/static review, but do not execute the native path under the present restrictions.

### Configuration, timers, and dispatch

Provide one native schedule dialog with an explicit enabled choice and two validated times; Cancel changes nothing. Enabling must review valid data, prepare a fresh observation baseline, confirm a window-bound timer, save the complete preference record, and only then publish the new runtime policy. Disabling must remain possible without a valid clock sample. If an intermediate step fails, retain the prior saved configuration, cancel any newly armed logical plan, and preserve failed native cleanup ownership. A previously installed timer must not be forgotten or re-owned after a failed replacement/removal. Startup can show a saved enabled preference as **not armed** on failure; it cannot claim runtime scheduling success.

Use a separate one-second window timer/ID and the existing checked timer helper. A disabled schedule makes no periodic clock/display calls. Always consume an observed boundary before dispatch; never queue an enable. Busy/preferences/queued recovery, absent tray/session hooks, invalid clock, failed timer ownership, explicit Changed/Unavailable verification, backend errors, or pending original recovery block enable. Already-active enable is idempotent and cannot restart countdown/fade. Restoration bypasses enable gates and uses the existing immediate/deferred restoration path. Profile/reset/manual toggle/Restore and environment events rebase or invalidate observation before later automatic actions can run.

Build the native clock adapter independently first. Integrate schema/IPC, dialog, cached preferred-versus-armed status, timer ownership, startup, and dispatch together only after the complete path has fake failure coverage and a static review of every restore/exit/callback branch. Do not temporarily ship an enabled preference with incomplete consent or runtime routing. A native control probe for issue #23 remains separate research and cannot take over display ownership as a scheduler fallback.

## Required evidence

Ten isolated policy scenarios cover every local minute, normal and cross-midnight schedules, once-only consumption, manual rebasing, skipped busy/missed boundaries, invalid dates/times, leap/year/century transitions, UTC/monotonic disagreement and exact tolerance, time jumps, first sample/rearm, gap/notification rebase, DST skip/repeat, both Windows calendar endpoints, monotonic wrap, and no heap/native calls. They bring the full code suite to 456 scenarios. Prototype events are intents only; tests do not claim native time conversion, backend gating, or schedule UI integration. Integration later needs fake timer/clock/conversion/backends, failed/partial reads/writes, original retention, reentrant restoration, stale callbacks, persistence/migration/consent/IPC, countdown/fade priority, and Gamma independence.

Manual Windows qualification remains pending and unauthorized: daily/cross-midnight operation, real DST/time-zone/clock correction, blocked/modal queues, lock/sleep/hibernate/resume, tray loss, external display tools, timer/notification/conversion/backend failures, manual controls and deadline precedence, portable-copy startup consent, all restore/exit paths, and actual black/red-only output on supported displays. Continue preserving the existing release restrictions.
