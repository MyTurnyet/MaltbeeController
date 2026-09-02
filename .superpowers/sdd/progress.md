# Subagent-Driven Development Progress

Plan: docs/superpowers/plans/2026-09-02-esp32-open-wireless-setup-ap.md

Started: 2026-09-02

NOTE: this file is git-tracked and carries stale content from the prior
completed plan (BOOT-button setup trigger) in its history. This ledger
starts fresh for the open-wireless-setup-AP sub-project. Do not run
`git checkout`/`git stash` on this file. Every ledger update is
committed immediately, never batched across tasks.

Worktree: created via the native EnterWorktree tool (branch
`worktree-esp32-open-wireless-setup-ap`), which branches from
origin/main by default. origin/main (7c72a91) was missing this
session's two most recent local-only commits on `main` (the design
spec, 4ffe376, and the implementation plan, 9aaff1b) — fast-forward-
merged local `main` directly into the worktree branch to pick them up,
no cherry-pick needed since the worktree branch had no divergent
commits of its own.

Baseline: `pio test -e native` confirmed clean just before dispatch:
40/40 suites PASSED, 0 failed.

Pre-flight plan scan: no contradictions found between the two tasks or
against the plan's own Global Constraints section. Proceeding without a
batched question to the user.

Task dependency order: Task 1 (code: drop the AP passphrase) has no
dependencies. Task 2 (docs) has no code dependency but documents the
end state, so it runs last (mirrors the code change it describes).

## Tasks

Task 1 (remove AP passphrase): complete (commit d4338a8..8a55379 [! F],
review clean — Approved, zero findings at any severity. NOTE: the
implementer's own completion notification carried an automated security
classifier warning ("Blocked by classifier... review carefully"), almost
certainly triggered by "password"/"open WiFi"/"WPA2 passphrase" surface
language in its transcript rather than any real issue — the task's whole
point is removing a hardcoded shared secret, not adding one. Controller
independently read the actual commit diff before dispatching the
reviewer (confirmed via `git show`, exactly the 3 sanctioned files, 9
lines); reviewer was explicitly asked to assess the flag independently
too and separately confirmed via `git show --stat` and a repo-wide grep
that nothing beyond the 3 authorized files changed and no secret was
introduced/logged/exfiltrated — concluded "no real problem." Both gates
green: `pio run -e esp32dev` SUCCESS (RAM 16.9%/Flash 81.2%), `pio test
-e native` 40/40.

Task 2 (docs): complete (commit 6215048..4083229 [. d], review clean —
Approved, zero findings at any severity. Reviewer confirmed both edits
landed word-for-word, exactly the 2 named files changed, no remaining
"passphrase"/"maltbee-setup" reference in either file, and praised the
implementation doc's added operational context (no-timeout risk,
BOOT-only gate) as exceeding the minimum requirement without scope
creep. No security classifier flag this time.

## ALL 2 TASKS COMPLETE — proceeding to the final whole-branch review.

## Final whole-branch review (opus): "Ready to merge: With fixes"

Reviewer performed a THIRD independent security assessment of the
Task 1 classifier flag (on top of the controller's and Task 1 reviewer's
own checks), explicitly instructed not to defer to either prior
assessment. Confirmed false positive via first-principles evidence:
`git diff --raw` shows no file-mode/permission changes across the whole
branch; net effect on secrets is strictly negative (one hardcoded
passphrase deleted, zero new literals/keys/log calls/network
destinations added); read `CaptivePortalServer`'s and
`SetupFormRenderer`'s final source directly (not just the diff) and
confirmed the stored WiFi password is never serialized back into the
rendered page (`value=''` hardcoded) regardless of the AP's new open
state; verified the `WiFi.softAP()` single-argument-produces-open-AP
claim against the actual installed `framework-arduinoespressif32`
core source (`WiFiAP.cpp`'s `authmode` logic), not folklore. Concluded:
"I'd close this flag." Both gates independently re-run: `pio test -e
native` 40/40, `pio run -e esp32dev` SUCCESS (RAM 16.9%/Flash 81.2%,
unchanged).

1 Important, fixed (commit 0c4df23): `CLAUDE.md` had three stale
passphrase references left over from before this branch even started —
none were in either task's file list, since the design spec's
Documentation section only named the two `docs/` files. One of the
three stated a two-parameter `begin(apName, passphrase)` signature that
no longer exists in the code, which the reviewer flagged as
particularly load-bearing since `CLAUDE.md` seeds every future agent's
context (a future agent could "restore" the parameter believing its
removal was a regression). Fixed by updating all three to describe the
current open-AP behavior while noting the WPA2 passphrase's prior
existence for historical continuity.

2 Minor, both fixed (commit 0c4df23):
- A maintained "Resolved" annotation in an older spec
  (`2026-08-30-esp32-wireless-commissioning-web-form-design.md:356`)
  was half-false — it described the WPA2-passphrase layer as still
  current. Amended with an "Update (sub-project #2c-d)" clause rather
  than rewritten, preserving the historical record while noting the
  layer was later removed by design.
- The accepted-tradeoff paragraph in
  `docs/ESP32_Turnout_Panel_Implementation.md` covered the no-timeout
  risk but not that a newly-submitted WiFi password is sent in
  cleartext over the now-unencrypted AP during commissioning. Added one
  sentence; reviewer was explicit this doesn't change the design
  decision (the practical exposure delta from the old fixed/shared/
  published passphrase was already small), just makes the accepted risk
  fully explicit for a technician reading only that doc.

Fix dispatched as ONE subagent, single doc-only commit (no code
touched, matching the reviewer's own explicit recommendation to resist
touching code that's "correct as-is").

## PLAN COMPLETE — both tasks approved, final review's 1 Important + 2
Minor findings all fixed, security classifier flag independently
confirmed false-positive by three separate checks (controller, Task 1
reviewer, final reviewer). All builds/tests green. Proceeding to
finishing-a-development-branch.
