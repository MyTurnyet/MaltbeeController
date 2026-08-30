# Subagent-Driven Development Progress

Plan: docs/superpowers/plans/2026-08-29-esp32-wireless-setup-trigger.md

Started: 2026-08-29

Prior plan (sub-project #7b, ESP32 composition root) is complete and merged
to main; this ledger starts fresh for sub-project #2c-a (ESP32 wireless
setup boot mode & trigger).

NOTE: this file is git-tracked and lives on a branch lineage with old,
unrelated prior-plan ledger content checked into history. A plain
`git checkout -- .superpowers/sdd/progress.md` will silently revert it to
that stale content. Do not run `git checkout` on this file for any reason;
every dispatched subagent in this branch is told so explicitly.

Baseline: worktree created from origin/main at 242964d (includes the design
spec and this plan file). `pio test -e native` confirmed clean at baseline:
all 29 suites PASSED, no failures (one pre-existing, unrelated compiler
warning in test_button about an ignored [[nodiscard]] return value).

All four tasks in this plan are mutually independent (none depends on
another's output) - ordered for reviewability, not dependency. Dispatched
sequentially (never parallel implementers, per skill red flags), each run
asynchronously/backgrounded rather than polled.

## Tasks

Task 1 (BootMode + BootModeSelector): complete (commits 242964d..6c0a867
[^ F], review clean — Approved, zero Critical/Important/Minor code
findings (one Minor note was purely informational, about NodeConfig's
pre-existing std::vector usage, not a defect of this diff). Reviewer
confirmed exact precedence order and that all 4 test cases specifically
exercise the request-wins-regardless-of-validity priority rule. 30/30
native suites pass (test_custom_runner.py is a Python helper matching the
test_ glob, not a 31st suite — resolved a report-accuracy slip from the
implementer, not a real discrepancy).
Task 2 (SetupModeRequestStore port + NvsSetupModeRequestStore + FakeSetupModeRequestStore):
complete (commits 6c0a867..5c49b93 [^ F], review clean — Approved, zero
Critical/Important. 1 Minor, informational only: this plan's brief guarded
NvsSetupModeRequestStore.h itself with #ifdef ARDUINO, stricter than
NvsConfigStore.h's actual existing convention (which leaves the header
unguarded, only guards the .cpp) - harmless, functionally correct either
way, not fixed. Reviewer independently confirmed NVS namespace/key
("mcs-boot"/"wsetup", distinct from NvsConfigStore's "mcsnode"),
read-and-clear semantics, and that no other files (NvsConfigStore,
main.cpp, Task 1 files) were touched. esp32dev SUCCESS (RAM 16.5%,
Flash 77.2%), native 30/30 (no new suite, as expected).
Task 3 (GatedDigitalInput): complete (commit 5c49b93..39d3500 [^ F], review
clean — Approved, zero findings at any severity. Reviewer confirmed no
#ifdef ARDUINO guard, no Task 1/2/main.cpp contamination, correct
suppression-overrides-forwarding semantics, and that all 3 tests exercise
real behavior against FakeDigitalInput rather than tautologies. Native
31/31 confirmed independently by the controller.
Task 4 (ComboSetupModeTrigger): complete (commit 9966429..430451d [^ F],
review clean apart from 1 Important finding — the implementation was
verified correct via the reviewer's own independent hand-trace of the
edge-triggering state machine (including a staggered-press scenario), but
none of the plan's own six specified test cases actually distinguished
"timer anchored to the later press" from "timer anchored to the first
press" — every given test pressed both buttons in the same tick. This was
a gap in the plan's test list, not an implementer deviation. Fixed by
adding one test case pinning down staggered-press timing (commit 430451d..
46002a7 [^ d], test-only, zero production code change). Fix subagent's own
test run crashed with a Windows STATUS_STACK_BUFFER_OVERRUN error across
all 32 suites in its execution environment; controller independently
re-ran the exact same commit in this worktree and got a clean 7/7 test
cases (16 assertions) on the focused suite and 32/32 on the full suite,
confirming the crash was specific to the fix subagent's own sandbox, not
a real regression. Diff independently confirmed byte-for-byte identical
to the specified test code, zero production files touched.

## Incident: a stray `git stash` wiped this file's uncommitted edits mid-branch
Between Task 2's commit and Task 3's completion notification, something
(likely a git operation inside the Task 3 implementer subagent's session,
not identified further) ran a bare `git stash` in this worktree, which
captured this file's then-uncommitted Task 1/2 updates and reset the
working tree to HEAD's stale, inherited #7b ledger content — exactly the
failure mode this file's own top-of-file warning describes, just via
`stash` instead of `checkout`. Recovered via the shared stash stack
(`git stash list --format='%H %gs'` to get a stable SHA, `git stash apply
<sha>` — never bare `pop`, since the stack is shared across worktrees/
sessions and a second, unrelated entry from `worktree-lib-target-split`
was sitting in the same stack). Content verified intact before continuing;
stash entry left in place (not dropped) until this file is committed, to
keep a recovery path if this happens again before the commit lands.
Lesson for future dispatches: commit this file's updates immediately after
each task's review, don't batch them across multiple tasks uncommitted.
