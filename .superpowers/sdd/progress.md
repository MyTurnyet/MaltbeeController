# Subagent-Driven Development Progress

Plan: docs/superpowers/plans/2026-08-29-esp32-matrix-button-scan.md

Started: 2026-08-29

Prior plan (sub-project #6, JMRI turnout command/feedback wiring) is
complete and merged to main; this ledger starts fresh for sub-project #3
(ESP32 matrix button scan).

NOTE: this file is git-tracked. A plain `git checkout -- .superpowers/sdd/progress.md`
in this worktree will silently revert it to an old, unrelated plan's stale
content (checked-in history from #6's own ledger on this branch's lineage)
— always diff before trusting a restored copy of this file. (Confirmed
happening once already in this very run: Task 1's implementer accidentally
reverted this file to #6's old ledger content while trying to undo its own
out-of-scope edit; recovered by rewriting from git log + the implementer's
actual report.)

Baseline: worktree created from origin/main at 4ffbbbb (includes the
design spec and this plan file). `pio test -e native` confirmed clean at
baseline: all 23 suites PASSED, no failures.

## Tasks

Task 1: complete (commit 4ffbbbb..51b251e [! F], review clean — Approved,
zero Critical/Important findings. Implementer initially stalled mid-task
after reading CLAUDE.md's "always use /arlo-commits" instruction and
invoking that skill's interactive confirmation gate, which a non-
interactive subagent can never get an answer to — resumed with an explicit
instruction to run the exact commit command from its own brief instead;
it also briefly touched this ledger file out of scope and then, trying to
undo that, ran a plain `git checkout` on it that reverted it to #6's old
stale ledger content (exactly the trap this file's own note warns about) —
recovered by rewriting from git log + the implementer's actual report.
Reviewer independently hand-traced update()'s row-cycling logic across all
4 call scenarios (first call, cycling, wraparound) and confirmed the
electrical-safety invariant genuinely holds (exactly one row asserted at
any point after the first call), not just that tests happen to pass; also
confirmed no forbidden files were touched and no other file yet references
MatrixScanner (correctly deferred to a later task). 2 Minor, non-blocking,
explicitly plan-mandated (identical to the brief's own prescribed code, not
implementer deviations): no bounds-checking on isActive(row,col), and no
explicit/doc-comment on the constructor.
Task 2: complete (commit 51b251e..6c2e8d1 [! F], review clean — Approved,
zero Critical/Important findings. Implementer completed cleanly this time
with no ledger interference (explicit warning in the dispatch worked).
Reviewer independently verified MatrixDigitalInput::isActive() is a genuine
pure one-line forward with no added logic, cross-checked parameter order
against MatrixScanner's actual signature (outside the diff), and confirmed
both tests exercise real scanner row-cycling/caching behavior rather than
hardcoded returns (test 1 requires 2 update() calls to reach row 1 before
its assertion means anything; test 2 proves column-cell independence within
one shared row-scan). 1 Minor, non-blocking: neither test exercises
row-axis independence (only column-axis), not required by the brief.

## Both tasks complete — proceeding to final whole-branch review.

## Final whole-branch review (Opus): "Ready to merge: With fixes"
Verified independently: 25/25 native suites, esp32dev SUCCESS (both new
.cpp.o files present), megaatmega2560 SUCCESS. Zero Critical findings.
2 Important: (1) design spec documented row electrical polarity for #7 but
not the symmetric (and more dangerous, since it also involves
useInternalPullup) column polarity trap on the 4 ArduinoDigitalInputs;
(2) MatrixDigitalInput's test 1 set col2.active=true before either
update() call, so an adapter that ignored its own row_ (hardcoded row 0)
would still have passed — vacuous w.r.t. the row dimension. 5 Minor:
residual same-column-different-row short risk undocumented, no-settle-
delay rationale compared the wrong two quantities (loop cadence vs. RC
recovery time), CLAUDE.md stale (missing new classes, wrong suite count),
un-bounds-checked isActive() (explicitly optional per reviewer, not
requested), domain/ vs adapters/ placement inconsistency (reviewer said
leave it), one-column indentation slip, and the ledger being uncommitted
(controller's own job).

Dispatched one fix subagent per skill process (commits a2fd9b6 doc-only +
11d9a95 test+cosmetic) covering both Important findings plus 3 of the 5
Minor findings (skipped the explicitly-optional bounds-check and the
explicitly-leave-it placement question). Clean execution this time — no
ledger interference (the explicit warning in the dispatch worked). Controller
independently verified before re-review: (a) the "cosmetic alignment fix"
is actually correct — counted characters, `MatrixScanner::MatrixScanner(`
is exactly 29 chars and the fixed continuation line has exactly 29 leading
spaces, matching; (b) re-ran `pio test -e native -f test_matrix_digital_input`
directly, 2 cases/4 assertions pass; (c) read both doc diffs in full for
technical accuracy before dispatching re-review.

## Re-review of fix commits (sonnet, commits 6c2e8d1..11d9a95): "Ready to merge: Yes"
All 6 findings verified fixed accurately and completely — reviewer
independently traced the test fix against MatrixScanner::update()'s actual
logic to confirm a row-ignoring adapter bug would now fail, cross-checked
the ArduinoDigitalInput constructor claims against the real header file,
and independently counted test_* directories to confirm the CLAUDE.md
suite-count fix (25) is correct. Zero Critical, zero Important. 1 Minor
(non-gating, notation-only): commit 11d9a95 bundles a genuine test-behavior
fix (`^ B`, correct) with a purely cosmetic whitespace fix that by strict
ACN should be its own `. r` commit — noted but not worth a separate
amend/split for a trivial one-liner, matching this project's own precedent
(7e334a7) for grouping small fix-pass items under one commit.

## PLAN COMPLETE — both tasks + final review + fix pass + re-review all
returned Ready to merge: Yes. All 3 environments (native 25/25, esp32dev,
megaatmega2560) confirmed green.
