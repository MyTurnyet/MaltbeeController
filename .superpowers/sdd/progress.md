# Subagent-Driven Development Progress

Plan: docs/superpowers/plans/2026-09-02-esp32-wireless-setup-wifi-scan.md

Started: 2026-09-02

NOTE: this file is git-tracked and carries stale content from the prior
completed plan (open wireless-setup AP) in its history. This ledger
starts fresh for the WiFi network scan sub-project. Do not run
`git checkout`/`git stash` on this file. Every ledger update is
committed immediately, never batched across tasks.

Worktree: created via the native EnterWorktree tool (branch
`worktree-esp32-wireless-setup-wifi-scan`), which branches from
origin/main by default. origin/main was missing this session's two most
recent local-only commits on `main` (the design spec, 694eebf, and the
implementation plan, f57a6e5) — fast-forward-merged local `main`
directly into the worktree branch to pick them up, no cherry-pick
needed since the worktree branch had no divergent commits of its own.

Baseline: `pio test -e native` confirmed clean just before dispatch:
40/40 suites PASSED, 0 failed.

Pre-flight plan scan: no contradictions found between the four tasks or
against the plan's own Global Constraints section. Proceeding without a
batched question to the user.

Task dependency order: Task 1 (`WifiScanFormatter`) has no dependencies.
Task 2 (`SetupFormRenderer` dropdown) depends on Task 1. Task 3
(`CaptivePortalServer` scan wiring) depends on both Task 1 and Task 2.
Task 4 (docs) has no code dependency but documents the end state, so it
runs last.

## Tasks

Task 1 (WifiScanFormatter): complete (commit a08ec2f..9b8f591 [^ F],
review clean — Approved, zero findings at any severity. Reviewer traced
the actual UTF-8 byte escapes for both the 4-bar and 1-bar cases by hand
(E2 96 82/84/86/88) and confirmed test and implementation agree exactly,
confirmed the cascading if/else-if correctly implements the four
half-open RSSI bands including boundary values, and confirmed
dedupeAndSort's drop-empty-ssid and keep-strongest-rssi behavior matches
both the brief and the plan's global constraint. 41/41 native suite (40
existing + 1 new, 8 cases/17 assertions).

Task 2 (SetupFormRenderer dropdown): complete (commit 2755da5..97e0915
[! F], review clean — Approved, zero Critical/Important. Plan's brief had
a harmless arithmetic slip ("10 existing + 4 new = 14" when the file
actually had 9 pre-existing cases, so 13 total) — implementer correctly
flagged it rather than forcing the wrong number, reviewer independently
confirmed via the diff's hunk structure (zero removed lines in either
hunk) that this is proof, not just a visual read, that every pre-existing
case survived byte-for-byte unmodified. Confirmed the default `= {}`
argument, the dropdown's `<select>` has no `name=` (never itself
submitted), its `onchange` correctly targets `wifi_ssid` by
`getElementsByName`, and `escapeHtml` is applied to both the option value
and label. 1 Minor recorded, not fixed (intentional per the brief/tests,
not an oversight): `renderNetworkOptions`'s placeholder is unconditional,
asymmetric with `renderIdOptions`'s conditional one. 41/41 native suite
unchanged (13 cases/33 assertions in the existing suite, no new binary).

