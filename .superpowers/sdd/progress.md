# Subagent-Driven Development Progress

Plan: docs/superpowers/plans/2026-08-29-jmri-turnout-control-wiring.md

Started: 2026-08-29

Prior plans (2a NodeConfig/commissioning, 2b JMRI/MQTT transport, lib target
split) are all complete and merged to main; this ledger starts fresh for
sub-project #6 (JMRI turnout command/feedback wiring).

Baseline: worktree created from origin/main at 88e1dec (includes the
2026-08-29 self-echo design spec and this plan file). `pio test -e native`
confirmed clean at baseline: all 22 suites PASSED, no failures.

## Tasks

Task 1: complete (commit 5b93bb1..3a3ff05 [^ B], review clean — Approved, zero findings. Reviewer independently verified the new regression test genuinely discriminates: traced FakeMqttTransport's exact-match deliver() semantics by hand to confirm the pre-fix subscription (topicFor) would have matched the plain command-topic delivery and made poll() wrongly return true, while post-fix (stateTopicFor) it correctly returns false. All 4 touched files matched the brief's named scope exactly; no other files changed.)
Task 2: complete (commit 3a3ff05..102825e [. r], review clean — Approved. Reviewer independently verified every constructor/API the test calls against actual production source (TurnoutControl.cpp, Button.cpp, Turnout.cpp, TurnoutIndicator.cpp, JmriFeedbackSource.cpp, JmriTurnoutCommandAdapter.cpp) and confirmed the first test case derives the echoed topic/payload dynamically from transport.published[0] rather than a hardcoded guess, genuinely proving self-echo immunity would have failed pre-Task-1. 2 Minor-only findings, not blocking: (1) ~10-line setup duplication between the two test cases, not yet costly at 2 cases; (2) test case 2's closeButton/closeInput declared but unused beyond satisfying TurnoutControl's constructor, matches brief's own prescribed code.)

## Both tasks complete — proceeding to final whole-branch review.

## Final whole-branch review (Opus): "Ready to merge: Yes"
Verified independently: 23/23 native suites, esp32dev SUCCESS, megaatmega2560
SUCCESS. Zero Critical findings. 2 Important findings, both documentation-
accuracy gaps (not code defects) — reviewer independently checked the
sibling ../MaltbeeTurnoutController repo directly and found the cross-project
dependency section understated the real precondition list (also needs
name-vs-id keying agreement, the documented /trains prefix reconciled, and
#7's composition root), and that the self-echo spec resolved consequence (a)
self-confirmation from 2b's amendment but left consequence (b) retained-slot
contention unmentioned. 4 Minor findings (stale CLAUDE.md suite count,
jmriName character validation deferred as out of scope, stateTopicFor("")
test symmetry gap, missing free assertion + test hygiene in the integration
test) plus one pre-existing tooling quirk noted as out of scope (Catch2 v3
console-format parsing).

Dispatched one fix subagent per skill process (commits bbcc021 doc-only +
ed514e3 test additions) covering both Important findings plus 3 of the 4
Minor findings (skipped jmriName validation and the pre-existing tooling
quirk, both explicitly out of scope per the dispatch). NOTE: the fix
subagent got stuck in a bad pattern — it correctly made all file edits, but
each time it launched a `pio test -e native` in the background to verify,
its own turn ended without ever receiving the completion notification
(subagents don't appear to receive background-task notifications the way
the top-level session does), so it reported "still waiting" repeatedly
across what the orchestration layer saw as multiple separate completed
invocations. Each of those stray background pio processes was still running
concurrently with the controller's own foreground test runs, corrupting the
shared .pio/build/native output (Windows STATUS_ACCESS_VIOLATION and
"WinError 193: not a valid Win32 application" on shared binaries, then a
shifting set of ERRORED suites on each subsequent run — classic concurrent-
build corruption, not real regressions, confirmed by the failing suite set
changing between runs and including suites this branch never touched).
Controller killed the stray pio processes (PIDs 867, 1058) by hand, tried
TaskStop on the agent id (not recognized — likely a subagent-internal
background task not exposed to the parent session's task table), then did
a full `rm -rf .pio/build/native` clean rebuild once no stray processes
remained. Clean rebuild: all 23 suites PASSED. Controller then verified
all five changed files' content directly (both spec edits, CLAUDE.md, both
test additions) against the fix dispatch's exact instructions before
committing them itself in the two commits the dispatch specified, since the
subagent's actual content work was correct and complete — only the
verify-and-commit tail end was never reached.

Controller re-verified esp32dev and megaatmega2560 after the fix commits
too (not just native): both SUCCESS, memory usage identical to every prior
baseline in this plan (esp32dev RAM 6.4%/Flash 17.8%, megaatmega2560 RAM
9.9%/Flash 2.7%).

## PLAN COMPLETE — both tasks + final review + fix-and-verify done, all 3 environments (native 23/23, esp32dev, megaatmega2560) confirmed green after the fix commits.
