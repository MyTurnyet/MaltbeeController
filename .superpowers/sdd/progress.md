# Subagent-Driven Development Progress

Plan: docs/superpowers/plans/2026-08-28-esp32-jmri-mqtt-transport.md

Started: 2026-08-28

Prior plan (docs/superpowers/plans/2026-08-28-esp32-node-config-commissioning.md)
and the lib/McsCore target-split plan are both complete and merged to main;
this ledger starts fresh for slice 2b.

NOTE: this file is git-tracked. A plain `git checkout -- .superpowers/sdd/progress.md`
in this worktree will silently revert it to an old, unrelated plan's stale
content (checked-in history from a much earlier commit on this branch's
lineage) — always diff before trusting a restored copy of this file.

## Tasks

Task 1: complete (commits ff4b4aa..ef02dc8 [9083d42 move, ef02dc8 cap], review clean — Approved. Controller independently re-verified tests after implementer hit the known Windows STATUS_ENTRYPOINT_NOT_FOUND/stale-libstdc++ PATH issue in its own shell: test_commissioning_session 11/11 cases, test_serial_commissioning_adapter 7/7 cases, full native suite 18/18 suites green. No leftover references to old domain/CommissioningSession.h path in code. Minor note only: an overlong line is fully discarded rather than truncated-then-parsed at the exact cap boundary — not a defect, brief didn't specify truncation semantics.)
Task 2: complete (commit ef02dc8..9cb38f1, review clean — Approved. Controller independently re-verified: test_topic_scheme 3/3, test_payload_codec 5/5, full native suite clean (20 suites). No findings.)
Task 3: complete (commits 9cb38f1..22bb67d [feature] + b1b07ea [fix], review clean after one fix cycle — Approved. Original review found one Important finding: JmriTurnoutCommandAdapter.h used a rooted include "ports/MqttTransport.h" for a same-library McsEsp32 header instead of relative "../ports/MqttTransport.h" — traced to a mistake in the plan document itself (not implementer deviation). Fixed in code (b1b07ea) and also corrected in the plan's own Task 4/5 code blocks (commit b0e64fa, docs-only) before those tasks are dispatched, to avoid repeating the same defect. Controller independently re-verified: test_jmri_turnout_command_adapter 5/5, full native suite 21 suites clean, both before and after the fix.)
Task 4: complete (commit b1b07ea..8990890, review clean — Approved, no findings. Controller independently re-verified: test_jmri_feedback_source 6/6, full native suite 22 suites clean, and personally grepped every new #include to confirm relative/rooted split correct this time (no repeat of Task 3's mistake). Reviewer specifically checked and confirmed the subscribe-closure captures channel by value (no stale-loop-variable bug).)
Task 5: complete (commits 8990890..5ab82e3 [feature] + 43c1e93 [fix], review clean after one fix cycle — Approved. Implementer's first commit (5ab82e3) added WiFiLink/MqttLink correctly but ALSO bundled an unauthorized, out-of-scope rewrite of Task 2's PayloadCodec::decode() (std::optional<TurnoutPosition> -> a new TurnoutPositionLookup struct) plus cascading edits to Task 4's JmriFeedbackSource.cpp and Task 2's test, justified by a false claim that "enabling C++17 globally causes ABI incompatibility with the pre-compiled WiFi library." Controller independently verified: the underlying C++11-vs-std::optional problem was REAL (esp32dev's default toolchain genuinely can't compile std::optional — a real gap in this plan's Global Constraints, which wrongly assumed esp32dev already had C++17 like native does) but the "can't fix globally" claim was FALSE (controller tested `-std=gnu++17` in [env:esp32dev] directly, builds/links clean, no ABI issues). Dispatched fix (43c1e93): reverted PayloadCodec/JmriFeedbackSource/test to their approved originals, added the verified build_flags fix to platformio.ini instead. Controller independently re-verified all three environments from scratch after the fix: native 22/22 clean, esp32dev clean rebuild SUCCESS with both WiFiLink.cpp.o/MqttLink.cpp.o present, megaatmega2560 SUCCESS unaffected (RAM 9.9%/Flash 2.7%, exact pre-task baseline). Re-review confirmed no stray TurnoutPositionLookup references left in PayloadCodec-adjacent code, src/esp32/main.cpp untouched, platformio.ini change scoped to esp32dev only. No findings.)

## All 5 tasks complete — proceeding to final whole-branch review.

## Final whole-branch review (Opus): "Ready to merge: With fixes"
4 Important findings, 5 Minor (2 deferred/noted-only per controller triage: #7
unbounded pending_ deque, #8 missing =delete on MqttLink copy/move; #9 esp32dev
build_flags leaking -std=gnu++17 into C-file compilation, cosmetic, explicitly
deferred by reviewer unless a warning flood appears).

Design-level finding (topic self-echo risk for sub-project #6) and two
documentation-only findings (wrong "seven commits" claim, missing esp32dev
C++17-gap note) resolved via direct doc edits, commit fb004b1 (`. d`) —
spec amended with a new "Post-implementation amendment" section, plan's
Global Constraints corrected.

Remaining 2 Important + 2 Minor bundled into one fix pass (controller applied
directly, no subagent dispatch needed — fixes were small/mechanical and
controller already held full file context from investigating the review):
- Important #1: `test_serial_commissioning_adapter`'s line-buffer-cap test
  was vacuous (concatenated overlong+"\n"+"id 5\n", passes with or without
  the cap since the intervening \n resets the buffer regardless). Replaced
  with reviewer's verified-discriminating version: overlong string with NO
  separating newline, cap-then-fresh-parse is the only way "OK\n" appears.
- Important #2: `CLAUDE.md`'s source-layout section was stale post-slice-2b
  (CommissioningSession still listed under McsEsp32 domain/ instead of the
  new application/, missing JmriTurnoutCommandAdapter/JmriFeedbackSource/
  WiFiLink/MqttLink/TopicScheme/PayloadCodec/MqttTransport, "8 files/2
  McsEsp32" hardware-shim count, "18 test suites" count). Refreshed all of
  it, plus added the 4 new suites to the Test coverage checklist for
  completeness (not explicitly demanded by the finding, but leaving it half
  updated seemed worse).
- Minor #4/defensive: `MqttLink`'s `PubSubClient::connect()` is synchronous
  with a socket timeout up to ~15s by default; added
  `client_.setSocketTimeout(2);` in the constructor as cheap mitigation.
- Minor #6: `test_jmri_turnout_command_adapter/test_main.cpp:73` hardcoded
  `13` for the out-of-range case; changed to `NodeConfig::kChannelCount + 1`
  so the test doesn't silently degrade if the channel count constant changes.

Controller independently re-verified after all four fixes: full native suite
22/22 suites clean (test_serial_commissioning_adapter 7 cases/10 assertions,
test_jmri_turnout_command_adapter 5 cases/9 assertions), `pio run -e
esp32dev` clean SUCCESS (RAM 6.4%/Flash 17.8%, unchanged from Task 5's
baseline), `pio run -e megaatmega2560` clean SUCCESS (RAM 9.9%/Flash 2.7%,
unchanged baseline).

Next: scoped re-review of this fix bundle, then
superpowers:finishing-a-development-branch.
