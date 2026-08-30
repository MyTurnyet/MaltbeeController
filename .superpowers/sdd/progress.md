# Subagent-Driven Development Progress

Plan: docs/superpowers/plans/2026-08-30-esp32-presence-collision-detection.md

Started: 2026-08-30

NOTE: this file is git-tracked and carries stale content from the prior
completed plan (#2c-b2, ESP32 wireless setup composition-root wiring) in
its history. This ledger starts fresh for sub-project #2d-a (ESP32
presence + collision detection), the first half of #2d after the split.
Do not run `git checkout` on this file (would revert to stale content) or
`git stash` (bare or otherwise) at any point in this branch — lesson
carried forward from an earlier stray-stash incident this session. Every
ledger update is committed immediately, never batched across tasks.

Worktree branched from origin/main (83e6e64) via the native EnterWorktree
tool; the design spec (9bbf67c) and this plan (d212ecb) were cherry-picked
onto this branch from local main afterward, since local main was two
commits ahead of origin/main when the worktree was created.

Baseline: `pio test -e native` confirmed clean just before dispatch: 36/36
suites PASSED, 0 failed.

Pre-flight plan scan: no contradictions found between tasks or against the
plan's own Global Constraints section; no plan-mandated test asserts
nothing. Proceeding without a batched question to the user.

Task dependency order: Task 1 (PresenceTopics) and Task 2 (NodeIdentityGuard)
are mutually independent. Task 3 (MqttPresenceAnnouncer) depends on Task 1
but not Task 2. Task 4 (src/esp32/main.cpp wiring) depends on Tasks 1-3
all being complete and must run last.

Process note carried forward from #2c-b2's branch: every implementer/fix
dispatch pre-authorizes direct ACN commits (no interactive /arlo-commits
invocation, which stalls waiting for approval a subagent can never get),
and reminds dispatches that F/B commits over 8 lines cap at `!` regardless
of test coverage, and to run `pio test -e native` via Bash, not PowerShell
(a known false-failure issue on this machine, now documented in CLAUDE.md).

## Tasks
