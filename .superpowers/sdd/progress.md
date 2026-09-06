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

(none started yet)
