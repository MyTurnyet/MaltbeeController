# Subagent-Driven Development Progress

Plan: docs/superpowers/plans/2026-10-22-esp32-node4-turnout-station-wiring.md

Started: 2026-10-22

Ledger reset for sub-project #4 (turnout station wiring). Previous plan
(sub-project #3, composite setup trigger) is complete and merged into
main's history.

Workspace: working directly on main per the user's earlier explicit
confirmation for this same overall effort.

Pre-flight plan scan: 2 tasks. Task 1 is a new composition-helper class
with zero logic of its own (mirrors LedPairStation's own no-test
precedent) - verified by build only. Task 2 is a main.cpp composition-root
rewrite with no native test (consistent with every prior main.cpp change
in this project) - verified by build only. No contradictions against the
plan's Global Constraints. Proceeding without a batched question to the
user.

## Tasks

(none started yet)

Task 1 (IndependentLedPairStation): complete (commit 7511e4f..2c8a65d
[^ F], review clean — Approved, zero findings. Reviewer independently
cross-checked the IndependentLedPairDriver constructor call's green-
before-red argument order against that class's real unmodified header,
confirmed the structure mirrors LedPairStation 1:1, confirmed member/
initializer-list order consistency, and confirmed only the 2 planned
files were touched. Controller independently verified both files on disk
matched the brief before approving the commit. All 3 builds + full
native suite (51/51) per implementer report.

Task 2 (Wire 4 real turnout stations into src/esp32node4/main.cpp):
complete (commit d1f970f..81f3a41 [@ F], review clean — Approved, zero
findings. Reviewer independently cross-checked all 4 GPIO triples against
the hardware pin map (no transpositions), confirmed green=closedOutput/
red=thrownOutput for all 4 stations, confirmed control-flow placement
(begin before boot-mode check, update gated out of WirelessSetup by the
existing early return), confirmed purely additive (0 deletions), and
confirmed the @ F risk symbol correctly matches established repo
convention this time. All 3 builds + full native suite (51/51) per
implementer report.

ALL 2 TASKS COMPLETE. Proceeding to final full-suite verification and the
final whole-branch review.

## Final whole-branch review (opus): "Ready to merge: With fixes" (docs only, code was correct)

Reviewer found the code itself fully correct and plan-exact (independently
re-verified all 12 GPIO numbers against the hardware spec, confirmed zero
modification to any protected file via git diff --name-only, traced the
full button-to-LED path, and caught a genuine non-obvious hazard already
handled correctly: ArduinoDigitalOutput::begin() discards the constructor's
initial write, so LED-GPIO begin() must precede driver begin() and
ledStation.begin() must precede station.begin()'s clearIndicator() - both
already in the right order).

1 Important finding (the headline one): the design spec's claim that this
sub-project "proves the button-to-LED mechanics work on real hardware" is
FALSE. Controller independently verified by reading ToggleTurnoutControl.cpp
directly: update() only sends a command, never mutates Turnout or calls
indicator_.display() - the LED only changes via applyFeedback(), which
nothing calls with a no-op command port and no feedback source. A button
press today produces literally zero observable effect. Fixed: corrected
the design spec's claim, added a full "Turnout station wiring (sub-project
#4)" CLAUDE.md section plainly stating this as a "Known, accepted gap"
(what bring-up CAN verify: LED GPIO wiring + 500ms blink on all 4 channels;
CANNOT verify: anything about button response, until real Loco2MQTT wiring
exists).

1 Important finding (doc gap, same recurring pattern): CLAUDE.md was never
updated for sub-project #4, and 3 passages (sub-project #1/#2/#3 sections)
now asserted things that were false (no composition helper/button wiring -
both now exist). Fixed: corrected all 3, re-grepped the rooted-include
tally (44, not a naive 41+3=44 guess - verified independently by controller
too, confirms the fix agent's math), fixed the "only rooted adapter include"
claim to name both files, checked off PCB spec §7 items 2-3.

1 Minor finding -> user decision: all 4 LEDs sit solid red during
BootMode::WirelessSetup (begin() runs before the boot-mode check, loop()
returns early before ever reaching update()). User chose: document only,
don't reorder (reordering risks leaving LED GPIOs floating during setup
mode). Documented as a "Known, accepted gap" pointing at whichever future
sub-project owns boot-mode LED feedback.

2 Minor findings -> user approved as code additions (not required fixes,
new-code safety hardening): deleted copy/move on IndependentLedPairStation
(self-referential, a copy would dangle) and designated initializers for
IndependentLedPairConfig (closes a silent-transposition footgun). Both
implemented, verified by all 4 build/test commands, coded "^ r" not "^ F"
with sound reasoning (runtime behavior provably unchanged, compile-time-
only constraint).

Fix implementer self-flagged 2 reasonable scope judgment calls (fixed 5
false CLAUDE.md passages instead of the literal 3 briefed, to avoid
self-contradiction; used "r" not "F" intention letter) - both reviewed and
accepted as sound judgment, not scope creep.

Controller independently re-verified after all fixes: pio test -e native
51/51, both esp32node4 and esp32dev builds SUCCESS (confirming the
designated-initializer/deleted-copy-constructor changes compile under the
real toolchain), new CLAUDE.md section read in full and confirmed accurate/
complete, PCB doc checklist read and confirmed correct, rooted-include
count (44) independently re-derived and confirmed matching.

## SUB-PROJECT #4 (TURNOUT STATION WIRING) COMPLETE.
