# Subagent-Driven Development Progress

Plan: docs/superpowers/plans/2026-09-06-esp32-loco2mqtt-turnout-bridge.md

Started: 2026-09-06

NOTE: this file is git-tracked and carries stale content from the prior
completed plan (WiFi network scan) in its history. This ledger starts
fresh for sub-project #9a (ESP32 Loco2MQTT turnout bridge). Do not run
`git checkout`/`git stash` on this file. Every ledger update is
committed immediately, never batched across tasks.

Worktree: created manually via `git worktree add` (branch
`esp32-loco2mqtt-turnout-bridge`), branching from local `main`'s actual
HEAD rather than the native `EnterWorktree` tool's default (which
branches from `origin/main`) — origin/main was 3 commits behind local
main at the time (the design spec, the implementation plan, and a
`.gitignore` update to ignore `.worktrees/`), and the native tool has no
per-call override for that default. Branching from local HEAD carried
all 3 commits in cleanly, no merge/cherry-pick needed.

Baseline: `pio test -e native` confirmed clean just before dispatch —
all suites PASSED, 0 failed.

Pre-flight plan scan: no contradictions found between the 12 tasks or
against the plan's own Global Constraints section. Proceeding without a
batched question to the user.

Task dependency order (per the plan): Tasks 1 (`TopicScheme`), 2
(`NodeConfig`), 3 (`ParsedCommand`/`CommandLineParser`), and 4
(`MdnsResolver`/`BrokerAddressResolver`) have no dependencies on each
other. Tasks 5/6 (`Loco2MqttTurnoutCommandAdapter`/`Loco2MqttFeedbackSource`)
depend on 1+2. Task 7 (wiring test) depends on 5+6. Task 8
(`CommissioningSession`) depends on 2+3. Task 9
(`WebFormSubmission`/`WebFormCommissioningAdapter`) depends on 2+3+8.
Task 10 (`SetupFormRenderer`/`FirmwareVersion`) depends on 9. Task 11
(`EspMdnsResolver`) depends on 4. Task 12 (main.cpp wiring) depends on
everything. Executing in plan order (1→12), which already respects this.

## Amendment: task grouping (discovered during Task 1 dispatch)

First Task 1 dispatch (isolated `TopicScheme` change) failed with
NEEDS_CONTEXT: PlatformIO compiles `McsEsp32` as one shared static
library for every native test target, so changing `TopicScheme`'s
signature broke compilation of `JmriTurnoutCommandAdapter.cpp`/
`JmriFeedbackSource.cpp` even under a scoped `-f test_topic_scheme` run.
Controller independently verified this by applying Task 1's exact change
in the main worktree and reproducing the build error, then reverted the
probe. Traced the reference graph and confirmed 9 of 12 tasks (1, 2, 3,
5, 6, 7, 8, 9, 10) are compile-coupled via the `NodeConfig` hub type and
must land as one commit group; only Task 4, Task 11, and Task 12 are
independent. Presented finding + fix to the user, who chose "merge into
one big task." Plan file amended in place (commit ba3ee69) with a
grouping note; original per-task text unchanged and still the source of
truth for exact code/tests. Execution now proceeds as 4 dispatches:
Merged-Task-1 (old 1,2,3,5,6,7,8,9,10), Task 4, Task 11, Task 12.

Cleanup note: the first Task 1 dispatch was also mistakenly launched
with `isolation: "worktree"`, which silently created a second, separate
worktree (`.claude/worktrees/agent-ad7233d995ad0726b` on branch
`worktree-agent-ad7233d995ad0726b`) instead of using this session's
already-prepared `esp32-loco2mqtt-turnout-bridge` worktree. That stray
worktree had no commits (implementer correctly stopped at NEEDS_CONTEXT
before committing) and was removed via `git worktree remove --force` +
`git branch -D`. Every dispatch from here on omits `isolation` so
subagents work directly in this worktree.

## Tasks

