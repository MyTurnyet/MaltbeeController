# Subagent-Driven Development Progress

Plan: docs/superpowers/plans/2026-09-06-esp32-presence-heartbeat.md

Started: 2026-09-06

NOTE: this file is git-tracked and carries stale content from the prior
completed plan (sub-project #9a, ESP32 Loco2MQTT turnout bridge) in its
history. This ledger starts fresh for sub-project #9b (presence/collision-
detection heartbeat). Do not run `git checkout`/`git stash` on this file.
Every ledger update is committed immediately, never batched across tasks.

Worktree: created manually via `git worktree add` (branch
`esp32-presence-heartbeat`), branching from local `main`'s actual HEAD
(99dd737) rather than the native `EnterWorktree` tool's default (branches
from `origin/main`) — origin/main was 2 commits behind local main at the
time (the #9b design spec and implementation plan docs, not yet pushed).

Baseline: `pio test -e native` confirmed clean just before dispatch —
42/42 suites PASSED, 0 failed.

Pre-flight plan scan: no contradictions found between the 2 tasks or
against the plan's own Global Constraints section. Proceeding without a
batched question to the user.

Task dependency order: Task 1 (`MqttPresenceAnnouncer` heartbeat) has no
dependencies — nothing else in the native build references it. Task 2
(`main.cpp` wiring) depends on Task 1's new constructor signature, and is
not part of the native build at all (verified only via `pio run -e
esp32dev`).

## Tasks

Task 1 (MqttPresenceAnnouncer heartbeat): complete (commit
04f8e91..a65d896 [^ F], review clean — Approved, zero findings. Reviewer
confirmed the heartbeat calculation, the timer reset on every publish
(edge and periodic alike), the `retained=false` change on both topics,
the new `Clock&` constructor parameter, and the rooted `"ports/Clock.h"`
include (correct — Clock lives in McsCore). Confirmed NodeIdentityGuard/
PresenceTopics and their tests are completely untouched. 42/42 native
suite, independently re-verified by the controller.

Task 2 (main.cpp wiring — unique client ID + heartbeat construction):
complete (commit 9890edf..cb86b25 [! F], review clean on the SECOND
attempt — Approved, zero findings. First review attempt returned a false
Critical ("constructor signature mismatch, missing Clock& parameter")
that the controller disproved immediately by reading the actual current
`MqttPresenceAnnouncer.h` directly (confirmed the 4-param constructor
with `Clock&` genuinely is there) and by having already independently
re-run `pio run -e esp32dev` twice with BUILD SUCCESS on this exact
commit — a real signature mismatch would have failed that build, so the
first reviewer's claim was self-evidently a misread, not a real defect.
Re-dispatched a fresh reviewer with an explicit instruction to read the
real file directly rather than trust either party's claim; it did so and
confirmed the correct 4-parameter signature, approved cleanly. Both
`pio run -e esp32dev` (BUILD SUCCESS, RAM 17.3%/Flash 83.8%) and
`pio test -e native` (42/42) independently re-verified by the controller
before dispatching either review.

## ALL 2 TASKS COMPLETE — proceeding to the final whole-branch review.

## Final whole-branch review (opus): "Ready to merge: With fixes"

Reviewer independently verified the cross-task seam (MqttPresenceAnnouncer's
4-param constructor vs. main.cpp's call site — confirming the earlier
false-positive from Task 2's first review was indeed wrong), traced the
29999ms/30000ms boundary and wraparound-safety by compiling and running a
standalone probe of the real logic, confirmed both Q&A-scope decisions
(heartbeat both status+mac, 30000ms exactly) were genuinely delivered,
confirmed disconnect-mid-cycle correctly re-triggers an immediate edge
publish on reconnect, grepped the whole repo for stale dependencies on the
old nodeId-derived clientId format (none found outside historical spec/plan
docs, which correctly describe the past), confirmed NodeIdentityGuard/
PresenceTopics and their tests genuinely untouched, and confirmed the
design's full reasoning chain (unique clientId → simultaneous connection →
heartbeat observation → NodeIdentityGuard latch) holds end-to-end by
checking that `presenceAnnouncer.update()` runs unconditionally every tick
outside the configValid gate. Both gates re-verified independently at true
HEAD: `pio test -e native` 42/42, `pio run -e esp32dev` BUILD SUCCESS
(RAM 17.3%/Flash 83.8%, matching every prior checkpoint).

**2 Important, both fixed:**
1. The timer-reset line in `announce()` (`lastAnnouncedAtMs_ = ...`) had
   zero test coverage — reviewer proved this empirically (compiled the
   logic with that line removed, all 5 existing tests still passed while
   producing an unbounded publish flood past t=30s). Controller
   independently re-verified this exact claim before accepting it: removed
   the line, watched the new test fail with `8 == 4` (published messages),
   reverted. Fixed (commit 407805e) — one new test case
   ("a heartbeat publish rearms the interval rather than firing every
   tick").
2. `CLAUDE.md` still described the retained-message design in the present
   tense, including a line that read as a direct prohibition on the change
   this branch made ("do not simplify it to a plain publish"). Same class
   of gap as #9a hit (neither plan had a documentation task — flagged by
   the reviewer as a recurring process gap worth fixing at the
   writing-plans level, not just per-branch). Fixed (commit 0bd7a84) — 4
   targeted edits plus a full rewrite of the "Presence + collision
   detection" section explaining the #9a→#9b causal chain (why retained
   broke, why unique clientId + heartbeat fixes it, explicit "do not
   revert" guidance, and the new ≤30s detection-latency tradeoff).
   `docs/HARDWARE_BRINGUP_CHECKLIST.md` §2.2/§2.5 also fixed (commit
   0605f66) — the old checklist would have led an operator to expect
   near-instant collision detection and conclude the feature was broken
   when it took up to 30s instead.

**5 Minor recorded, none acted on** (none rose to a severity or clarity
level warranting action): a now-inapplicable-but-harmless limitation note
in a separate doc; a theoretical status-topic flap during an already-
erroneous collision state (confirmed no functional impact — nothing
subscribes to the status topic); residual non-zero collision probability
from using only 4 MAC hex digits for the clientId (consistent with
existing codebase convention, net improvement over the guaranteed
collision the old design had); and the reviewer's own explicit opinion
that extracting the clientId construction into a testable helper (mirroring
`SetupApName`) would be gold-plating for this branch, since the logic is a
total function with no branch to test.

Both gates re-verified by the controller after the fix pass (native 42/42,
`esp32dev` SUCCESS at unchanged RAM/Flash) — plus an independent empirical
re-verification of the new test's actual regression-catching power (probe:
removed the timer-reset line, watched the new test fail correctly, reverted).

## SUB-PROJECT #9b COMPLETE — proceeding to finishing-a-development-branch.
