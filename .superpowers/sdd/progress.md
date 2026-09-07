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
