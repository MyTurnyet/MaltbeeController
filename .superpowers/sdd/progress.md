# Subagent-Driven Development Progress

Plan: docs/superpowers/plans/2026-08-30-esp32-presence-collision-detection.md

Started: 2026-08-30

NOTE: this file is git-tracked and carries stale content from the prior
completed plan (#2c-b2, ESP32 wireless setup composition-root wiring) in
its history. This ledger starts fresh for sub-project #2d-a (ESP32
presence + collision detection), the first half of #2d after the split.
Do not run `git checkout` on this file (would revert to stale content) or
`git stash` (bare or otherwise) at any point in this branch — lesson
carried forward from an earlier stray-stash incident this session. Every
ledger update is committed immediately, never batched across tasks.

Worktree branched from origin/main (83e6e64) via the native EnterWorktree
tool; the design spec (9bbf67c) and this plan (d212ecb) were cherry-picked
onto this branch from local main afterward, since local main was two
commits ahead of origin/main when the worktree was created.

Baseline: `pio test -e native` confirmed clean just before dispatch: 36/36
suites PASSED, 0 failed.

Pre-flight plan scan: no contradictions found between tasks or against the
plan's own Global Constraints section; no plan-mandated test asserts
nothing. Proceeding without a batched question to the user.

Task dependency order: Task 1 (PresenceTopics) and Task 2 (NodeIdentityGuard)
are mutually independent. Task 3 (MqttPresenceAnnouncer) depends on Task 1
but not Task 2. Task 4 (src/esp32/main.cpp wiring) depends on Tasks 1-3
all being complete and must run last.

Process note carried forward from #2c-b2's branch: every implementer/fix
dispatch pre-authorizes direct ACN commits (no interactive /arlo-commits
invocation, which stalls waiting for approval a subagent can never get),
and reminds dispatches that F/B commits over 8 lines cap at `!` regardless
of test coverage, and to run `pio test -e native` via Bash, not PowerShell
(a known false-failure issue on this machine, now documented in CLAUDE.md).

## Tasks

