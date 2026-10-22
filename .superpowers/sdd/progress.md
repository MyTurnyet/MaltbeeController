# Subagent-Driven Development Progress

Plan: docs/superpowers/plans/2026-10-21-esp32-node4-channel-count.md

Started: 2026-10-21

NOTE: this file is git-ignored scratch, carried over from prior completed
plans. This ledger starts fresh for the 4-turnout node board effort,
sub-project #1 (channel-count build target).

Workspace: working directly on `main` per explicit user confirmation
(matches this repo's established history of direct-to-main commits, no
feature-branch workflow in use).

Baseline: HEAD before Task 1 = 669279f (". d Add node4 channel-count
implementation plan"). Will confirm `pio test -e native` is clean before
dispatching Task 1.

Pre-flight plan scan: reviewed all 10 tasks against the plan's own Global
Constraints section and against each other. No contradictions found.
Tasks 1-3 and 6 each add a new test suite and delete a .cpp file whose
body folds into the corresponding .h (required by C++ template mechanics,
not a scope cut). Tasks 4-5 append-only to existing test files. Tasks 7-8
are hardware-adapter-only changes verified by build, not native test
(matches the existing no-native-test precedent for these two classes).
Task 9 adds the new environment + placeholder main.cpp. Task 10 is a
final verification pass, no commit. Proceeding without a batched question
to the user.

## Tasks

(none started yet)

Task 1 (Template NodeConfig): complete (commit 865ffa2..edba58c [^ F], review
clean — Approved, zero blocking findings. Reviewer confirmed line-for-line
match to the brief, confirmed test_node_config (36 assertions/16 cases)
unchanged and test_node_config4 (13 assertions/6 cases) new/passing, and
independently checked two named risks: no forward-declarations of NodeConfig
anywhere in McsEsp32 that the alias conversion could break, and no
platformio.ini reference to the deleted NodeConfig.cpp. 1 Minor, non-blocking:
flagged that the inline-everything-in-header pattern is about to repeat 7
more times per the plan — worth watching for header bloat on heavier classes,
not a defect here.

Task 2 (Template ConfigStore and CommissioningSession): complete (commit
3506f8b..2a7ed4c [^ F], review clean — Approved, zero blocking findings.
Reviewer independently confirmed the core correctness requirement (existing
callers compile unchanged against the aliased names) by inspecting
FakeConfigStore.h and all three production consumers (WebFormCommissioningAdapter,
SerialCommissioningAdapter, NvsConfigStore), not just the implementer's claim.
Full native suite 46/46 passing per implementer report. 2 Minor, non-blocking:
inline-header duplication across N instantiations is plan-mandated, not a
defect; new N=4 suite doesn't cover a failed-save case the N=12 suite likely
has (acceptable — focused N-sensitive coverage was the brief's intent).

Task 3 (Template WebFormSubmission and WebFormCommissioningAdapter): complete
(commit 8305f66..95e46c9 [^ F], review clean — Approved, zero blocking
findings. Reviewer confirmed the explicit include-retention constraint
(WebFormSubmission.h keeps #include "NodeConfig.h" for test_setup_form_renderer's
transitive dependency) was honored, confirmed alias preservation, and confirmed
the existing test_web_form_commissioning_adapter suite is untouched. 1 Minor,
non-blocking: one new test asserts a compile-time array-size invariant rather
than runtime behavior — plan-mandated (copied from the brief), not a defect.

Task 4 (SetupFormRenderer::render() becomes a function template): complete
(commit 5b5c040..a685311 [^ F], review clean — Approved, zero findings.
Reviewer confirmed the two-line edit is exact and minimal, confirmed
escapeHtml/renderIdOptions/renderNetworkOptions/renderChannelField/kStyle
are untouched, confirmed the test append is byte-for-byte per the brief,
and checked that WebFormSubmission4 (from Task 3) exists for the template
to bind against.

Task 5 (BootModeSelector::select() becomes a function template): complete
(commit f7729ca..e57da17 [^ F], review clean — Approved, zero findings.
Reviewer confirmed the append-only test edit, confirmed BootModeSelector
stays non-template with only select() templated, and checked that NodeConfig4
(from Task 1) was visible to the test file without needing a new include.
1 Minor, non-blocking: no "valid 4-channel config selects Normal" positive
case added, but the brief specified exactly the two cases that were added.

Task 6 (Template SerialCommissioningAdapter): complete (commit b6b5296..a00b57c
[^ F], review clean — Approved, code zero findings. 1 Important finding, but
report-accuracy only (not code): the implementer's report claimed "44 test
suites passed" for the full-suite run, internally inconsistent with its own
GREEN-phase section already showing this task's new suite passing. Controller
independently re-ran pio test -e native and confirmed the true count: 48
suites passed, 0 failed, 0 errored (44 pre-existing + 4 new across Tasks
1/2/3/6). Corrected the report file directly (no fix subagent needed, since
code/tests were never wrong) rather than re-dispatching for a non-code issue.

Task 7 (Template NvsConfigStore, hardware adapter, no native test): complete
(commit ad4e1a8..084dd2f [^ r], review clean — Approved, zero findings.
Reviewer confirmed NVS namespace/key strings and the putString() comment are
byte-for-byte unchanged (hard constraint satisfied), confirmed the explicit
template instantiations for N=12/N=4, and checked that BasicConfigStore<N>
(Task 2's dependency) exists and matches. Native suite 48/48, esp32dev build
SUCCESS per implementer report.

Task 8 (Template CaptivePortalServer, hardware adapter, no native test):
complete (commit 2f1aea2..037de16 [^ r], review clean — Approved, zero
blocking findings. This was the last task touching shared McsEsp32 code
before the new environment/composition-root task; reviewer confirmed the
line-for-line match to the brief, confirmed both aliases exist, and checked
unused-template-instantiation flash bloat risk (unreferenced <4> code is
expected to be garbage-collected by the toolchain's default linker flags).
Native suite 48/48, esp32dev build SUCCESS at 17.4%/83.8% per implementer
report. 1 Minor, non-blocking: one now-unused #include inherited verbatim
from the brief's own prescribed code, not introduced by the implementer.

ALL 8 TEMPLATING TASKS (1-8) COMPLETE. Proceeding to Task 9 (new esp32node4
environment + placeholder composition root).

Task 9 (New esp32node4 environment + placeholder composition root): complete
(commit 8ce2924..d4c1f23 [@ F], review clean — Approved, zero blocking
findings. Reviewer directly compared the new main.cpp against src/esp32/main.cpp
line-by-line, confirmed byte-for-byte match to the brief for both platformio.ini's
new block and the new main.cpp, confirmed zero turnout/LED/button/WiFi/MQTT
scope creep, and confirmed global declaration ordering matches the established
NvsBootstrap-first pattern. All three builds (esp32node4 new/SUCCESS,
esp32dev/SUCCESS unaffected, megaatmega2560/SUCCESS unaffected) per implementer
report. Controller independently confirmed git status clean and .pio/ properly
gitignored after the implementer's reported transient registry-fetch workaround
(no trace of it in the commit).
