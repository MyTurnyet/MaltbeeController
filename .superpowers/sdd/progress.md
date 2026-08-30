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
(Applied for the remainder of this branch: every ledger update from here
on was committed in the same message as the finding it records.)

## All 4 tasks complete — proceeded to final whole-branch review.

## Final whole-branch review (opus): "Ready to merge: With fixes"
Reviewer independently ran pio test -e native (32/32) and pio run -e
esp32dev (SUCCESS, RAM 16.5%/Flash 77.2% - byte-identical to pre-branch,
confirming zero runtime footprint since nothing references these classes
yet), confirmed zero hardware-verified files (Button/MatrixScanner/
MatrixDigitalInput/ToggleTurnoutControl/ToggleTurnoutStation) touched,
confirmed the two seams #2c-b will need (SetupModeRequestStore::
consumeRequest() -> BootModeSelector::select()'s bool param; GatedDigitalInput
slotting into ToggleTurnoutStation's DigitalInput& button param unmodified)
actually compose, confirmed unsigned-long millis() wraparound arithmetic is
correct, confirmed NVS namespaces ("mcsnode" vs "mcs-boot") are genuinely
disjoint. Zero Critical.

1 Important: a FOURTH test-coverage gap of the same shape as the
already-fixed staggered-press one - none of the (then-)7 test cases called
update() more than once while a hold was already in progress, so the
`&& !holding_` re-stamp guard (ComboSetupModeTrigger.cpp:14) was completely
unexercised. Reviewer traced that removing it would still pass all 7
existing tests while silently disabling the entire feature on real
hardware (loop() runs at kHz, so holdStartMs_ would re-stamp every tick
and heldFor would never accumulate). Controller independently confirmed
both halves of this claim before dispatching a fix: read the actual
guard condition, and grepped every trigger.update() call site in the test
file to confirm none repeats mid-hold. Fixed by adding one test case
(commit 46002a7..528518b [^ d], test-only, zero production code change) -
this fix subagent's own run also hit the Windows STATUS_STACK_BUFFER_OVERRUN
environment crash seen once before in this branch, but reported it
honestly rather than fabricating a pass; controller independently
re-ran the exact commit cleanly (8/8 test cases, 37 assertions).

1 Important, not fixed in code (a spec/design gap, not an implementation
defect): GatedDigitalInput's suppression only engages once
ComboSetupModeTrigger::isHolding() goes true, which by design only happens
at the LATER of the two combo presses - so if a human's two-finger press
staggers by more than Button's 30ms debounce (routine), the EARLIER-pressed
button's own wasPressed() edge fires (and sends a live toggle command to
JMRI) before suppression ever engages. Every one of the four classes
implements its own stated contract correctly; this is a gap in how the
spec described their composition, only visible once you look at all four
together - exactly what a whole-branch review is for. Recorded in the
design spec's new "Known gap for #2c-b to resolve" section rather than
fixed here, since resolving it (accept one spurious command / command-on-
release / a short hold-off) is a real design decision #2c-b must make
explicitly, not something to bake into this branch silently.

4 Minor: (1) NvsSetupModeRequestStore.h guarded stricter than
NvsConfigStore.h's own convention - already known from Task 2's review,
not re-litigated. (2) requestOnNextBoot()'s void return silently swallows
an NVS begin() failure - flagged as worth reconsidering before #2c-b
becomes the first consumer, not changed here (would ripple the port
signature and the fake). (3) ComboSetupModeTrigger sits in adapters/ but
is arguably pure domain logic by this project's own LedPairDriver
precedent - spec's choice, not an implementer deviation, not moved
(reviewer's own judgment: would churn an include for no functional gain).
(4) FakeSetupModeRequestStore.h isn't yet referenced by any test binary -
reviewer closed this themselves by compiling it standalone and confirming
it's correct; informational only.

Controller handled CLAUDE.md and the design spec's new gap section
directly (commit 38e7487), separately from any fix subagent, mirroring
the established pattern from #7b. While updating CLAUDE.md, discovered
and fixed a THIRD, independent doc-drift issue: the "Include convention"
paragraph's rooted-include count (19) had been stale since sub-project
#7a (never updated for ToggleTurnoutControl.h's or ToggleTurnoutStation.h's
own rooted includes) - reconciled via a fresh grep recount to the true
current value (34).

## PLAN COMPLETE (with two review-driven test-coverage fixes, zero
production code changes) — all builds/tests green.
Final state: commits 242964d..38e7487 (6c0a867 Task1, 5c49b93 Task2,
39d3500 Task3, 430451d Task4, 46002a7 fix1, 528518b fix2), plus controller
doc commits for CLAUDE.md and the design spec's gap section, plus three
ledger-recording commits (60a469d, 9966429, 808aaa3). Proceeding to
finishing-a-development-branch.
