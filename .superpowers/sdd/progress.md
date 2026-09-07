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

## Final whole-branch review (opus): "Ready to merge: With fixes"

Reviewer diffed all 17 planned edits against the plan text (byte-for-byte
match, zero deviations), independently re-verified both gates at true
HEAD (native 42/42, esp32dev SUCCESS at unchanged 17.3%/83.8%), confirmed
`jmri/panel_mqtt_turnout_bridge.py` is genuinely gone with no dangling
reference anywhere in the repo (checked for CI/build-script/README links,
not just the 4 files this plan named), and spot-checked the rewritten
"Loco2MQTT Communication (MQTT)" section's concrete technical claims
against the real code (TopicScheme's topic format, NodeConfig's actual
field name, etc.) — all accurate.

**1 Important, fixed (scope-expanded per user decision):** `README.md`
still called the ESP32 panel "in planning" and described its transport as
"Wi-Fi to JMRI" — invisible to every grep this plan ran, since those greps
were scoped to the 4 files the plan already intended to touch (a
methodology gap the reviewer explicitly named: for a "decommission X
everywhere" sub-project, the grep should run repo-wide FIRST and the file
list should be derived from its output, not the reverse). Controller
verified the claim directly, found the whole "Current Status" section
was stale (not just the JMRI wording — Mega milestone status and ESP32
"planned" framing both predated the ESP32 work actually shipping). Asked
the user how far to go; chose a full refresh over a narrow word-swap.
Fixed (commit 3eece90) — Current Status, Requirements, Building/Testing
commands, Architecture, and Next Steps sections all brought current.

**2 more findings, real but not JMRI-text issues at all (leftover
#9a/#9b inconsistencies the review only surfaced because it read the
assembled docs) — user chose to fix now rather than defer:**
1. `docs/ESP32_Turnout_Panel_Implementation.md`'s "Multi-Panel Presence"
   section still described #2d-a's original retained-message collision
   model, directly contradicting `CLAUDE.md`'s actual #9b heartbeat
   design (which explicitly says "do not revert to retained"). Rewritten
   to match, including the "why not retained" causal chain and correctly
   noting the old "stale retained MAC" limitation can no longer occur at
   all against Loco2MQTT (not just mitigated).
2. `docs/HARDWARE_BRINGUP_CHECKLIST.md` §2.4 still said "channel names"
   where the panel has used numeric addresses since #9a.

**3 Minor, all fixed:** `CLAUDE.md:80`'s one remaining "JMRI" word inside
a current-state description (buried in ~4000 characters of unbroken
prose, missed by the original #9c plan's scoped greps); `CLAUDE.md`'s
git-history pointer for the deleted script named the wrong file
(pointed at the doc's history instead of the script's own); a dangling
cross-reference inside the frozen "Suggested milestones" section (left
the frozen historical text itself untouched per the plan's own
constraint, added one italic note above it explaining the section/class
names are pre-implementation originals).

**Repo-wide re-verification after all fixes:** grepped every `.md`/`.py`/
`.cpp`/`.h`/`.ini` file in the repo for "JMRI" (excluding the historical
`docs/superpowers/specs|plans/` directories) — remaining hits are
`internal_documents/MaltBee_Control_System_Architecture_and_Roadmap.md`
(JMRI mentioned generically as "another controller" on the shared
LocoNet bus, Mega-roadmap-specific, unrelated to this sub-project) and
`internal_documents/archive/original-overview.md` (explicitly archived
historical doc) — both independently confirmed correctly out of scope by
reading their surrounding context, not just the grep hit. Both gates
re-verified unchanged after the full fix pass (native 42/42, esp32dev
SUCCESS at 17.3%/83.8%).

## SUB-PROJECT #9c COMPLETE — proceeding to finishing-a-development-branch.
