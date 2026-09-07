# Subagent-Driven Development Progress

Plan: docs/superpowers/plans/2026-09-06-esp32-jmri-decommission.md

Started: 2026-09-06

NOTE: this file is git-tracked and carries stale content from the prior
completed plan (sub-project #9b, ESP32 presence heartbeat) in its history.
This ledger starts fresh for sub-project #9c (JMRI bridge decommissioning).
Do not run `git checkout`/`git stash` on this file. Every ledger update is
committed immediately, never batched across tasks.

Worktree: created manually via `git worktree add` (branch
`esp32-jmri-decommission`), branching from local `main`'s actual HEAD
(b6e8c26) rather than the native `EnterWorktree` tool's default (branches
from `origin/main`) — origin/main was 2 commits behind local main at the
time (the #9c design spec and implementation plan docs, not yet pushed).

Baseline: `pio test -e native` confirmed clean just before dispatch —
42/42 suites PASSED, 0 failed.

Pre-flight plan scan: no contradictions found between the 3 tasks or
against the plan's own Global Constraints section. This plan is docs-only
(no lib/src/test changes), so there is no compile-coupling risk of the
kind that hit sub-project #9a — every task is independently verifiable.
Proceeding without a batched question to the user.

Task dependency order: Task 1 (delete jmri script + update CLAUDE.md) has
no dependencies. Task 2 (rewrite docs/ESP32_Turnout_Panel_Implementation.md)
and Task 3 (rewrite docs/HARDWARE_BRINGUP_CHECKLIST.md) both depend only on
Task 1 and are otherwise independent of each other.

## Tasks

Task 1 (delete jmri script + update CLAUDE.md): complete (commit
78cc8a4..2d893e4 [. d], review clean — Approved, zero findings. Reviewer
confirmed exact textual match to the brief for both CLAUDE.md edits,
confirmed the script deletion, and confirmed no lib/src/test file appears
anywhere in the diff. Controller independently verified: `jmri/` directory
fully gone, both CLAUDE.md passages read correctly in past tense.

Task 2 (rewrite docs/ESP32_Turnout_Panel_Implementation.md): complete
(commit 42dd00f..b8b6a05 [. d], review clean — Approved, zero findings.
Reviewer confirmed all 12 labeled edits (A-L) byte-for-byte match the
brief, confirmed only the one permitted file was touched, confirmed
wiring/GPIO tables and "Suggested milestones" genuinely untouched, and
independently verified the implementer's self-reported bonus fix (a
State Model cross-reference broken by Edit H's section rename) is real,
correctly scoped to exactly one line-pair, and necessary. 1 Minor noted
(reviewer didn't independently re-run the Step 13 verification grep,
per its own no-command-re-run instructions) — resolved: controller
already ran that exact grep directly against this checkout earlier and
confirmed the same result.

Task 3 (rewrite docs/HARDWARE_BRINGUP_CHECKLIST.md + final sanity check):
complete (commits 05f9631..e8dd3ad, 2 commits [. d, . d], review clean —
Approved, zero findings. Implementer applied all 3 edits, ran both build
gates and both grep sweeps, but correctly stopped short of fixing a
4th stale line (§2.7's "matching JMRI") since it was outside the brief's
3 listed edits, reporting DONE_WITH_CONCERNS rather than silently
expanding scope or silently leaving it. Controller independently
re-verified all of the implementer's claims directly against the
checkout (build gates unchanged: native 42/42, esp32dev SUCCESS at
17.3%/83.8%; grep sweep — confirmed the two Part-1 "JMRI" mentions the
implementer flagged are generic LocoNet-monitoring-tool examples,
correctly out of scope) and fixed the §2.7 line directly in a small
follow-up commit. Reviewer confirmed the follow-up fix is narrowly
scoped and matches the established rewrite pattern.

## ALL 3 TASKS COMPLETE — proceeding to the final whole-branch review.
