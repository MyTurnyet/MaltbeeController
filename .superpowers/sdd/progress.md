# Subagent-Driven Development Progress

Plan: docs/superpowers/plans/2026-08-29-esp32-toggle-turnout-control.md

Started: 2026-08-29

Prior plan (sub-project #5, ESP32 hardware adapters for #3/#4) is complete
and merged to main; this ledger starts fresh for sub-project #7a (ESP32
single-button toggle turnout control).

NOTE: this file is git-tracked and lives on a branch lineage with old,
unrelated prior-plan ledger content checked into history. A plain
`git checkout -- .superpowers/sdd/progress.md` will silently revert it to
that stale content. Do not run `git checkout` on this file for any reason;
every dispatched subagent in this branch is told so explicitly.

Baseline: worktree created from origin/main at 0621cd2 (includes the
design spec and this plan file). `pio test -e native` confirmed clean at
baseline: all 27 suites PASSED, no failures.

## Tasks

Task 1: complete (commit 0621cd2..554c38d [^ F], review clean — Approved,
zero Critical/Important findings. Implementation is a byte-for-byte match
of the brief's code (constructor, update(), applyFeedback()). The
implementer's own self-review caught a genuine bug in the brief's literal
test code for "Repeated presses...": releasing the button with a single
update() call (no clock advance) never lets the release debounce, so the
second press's wasPressed() never re-fires and the original assertion
(sentCommands.size()==2) would fail. Implementer added the missing
release-debounce cycle. Controller independently hand-traced Button's
debounce state machine against both versions and confirmed the bug and
fix are both real; task reviewer independently re-traced the same logic
from scratch (not trusting either the implementer's or controller's
claim) and reached the identical conclusion. 1 Minor, non-blocking,
plan-mandated (brief specifies this exact code): applyFeedback() is a
byte-for-byte duplicate of TurnoutControl::applyFeedback(), a deliberate
design decision from the spec (avoids touching hardware-verified
TurnoutControl), flagged only as a future extraction candidate once a
third caller of the same pattern appears.

Controller independently verified before final review: native 28/28
PASSED (27 existing + this new suite), esp32dev SUCCESS (RAM 6.4%/Flash
17.8%, unchanged from sub-project #5's build since ToggleTurnoutControl
isn't referenced by src/esp32/main.cpp yet — dead-stripped, as expected
for code sub-project #7b will wire in), megaatmega2560 SUCCESS (unchanged).

## Only task complete — proceeding to final whole-branch review.

## Final whole-branch review (sonnet): "Ready to merge: Yes"
Zero Critical, zero Important. Reviewer independently re-traced the
debounce-fix claim a third time from scratch (matching controller's and
task reviewer's own independent traces) and confirmed it's a correct,
minimally-scoped test-only fix, not a spec violation. Confirmed via
platformio.ini inspection (not re-trusted claims) why esp32dev/mega
builds are unaffected: McsEsp32 is lib_deps for both native and esp32dev,
but src/esp32/main.cpp's build_src_filter never references
ToggleTurnoutControl yet (dead-stripped), and McsEsp32 is lib_ignore'd
entirely for megaatmega2560. 2 Minor, both forward-looking notes for
sub-project #7b rather than blockers for this branch: (1) applyFeedback()
duplication with TurnoutControl is spec-mandated and reasoned soundly,
but a future fix to one won't automatically propagate to the other —
recommended a cross-reference comment (optional, not required); (2) the
pre-existing lack of isLocked()/isDisabled() checks (parity with
TurnoutControl) becomes more user-visible on a single-button panel where
#7b should explicitly re-decide whether to close this gap, not silently
inherit it.

## PLAN COMPLETE — the one task + final review both clean.
All 3 environments (native 28/28, esp32dev, megaatmega2560) confirmed
green as of commit 554c38d. No fix pass needed — nothing to fix.