Task 1 (PresenceTopics): complete (commit 98b834b..fc2d692 [! F, 36-line
diff exceeds the 8-LoC cap so ! is correctly self-capped regardless of
production code being only 17 lines], review clean — Approved, zero
findings at any severity. Reviewer confirmed byte-for-byte match to the
brief's exact header code and exact 3-test-case test file, header-only
discipline maintained (no stray .cpp), and the pattern mirrors
TopicScheme.h exactly. Controller independently verified the diff stat
(36 total lines) and re-ran the focused test: 3/3 test cases, 4
assertions.

Task 2 (NodeIdentityGuard): complete (commit ec2a43c..9fe1a8a [! F, 83-line
diff], review clean — Approved, zero Critical/Important. Reviewer
independently traced the actual `if (observedMac != ownMac_)
{ collisionDetected_ = true; }` code (no else branch, no reset path
anywhere) to confirm the one-way latch, rather than trusting the
implementer's own empirical swap-test claim, and verified each of the 5
test cases would genuinely fail against at least one plausible wrong
implementation (unconditional-suspicion, self-clearing-on-reobservation).
1 Minor (non-blocking): test 5 is largely redundant with test 2, but was
specified verbatim by the brief itself so not a deviation. Implementer's
own self-review honestly flagged test 5 as the weakest case rather than
overclaiming. Controller independently confirmed the commit's diff size
(83 lines, correctly capped at !) and re-ran the focused suite: 5/5 test
cases.

Task 3 (MqttPresenceAnnouncer): complete (commit 42f77ba..710a105 [! F,
92-line diff], review clean — Approved, zero Critical/Important. Reviewer
traced all 4 edge-detection scenarios against the real committed code
(false-first, true-first-publishes, true-while-connected-no-republish,
false-then-true-reannounces) and confirmed topics/payloads/retained flags
match PresenceTopics.h and MqttTransport.h exactly (checked against the
real headers in the worktree, not just the brief's assumed text).
Confirmed test/support/FakeMqttTransport.h genuinely untouched via a
direct diff check. 2 Minor (both non-actionable/informational, not fixed):
the two sequential publish() calls can't be observed out of order given
MqttTransport's synchronous contract; nodeId_ is stored but only used to
rebuild topic strings, a theoretical micro-optimization not worth pursuing
for a connect-edge-only call. Controller independently re-ran the focused
suite: 4/4 test cases, 12 assertions; also confirmed via `git diff` that
FakeMqttTransport.h has zero changes in this commit.

## Tasks 1-3 complete — proceeding to Task 4 (main.cpp wiring), dispatched
on the most capable available model per the plan's own guidance (highest-
judgment task: real composition-root restructuring, no native test exists
for src/, verification is esp32dev build + the plan's own 7-item
manual-read-through checklist).

Task 4 (src/esp32/main.cpp wiring — presence announcement, collision
detection): complete (commit f6837e1..0b10f62 [! F, no native test
possible for src/], review clean (opus) — Approved, zero Critical.
Reviewer independently traced all 9 spec-compliance points against the
actual file and source (not the report), including verifying at the
source level (not just trusting the brief) that MqttLink::connect()
really does replay subscribed handlers on every reconnect and that
esp_efuse_mac_get_default() really has no NVS/WiFi-driver dependency.
Confirmed the subscribe-before-announce ordering (subscribe() in setup(),
first publish only possible in loop()) makes mutual collision detection
correct rather than racy in a simultaneous-boot scenario, and that
presenceAnnouncer.update() being unconditional (not gated on !collision)
is load-bearing, not just safe — a collided node must keep re-publishing
its mac on reconnect for the OTHER node in the pair to ever observe the
mismatch. 1 Important (plan-level design gap, NOT a Task 4 wiring defect
per the reviewer's own explicit framing, task still Approved): the
retained panel/<id>/mac message is never cleared, so reassigning a nodeId
to a replacement panel (normal fleet management, not just operator error)
makes the new panel latch a permanent false collision that not even a
reboot clears (a fresh boot re-subscribes and immediately re-receives the
same stale retained value) — recovery requires manually clearing the
retained topic on the broker, undocumented anywhere in this project.
Escalated to the final whole-branch review / user decision rather than
fixed unilaterally, since multiple legitimate resolutions exist with real
tradeoffs (non-retained announce vs. an explicit clear-on-decommission
mechanism vs. accept-and-document). 4 Minor, recorded: (2) the "one-tick
lag" is real and one step more precise than the implementer's own report
described (one station.update() can run un-suppressed after collision is
internally known, before this tick's suppression write) — inconsequential
per the reviewer's own assessment, worth stating precisely rather than
vaguely in project notes; (3) ComboSetupModeTrigger's use of the UNGATED
matrixButtons[0]/[1] is now load-bearing (it's one of only two escape
hatches from a collision lockout, the other being serial commissioning) —
a future refactor "tidying" it onto gatedButtons could silently remove
wireless recovery with no test to catch it, worth a one-line comment;
(4) loop() is at ~88 lines and near, but not over, the line for needing
decomposition — the suppression composition block is the one part dense
enough to consider extracting into a named function, not blocking; (5)
a cosmetic indentation quirk, consistent with existing file style, ignore.
Controller independently re-ran all three builds/tests on this exact
commit: esp32dev SUCCESS (RAM 16.8%/Flash 81.1%, small genuine increase
from 80.9% baseline), megaatmega2560 SUCCESS unchanged, native 39/39
PASSED via Bash. Controller also independently read the full final
main.cpp and confirmed byte-for-byte match to the plan's specified code.

## ALL 4 TASKS COMPLETE — proceeding to the final whole-branch review
(most capable available model), which will also weigh Task 4's escalated
Important finding (stale retained mac / nodeId-reassignment lockout).