Task 3 (CaptivePortalServer scan wiring): complete (commit
05d8f7f..d0f5e1b [! F], review clean — Approved, zero Critical/Important.
Reviewer confirmed `scanNetworks()` is called from exactly the two
sanctioned places (begin(), right after softAP(); and the new
handleRescan()) and never from handleRoot()/onNotFound, confirmed
handleRoot() passes scannedNetworks_ to the renderer, confirmed
src/esp32/main.cpp is untouched, and traced WiFi.SSID(i)/WiFi.RSSI(i)'s
argument order against ScannedNetwork's field order. `pio run -e
esp32dev` SUCCESS (RAM 16.9%/Flash 81.6%), `pio test -e native` 41/41
unchanged. 1 Minor recorded, not fixed (matches this class's existing
style, no other resource cleanup is explicit here either): no explicit
`WiFi.scanDelete()` after a rescan, relying on implicit cleanup on the
next scan. Controller independently verified the reviewer's
own-flagged "no proof of an end-to-end path to /rescan" note — grepped
the merged tree and confirmed Task 2's rendered "Rescan" link
(SetupFormRenderer.h:58) and this task's `/rescan` route
(CaptivePortalServer.cpp:21) do connect.

Task 4 (docs): complete (commit aaa74b9..709a644 [. d], review clean —
Approved, zero findings. Reviewer confirmed all 3 edits landed verbatim
in the correct locations across exactly the 2 named files, and that the
doc text is accurate (not just present) against the actual shipped
behavior: scan-once-at-startup, the `/rescan` route, and the
dropdown-writes-into-existing-field convenience with free-text fallback
intact.

## ALL 4 TASKS COMPLETE — proceeding to the final whole-branch review.

## Final whole-branch review (opus): "Ready to merge: With fixes"

Reviewer independently traced the full scan-to-dropdown-to-textfield
chain across all 4 tasks in the assembled code, confirmed `/rescan`'s
route and rendered link genuinely connect, confirmed `scanNetworks()` is
called from exactly the two sanctioned places (never `handleRoot()`),
confirmed the network `<select>` has no `name=` so it can never itself
be submitted, and independently verified the design spec's central
hardware claim by reading the installed core's `WiFiGeneric.cpp`
directly (`enableSTA(true)` ORs into the current mode, so softAP
survives into APSTA — spec's citation confirmed accurate, not just
trusted). Re-ran both gates: `pio test -e native` 41/41 (40 baseline +
`test_wifi_scan_formatter`, `test_setup_form_renderer` at 13 cases),
`pio run -e esp32dev` SUCCESS (RAM 16.9%/Flash 81.6%). Zero Critical.

2 Important, both fixed (commit 7d13bad):
1. The dropdown's signature UTF-8 signal-bar characters (▂▄▆█) were
   served with no charset declaration anywhere (`SetupFormRenderer.h`'s
   `<head>` had a viewport meta but no charset meta;
   `CaptivePortalServer.cpp`'s `webServer_.send()` used bare
   `"text/html"`) — reviewer confirmed the ESP32 WebServer core doesn't
   inject one either, so browsers fall back to a locale-dependent
   default and would likely mangle the bars on a real phone. Fixed by
   adding `<meta charset='utf-8'>` and `"text/html; charset=utf-8"`.
2. `begin()` ran the (up-to-10-second-worst-case, per the core's own
   `waitStatusBits` timeout) blocking scan BEFORE starting
   `dnsServer_`/`webServer_` — reviewer identified this as directly
   undermining Decision 2's own goal (a phone associating during that
   window could get a DHCP lease with no DNS/HTTP answer yet, missing
   the captive-portal popup entirely). Fixed by moving `scanNetworks()`
   to the last line of `begin()` — free, zero-risk, since
   `scannedNetworks_` is only ever read by `handleRoot()`, which cannot
   run until `poll()` is called, which cannot happen until `begin()`
   returns.

1 Minor, fixed (commit 3862f88 + 949971b): `CLAUDE.md`'s test-suite
count (40) was stale post-branch (41) — fixed. The fix subagent itself
caught a second-order staleness this introduced (CLAUDE.md's
`CaptivePortalServer` prose still said the scan ran "right after
`WiFi.softAP()`", now false after the Fix 2 reorder) and flagged it as
a concern rather than silently leaving it or overstepping its scoped
instructions — controller fixed it directly in a follow-up commit.

3 Minor recorded, deliberately NOT fixed per the reviewer's own explicit
recommendation ("fine to defer or drop entirely; none affect
correctness"): the Rescan link can discard unsaved form input on
navigation (no warning); a mid-session rescan freezes the setup-mode LED
flash and takes the AP off-channel for up to 10s (consistent with the
already-accepted synchronous-scan tradeoff, just not spelled out for the
mid-session case); `std::sort` vs `std::stable_sort` for RSSI ties
(cosmetic, no test depends on ordering among ties). A 4th Minor
(`CLAUDE.md`'s Test coverage checklist missing `WifiScanFormatter`) was
explicitly declined by the controller's own fix-dispatch instructions,
since that checklist already has multiple pre-existing gaps unrelated to
this branch and partially patching it would be misleading rather than
helpful — reviewer's own calibration agreed this is "conformance with an
already-lagging list, not a new regression."

Disposition on the two items carried over from per-task review, both
re-affirmed by the final reviewer from first principles rather than
deferring to the earlier verdict: `renderNetworkOptions()`'s
unconditional placeholder is architecturally required, not just
stylistically acceptable (the network dropdown has no `name=`/persisted-
selection concept to reflect, unlike `renderIdOptions`) — left as-is.
Missing `WiFi.scanDelete()` after a rescan is a genuine non-issue —
reviewer read the installed core's `WiFiScan.cpp` directly and found
`scanNetworks()` already calls `scanDelete()` internally before every
scan, so results structurally cannot accumulate; adding explicit cleanup
would be dead code.

Fix dispatched as ONE subagent, split into two commits per Arlo's
Commit Notation (code bugfix vs. doc): `! B Fix WiFi scan charset and
begin() blocking order` (7d13bad) and `. d Fix stale test-suite count in
CLAUDE.md` (3862f88), plus one controller follow-up doc commit
(949971b) for the second-order staleness the fix subagent flagged.
Controller independently re-ran both gates after all fixes: `pio run -e
esp32dev` SUCCESS, `pio test -e native` 41/41.

## PLAN COMPLETE — all 4 tasks approved, final review's 2 Important + 1
Minor findings fixed, plus one controller follow-up doc fix for
second-order staleness the fix subagent itself flagged; 3 Minor findings
recorded and left as accepted per the reviewer's explicit guidance. All
builds/tests green. Proceeding to finishing-a-development-branch.
