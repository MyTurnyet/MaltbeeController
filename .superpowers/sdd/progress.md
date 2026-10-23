# Subagent-Driven Development Progress

Plan: docs/superpowers/plans/2026-10-22-esp32-node4-loco2mqtt-channel-count.md

Started: 2026-10-22

Ledger reset for sub-project #6 (Loco2MQTT channel-count templating).
Previous plan (sub-project #5, boot-mode LED indicator) is complete and
merged into main's history.

Workspace: working directly on main per the user's earlier explicit
confirmation for this same overall effort.

Pre-flight plan scan: 3 tasks. Task 1 templates
Loco2MqttTurnoutCommandAdapter for N=4, fully covered by 5 new native
test cases plus the unchanged existing N=12 suite proving zero behavior
change. Task 2 templates Loco2MqttFeedbackSource for N=4, fully covered
by 7 new native test cases plus both the unchanged existing N=12 suite
and the cross-class wiring integration suite. Task 3 is documentation
only. No contradictions against the plan's Global Constraints (the F/B
risk-symbol judgment call is explicitly left to whoever writes the real
diff, consistent with this project's established convention, not a
contradiction). Proceeding without a batched question to the user.

## Tasks

Task 1 (Template Loco2MqttTurnoutCommandAdapter for N=4 + tests): complete
(commit 132bbda..f9890c4 [^ F], review clean — Approved, zero findings.
Reviewer independently read NvsConfigStore.h/.cpp to confirm the
Basic<X><N>-with-.cpp structural claim genuinely matches, confirmed both
required test-case index adjustments (channel 4/index 3; out-of-range
probe 5 not 13) were applied correctly, confirmed NodeConfig.h include
fully removed, confirmed instantiation/alias ordering (12 then 4) both
correct. 5/5 N=4 test cases + 5/5 unchanged N=12 cases + 53/53 full
native suite per implementer report. Controller independently re-read
all 3 files on disk against the plan before dispatching review.

Task 2 (Template Loco2MqttFeedbackSource for N=4 + tests): complete
(commit f9890c4..c20d2a8 [^ F], review clean — Approved, zero findings.
Reviewer independently read the untouched test_loco2mqtt_turnout_wiring
suite to confirm it's a genuine cross-class N=12 interoperability check
(not a rubber-stamp) that would catch a divergence between the two
templated sibling classes, confirmed the one required test-case index
adjustment (channel 4/index 3) was applied correctly, confirmed
NodeConfig.h include removed, confirmed instantiation/alias ordering
correct. 7/7 N=4 cases + unchanged N=12 suite + unchanged wiring suite +
54/54 full native suite + all 3 hardware builds (esp32dev, megaatmega2560,
esp32node4) per implementer report. Controller independently re-read all
3 files on disk against the plan before dispatching review.

ALL CODE TASKS COMPLETE (Tasks 1-2). Proceeding to Task 3 (documentation).

Task 3 (CLAUDE.md + PCB doc updates): complete (commit c20d2a8..d65e090
[. d], review clean — Approved, zero findings. Reviewer independently
re-ran the rooted-include grep (44, unchanged, correctly no edit made),
independently verified every technical claim in the new CLAUDE.md section
against the real Tasks 1-2 code (range check, loop bound, instantiation/
alias ordering, dropped NodeConfig.h include), independently confirmed
"no main.cpp changes" via grep, independently counted 54 real test suite
dirs. pio test -e native (54/54) per implementer report. Controller
independently re-read both diffs (CLAUDE.md and PCB doc) before
dispatching review.

ALL 3 TASKS COMPLETE. No fix cycles needed this sub-project. Proceeding
to final full-suite verification and the final whole-branch review.

## Final whole-branch review (opus): "Ready to merge: With fixes" (docs only, code confirmed correct)

Reviewer independently re-derived the 10 Basic<X><N> alias pairs and 4
explicit-instantiation pairs across the repo and confirmed the 2 new
ones are indistinguishable in shape from the 8 sub-project #1 already
built; confirmed src/esp32node4/main.cpp genuinely untouched; confirmed
all 3 "zero edits" test suites genuinely unmodified; checked ODR/
linkage/final-keyword/dialect risks on the template-with-.cpp pattern
and found none; confirmed the N=4 command-adapter suite's boundary
assertions (channel 4 accepted, channel 5 - inside the OLD valid range -
rejected) genuinely proves the range tightened, not just exercises it.

2 Important findings, both documentation-only, and both a RECURRENCE of
the exact pattern flagged in sub-project #5's final review: present-
tense claims in CLAUDE.md's sub-project #1 ("still hard-bind", "still
range-check", "need...Budget for it") and sub-project #5 ("still need",
"still open") sections are now false, since #6 did exactly what those
notes said still needed doing - and neither was annotated per this
project's own established in-place-annotation convention (CLAUDE.md line
514's "Closed by sub-project #5 - see below" being the precedent).

6 Minor findings: the explicit-instantiation maintenance warning (~line
230) still says "two classes" when there are now four; the adapters/
inventory bullet wasn't tagged with #6; a numbering ambiguity between
the old "#7b" presence spec and the new forward-referenced "#7" wiring
sub-project, with no disambiguation note (sub-project #1's equivalent
note is the precedent); the PCB doc's §7 spec/section reference lists
weren't extended for #5/#6; a pre-existing (not caused by #6, but
adjacent to its edit) stale parenthetical in PCB doc §7 item 1; and the
"identical" treatment claim for the Basic<X><N> pattern is slightly loose
(the Loco2Mqtt .cpp files are deliberately NOT #ifdef ARDUINO-guarded,
unlike NvsConfigStore/CaptivePortalServer - required for native N=4
tests to link, not a defect, but "identical" overstates it by one
dimension). Two more Minors (commit trailer inconsistency; harmless
unused-instantiation linking into esp32dev) explicitly need no action.

Dispatching ONE fix subagent for both Important findings plus the 6
actionable-adjacent Minors (excluding the 2 "no action needed" ones) -
no code changes required, only docs.

Fix landed: commit 5268c69 [. d]. Controller independently re-read every
edit on disk: confirmed sub-project #1's forward-looking note now carries
a "Done, sub-project #6" annotation in place (not deleted); confirmed
sub-project #5's "still need"/"still open" claim now reads "now closed";
confirmed the explicit-instantiation warning names all 4 classes
(dropped "two", added both Loco2Mqtt classes tagged "sub-project #6");
confirmed the adapters/ inventory bullet now tags the N=4 templating;
confirmed the #7 numbering collision is disambiguated against the old
"#7b" presence spec; confirmed the PCB doc's §7 spec-file and CLAUDE.md-
section reference lists were extended to 6 items each (including #5/#6);
confirmed the "identical" treatment claim was softened to "the same
general shape" with a new sentence naming the real #ifdef ARDUINO-guard
difference. pio test -e native re-run independently by controller:
54/54 suites PASSED. Working tree clean except this ledger file.

## SUB-PROJECT #6 (LOCO2MQTT CHANNEL-COUNT TEMPLATING) COMPLETE.

Final state: 6 commits (d7ae094 design spec .. 5268c69 final-review doc
fix). Code confirmed correct by both per-task review (zero findings on
all 3 tasks, no fix cycle needed) and the final whole-branch review
(zero code findings; all findings were stale cross-section CLAUDE.md
claims from sub-projects #1/#5 that #6 itself made false). No
regressions to the Mega, panel, or any existing suite. Node board's
Loco2MqttTurnoutCommandAdapter/Loco2MqttFeedbackSource are now
constructible at N=4; wiring them into src/esp32node4/main.cpp alongside
real WiFi/MQTT is sub-project #7, deliberately out of scope here.
