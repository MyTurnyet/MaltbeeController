# Subagent-Driven Development Progress

Plan: docs/superpowers/plans/2026-10-25-esp32node4-status-activity-leds.md

Started: 2026-10-25

Ledger reset for sub-project #10 (status/activity LEDs). Previous plan
(sub-project #9, identify-overlay wiring) is complete and merged into
main's history (commit 9626043).

Workspace: working directly on main per the user's standing explicit
confirmation for this same overall effort.

Pre-flight plan scan: 4 tasks, a new shape for this project - Tasks 1
and 2 add genuinely new, native-testable library code via TDD (first
time since sub-project #6), not just composition-root wiring. Task 1
(StatusLedDriver) and Task 2 (ActivityTrackingTurnoutCommandPort) are
fully independent of each other (different files, no shared state) but
will be dispatched sequentially, not in parallel, per this skill's own
"never dispatch multiple implementation subagents in parallel" red
flag - file independence doesn't override that rule. Task 3 depends on
both 1 and 2 (constructs and wires both new classes into
src/esp32node4/main.cpp). Task 4 is the established documentation
pattern, now with a third required staleness-sweep step (README.md,
added after sub-project #9's final review found a gap the first two
sweeps couldn't catch). No contradictions against the plan's Global
Constraints found. Proceeding without a batched question to the user.

Base commit before Task 1: 063784ff17164aa8ea4b026758a5f51f654791f7

## Tasks

Task 1 (StatusLedDriver domain class): complete (commit 063784f..db278d5
[^ F], review clean — Approved, zero Critical/Important findings.
Implementer (haiku) ran all steps in the foreground, no
stalling/backgrounding issue. Reviewer independently ran both the
filtered and full native suite itself, confirmed the diff is
byte-for-byte identical to the plan's prescribed code (no drift), and
hand-traced every test case against plausible buggy variants of the
real implementation to confirm each assertion is actually
discriminating, not tautological.

CORRECTION (caught by the final whole-branch review, see below): the
suite-count note Task 1's reviewer made here was itself wrong. The
reviewer's `git ls-tree` count of 55 directories at the pre-task
baseline included `test/support` - a test-double directory, not a
suite. The true baseline was 54, exactly what CLAUDE.md always said.
HEAD's 57 directories (minus `test/support`) = 56 suites, matching
every measured `pio test -e native` run in this sub-project. Do not
act on the struck claim above; 54 was never wrong.

3 Minor findings, all inherited verbatim from the plan's own prescribed
test code (not introduced by the implementer) - not fixed now, flagged
for the final whole-branch review's triage:
1. Test 1 ("begins off...") is tautological - FakeDigitalOutput
   defaults to false, so it passes even if the constructor never calls
   applyGpio() at all.
2. Test 7 ("update() is a no-op while On/Off...") never advances the
   clock, so it only catches "writes GPIO unconditionally" bugs, not a
   removed state-guard specifically (the elapsed-time check alone would
   mask that specific regression).
3. Test 9's first assertion (immediately after begin()) is weak in
   isolation - true regardless of whether begin() re-stamps anything,
   since blinkShowingOn_ was never toggled before begin() is called.
   The test's second assertion (after advancing the clock) is the one
   that actually catches a missing re-stamp, confirmed by trace.
No live bug exists in the shipped StatusLedDriver from any of these -
purely test-suite rigor notes for future maintainers.

Task 2 (ActivityTrackingTurnoutCommandPort decorator): complete (commit
db278d5..64e26b6 [^ F], review clean — Approved, zero findings of any
severity). Implementer (haiku) ran all steps in the foreground, no
stalling. Reviewer independently confirmed: diff touches exactly the 2
new files (no .cpp, as required - trivial header-only decorator);
confirmed TurnoutCommandPort.h/IdentifyModeTimer.h/.cpp all genuinely
untouched; hand-traced all 3 test cases against plausible buggy variants
(never-forwards, never-triggers, decorator-level trigger-suppression on
repeat sends) and confirmed each would fail against that bug - none are
tautological; independently ran both the filtered and full native suite
(56/56, matches report exactly).

Task 3 (wire StatusLedDriver + ActivityTrackingTurnoutCommandPort into
node4 main.cpp): complete (commit 64e26b6..0efe7d6 [@ F], review clean
— Approved, zero findings of any severity). Implementer (sonnet) ran
all steps in the foreground, no stalling. Reviewer independently
checked all 7 edit points against the brief byte-for-byte, traced the
real control flow in the fresh final file (not just the diff) to
confirm the collision-first priority order genuinely holds, the
activity-LED read is the literal last statement in loop(), and the
WirelessSetup branch's early return genuinely never reaches any new
code (traced ArduinoDigitalOutput::begin()/StatusLedDriver::begin()
by hand to confirm both new GPIOs rest off through that boot path
rather than assuming it); confirmed diff touches only
src/esp32node4/main.cpp; independently rebuilt all 3 hardware
environments (all SUCCESS) and reran the full native suite (56/56)
itself.

Task 4 (CLAUDE.md + PCB doc + README.md docs, 3-way staleness sweep):
complete (commit 0efe7d6..63dbc1e [. d], review clean — Approved, 2
Minor findings neither blocking). No stalling - pure docs task, no
pio commands needed. Staleness sweep found 3 stale "future
sub-project" locations (1 more than the brief's list named, found by
reading context) plus the PCB-doc status-line sweep plus 3 README.md
fixes found by a genuine full read (not just grep) - the new,
widened-scope check specifically added after sub-project #9's final
review caught README.md drift no prior sweep had ever touched.
Reviewer independently re-ran all 3 greps against live post-commit
files, read README.md and the PCB doc's §7/§11 in full, and
cross-checked every factual claim in the new CLAUDE.md section
against the live src/esp32node4/main.cpp. 2 Minor findings, both
inherited from the brief's own prescribed text rather than introduced
by the implementer: (1) the new section says "test_status_led_driver
(8 cases)" but the real file has 9 TEST_CASE blocks (the brief's own
prose had this same off-by-one, traced back to this controller's own
plan-writing count error); (2) the two new classes weren't added to
CLAUDE.md's "Current source layout" top-level class-inventory bullet
list, unlike precedent (e.g. IndependentLedPairFlashIndicator was
added there for #5) - not a literal spec violation since Task 4's
brief only specified the new section's text, but a consistency gap
worth noting for the final review.

ALL 4 TASKS COMPLETE. Proceeding to final full-suite verification and
the final whole-branch review.

## Final whole-branch review (opus): "Ready to merge — with doc-only fixes" (zero code defects)

Reviewer independently traced the real control flow in the live
main.cpp: confirmed collision priority genuinely sits above WiFi/MQTT
connectivity in the network-LED state chain; confirmed the
activity-LED GPIO read is genuinely the last statement in loop();
confirmed the WirelessSetup branch's early return genuinely means
neither new LED's code ever runs during that mode (traced
ArduinoDigitalOutput::begin()/StatusLedDriver::begin() by hand);
confirmed zero changes to src/esp32/main.cpp, src/mega/main.cpp,
TurnoutCommandPort.h, or IdentifyModeTimer.h/.cpp; independently
rebuilt all 3 hardware environments (all SUCCESS) and reran the full
native suite itself (56 PASSED, 0 FAILED/ERRORED, cross-checked
against real suite directories). Also confirmed sub-project #9's
widened-scope README.md sweep genuinely held up this time by reading
README.md in full itself.

8 Minor findings, all documentation, none blocking:
1. CLAUDE.md:82's top-of-file suite count still said 54, contradicting
   the new section's own correct 54→56 claim.
2. The new section said "test_status_led_driver (8 cases)" - real file
   has 9 (already known from Task 1's review).
3. StatusLedDriver/ActivityTrackingTurnoutCommandPort weren't added to
   CLAUDE.md's "Current source layout" class-inventory bullets, unlike
   precedent (IndependentLedPairFlashIndicator was added there for #5).
4. The rooted-include tally (44→47 now) had drifted a third time - the
   two new files add 3 more cross-library includes.
5. The activity LED's documentation overclaimed what it proves - it
   flashes on every attempted send (even one that returns early with
   nothing transmitted, e.g. an unconfigured channel address, or any
   send while MQTT is disconnected), not confirmed delivery. No
   "Bring-up note" existed for #10, unlike the precedent #7/#8 both
   set for exactly this class of ambiguity.
6. PCB doc §7's status line now claims "all seven items done," but
   items 2/3's own paragraphs (both actually completed back in #4)
   had no "- done" marker, contradicting the header.
7. PCB doc §11 had no dedicated checklist bullet for item 5 - its
   completion was buried as a trailing sentence inside item 4's bullet.
8. CLAUDE.md's WirelessSetup description was subtly wrong - said the
   state computation "reads Off naturally," when actually the
   computation is never evaluated at all (the existing early-return
   branch is what keeps the new code from running, not a computation
   that happens to land on Off).

Also caught and corrected: Task 1's own per-task review had left a
ledger note claiming the true pre-task suite baseline was 55, not 54 -
that note was itself wrong (it double-counted test/support, a test-
double directory, as a suite). Struck from the ledger before any
future doc task could act on it.

Dispatched ONE fix subagent for all 8 findings - no code changes, only
CLAUDE.md and the PCB doc. Fix landed as commit 2a65a5f [. d], no
stalling (pure documentation, no pio commands needed). Controller
independently verified every fix via git diff + targeted grep: suite
count corrected to 56 everywhere; "8 cases" corrected to "9 cases";
both new classes added to the source-layout inventory with accurate
parentheticals; the rooted-include tally bumped to 47 (42+5 split) with
the drift noted explicitly a third time; a new Bring-up note added
matching #7/#8's exact tone, softening the overclaim; PCB doc items 2/3
given the same "- done" + resolution-parenthetical treatment as items
1/4/6; a new dedicated item-5 bullet added to §11, trimmed out of
item 4's; the WirelessSetup wording corrected to accurately describe
the early-return mechanism. All fixes confirmed accurate and
proportionate - no code files touched.

## SUB-PROJECT #10 (STATUS/ACTIVITY LEDS) COMPLETE.

Final state: 7 commits (063784f design spec .. 2a65a5f final-review doc
fix). Code confirmed correct and safe by both per-task review (zero
Critical/Important findings across all 4 tasks) and the final
whole-branch review (traced the full network-LED priority chain,
activity-LED ordering, and WirelessSetup non-interaction by hand
through the real main.cpp and both new classes - no code defect
found). This is the first sub-project since #6 to add genuinely new,
native-tested library code (StatusLedDriver, ActivityTrackingTurnoutCommandPort)
rather than pure composition-root wiring, and both per-task and final
review explicitly verified the new tests exercise real behavior rather
than being tautological (with 3 known, accepted Minor test-rigor gaps
in test_status_led_driver, inherited from this controller's own plan-
writing, none hiding a live bug). All findings across both review
passes were documentation-only. Closes the LAST open item in the PCB
doc's §7 firmware checklist - every item is now done. No regressions
to the Mega, panel, or any existing suite (56/56 native, all 3 hardware
builds clean). The node4 board now has a network status LED
(collision/WiFi/broker-aware) and an activity LED (Loco2MQTT traffic-
aware, with the known attempted-vs-confirmed-send caveat now
documented). Remaining roadmap items: status/activity LEDs' own
hardware doesn't exist yet (same as every other node4 feature - no
physical board exists), the 4-channel commissioning form (already
confirmed working and tested as of sub-project #1, despite stale
CLAUDE.md language that once called it a future sub-project - worth a
dedicated staleness check next time a doc task runs), and the overall
physical hardware bring-up checklist.
