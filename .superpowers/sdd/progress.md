# Subagent-Driven Development Progress

Plan: docs/superpowers/plans/2026-09-01-esp32-boot-button-setup-trigger.md

Started: 2026-09-01

NOTE: this file is git-tracked and carries stale content from the prior
completed plan (#2d-b, ESP32 identify-blink) in its history. This ledger
starts fresh for the BOOT-button wireless setup trigger sub-project. Do
not run `git checkout`/`git stash` on this file. Every ledger update is
committed immediately, never batched across tasks.

Worktree: created via the native EnterWorktree tool (branch
`worktree-esp32-boot-button-setup-trigger`), which branches from
origin/main by default. origin/main (a64b18e) was missing this session's
two most recent local-only commits on `main` (the design spec, 7cbbc19,
and the implementation plan, 04b9c04) — fast-forward-merged local `main`
directly into the worktree branch to pick them up (04b9c04), no
cherry-pick needed since the worktree branch had no divergent commits of
its own.

Baseline: `pio test -e native` confirmed clean just before dispatch:
40/40 suites PASSED, 0 failed (one transient ERRORED on `test_button`
during a first run was traced to a concurrent `pio test` invocation
still finishing in the original (non-worktree) checkout at the same
time — re-running in isolation, and then the full suite again, both came
back clean; not a real regression).

Pre-flight plan scan: no contradictions found between tasks or against
the plan's own Global Constraints section; no plan-mandated test asserts
nothing. Proceeding without a batched question to the user.

Task dependency order: Task 1 (`ButtonSetupModeTrigger`) has no
dependencies. Task 2 (`main.cpp` wiring) depends on Task 1. Task 3
(delete `ComboSetupModeTrigger`) depends on Task 2 (nothing may still
reference it). Task 4 (docs) has no code dependency but documents the
end state, so it runs last.

## Tasks

Task 1 (ButtonSetupModeTrigger): complete (commit 7a545b4..2e78503
[^ F], review clean — Approved, zero findings at any severity. Reviewer
confirmed the public surface exactly matches the plan's global constraint
(no `isHolding()` accessor, unlike `ComboSetupModeTrigger`), the
implementation mirrors `ComboSetupModeTrigger`'s existing idiom file-for-
file, and the test file is byte-for-byte identical to the brief's
prescribed code covering all 5 cases (never-pressed, released-early,
released-at-threshold, survives-many-ticks-then-fires, re-triggerable
after a full cycle). 41/41 native suite (40 existing + 1 new).

Task 2 (main.cpp wiring): complete (commit 4c46a55..a85cfd5 [! F],
review clean — Approved, zero Critical/Important. Reviewer independently
reconciled the diff's insert/delete counts per hunk (12/6) against the
reported stat line and confirmed all 6 brief edits landed verbatim with
nothing extra, `ComboSetupModeTrigger`'s own files untouched. `pio run -e
esp32dev` SUCCESS (RAM 16.9%/Flash 81.2%), `pio test -e native` 41/41
unchanged. 1 Minor recorded, not fixed (correctly out of scope for this
task per the brief's literal-edit constraint): the include swap left
`ButtonSetupModeTrigger.h` out of the file's otherwise-alphabetical
include order (sits where `ComboSetupModeTrigger.h` used to be, between
`CaptivePortalServer.h` and `EspDeviceIdentity.h`, rather than between
`ArduinoDigitalOutput.h` and `CaptivePortalServer.h`) — flagged for the
final whole-branch review to triage.

Task 3 (remove ComboSetupModeTrigger): complete (commit e83617a..5c1d8c4
[. r], review clean — Approved, zero findings at any severity. Reviewer
confirmed the diff is a pure 3-file deletion (259 lines, 0
modifications/additions) matching exactly the required paths, native
suite count correctly dropped to 40/40, esp32dev build SUCCESS.

Task 4 (docs): complete (commit 41b275e..b88f074 [. d], review clean —
Approved, zero Critical/Important. Reviewer verified all 13 find/replace
edits landed exactly across the 4 named files (CLAUDE.md,
ESP32_Turnout_Panel_Implementation.md, HARDWARE_BRINGUP_CHECKLIST.md, the
2026-08-29 spec's superseded pointer), confirmed the historical-record
boundary was respected (old spec body untouched, no other historical
plan/spec touched), and confirmed BOOT-button/LED-flash terminology is
consistent across all three living docs. 1 Minor recorded, not fixed
(purely cosmetic Markdown line-wrap, no rendered difference): CLAUDE.md:143's
line-wrap merges an unchanged clause onto the same line as new text.

## ALL 4 TASKS COMPLETE — proceeding to the final whole-branch review.

## Final whole-branch review (opus): "Ready to merge: With fixes"

Reviewer independently traced the full BOOT-button-to-LED-flash chain
across all 4 tasks in the assembled code (bootButton GPIO read ->
ButtonSetupModeTrigger -> NvsSetupModeRequestStore -> ESP.restart() ->
BootModeSelector -> BootMode::WirelessSetup -> the LED-flash loop),
confirmed all spec Decisions and Non-goals hold, confirmed
ComboSetupModeTrigger is fully gone (repo-wide grep), and independently
re-ran both gates: `pio test -e native` 40/40 PASSED, `pio run -e
esp32dev` SUCCESS (RAM 16.9%/Flash 81.2%, unchanged). Zero Critical.

2 Important, both fixed (commit 0e64cce):
1. `docs/ESP32_Turnout_Panel_Implementation.md`'s "Pins intentionally
   avoided" table still listed GPIO0 as avoided, directly contradicting
   the wireless-setup section 90 lines earlier that documents GPIO0 as
   the trigger pin — fixed by removing GPIO0 from that list and adding a
   new "Onboard BOOT button" subsection with the pin's role and a
   power-on/EN-strapping caveat.
2. `CLAUDE.md`'s main.cpp bullet contradicted itself within one paragraph
   — an early clause said WirelessSetup mode skips "...matrix/trigger/LED/
   station machinery entirely," while a later sentence in the same
   paragraph (added by Task 4) says it does drive all 12 LedPairStations.
   Reviewer flagged this as exactly the kind of stale invariant that
   could lead a future agent to "restore" it by deleting the new LED
   loop. Fixed by removing "LED" from the skip-list and noting the
   LedPairStations as the one exception.

4 Minor, 3 fixed (commit 0e64cce), 1 recorded and left as-is:
- Fixed: CLAUDE.md:89's `ComboSetupModeTrigger` mention annotated "(since
  replaced — see below)"; HARDWARE_BRINGUP_CHECKLIST.md's LED-count
  wording ("confirm all 12 LED pairs" -> "confirm every LED pair you've
  wired") to match the earlier "wire however many turnouts" prerequisite
  change; a GPIO0/USB-auto-reset bring-up watch-item note added to
  section 2.4; identify-blink section cross-references that wireless
  setup shares its exact visual; new spec's title carries its own
  "(Sub-project #2c-c)" label for cross-reference resolvability.
- Left as-is (reviewer's own explicit disposition, not a merge blocker):
  the pre-existing cosmetic Markdown line-wrap in CLAUDE.md's
  collision-lockout sentence — re-wrapping would touch an unchanged
  clause for zero reader benefit.

2 Minor explicitly NOT fixed, reviewer's own guidance ("not a merge
blocker" / "judgment call" / "explicitly not this branch's fault"):
- No debounce/settle delay before `ESP.restart()` on BOOT release —
  theoretical contact-bounce race with GPIO0 sampling at reset, rare and
  self-recoverable (press EN), reviewer suggested checking whether the
  sibling project's shipped hardware experience already answers whether
  this bites in practice before adding complexity.
- CLAUDE.md's rooted-include tally (35) vs. a mechanical recount (37) —
  reviewer explicitly verified this drift already existed identically at
  the branch's base commit, unrelated to this branch's own changes.

Fix dispatched as ONE subagent covering all Important+batchable-Minor
findings, split into two commits per Arlo's Commit Notation (code vs.
docs): `. r Alphabetize ButtonSetupModeTrigger.h include` (e1c2c50) and
`. d Fix final-review doc findings: GPIO0 table, CLAUDE.md
self-contradiction` (0e64cce). Fix subagent independently re-ran both
gates after fixing: `pio run -e esp32dev` SUCCESS (unchanged RAM/Flash,
confirming the include reorder is behavior-neutral), `pio test -e
native` 40/40. One transient "Permission denied" during the first
`git commit` (concurrent-process object-write contention, same class
seen earlier this session) did not corrupt the commit — verified via
`git fsck` before trusting it.

## PLAN COMPLETE — all 4 tasks approved, final review's 2 Important + 6 of
8 Minor findings fixed, 2 Minor explicitly deferred per the reviewer's
own guidance. All builds/tests green. Proceeding to
finishing-a-development-branch.