Merged Task 1 (old Tasks 1,2,3,5,6,7,8,9,10 — core Loco2MQTT
domain/application/adapter migration): complete (commits
138d857..63966d0, 9 commits, all `^ F`, review clean — Approved, zero
Critical/Important. Reviewer diffed every one of the 9 brief sections'
"replace full contents" blocks against the diff hunks line-by-line and
found no deviation; specifically verified `NodeConfig`'s `{}`
zero-init on `channelTurnoutAddresses` (load-bearing for the 0-sentinel
semantics), both address-range boundaries (1 and 2048) plus
below/above/duplicate cases, `PayloadCodec` genuinely absent from the
diff (unchanged as required), and the new heartbeat-repeat regression
test in the wiring suite. 2 Minor recorded, not fixed: commit 3043708's
message doesn't mention the 3 inert `git mv` renames it also carries
(controller independently confirmed via `git show --stat 3043708` —
all rename entries show 0 content change, pure renames, just an
incomplete commit-message description); `addressesWithChannel()` test
helper duplicated verbatim across 3 test files (mandated by the brief
itself, each is its own Catch2 binary). 2 ⚠️ items, both independently
resolved by the controller: `git show --stat 3043708` confirmed the
rename-bundling claim exactly as the implementer described (zero
content diff on all 4 renamed files); `git log --follow` on
`Loco2MqttTurnoutCommandAdapter.cpp` confirmed history traces cleanly
back through the rename to the file's original creation (fbee4f7 →
3043708 → 20dea41 → 22bb67d) despite the squashed base→head diff not
showing rename-detection markers for this particular file (a
similarity-heuristic artifact, not a real history loss). 41/41 native
suite, independently re-verified by the controller both before and
after the review (`pio test -e native`, grepped for
PASSED/FAILED/ERRORED counts directly).

