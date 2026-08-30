# Subagent-Driven Development Progress

Plan: docs/superpowers/plans/2026-08-30-esp32-wireless-setup-composition-wiring.md

Started: 2026-08-30

NOTE: this file is git-tracked and carries stale content from the prior
completed plan (#2c-b1, ESP32 wireless commissioning web form) in its
history. This ledger starts fresh for sub-project #2c-b2 (ESP32 wireless
setup composition-root wiring), the final piece of the #2c arc. Do not
run `git checkout` on this file (would revert to stale content) or
`git stash` (bare or otherwise) at any point in this branch — lesson
carried forward from an earlier stray-stash incident this session. Every
ledger update is committed immediately, never batched across tasks.

Worktree branched from origin/main (571d185) via the native EnterWorktree
tool; the design spec (6701fb0) and this plan (88ab354) were cherry-picked
onto this branch from local main afterward, since local main was one
commit ahead of origin/main when the worktree was created.

Baseline: `pio test -e native` confirmed clean just before dispatch: 36/36
suites PASSED, 0 failed.

Pre-flight plan scan: no contradictions found between tasks or against the
plan's own Global Constraints section; no plan-mandated test asserts
nothing. Proceeding without a batched question to the user.

Task dependency order: Tasks 1-5 are mutually independent of each other.
Task 6 (src/esp32/main.cpp wiring) depends on Task 1 (bool-returning
requestOnNextBoot()) and Task 4 (two-argument CaptivePortalServer::begin())
and must run last.

## Tasks

Task 1 (SetupModeRequestStore::requestOnNextBoot() void->bool): complete
(commit b00aedc..5432236 [^ F], review clean — Approved, zero
Critical/Important. Reviewer confirmed both prefs.begin() and
prefs.putBool() failures propagate correctly, prefs.end() called on the
success path, fake test double's unconditional `return true` is correct
(a test double with no real NVS has no failure mode). 1 Minor
(non-blocking): F vs B classification is a matter of interpretation, not
actionable. Implementer's first pass stalled invoking /arlo-commits
interactively (per CLAUDE.md) and waited for approval it couldn't get as
a subagent — controller resumed it with explicit authorization to commit
directly; future task dispatches in this branch pre-authorize direct
committing to avoid repeating the stall. Implementer's own `pio test -e
native` run hit the known sandbox-specific STATUS_STACK_BUFFER_OVERRUN
crash (exit 3221225785); controller independently re-ran clean: 36/36,
0 failed. esp32dev SUCCESS (9.89s, RAM 16.5%/Flash 77.2%, unchanged from
#2c-b1's baseline since nothing yet calls the changed method).

Task 2 (WebFormCommissioningAdapter blank-channel-clears + password-keeps-
current): complete (commit 4493a36..7fe21ae, review clean apart from one
Important process finding — code and tests were spec-compliant and
correct with zero functional issues (reviewer traced the exact draft()
read-timing, the unconditional `""` in currentValues(), and confirmed the
two pre-existing non-blank-password tests are unaffected), but the
original commit's ACN classification (`^ B`) violated the project's own
documented 8-line-of-code cap for a `^`-rated F/B commit (this change was
48 lines including tests). Controller independently confirmed the rule in
references/acn-notation.md before acting. Asked the user how to resolve
given amending conflicts with the standing "never amend" default; user
chose to amend. Commit `8669a92` amended to `7fe21ae` (message only, `! B`
in place of `^ B` — diff content unchanged, 2 files/39 insertions/9
deletions both before and after). Controller independently re-ran the
focused suite: 9/9 test cases, 23 assertions.

Task 3 (SetupFormRenderer never renders the stored password): complete
(commit c6edaa7..c6169e8 [! F, correctly self-capped at ! since the
30-line diff exceeds the 8-LoC ceiling for F/B], review clean — Approved,
zero findings at any severity. Reviewer confirmed the password `<input>`
hardcodes `value=''` with no variable reference, the original combined
ssid+password test was genuinely split (not duplicated), the new
never-renders-password test asserts against both the raw and
HTML-escaped password appearing anywhere in the output (not just the
`value=''` attribute), and `.hint`'s CSS is visually distinct from
`.warning`. Implementer's first commit used `!F` (missing the space ACN
requires between symbol and letter) — controller amended the message only
(same category of fix the user approved for Task 2, applied directly
without re-asking) to `! F`, diff content unchanged (24 insertions/6
deletions both before and after). Controller independently re-ran the
focused suite: 9/9 test cases, 24 assertions.

Task 4 (CaptivePortalServer::begin() WPA2 passphrase parameter): complete
(commit 983d861..af3fe40 [^ F, 6-line diff within the 8-LoC cap], review
clean — Approved, zero findings at any severity. Reviewer confirmed the
header declaration and .cpp implementation's parameter list match exactly,
`WiFi.softAP()` is called with exactly two `.c_str()` arguments, and no
scope creep beyond the one signature/call-site change. No native test
(matches this class's established build-check-only convention). Controller
independently re-ran `pio run -e esp32dev`: SUCCESS, RAM 16.5%/Flash 77.2%
(unchanged from before this task, since nothing calls this method until
Task 6).

Task 5 (document the AP passphrase): complete (commit b47273d..6817f4a
[. d], review clean — Approved, zero findings. Documentation-only, 21
insertions/0 deletions in docs/ESP32_Turnout_Panel_Implementation.md.
Controller independently confirmed byte-for-byte match to the plan's
exact specified text and insertion point (immediately after `## Power`'s
closing `---`, before `## JMRI Communication (MQTT)`) via `git show HEAD`
before dispatching the reviewer.

