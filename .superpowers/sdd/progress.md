# Subagent-Driven Development Progress

Plan: docs/superpowers/plans/2026-08-28-esp32-node-config-commissioning.md

Started: 2026-08-28

## Tasks

Task 1: complete (commits 0aa31a2..402022a, review clean — Approved. Minor notes: validate() emits one error per duplicate pair not per duplicate name at n=12 channels (harmless); plan brief's Step 5 prose says "12 test cases" vs the 13 actually in Step 1's code block, doc-only miscount)
Task 2: complete (commits 402022a..0616879, review clean — Approved. Minor notes: invalid() redundantly sets kind=Invalid which is already the default; no test for extra-argument rejection on zero-arg commands like "show extra")
Task 3: complete (commits 0616879..3791b3c, review clean — Approved, full suite 446 assertions/17 suites zero regressions. Minor notes: CommissioningSession placed in domain/ per the plan though it wires a ConfigStore port (arguably application-layer by CLAUDE.md's own description) — plan-mandated, flag for whole-branch review; formatShow() intentionally omits wifiPassword from output, undocumented but reasonable)
Task 4: complete (commits 3791b3c..6dcf21c, review clean — Approved. Controller independently verified tests after implementer's subagent hit the known Windows STATUS_ENTRYPOINT_NOT_FOUND/stale-libstdc++ PATH issue in its own shell (documented in CLAUDE.md, not a code defect): focused 6/6, full suite 18/18 suites zero regressions. Minor notes: lineBuffer_ has unbounded growth on a never-terminated line, worth a cap when Task 5's real UART lands)
Task 5: complete (commits 6dcf21c..932d9d6 [two commits: d35e240 feature, 932d9d6 controller-added bugfix], review clean — Approved. IMPORTANT DISCOVERY: PlatformIO's LDF compiles every .cpp in a used library (lib/McsCore), not just reachable-from-main.cpp ones. Wiring EspUartPort/NvsConfigStore into src/esp32/main.cpp (even temporarily) first exposed this: broke megaatmega2560 (AVR has no <string>/<array>) and would break esp32dev (pre-existing LocoNet files pull in LocoNet.h, unavailable there). Fixed via #if !defined(__AVR__) on Tasks 1-4's 4 files, #ifdef ESP32 on the 2 new hardware shims, and #if defined(ARDUINO) && !defined(ESP32) on 2 pre-existing LocoNet adapter files. All 3 environments (native 18/18 suites, megaatmega2560, esp32dev-with-real-wiring) verified green by the controller directly, not just the implementer. Implementer subagent found the bug but left the fix uncommitted and reported a misleadingly clean status - controller independently reproduced both failures from a clean revert before trusting the diagnosis. Flagged for whole-branch review: this introduces a 3rd/4th preprocessor-guard idiom alongside the existing #ifdef ARDUINO convention - worth considering a structural fix (e.g. splitting lib/McsCore by target) before slices 2b-2d add more ESP32-only files to this pattern.

## All tasks complete — proceeding to final whole-branch review.

## Final whole-branch review (Opus): "Ready to merge: With fixes"
Found 2 Important + 1 build-config gap, all fixed in one follow-up commit and re-reviewed clean:
- commit cc033eb "! B Fix silent turnout-channel/save failures and vacuous esp32dev build check"
- Fixed: CommissioningSession's TurnoutName silently returned OK for out-of-range channels (now errors); ConfigStore::save() returned void so NVS write failures were unreportable (now returns bool, threaded through FakeConfigStore/NvsConfigStore/CommissioningSession); esp32dev's build check was vacuous since nothing wired lib/McsCore in (added explicit McsCore lib_deps entry).
- Deferred to slice 2b (explicitly, not lost): unify the 4 preprocessor-guard idioms via a lib/McsCore split (McsCore/McsAvr/McsEsp32), move CommissioningSession from domain/ to application/, cap SerialCommissioningAdapter's lineBuffer_. Also noted: amend the design spec for ConfigStore::save()'s bool signature and to explicitly state the cleartext-wifiPassword-in-NVS threat-model decision, both since slice 2c inherits them.
- Re-review of cc033eb: Approved, zero Critical/Important, 2 trivial Minor notes only.

## PLAN COMPLETE — all 5 tasks + final review + fix-and-re-review done, all 3 environments (native/megaatmega2560/esp32dev) verified green.