Task 4 (MdnsResolver port + BrokerAddressResolver): complete (commit
4a6bec8..3eeb66e [^ F], review clean — Approved, zero findings. Reviewer
confirmed all 5 files match the brief verbatim line-by-line. Controller
independently re-ran the scoped suite with `-vvv` and found a real
`[-Wunused-result]` compiler warning on the `[[nodiscard]]`-ignoring
test call (invisible in PlatformIO's default non-verbose output) —
flagged to the reviewer explicitly rather than pre-judging it; reviewer
confirmed the exact uncaptured-return-value test code is specified
verbatim in the brief itself, so this is plan-mandated, not an
implementer defect. 3/3 native suite.

Task 11 (EspMdnsResolver hardware adapter): complete (commit
3194c82..a009b5e [@ F — no native test coverage possible, Arduino-guarded,
matches the existing MqttLink/WiFiLink convention], review clean —
Approved, zero Critical/Important. Reviewer confirmed the `#ifdef
ARDUINO` guard wraps the whole file in both `.h`/`.cpp`, the bounded
WiFi-wait-then-mDNS-query logic matches the brief exactly, and
cross-checked the rooted `"ports/MdnsResolver.h"` include against
`MqttLink.h`/`WiFiLink.h`'s existing `"ports/Clock.h"` pattern to
confirm convention compliance. 1 Minor recorded, resolved: implementer's
report said "41 suites passing" but the controller recounted twice
(before and after this task) and got 42/42 both times, 0 failures — a
stale/mistaken count in the report text, not a real regression (Task 4
had already brought the total to 42; this task adds 0 native-visible
suites by design).

Task 12 (main.cpp + CaptivePortalServer + NvsConfigStore wiring — final
integration): complete (commits 1b84430..1b125f1, 2 commits, both
`! F`, review clean — Approved, zero findings. Reviewer independently
confirmed every renamed identifier/constructor call/include-ordering
matches the actual headers earlier tasks produced (not just the brief's
text) by reading `NodeConfig.h`, `WebFormSubmission.h`,
`BrokerAddressResolver.h` directly, confirmed `resolvedBrokerHost` is
genuinely threaded into `mqttLink.begin(...)` rather than computed and
discarded, and confirmed `main.cpp` stays strictly composition-root-only
per this repo's CLAUDE.md (no conditionals/business logic added beyond
wiring). Controller independently re-ran both gates before dispatching
the reviewer: `pio test -e native` 42/42, `pio run -e esp32dev` BUILD
SUCCESS (RAM 17.3%/Flash 83.8%) — both matched the implementer's
reported numbers exactly. One transient environment wrinkle (not a code
issue): the first `esp32dev` build attempt hit a network flake
downloading `PubSubClient`; implementer recovered by reusing the
already-resolved package from the main worktree's `.pio/libdeps/`.

## ALL 4 DISPATCHED TASK GROUPS COMPLETE (12 original plan tasks,
executed as: merged-Task-1, Task 4, Task 11, Task 12) — proceeding to
the final whole-branch review.

## Final whole-branch review (opus): "Ready to merge: With fixes"

Reviewer independently confirmed the migration is genuinely complete —
grepped `lib/`/`src/`/`test/` for every JMRI-era name and found zero
hits, diffed every planned `TEST_CASE` string from the plan's Task
1-10 briefs against the actual test tree and found none dropped by the
9-task merge, confirmed both in-flight amendments (task-grouping
restructure, missed `NvsConfigStore.cpp`) actually landed in code not
just documentation, confirmed `jmri/panel_mqtt_turnout_bridge.py` is
present-but-orphaned exactly as #9c intends, and independently re-ran
both gates at true HEAD (42/42 native, `esp32dev` BUILD SUCCESS,
RAM 17.3%/Flash 83.8% — matching every prior checkpoint exactly).

**1 Critical, fixed:** `src/esp32/main.cpp:243-244`'s `resolvedBrokerHost`
was a `setup()`-local `std::string` handed to `MqttLink::begin()` →
`PubSubClient::setServer(const char*, ...)`, which stores the raw
pointer (`this->domain = domain;`, confirmed by reading the installed
PubSubClient source directly) rather than copying — every MQTT
reconnect after the first dereferences memory `loop()` has since
reused. A genuine regression this branch introduced (previously
`runningConfig.brokerHost` came from a *global* `NodeConfig`, so the
same call site had accidentally satisfied the lifetime contract).
Controller independently verified the claim by reading
`PubSubClient.cpp:715-718,190` directly before dispatching a fix.
Fixed (commit 9315ce6) by giving `MqttLink` an owned `host_` member
instead of forwarding a transient caller-owned string.

**1 Important, fixed:** `CLAUDE.md` never got a documentation task in
the plan (unlike the immediately preceding sub-project, which had one)
and was left describing the removed JMRI integration throughout — stale
class names, stale field descriptions, stale test count (41 vs 42), and
a bridge-script description that was no longer true. Fixed (commit
17109a6) with 11 targeted corrections plus a new "Loco2MQTT turnout
bridge (sub-project #9a)" narrative subsection matching this file's
existing per-sub-project prose convention.

**1 Important (narrowly scoped), fixed:** `docs/HARDWARE_BRINGUP_CHECKLIST.md`'s
serial-commissioning example still used the old `turnout 1 name LT1`
syntax, which no longer parses. Fixed (commit eadc363) — just the one
line; the surrounding JMRI/broker-setup prose is explicitly #9c's scope,
not this branch's, per the reviewer's own scoping.

**4 Minor recorded**, 2 acted on after user follow-up, 2 left as-is:
- mDNS self-hostname collision (`EspMdnsResolver` used the same literal
  for every panel) — user asked for clarification on whether this
  could affect finding `loco2mqtt.local` (it can't; that's a wholly
  separate lookup from a panel's own self-announced name), then asked
  to fix it anyway using the MAC-suffix convention already used
  elsewhere in this codebase. Fixed directly (commit dfab1af):
  `EspMdnsResolver` now takes a `selfHostname` constructor parameter,
  `main.cpp` passes `"maltbee-panel-" + ownMac.lastFourHexDigits()`.
- Missing `test_commissioning_session` case for "address-range error
  surfaces at save" (a real design-doc-vs-shipped gap, plan-trimmed) —
  user asked to add it. Added directly (commit b0fb4b4) — passed
  immediately with no production change, confirming the composed path
  (`CommissioningSession` → `NodeConfig::validate()`) already worked
  end-to-end; closes the coverage gap rather than fixing a bug.
- `EspMdnsResolver.h`'s rooted `"ports/MdnsResolver.h"` include (should
  be relative `../ports/MdnsResolver.h` per this codebase's own
  documented convention, since the port is in the same library) — left
  as-is, not raised with the user; low real risk (compiles correctly
  today since `MdnsResolver.h` is basename-unique) but worth a follow-up
  if this file is touched again.
- The ~8-second worst-case boot-time WiFi-wait-then-mDNS-query window —
  an explicitly accepted design-doc tradeoff, not something to fix.

Both gates re-verified by the controller after every fix pass in this
section (native 42/42, `esp32dev` SUCCESS at unchanged RAM/Flash).

## SUB-PROJECT #9a COMPLETE — proceeding to finishing-a-development-branch.

Also discovered during this task's post-hoc verification: the plan
never listed `lib/McsEsp32/src/adapters/NvsConfigStore.cpp`, which also
references the old `channelJmriNames` field (via `Preferences`
get/putString). It's `#ifdef ARDUINO`-guarded so it didn't break the
native suite, but would break Task 12's `pio run -e esp32dev` check.
Plan amended (commit 3293a8b) to add a Step 0 to Task 12 fixing this —
switches to `getInt`/`putInt` (matching the field's new int type) and
folds channel-address writes into the `ok` failure-tracking chain,
since `putInt`'s return is unambiguous (unlike `putString`, where an
empty-string write and a failure are indistinguishable).