## Tasks 1-5 complete — proceeding to Task 6 (main.cpp wiring), dispatched
on the most capable available model per the plan's own guidance (highest-
judgment task: real composition-root restructuring, no native test exists
for src/, verification is esp32dev build + manual read-through).

Task 6 (src/esp32/main.cpp wiring — BootMode branching, combo trigger,
gated inputs): complete (commit 6b8abf1..0f0c6cd [! F, no native test
possible for src/ so ! is correct independent of line count], review
clean (opus) — Approved, zero Critical/Important. Reviewer independently
traced all 9 spec-compliance points against the final file rather than
trusting the implementer's own Step 11 checklist: confirmed the
WirelessSetup branch in setup()/loop() genuinely returns before any
network/matrix code can run in the same tick; confirmed ComboSetupModeTrigger
reads raw matrixButtons[0]/[1] (declared before gatedButtons, so it
couldn't reference the gated array even by accident) and that
GatedDigitalInput.isActive() being suppression-dependent would deadlock
it if wired the other way; checked all 12 stations[] entries individually
by index and confirmed each reads gatedButtons[i] with zero stray
matrixButtons references; confirmed requestOnNextBoot()'s bool return
gates ESP.restart() with a non-spammy failure path (requested() is
edge-triggered, cleared every tick); confirmed EspDeviceIdentity is
constructed only as a setup()-local inside the WirelessSetup branch, never
at global scope; confirmed NvsBootstrap's global-declaration-order
position still precedes the new consumeRequest() NVS read; confirmed the
gatedButtons[12] aggregate array matches this file's own established
copy-list-init pattern; confirmed no code was added attempting to close
the accepted staggered-press gap. 5 Minor (all recorded for the final
whole-branch review to triage, none fixed here): (1) the WirelessSetup
exit-via-reboot path works correctly today but only through an implicit
coupling — serialCommissioningAdapter.rebootRequested() forwards to the
SAME shared CommissioningSession that WebFormCommissioningAdapter's own
`reboot` command sets, so the web form's reboot request is actually
serviced by the serial adapter's check; WebFormCommissioningAdapter::
rebootRequested() itself is built but called from nowhere — a future
refactor that scopes the reboot flag per-adapter or reorders loop() could
silently strand the panel in AP mode; (2) a spurious turnout-1 toggle is
possible on the specific NVS-write-failure path if only T1 is still held
when suppression clears — same family as the already-accepted
staggered-press gap, reachable only on storage failure; (3) no panel-side
LED indication of WirelessSetup mode (serial log only) — matches spec
exactly, a UX note for later, not a defect; (4) reviewer noted the
design spec's "open gap: password rendered back in cleartext" is actually
ALREADY closed by this branch's own Tasks 2/3 (currentValues() blanks the
password) — worth updating in the design spec's gap-tracking language
during controller doc pass, not a code fix; (5) zero automated test
coverage for main.cpp is correctly unavoidable (test_build_src=false,
composition root is the correct architectural seam to leave untested).
Controller independently re-ran all three builds on this exact commit
after the implementer's own run: esp32dev SUCCESS (RAM 16.8%/Flash 80.9%,
a genuine increase — CaptivePortalServer/WebFormCommissioningAdapter/
ComboSetupModeTrigger/GatedDigitalInput/EspDeviceIdentity are now really
linked in, pulling in DNSServer/WiFi/WebServer), megaatmega2560 SUCCESS
(RAM 9.9%/Flash 2.7%, byte-identical, src/esp32/* excluded from this
env's build_src_filter), native 36/36 PASSED via Bash (implementer
correctly flagged that running pio test -e native through PowerShell on
this machine produces false ERRORED results from a MinGW runtime DLL/PATH
issue in that shell — Bash is required for this project's native test
runs going forward). Controller also independently read the full final
main.cpp and confirmed it is byte-for-byte identical to the plan's
specified target code.

## ALL 6 TASKS COMPLETE — proceeding to the final whole-branch review
(most capable available model), then superpowers:finishing-a-development-branch.
ESP32 Flash headroom note for the ledger: 80.9% used (~250KB free on the
default partition scheme) — worth watching in the remaining milestones
(routes, persistent config), not an immediate concern.

## Final whole-branch review (opus): "Ready to merge: With fixes"
Reviewer independently re-ran pio test -e native (36/36), pio run -e
esp32dev (SUCCESS, RAM 16.8%/Flash 80.9%, confirming genuine linkage of
the new classes), pio run -e megaatmega2560 (SUCCESS unchanged), and a
repo-wide (not diff-scoped) grep for requestOnNextBoot/EspDeviceIdentity
to confirm no stray caller anywhere still assumes the old void signature
or constructs EspDeviceIdentity outside main.cpp's WirelessSetup branch.
Traced the full cross-task blank-password loop end-to-end (currentValues()
-> render() -> submit() across Tasks 2/3/6 together, not just each task's
own unit tests) including verifying ParsedCommand::stringArg2 is a
std::string by value so CommissioningSession::apply()'s draft_ reassignment
can't leave a dangling reference. Confirmed all 4 design-spec Decisions
landed and the two #2c-b1 gaps are genuinely closed end-to-end (not just
per-task). Zero Critical.

3 Important (all resolved before merge):
1. docs/ESP32_Turnout_Panel_Implementation.md's new AP section claimed a
   factory-fresh panel auto-opens the AP; BootModeSelector only enters
   WirelessSetup on a pending request regardless of config validity, so
   this was false and would have misled a field technician. Fixed by the
   controller directly (commit 2da9b4d), doc-only.
2. CLAUDE.md's "Not yet wired" section and the #2c-b1 spec's "Known gaps"
   section were stale, describing this same branch's own completed work
   as still-pending. Fixed by the controller directly (commit 2da9b4d,
   same commit as #1): CLAUDE.md's main.cpp bullet rewritten to describe
   the actual BootMode/combo-trigger/captive-portal wiring, the "Not yet
   wired" section retired and replaced with a resolution note, and the
   #2c-b1 spec's gap section annotated "Resolved in #2c-b2" with a
   one-line explanation of each fix, left otherwise intact for historical
   record. Rooted-include count reconfirmed unchanged at 34 before editing.
3. NvsSetupModeRequestStore::consumeRequest() left unhardened while its
   sibling requestOnNextBoot() (Task 1) was hardened — if the CLEARING
   prefs.putBool() write fails, the flag stays true in NVS forever,
   permanently locking the panel into BootMode::WirelessSetup on every
   subsequent boot (WirelessSetup always wins over config validity).
   Fixed via one dispatched fix subagent (commit 7d72fd8 [! B]): added the
   same prefs.begin() failure check requestOnNextBoot() already has.
   Deliberately did NOT attempt to harden the clearing putBool() call
   itself (would need a retry loop or a logging dependency this class
   doesn't have, for a residual risk requiring a rare repeated NVS-write
   failure) — reviewer explicitly offered this narrower scope as
   acceptable. Controller independently re-ran: esp32dev SUCCESS
   (unchanged RAM/Flash), native 36/36.

8 Minor findings recorded, none fixed (per reviewer's own triage,
matching this project's established "record Minor, don't block on it"
pattern): (4) implicit WirelessSetup-exit coupling — the web form's
reboot request is actually serviced by serialCommissioningAdapter's check
forwarding to the same shared CommissioningSession, and
WebFormCommissioningAdapter::rebootRequested() is built but never called;
correct today, recommend a clarifying comment at main.cpp:235-236 in a
future pass rather than wiring the unused method (wiring it would create
two blocks reading one flag). (5) a spurious turnout-1 toggle is possible
on the specific NVS-clear-failure path if only T1 is still held when
suppression clears — same family as the already-accepted staggered-press
gap. (6) no panel-side LED indication of WirelessSetup mode — LEDs freeze
at a steady color (loop() returns before ledStation.update()) rather than
blinking, which happens to be visually distinguishable from the
disconnected/blink state but by accident, not design; worth one sentence
in a future doc pass. (7) the design spec's passphrase rationale
("same model as consumer routers") is inaccurate since routers use a
per-panel secret, not a shared one — the decision itself still stands
since the material leak (cleartext password) is closed independently, but
the reasoning as written overstates precedent; MacAddress/EspDeviceIdentity
already exist so a per-panel passphrase would be nearly free later. (8)
SetupFormRenderer's "leave blank to leave it unconfigured" copy is now
correct but could read more clearly as "leave blank to clear its name".
(9) changing WiFi SSID while leaving password blank silently keeps the
old password — covered by the hint text, worth a line in the field doc
later. (10) WebServer/DNSServer are now constructed at global scope in
every boot mode, the same "global constructor timing" territory that bit
this project twice before (NVS in #7b, clock/clock_t in #5) — build is
clean, both constructors should be inert, but only on-hardware bring-up
(sub-project #8) can settle it; added to that milestone's checklist
mentally, not a code change here. (11) zero automated main.cpp coverage
is correctly unavoidable (test_build_src=false).

Recommendation also noted: promote the "run pio test -e native via Bash,
not PowerShell, on this machine" process note from this ledger into
CLAUDE.md's Commands section before it's lost — ledgers don't survive
into the next branch. Deferred to the controller's next available point
in this closing sequence (see below) rather than treated as a merge
blocker.

## PLAN COMPLETE (3 Important final-review findings fixed: 2 doc-only by
the controller, 1 code hardening via one dispatched fix subagent; 8 Minor
recorded, none fixed) — all builds/tests green.
Final state: commits 571d185..7d72fd8 (5432236 Task1, 8669a92->7fe21ae
Task2+ACN-amend, 5111d4d->c6169e8 Task3+ACN-amend, af3fe40 Task4, 6817f4a
Task5, 0f0c6cd Task6, 2da9b4d final-review doc fix, 7d72fd8 final-review
code fix), plus seven ledger-recording commits. Proceeding to
finishing-a-development-branch.
