# Subagent-Driven Development Progress

Plan: docs/superpowers/plans/2026-10-22-esp32-node4-loco2mqtt-wiring.md

Started: 2026-10-22

Ledger reset for sub-project #7 (Loco2MQTT WiFi/MQTT wiring). Previous
plan (sub-project #6, channel-count templating) is complete and merged
into main's history.

Workspace: working directly on main per the user's earlier explicit
confirmation for this same overall effort.

Pre-flight plan scan: 2 tasks. Task 1 is a main.cpp-only composition-root
change with no possible native test (consistent with every prior
main.cpp-only change in this project) - verified by build across all 3
environments + full native suite unaffected. Task 2 is documentation
only, including a proactive fix to sub-project #6's forward reference to
#7 (avoiding a 3rd recurrence of a staleness pattern the last two final
reviews each caught once). No contradictions against the plan's Global
Constraints. Proceeding without a batched question to the user.

## Tasks

Task 1 (Wire real WiFi/MQTT/Loco2MQTT into node4 main.cpp): complete
(commit 552a0a7..3fdb538 [@ F], review clean — Approved, zero findings.
Implementer's first dispatch stalled waiting on a self-launched
background test run rather than running it in the foreground; resumed
twice with explicit "stop waiting, run in foreground" instructions
before it completed. Task reviewer's first dispatch failed outright
(600s stall, stream watchdog did not recover) - re-dispatched fresh,
completed clean on retry. Reviewer independently cross-checked the full
diff against src/esp32/main.cpp's live globals/setup()/loop() (outside
the diff) and confirmed a genuine structural mirror, not just brief
compliance; confirmed C++ global constructor ordering is safe
(brokerAddressResolver before mqttLink, turnoutCommandPort/feedbackSource
after); confirmed the deliberate mDNS-hostname-equals-client-ID-prefix
divergence from the panel (panel uses 2 different prefixes, node4 uses 1
per this plan's explicit constraint) was intentional, not an oversight.
Controller independently re-ran the full native suite in isolation after
observing spurious ERRORED results from an overlapping backgrounded test
run (confirmed concurrency artifact, not a real regression - clean
54/54 on isolated re-run) before dispatching review. All 3 builds +
54/54 native suite per implementer report, independently reproduced by
controller.

Task 2 (CLAUDE.md + PCB doc updates): implementer commit c4c6092 [. d]
complete. Controller independently caught a gap in the TASK 2 PLAN TEXT
ITSELF before dispatching review (not an implementer deviation): the
plan never instructed extending the PCB doc's spec-file/CLAUDE.md-section
reference lists for #7's own files - the same staleness-gap pattern the
final reviews of sub-projects #5 and #6 each caught once before, in
different sections. Controller fixed this directly (not via a fix
subagent, given its triviality - a 2-item list extension) in a
follow-up commit 73790e0 [. d], then packaged BOTH commits together for
review. Review: Approved, zero Critical/Important findings. Reviewer
independently verified every prose claim in the new CLAUDE.md section
against the real main.cpp code (MAC-derived naming, LWT values, the
missing-subscribe-calls claim, the setup()/loop() gating) and confirmed
all accurate; independently re-ran the rooted-include grep (44,
confirmed unchanged); confirmed the addendum commit's 2 list extensions
are both correct. 2 Minor findings, explicitly labeled plan-mandated
(trivial wording imprecision in text this plan itself specified
verbatim, not implementer-introduced) - recorded here for the final
whole-branch review to triage, not worth a dedicated fix cycle for
wording this trivial. pio test -e native (54/54) per implementer report,
unaffected by the docs-only addendum commit.

ALL 2 TASKS COMPLETE. Proceeding to final full-suite verification and
the final whole-branch review.

## Final whole-branch review (opus): "Ready to merge: With fixes" (docs only, code confirmed correct and safe)

Reviewer traced the full boot-to-connected-to-feedback-applied path by
hand through main.cpp and the real driver/control classes: confirmed
global constructor ordering is sound; confirmed no dangling references
(MqttLink takes clientId/willTopic/willMessage by value, not by
reference); confirmed feedbackSource's constructor-time subscribe() is
safe (MqttLink.cpp replays all stored topics on every successful
connect, confirmed by reading that file); confirmed no stale-LED path
(IndependentLedPairDriver::applyState() early-returns on no-mode-change,
so per-tick clearIndicator() doesn't re-stamp the blink timer); confirmed
no conflict with sub-project #5's boot-mode indicator; confirmed no
"collision" ghost reference anywhere; confirmed all 8 protected files
genuinely untouched via git diff --name-only; confirmed the one blocking
call (EspMdnsResolver's ~8s WiFi-wait) is the same already-accepted,
60s-throttled tradeoff the panel already has, not new blocking behavior.

2 Important findings, both documentation-only:
1. Only 1 of 5 now-false "future sub-project"/"blocked on"/"not done"
   claims scattered across CLAUDE.md was annotated (sub-project #6's,
   which my plan explicitly targeted) - 4 more survived in the #1
   section's main.cpp bullet, the #1 section's own summary line, #4's
   "Known, accepted gap" paragraph (the worst one - #7's own new section
   160 lines below explicitly claims to close this exact gap), and #5's
   non-goals/scope-narrowing text. Root-caused to the PLAN itself, not
   the implementation: 3 consecutive sub-projects' final reviews have
   now caught this same pattern once each, and #7's plan only hard-coded
   the single known instance instead of adding a general "grep for
   staleness and annotate every hit" step. Noted as a durable process
   lesson (not just a one-off fix).
2. Nothing documents what a bring-up technician will actually observe on
   the node board's first-ever network bring-up - specifically, that
   wrong WiFi credentials and an unreachable broker produce visually
   IDENTICAL symptoms (solid unconfirmed blink forever), that the board
   is throw-only and looks "dead after the first press" until real
   Loco2MQTT state-topic echoes exist (correct by design, easily
   misread as a bug), and that commands are silently dropped while
   disconnected (same as the panel, not a new defect, but undocumented).

4 Minor findings folded into the fix (all cosmetic doc issues): a
garden-path sentence in the PCB doc's §7 status line; a stale "52 test
suites" count elsewhere in CLAUDE.md that now visibly self-contradicts
#7's own "54" claim; PCB doc item 1's stale "minimal placeholder"
parenthetical (3 sub-projects out of date); PCB doc item 7's MQTT
client-ID example now wrong for node4 specifically.

2 Minor findings -> user declined as optional code cleanups (not
required - reviewer explicitly said "not required," user chose skip):
reusing mqttClientId for the mDNS hostname instead of duplicating the
literal; adding an explicit TurnoutFeedback include instead of relying
on a transitive one. main.cpp stays exactly as Task 1 landed it.

1 Minor (commit trailer inconsistency) and 1 forward-looking note
(cross-board nodeId-collision interaction once presence detection exists
on node4 too) explicitly need no action now - recorded for whoever
later works on presence detection.

Dispatching ONE fix subagent for both Important findings plus the 4
doc-only Minors - no code changes, only CLAUDE.md and the PCB doc.

Fix landed: commit b506a7e [. d]. Controller independently re-read every
edit on disk against the review's exact findings: confirmed all 4
remaining stale claims (the #1-section main.cpp bullet, the #4 section's
"Known, accepted gap" paragraph now prefaced "Closed by sub-project #7 -
see below", the #5 section's identify-overlay forward-reference, and
the #5 non-goals "blocked on" claim) are now annotated in place, not
deleted; confirmed the new "Bring-up note" paragraph landed in the #7
section covering all 3 failure signatures the reviewer traced; confirmed
the PCB doc's garden-path status-line sentence was reworded; confirmed
the stale "52 test suites" count was corrected to 54; confirmed both PCB
doc item-list staleness fixes (item 1's placeholder claim, item 7's
MQTT-client-ID example) landed. User declined the 2 optional main.cpp
code cleanups (string-literal dedup, explicit include) - main.cpp
untouched since Task 1. pio test -e native re-run independently by
controller twice (once before dispatching the fix's review, once after):
54/54 PASSED both times. Working tree clean except this ledger file.
Saved a durable feedback memory ("grep for staleness markers before
writing a doc task, don't hand-pick the one known instance") given this
is the 3rd consecutive sub-project whose final review caught this exact
pattern.

## SUB-PROJECT #7 (LOCO2MQTT WIFI/MQTT WIRING) COMPLETE.

Final state: 8 commits (3aa301c design spec .. b506a7e final-review doc
fix). Code confirmed correct and safe by both per-task review (zero
findings on Task 1's first try) and the final whole-branch review
(traced global-init ordering, reference lifetimes, the constructor-time
MQTT subscribe, blink-timer non-regression, and the one already-accepted
blocking call by hand through the real driver classes - no hazard
found). All findings across both review passes were documentation-only.
The node board's "a button press produces zero observable effect" gap
(documented since sub-project #4) is now genuinely closed: real
WiFi/MQTT/Loco2MQTT wiring exists, commands publish for real, and real
state-topic feedback updates the turnout and LED. No regressions to the
Mega, panel, or any existing suite. Presence/collision detection and the
identify-overlay remain deferred to a future sub-project.
