# Subagent-Driven Development Progress

Plan: docs/superpowers/plans/2026-10-24-esp32node4-presence-collision.md

Started: 2026-10-24

Ledger reset for sub-project #8 (presence/collision detection). Previous
plan (sub-project #7, Loco2MQTT WiFi/MQTT wiring) is complete and merged
into main's history.

Workspace: working directly on main per the user's earlier explicit
confirmation for this same overall effort.

Pre-flight plan scan: 2 tasks. Task 1 is a main.cpp-only composition-root
change with no possible native test (consistent with every prior
main.cpp-only change in this project) - verified by build across all 3
environments + full native suite unaffected. Task 2 is documentation
only, and this time includes a REQUIRED general staleness-sweep grep
step rather than a single hard-coded annotation - the process fix
adopted after sub-project #7's final review caught the 3rd consecutive
recurrence of that exact pattern. No contradictions against the plan's
Global Constraints. Proceeding without a batched question to the user.

## Tasks

Task 1 (Wire presence/collision detection into node4 main.cpp): complete
(commit 87a344d..4db6c92 [@ F], review clean — Approved, zero findings.
No stalling or backgrounding issue this time - implementer ran all 4
verification commands in the foreground as instructed, clean first try.
Reviewer independently cross-checked the full diff against
src/esp32/main.cpp's live, unmodified presence/collision blocks and
confirmed a byte-identical structural mirror (declaration ordering,
collision-block placement before the setup-trigger check,
presenceAnnouncer.update() placement before the feedback gate,
suppression-loop ordering), confirmed the one mandated wording
deviation ("node" vs "panel") was applied correctly and nowhere else,
confirmed C++ global constructor ordering is safe. All 3 builds +
54/54 native suite (clean on first run, no re-run needed) per
implementer report. Controller independently re-diffed the full commit
against the plan before dispatching review.

Task 2 (CLAUDE.md + PCB doc updates + REQUIRED staleness sweep): first
dispatch stalled waiting on a self-launched background test run (same
mistake as sub-project #7's Task 1, despite the dispatch not having
explicitly warned against it this time) - resumed once with explicit
"stop waiting, run in foreground" instructions, completed clean.
Staleness sweep worked exactly as intended: found and fixed 6 stale
locations total, 3 more than the brief's own anticipated 3 - 2 extra
found by reading full context around grep hits (sub-project #6/#7
sections both saying presence/collision "remains deferred"), 1 more
found by contextual reading near a hit, not a literal grep match (the
LWT-dormant note). Correctly left 1 pre-existing stale item alone (PCB
doc's identify-blocked claim, made false by #7 not #8) and flagged it in
the report rather than silently ignoring or incorrectly fixing it.
Commit bd80ce3 [. d]. Review: Approved, zero Critical/Important
findings - reviewer independently re-ran the exact staleness grep and
confirmed the implementer's hit list was complete and accurate (not
selectively quoted), independently verified every factual claim in the
new section against the real main.cpp code, confirmed all 6 annotations
follow the established in-place convention. 2 trivial Minors (PCB doc's
own dateline is one sub-project behind; a reminder that the pre-existing
left-alone item will need picking up by a future identify-overlay task)
- not fixed, noted for awareness only. pio test -e native (54/54)
confirmed by implementer (foreground) and independently reproduced by
controller in isolation.

ALL 2 TASKS COMPLETE. The staleness-sweep process fix (adopted after
sub-project #7's final review) worked as intended on its first real
test - 3 consecutive prior final reviews each caught this pattern once;
this time the per-task doc review caught it fully before reaching final
review. Proceeding to final full-suite verification and the final
whole-branch review.

## Final whole-branch review (opus): "Ready to merge: With fixes" (docs only, code confirmed correct and safe)

Reviewer traced the full collision-detection path by hand through the
real main.cpp: confirmed gatedButtons[4] is threaded everywhere a
button's identity matters (the one surviving raw buttons[i] reference
is begin(), required since GatedDigitalInput has no begin()); confirmed
loop() ordering guarantees same-tick suppression; confirmed the MAC-
derived mqttClientId precondition this whole sub-project rests on is
genuinely still true in the live file, not just asserted; confirmed
global construction ordering is safe; confirmed all 5 protected files
genuinely untouched via git diff --name-only.

**The staleness-sweep process fix held** - reviewer independently
re-checked the ENTIRE CLAUDE.md file (not just grep hits) across every
sub-project section and found ZERO surviving false claims about
presence/collision being unwired on the node board. The 3-sub-project
streak (#5, #6, #7 each catching this once) is genuinely broken, at
least for CLAUDE.md.

3 Important findings, all documentation-only:
1. The PCB doc's own §7 status line/reference lists were NOT updated
   for #8 - the exact same staleness pattern, but in the one file the
   sweep's grep keywords don't match ("Status as of sub-project #7"
   doesn't contain "future sub-project/blocked on/unwired/not done/
   still/yet"). This is the second time this specific PCB-doc-reference-
   list gap has been missed (first time: sub-project #6, fixed in #7's
   own addendum commit 73790e0).
2. No "Bring-up note" for sub-project #8, breaking the precedent #7's
   own final review established: a latched collision produces the exact
   same LED signature (solid unconfirmed blink) as a WiFi/broker
   failure, the only discriminator (the serial collision line) fires
   once per boot and won't reprint for a technician who attaches late,
   the latch never clears without a reboot, and serial/setup-gesture
   recovery stays live during lockout.
3. The cross-board-family interaction #7's own final review explicitly
   flagged forward to "whoever later works on presence detection" (this
   sub-project) was not picked up: PresenceTopics hard-codes a "panel/"
   prefix for both board families, so a panel and a node4 board sharing
   a nodeId now mutually detect and lock each other out - new behavior
   as of this commit, undocumented anywhere.

1 Minor folded into the fix: a wording imprecision in the #8 section
("once both reach BootMode::Normal" - the real gate doesn't test
BootMode::Normal specifically). Other Minors are shared accepted
limitations with the panel (one-tick detection lag, collisionLogged
stickiness) or pre-existing items already correctly left alone and
flagged by the implementer - no action needed on those.

Dispatching ONE fix subagent for all 3 Important findings plus the 1
Minor wording fix - no code changes, only the PCB doc and CLAUDE.md.

Fix landed: commit e96456e [. d]. The fix agent's edits were correct
and complete, but it stalled waiting on a self-launched background
pio test -e native run for the 3rd time this sub-project (despite being
explicitly warned against backgrounding in this exact dispatch) - this
time, rather than resuming it again, the controller independently
verified both diffs directly (confirmed Fix A's status-line/reference-
list update, Fix B's bring-up paragraph, Fix C's cross-family note, and
Fix D's wording tightening all matched the review's exact
recommendations), ran pio test -e native in the foreground itself
(54/54 clean), and committed directly. One git lock contention hiccup
along the way (a stale .git/index.lock left by the stalled agent's
earlier interrupted git command - confirmed via `tasklist` that no
git.exe process was actually still running before removing it; working
tree was intact throughout, confirmed via git status before and after).

## SUB-PROJECT #8 (PRESENCE/COLLISION DETECTION) COMPLETE.

Final state: 5 commits (ae9e38e design spec .. e96456e final-review doc
fix). Code confirmed correct and safe by both per-task review (zero
findings on Task 1's first try) and the final whole-branch review
(traced the full collision-detection path, construction ordering, and
the MAC-derived-client-ID precondition by hand through the real
main.cpp - no defect found). All findings across both review passes
were documentation-only. The staleness-sweep process fix (adopted after
sub-project #7's final review, after a 3-sub-project streak of this
exact pattern) held for CLAUDE.md on its first real test, independently
re-verified by the final reviewer across the WHOLE file, not just one
diff - the streak is broken there. It missed the PCB doc's parallel
reference-list pattern a 2nd time (1st: sub-project #6/#7), since the
sweep's keyword list doesn't match that file's "Status as of sub-project
#N" dateline idiom - a durable lesson for whoever next writes a doc task
in this effort: the sweep needs a second keyword set for the PCB doc's
own staleness idiom, not just CLAUDE.md's. No regressions to the Mega,
panel, or any existing suite. Two node4 boards (or a node4 board and the
panel) sharing a nodeId by mistake now detect each other and suppress
their buttons, identical behavior and latency to the panel's existing
mechanism. Identify-overlay remains deferred to a future sub-project.
