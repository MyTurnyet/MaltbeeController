# Subagent-Driven Development Progress

Plan: docs/superpowers/plans/2026-10-25-mega-routes-milestone-12.md

Started: 2026-10-25

Ledger reset for Mega panel Routes (Milestone 12). This is a DIFFERENT
effort from the ESP32 node4 sub-project series that occupied this same
ledger file through sub-project #10 (complete, merged into main's
history) - it reuses the same subagent-driven-development workflow and
ledger file, but targets the Mega 2560 / LocoNet panel's own roadmap
milestone, not the node4 board.

Workspace: working directly on main per the user's standing explicit
confirmation for this whole overall effort.

Pre-flight plan scan: 3 tasks. Task 1 is a new, fully native-testable
application class (RouteExecutionService) via TDD, independent of
main.cpp. Task 2 is a main.cpp-only composition-root change with no
possible native test (consistent with every prior main.cpp-only change
in this project, on both the Mega and ESP32 sides). Task 3 is
documentation across 4 files with a required staleness-sweep grep step.
No contradictions against the plan's Global Constraints. Proceeding
without a batched question to the user.

Process note: the plan file itself was committed as 29c7251 BEFORE
dispatching Task 1's implementer - this exact step was missed twice in
the immediately preceding ESP32 node4 sub-projects (#9, #10) and only
caught late both times. Committed first this time, deliberately, to
break that pattern.

Base commit before Task 1: 29c72512f1a7848c3e17eef802c8ea3e89c0b777

## Tasks

Task 1 (RouteExecutionService application class): complete (commit
29c7251..a3e2aad [^ F], review clean — Approved, zero findings. No
stalling/backgrounding issue - implementer (haiku) ran all steps in
the foreground, clean first try. Reviewer independently read the real
PulsingLocoNetTransport.cpp and confirmed the single-pending-pulse
hazard is genuinely real (sendPacket() force-releases any pending
pulse before issuing a new one, unconditionally), then traced
RouteExecutionService's start()/update() by hand and confirmed every
sequential send within one route is genuinely gated by the spacing
interval - the only ungated send is the deliberate immediate first
send in start(), correct since nothing is pending yet. One design-
level observation noted for future awareness (not a defect): replacing
an in-progress route via a fresh start() call sends the new route's
first command with no guaranteed elapsed time since the old route's
last send, which could truncate a still-pulsing solenoid - this is
spec-mandated "no queueing, replaces immediately" behavior, explicitly
tested and asserted, not something this task introduced. Reviewer
independently re-ran both the filtered and full native suite (25
assertions/8 cases; 57/57 suites), hand-traced all 8 test cases against
plausible buggy variants and confirmed none are tautological, and
confirmed the diff touches only the 3 plan-specified files with
PulsingLocoNetTransport/Route/TurnoutCommandPort/Clock all genuinely
untouched.

Task 2 (wire RouteExecutionService + 2 route buttons + 2 placeholder
routes into Mega main.cpp): complete (commit a3e2aad..fa56833 [@ F],
review clean — Approved, zero findings of any severity). Implementer
(sonnet) ran all steps in the foreground, no stalling. Reviewer
independently traced the live final file (not just the diff): both
route buttons' update() calls genuinely precede their wasPressed()
checks; routeExecutionService.update() genuinely runs unconditionally
every tick outside both button-press if-blocks; both placeholder
routes are genuinely populated via addTurnout() in setup(), not left
empty. Confirmed diff touches only src/mega/main.cpp. Independently
rebuilt all 3 hardware environments and reproduced the exact reported
RAM/Flash figures itself (Mega: RAM 18.1%/1486B, Flash 3.1%/7788B;
esp32node4/esp32dev both byte-for-byte unaffected) and reran the full
native suite (57/57) itself.

Task 3 (CLAUDE.md + roadmap doc + README.md + hardware checklist docs,
staleness sweep): complete, but needed one fix cycle (commit
fa56833..dac9005 [. d] initial, then 9e14bdd [. d] fix) - first review
found a Critical self-contradiction (CLAUDE.md's new Milestone 12
section claimed TWO RouteExecutionService instances, "each driving its
own," when the real main.cpp has exactly ONE shared instance driving
both route buttons - this also contradicted the same section's own
correctly-stated interrupt-replaces-in-progress behavior, which only
makes sense for a single shared instance) plus an Important stale
claim the implementer's own sweep missed outside its literal grep
pattern (HARDWARE_BRINGUP_CHECKLIST.md's pre-existing "Cross-cutting
notes" section still said "routes...still ahead" after routes were
done). Dispatched ONE fix subagent for both; fix corrected CLAUDE.md to
accurately describe the single shared instance (tying it explicitly to
the interrupt behavior) and narrowed the checklist's claim to only
persistent configuration remaining. Controller independently verified
the fix diff directly, then dispatched a re-review which independently
confirmed against the live main.cpp (exactly one instance, both button
handlers calling start() on it, one update() call) and confirmed no
new inaccuracy was introduced - Approved, zero remaining issues. Master
checklist and all other sections genuinely untouched throughout both
passes.

ALL 3 TASKS COMPLETE. Proceeding to final full-suite verification and
the final whole-branch review.

## Final whole-branch review (opus): "Ready to merge — with fixes" (no live defect)

Reviewer independently traced RouteExecutionService together with
PulsingLocoNetTransport by hand: confirmed the single-pending-pulse
hazard is real, confirmed 300ms spacing genuinely exceeds the 250ms
pulse duration with a real margin, confirmed loop() ordering releases
a route's prior pulse before starting its next command. Independently
rebuilt all 3 hardware environments (Mega: RAM 18.1%/1486B, Flash
3.1%/7788B - exact match) and reran the full native suite (57/57)
itself. Confirmed all 3 design non-goals genuinely hold (no status LED,
no per-route spacing, Route/RouteService/TurnoutService/TurnoutCollection
genuinely untouched via git diff). Confirmed the already-fixed
single-instance claim from Task 3's own fix cycle didn't resurface
elsewhere in CLAUDE.md. Confirmed the much older, pre-existing
roadmap-doc master checklist was genuinely left untouched, not
half-fixed.

1 Important code finding (no static_assert tying ROUTE_COMMAND_SPACING_MS
to LOCONET_PULSE_DURATION_MS - a future retune of one constant without
the other would silently reintroduce the pulse-truncation hazard this
whole class exists to avoid, with no build or test failure to catch
it), 1 Important doc finding (RouteExecutionService missing from
CLAUDE.md's Test coverage checklist), and 5 Minor findings: a
non-wrap-safe absolute-deadline time comparison in RouteExecutionService
(inconsistent with 8 other classes in this codebase using wrap-safe
elapsed-time comparison - proven to actually matter by the reviewer
constructing a concrete rollover failure scenario), an untested
empty-route-cancels-in-progress-route edge case (proven by mutation
testing - deleted the line, tests still passed), an undocumented
pointer-lifetime contract on start(), a stale 81.2%-vs-83.8% flash
figure in the hardware checklist, and a missing note about station/
route-button LocoNet pulse-slot contention. 2 further Minor items were
explicitly left as informational only (a historical roadmap §6.7 note,
and uncommitted ledger housekeeping - this file).

Dispatched ONE fix subagent for all 7 actionable findings - this wave
touched real production code (RouteExecutionService.h/.cpp,
src/mega/main.cpp), not just docs, the first time in this session a
final-review fix wave included a non-doc code change. Fix landed as
commit 9f45f34 [^ B]. Controller independently read every code diff
line-by-line before trusting it, independently rebuilt the Mega
environment (clean, RAM 18.1%/Flash 3.1% - Flash dropped slightly to
7750B from the wrap-safe rewrite's smaller code), and independently
re-ran the one changed test suite directly (9 cases/29 assertions, up
from 8/25) after the backgrounded `pio test -e native` run's captured
output got truncated by the capture mechanism (confirmed via
task-notification that the process itself completed with exit code 0,
and independently re-ran the specific suite to confirm rather than
trusting a truncated log). Dispatched a dedicated re-review given the
code-touching nature of this fix wave (higher bar than a doc-only fix);
it independently reconstructed the actual rollover failure scenario
the wrap-safe rewrite fixes, hand-traced both start()/update() for
behavioral equivalence, and confirmed all 7 fixes plus zero remaining
issues.

## MILESTONE 12 (MEGA PANEL ROUTES) COMPLETE.

Final state: 8 commits (42e6ae5 design spec .. 9f45f34 final-review
fix). Code confirmed correct and safe by per-task review (zero
findings on Tasks 1-2's first try), a fix-and-re-review cycle on
Task 3's documentation (one Critical self-contradiction, one Important
stale claim, both corrected and re-verified), and the final
whole-branch review's own fix-and-re-review cycle (1 Important
code finding, fixed with a compile-time guard; 1 Important + 4 Minor
doc/test findings, all fixed). No regressions to the ESP32 panel or
node4 board (all 3 hardware builds clean throughout, 57/57 native
suite). A dedicated button on the Mega panel now fires a full,
correctly-ordered, properly-spaced sequence of turnout commands
through the exact same TurnoutCommandPort every existing button press
already uses - closing the next item in this project's original
roadmap. The ported Route/RouteService/TurnoutService/TurnoutCollection
stack remains completely untouched, as deliberately decided during
brainstorming. Persistent configuration (Milestone 13) is the only
remaining item in the Mega panel's original roadmap; physical hardware
bring-up (both the 4 existing stations and the 2 new route buttons)
remains outstanding and unrelated to this milestone's firmware scope.
