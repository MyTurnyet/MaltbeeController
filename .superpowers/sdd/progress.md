# Subagent-Driven Development Progress

Plan: docs/superpowers/plans/2026-10-25-esp32node4-identify-overlay.md

Started: 2026-10-25

Ledger reset for sub-project #9 (identify-overlay wiring). Previous plan
(sub-project #8, presence/collision detection) is complete and merged
into main's history (commit 7e9945d).

Workspace: working directly on main per the user's standing explicit
confirmation for this same overall effort.

Pre-flight plan scan: 2 tasks. Task 1 is a main.cpp-only composition-root
change with no possible native test (consistent with every prior
main.cpp-only change in this project) - verified by build across all 3
environments + full native suite unaffected. Task 2 is documentation
only, carrying forward both required staleness-sweep steps (the general
keyword sweep plus the PCB-doc-specific "Status as of sub-project"
sweep, the latter added as a standing requirement after it was missed
twice in sub-projects #6/#7 and #8). No contradictions against the
plan's Global Constraints. Proceeding without a batched question to the
user.

Base commit before Task 1: 62d6ebc4aa351036076bff64e17a71315c96987a

## Tasks

Task 1 (Wire identify-overlay into node4 main.cpp): complete (commit
62d6ebc..3b7387a [@ F], review clean — Approved, zero findings. No
stalling/backgrounding issue this time - implementer (haiku) ran all 3
builds + native suite in the foreground, clean first try. Reviewer
independently re-ran all 3 builds and the native suite itself (not just
trusting the implementer's report), confirmed diff touches only
src/esp32node4/main.cpp, and cross-checked structural fidelity against
the live src/esp32/main.cpp panel file directly. One minor judgment call
surfaced: the brief's prose said the new include goes "between
MacAddress.h and NodeConfig.h," but the implementer placed it before
LedPairDriver.h instead - correct, since that's true alphabetical order
given the real include block's full contents (the brief's prose summary
was imprecise, not the actual requirement) and matches the panel's own
file exactly. Reviewer confirmed this was the right call, not a
deviation.

Task 2 (CLAUDE.md + PCB doc updates + REQUIRED staleness sweep): complete
(commit 3b7387a..8bc63c1 [. d], review clean — Approved, zero findings.
No stalling/backgrounding issue this time - this task has no build/test
commands to stall on anyway. Staleness sweep found 9 stale locations (8
anticipated by the brief, 1 more found by the implementer re-reading
context - a second near-duplicate clause in CLAUDE.md's big
composition-roots paragraph, line 81). Reviewer independently re-ran
both greps against the live post-commit files, confirmed all 9
locations are correctly annotated in place (never deleted, per
convention), confirmed every "left untouched" hit is genuinely still
true, confirmed the PCB doc's status line/reference lists/§7 item 4
bullet all match the brief exactly, confirmed diff touches only the 2
doc files, and independently verified every factual claim in the new
CLAUDE.md section against the live src/esp32node4/main.cpp (exact line
numbers for the constant, global declaration order, subscribe
placement, loop() gating, and comment wording - verbatim match against
the panel's own comment).

ALL 2 TASKS COMPLETE. Proceeding to final full-suite verification and
the final whole-branch review.

(Process note: the plan file docs/superpowers/plans/2026-10-25-esp32node4-identify-overlay.md
was never committed before dispatching Task 1 - caught by the final
whole-branch review, not earlier. Committed separately as 1c42ade before
dispatching the final-review fix.)

## Final whole-branch review (opus): "Ready to merge — with doc-only fixes" (zero code defects)

Reviewer traced the full identify-overlay control flow by hand through
the real main.cpp and MqttLink.cpp: confirmed the MQTT subscribe
genuinely lands (replayed on every (re)connect, not just a no-op before
WiFi association), confirmed the loop() ordering guarantee (trigger()
lands via mqttLink.poll() before the setIdentifying loop, which runs
before ledStation.update()) is real in the file, not just claimed in
the comment, confirmed the bootMode == BootMode::Normal gate's claimed
redundancy actually holds given the full control flow, confirmed
src/esp32/main.cpp (the panel) was genuinely unmodified and the two
files' identify wiring is byte-identical in structure, confirmed no
lib/ or test/ changes and no hexagonal-architecture violation.
Independently rebuilt all 3 hardware environments and reran the native
suite (54/54 clean) itself.

2 Important + 4 Minor findings, all documentation:
1. README.md has been stale for 8 sub-projects (still called the node4
   board a "minimal placeholder" with nothing wired) and now directly
   contradicts #9 - the mandated staleness sweep only covers CLAUDE.md
   and the PCB doc, so README.md was structurally never in scope for
   any prior sweep.
2. The "deliberately still works during a latched collision - that's
   the intended way to tell two colliding boards/panels apart" claim
   (repeated in 3 places, 2 pre-existing + 1 new in #9's own section) is
   misleading: PresenceTopics::identifyTopic() keys purely on nodeId, so
   two SAME-family colliding boards both flash identically and
   simultaneously - identify locates the pair but provides zero
   discrimination between them. Only the cross-family case (panel vs.
   node4, different LED-pair counts) is actually distinguishable.
3. (Minor) New CLAUDE.md "Identify-overlay (sub-project #9)" heading
   sits directly above the unrelated, older "#9a" panel sub-project
   sequence with no disambiguating sentence (precedent: #6's section
   disambiguates an analogous #7/#7b clash).
4. (Minor) The committed design spec's include-placement claim ("between
   MacAddress.h and NodeConfig.h") was imprecise; true alphabetical
   order (and the panel's own file) puts it between BootModeSelector.h
   and LedPairDriver.h - the implementation got this right, the spec
   text didn't.
5. (Minor) PCB doc §7 item 4's main prose paragraph had no inline
   "- done (...)" marker even though the status line and §11 checklist
   bullet both correctly say it's done.
6. (Informational, no action) the identify/feedback-desync caveat
   already documented for the panel applies identically to node4 -
   already adequately covered by "identical behavior to the panel."

Dispatched ONE fix subagent for findings 1-5 (2 Important + 3 actionable
Minor; finding 6 needed no action). Fix landed as commit f0f4386 [. d]
- no stalling/backgrounding issue, no pio commands were even needed
(pure documentation, verified via git diff only). Controller
independently reviewed the full diff: README.md rewritten to state
current reality and list all sub-projects #1-9 (no longer calls the
board a placeholder anywhere); the "tell apart" claim corrected in all
3 locations (CLAUDE.md x2, docs/ESP32_Turnout_Panel_Implementation.md)
with an accurate same-family-collision caveat; #9/#9a disambiguation
sentence added matching #6's precedent; spec file's false claim
corrected in place with a note; PCB doc item 4 given its "- done"
marker matching items 1/6's style. All edits confirmed proportionate
and accurate against the live code - no code files touched.

## SUB-PROJECT #9 (IDENTIFY-OVERLAY WIRING) COMPLETE.

Final state: 6 commits (62d6ebc design spec .. f0f4386 final-review doc
fix, including 1c42ade recovering the plan-commit gap). Code confirmed
correct and safe by both per-task review (zero findings on both tasks'
first try) and the final whole-branch review (traced the full MQTT
subscribe → trigger() → isActive() → setIdentifying() control flow by
hand through the real main.cpp and MqttLink.cpp - no defect found). All
findings across both review passes were documentation-only. New
process gap surfaced and fixed: the staleness sweep's file scope
(CLAUDE.md + docs/pcb/*.md only) structurally cannot catch README.md -
worth widening in a future sub-project's plan if README.md accumulates
more node4-board claims. No regressions to the Mega, panel, or any
existing suite (54/54 native, all 3 hardware builds clean). The node4
board's identify-overlay now works identically to the panel's,
including the same-family discrimination limitation now documented
accurately in both places. Status/activity LEDs, the 4-channel
commissioning form, and physical hardware bring-up remain the only
open items for this board.
