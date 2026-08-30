# Subagent-Driven Development Progress

Plan: docs/superpowers/plans/2026-08-30-esp32-wireless-commissioning-web-form.md

Started: 2026-08-30

Prior plan (sub-project #2c-a, ESP32 wireless setup boot mode & trigger) is
complete and merged to main; this ledger starts fresh for sub-project
#2c-b1 (ESP32 wireless commissioning web form).

NOTE: this file is git-tracked and lives on a branch lineage with old,
unrelated prior-plan ledger content checked into history. A plain
`git checkout -- .superpowers/sdd/progress.md` will silently revert it to
that stale content. Do not run `git checkout` on this file for any reason.
Also: do not run `git stash` (bare or otherwise) at any point in this
branch - a stray stash in the #2c-a branch previously wiped this file's
uncommitted contents once. Lesson applied here: every ledger update in
this branch is committed immediately, never batched across tasks.

Baseline: worktree created from origin/main at c2ffae2 (includes the design
spec and this plan file). `pio test -e native` confirmed clean at baseline:
all 32 suites PASSED, no failures (one pre-existing, unrelated compiler
warning in test_button about an ignored [[nodiscard]] return value).

Task dependency order: Task 1 (MacAddress/SetupApName) and Task 2
(EspDeviceIdentity) are independent of everything else. Task 3
(WebFormSubmission) must precede Tasks 4 and 5. Task 4
(CommissioningSession::draft() + WebFormCommissioningAdapter) and Task 5
(SetupFormRenderer) are independent of each other. Task 6
(CaptivePortalServer) depends on both 4 and 5 and must be last.

## Tasks

Task 1 (MacAddress + SetupApName): complete (commit 2da9c64..dd1b39c
[^ F], review clean — Approved, zero Critical/Important. 2 cosmetic Minor
notes (constructor takes std::array by value not const&, hex-digit lookup
table is a function-local static pointer) - both match the brief's
verbatim specified code, not implementer deviations, not fixed. Reviewer
hand-verified the hex nibble math for both test cases (CDEF and the
zero-pad 0102 case). Native 34/34.
Task 2 (EspDeviceIdentity): complete (commit 6a4a200..1433476 [^ F],
review clean — Approved, zero findings at any severity. Uses the mandated
esp_efuse_mac_get_default() API, both files #ifdef ARDUINO-guarded, no
native test as expected. Implementer's own pio test -e native run crashed
with a Windows STATUS_STACK_BUFFER_OVERRUN error (exit 3221225785) in its
sandbox - same class of transient environment issue seen once before in
sub-project #2c-a's branch. Implementer diagnosed it themselves (removed
this task's files, crash persisted) before reporting DONE_WITH_CONCERNS
rather than fabricating a pass. Controller independently re-ran in this
worktree: clean 34/34, no crash. esp32dev SUCCESS (37.59s) confirmed by
the implementer.
Task 3 (WebFormSubmission): complete (commit e035058..0cb22cb [^ F], review
clean — Approved, zero findings. Byte-for-byte match to the brief's
specified 16-line struct, no test file (matches ParsedCommand precedent),
nothing else touched. Native 34/34.
Task 4 (CommissioningSession::draft() + WebFormCommissioningAdapter):
complete (commit 833e6fe..0b0b146 [^ F], review clean apart from 1
Important finding -- traced submit()'s full 6-step sequence against the
exact response-string checks, confirmed the CommandLineParser bypass for
wifi/turnout-name (and normal routing for id/broker/save/reboot) is
implemented correctly and draft() is genuinely read-only, but found every
test used single-word values for the fields that flow through the
bypass -- so a regression back to routing wifi/turnout through the
tokenizer (the exact bug this task's design prevents) would pass all 6
existing tests undetected. Same class of plan-level test-coverage gap as
sub-project #2c-a's staggered-press and mid-hold-restamp findings.
Controller independently confirmed via grep before dispatching a fix: zero
space-containing values anywhere in the test file. Fixed by adding one
test case with space-containing wifi ssid/password/turnout-name values,
asserting the full value round-trips intact through store.load() (commit
0b0b146..c7febe4 [^ d], test-only, zero production code change).
Controller independently re-ran: 7/7 test cases (19 assertions) on the
focused suite, 35 suites collected on the full run, diff confirmed
byte-for-byte identical to the specified test code.
Task 5 (SetupFormRenderer): complete (commit 6c4462b..97f5431 [^ F],
review clean — Approved, zero Critical/Important. Reviewer verified every
embedded field (wifiSsid, wifiPassword, brokerHost, brokerPort, all 12
channel names) passes through escapeHtml() before concatenation - no
missed field, confirmed by tracing every call site. Id dropdown boundary
(1-99 inclusive, not 0/100) confirmed against NodeConfig::kMinNodeId/
kMaxNodeId directly. 2 Minor, explicitly non-blocking per the reviewer's
own assessment (same escapeHtml() code path for every field, so which
specific field a test happens to use HTML-metacharacters for doesn't
change the actual risk profile): 2 of 7 tests use metacharacter-free
values for broker host/port/channel names, so escaping is verified by
code inspection rather than a targeted assertion for those specific
fields; the unselected-id placeholder option is untested UX, not a spec
item. Neither fixed. Native 36/36.
Task 6 (CaptivePortalServer): complete (commit d9ad2f4..6c91bdd [^ F],
review clean — Approved, zero Critical/Important. Reviewer cross-checked
readForm()'s field names against SetupFormRenderer's actual rendered
name='...' attributes directly (id/wifi_ssid/wifi_password/broker_host/
broker_port/t{N}_name, 1-12) and confirmed an exact match - no silent
field-name mismatch. Confirmed begin() sequencing (softAP -> DNS redirect
-> HTTP routes -> webServer_.begin()), poll() has zero blocking calls,
both files #ifdef ARDUINO-guarded, AP name taken as a parameter never
computed internally, zero encroachment on Tasks 1-5/main.cpp/mega/
McsLoconet. 2 Minor, both explicitly out-of-scope-by-design for #2c-b2
(rebootRequested() not yet consulted; no HTTP status differentiation on
submit failure) - neither fixed, both carried forward as notes.
esp32dev SUCCESS (39.7s, RAM 16.5%/Flash 77.2%), megaatmega2560 SUCCESS
unchanged, native 36/36 confirmed independently by the controller.

## All 6 tasks complete — proceeding to final whole-branch review.

## Final whole-branch review (opus): "Ready to merge: With fixes"
Reviewer independently re-ran pio test -e native (36/36), pio run -e
esp32dev (SUCCESS, unchanged footprint - confirming zero runtime effect
since nothing references these classes yet), pio run -e megaatmega2560
(SUCCESS unchanged), traced the full currentValues()<->render() round-trip
including the nodeId==0 special case, verified all 17 form field names
match exactly between SetupFormRenderer and CaptivePortalServer::readForm()
(not just a sample), verified draft() composes safely with apply()'s
draft_ reassignment (no dangling reference), and checked malformed/missing
POST field handling degrades safely (WebServer::arg() returns "" for an
absent key, which CommandLineParser correctly rejects as an invalid token
count rather than crashing or silently misparsing). Zero Critical.

1 Important (blocking, code fix): a FIFTH instance of this branch's/
project's standing test-coverage-gap pattern - "an empty channel name is
skipped" test's only assertion (channelJmriNames[1].empty()) was true
before submit() even ran, since FakeConfigStore starts from
factoryDefault() where every channel is already empty. Could not
distinguish the skip-guard working from the guard being deleted entirely.
Controller independently confirmed by reading the test file directly.
Fixed by replacing the test: pre-seed channel 2 with a real name, submit
a form with that channel blank, assert the name is untouched rather than
cleared (commit c11870e..93cd452 [^ B], test-only, zero production code
change - the implementation was already correct). Controller independently
re-ran: 7/7 test cases (19 assertions), 36 suites full run, diff confirmed
byte-for-byte identical to the specified replacement test.

2 Important (spec-level, not code defects, explicitly deferred to #2c-b2
since this branch has zero runtime effect): (1) the rendered form's own
copy ("leave a channel blank to unconfigure it") contradicts what
submit() actually does (leaves the existing name untouched) - no way to
clear a channel through the form at all currently; (2) CaptivePortalServer
opens an unauthenticated WiFi AP that renders the layout's EXISTING wifi
password back into the page in cleartext to anyone who joins - a larger
exposure than the 2a spec's already-accepted "a password being typed
travels over plaintext HTTP". Both recorded in the design spec's new
"Known gaps for #2c-b2 to resolve" section rather than fixed here, since
#2c-b2 is where the AP actually gets turned on and either decision
requires composition-root-level choices (a WPA2 passphrase, or a
skip-vs-clear semantics change affecting both fields together).

~10 Minor findings, all explicitly non-blocking: brokerHost inconsistently
NOT bypassing CommandLineParser (asymmetric with the wifi/turnout
rationale, low impact since hostnames rarely contain spaces); id dropdown
placeholder only shown for empty selectedId, not out-of-range; the
"t{N}_name" field-name construction duplicated independently in
SetupFormRenderer and CaptivePortalServer (silent-drift risk, portal side
is #ifdef ARDUINO-guarded so a mismatch wouldn't be caught by native
tests); missing <meta charset='utf-8'>; WiFi.softAP()'s bool return
discarded; handleSubmit() never re-renders the form after an error
(recoverable via re-navigating to /); a spec-to-plan test substitution
(non-numeric-id instead of out-of-range-channel, defensible since the
adapter's loop can only ever emit valid channel numbers). None fixed -
recorded for #2c-b2 or future polish.

Process note the reviewer flagged: this is the FIFTH instance of the same
test-coverage-gap shape across this project's last two sub-projects
(#2c-a: staggered-press, mid-hold-restamp; this branch: wifi/turnout
bypass round-trip, this empty-channel test). Standing lens going forward,
per the reviewer's suggested plan-writing checklist question: "what would
this assertion look like if the guard/bypass being tested were deleted?"

Controller handled CLAUDE.md and the design spec's new gap section
directly (commit 3a4775b), separately from any fix subagent, mirroring
the established pattern from #7b and #2c-a. Verified via grep that this
branch introduced zero new rooted cross-library includes (count stays at
34, unchanged) - no CLAUDE.md drift to reconcile this time.

## PLAN COMPLETE (with one review-driven test-coverage fix, zero
production code changes) — all builds/tests green.
Final state: commits c2ffae2..93cd452 (dd1b39c Task1, 1433476 Task2,
0cb22cb Task3, 0b0b146+c7febe4 Task4+fix, 97f5431 Task5, 6c91bdd Task6,
93cd452 final-review fix), plus controller doc commit 3a4775b, plus seven
ledger-recording commits. Proceeding to finishing-a-development-branch.
