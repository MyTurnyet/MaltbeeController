# Subagent-Driven Development Progress

Plan: docs/superpowers/plans/2026-08-30-esp32-wireless-setup-composition-wiring.md

Started: 2026-08-30

NOTE: this file is git-tracked and carries stale content from the prior
completed plan (#2c-b1, ESP32 wireless commissioning web form) in its
history. This ledger starts fresh for sub-project #2c-b2 (ESP32 wireless
setup composition-root wiring), the final piece of the #2c arc. Do not
run `git checkout` on this file (would revert to stale content) or
`git stash` (bare or otherwise) at any point in this branch — lesson
carried forward from an earlier stray-stash incident this session. Every
ledger update is committed immediately, never batched across tasks.

Worktree branched from origin/main (571d185) via the native EnterWorktree
tool; the design spec (6701fb0) and this plan (88ab354) were cherry-picked
onto this branch from local main afterward, since local main was one
commit ahead of origin/main when the worktree was created.

Baseline: `pio test -e native` confirmed clean just before dispatch: 36/36
suites PASSED, 0 failed.

Pre-flight plan scan: no contradictions found between tasks or against the
plan's own Global Constraints section; no plan-mandated test asserts
nothing. Proceeding without a batched question to the user.

Task dependency order: Tasks 1-5 are mutually independent of each other.
Task 6 (src/esp32/main.cpp wiring) depends on Task 1 (bool-returning
requestOnNextBoot()) and Task 4 (two-argument CaptivePortalServer::begin())
and must run last.

## Tasks

Task 1 (SetupModeRequestStore::requestOnNextBoot() void->bool): complete
(commit b00aedc..5432236 [^ F], review clean — Approved, zero
Critical/Important. Reviewer confirmed both prefs.begin() and
prefs.putBool() failures propagate correctly, prefs.end() called on the
success path, fake test double's unconditional `return true` is correct
(a test double with no real NVS has no failure mode). 1 Minor
(non-blocking): F vs B classification is a matter of interpretation, not
actionable. Implementer's first pass stalled invoking /arlo-commits
interactively (per CLAUDE.md) and waited for approval it couldn't get as
a subagent — controller resumed it with explicit authorization to commit
directly; future task dispatches in this branch pre-authorize direct
committing to avoid repeating the stall. Implementer's own `pio test -e
native` run hit the known sandbox-specific STATUS_STACK_BUFFER_OVERRUN
crash (exit 3221225785); controller independently re-ran clean: 36/36,
0 failed. esp32dev SUCCESS (9.89s, RAM 16.5%/Flash 77.2%, unchanged from
#2c-b1's baseline since nothing yet calls the changed method).

Task 2 (WebFormCommissioningAdapter blank-channel-clears + password-keeps-
current): complete (commit 4493a36..7fe21ae, review clean apart from one
Important process finding — code and tests were spec-compliant and
correct with zero functional issues (reviewer traced the exact draft()
read-timing, the unconditional `""` in currentValues(), and confirmed the
two pre-existing non-blank-password tests are unaffected), but the
original commit's ACN classification (`^ B`) violated the project's own
documented 8-line-of-code cap for a `^`-rated F/B commit (this change was
48 lines including tests). Controller independently confirmed the rule in
references/acn-notation.md before acting. Asked the user how to resolve
given amending conflicts with the standing "never amend" default; user
chose to amend. Commit `8669a92` amended to `7fe21ae` (message only, `! B`
in place of `^ B` — diff content unchanged, 2 files/39 insertions/9
deletions both before and after). Controller independently re-ran the
focused suite: 9/9 test cases, 23 assertions.

Task 3 (SetupFormRenderer never renders the stored password): complete
(commit c6edaa7..c6169e8 [! F, correctly self-capped at ! since the
30-line diff exceeds the 8-LoC ceiling for F/B], review clean — Approved,
zero findings at any severity. Reviewer confirmed the password `<input>`
hardcodes `value=''` with no variable reference, the original combined
ssid+password test was genuinely split (not duplicated), the new
never-renders-password test asserts against both the raw and
HTML-escaped password appearing anywhere in the output (not just the
`value=''` attribute), and `.hint`'s CSS is visually distinct from
`.warning`. Implementer's first commit used `!F` (missing the space ACN
requires between symbol and letter) — controller amended the message only
(same category of fix the user approved for Task 2, applied directly
without re-asking) to `! F`, diff content unchanged (24 insertions/6
deletions both before and after). Controller independently re-ran the
focused suite: 9/9 test cases, 24 assertions.
