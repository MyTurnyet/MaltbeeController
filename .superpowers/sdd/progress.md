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
