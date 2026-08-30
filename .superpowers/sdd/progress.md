# Subagent-Driven Development Progress

Plan: docs/superpowers/plans/2026-08-29-esp32-composition-root.md

Started: 2026-08-29

Prior plan (sub-project #7a, ESP32 single-button toggle turnout control) is
complete and merged to main; this ledger starts fresh for sub-project #7b
(ESP32 composition root).

NOTE: this file is git-tracked and lives on a branch lineage with old,
unrelated prior-plan ledger content checked into history. A plain
`git checkout -- .superpowers/sdd/progress.md` will silently revert it to
that stale content. Do not run `git checkout` on this file for any reason;
every dispatched subagent in this branch is told so explicitly.

Baseline: worktree created from origin/main at 5e711d8 (includes the design
spec and this plan file). `pio test -e native` confirmed clean at baseline:
all 28 suites PASSED, no failures.

## Tasks

Task 1 (ToggleTurnoutStation): complete (commits 5e711d8..a2177f3 [^ F], review
clean — Approved, zero Critical/Important. 1 trivial Minor: implementer's
report had inaccurate self-reported line counts, not a code defect. Reviewer
independently verified constructor signature, member init order vs.
declaration order (no -Wreorder risk), and each collaborator's argument
order against the brief's documented signatures.)
Task 2 (JmriTurnoutCommandAdapter retained fix): complete (commits
a2177f3..20dea41 [^ B], review clean — Approved, zero findings at any
severity. Reviewer confirmed diff is exactly the two specified one-line
edits, test_jmri_turnout_wiring untouched, TDD RED->GREEN evidence genuine.)
Task 3 (src/esp32/main.cpp composition root): complete (commits
20dea41..425bbaf [! F], review clean — Approved, zero findings at any
severity. One pre-approved deviation from the plan's literal code: the
brief's global `ArduinoClock clock;` does not compile on esp32dev (collides
with the ESP32 core's libc `clock_t clock(void)` from <time.h> — same class
of collision hit in sub-project #5's throwaway wiring, renamed there to
`clockAdapter`; not an issue on megaatmega2560, where avr-libc has no such
symbol). Implementer renamed the global to `systemClock` throughout (26
call sites). Reviewer independently grepped the whole file for `[Cc]lock`,
confirmed the rename is total/consistent with zero leftover bare `clock`
references and zero argument-order/wiring changes as a side effect, and
independently traced global initialization order end-to-end confirming no
forward references. esp32dev SUCCESS (RAM 16.5%, Flash 77.1%),
megaatmega2560 SUCCESS unchanged, native 29/29 suites pass.

## All 3 tasks complete — proceeding to final whole-branch review.

## Final whole-branch review (opus): "Ready to merge: With fixes"
Reviewer independently traced electrical polarity against docs/button-wiring.md
and docs/led-wiring.md entry-by-entry (all 12 LED GPIOs, both matrix GPIO
arrays), traced the feedback-drain loop for starvation risk, traced global
init order for forward references, and re-verified the clock->systemClock
rename from scratch. 1 Critical: global constructors (CommissioningSession,
runningConfig) read NVS before Arduino-ESP32's initArduino()/nvs_flash_init()
ever runs (global ctors run before app_main() on this toolchain), so
commissioned config would never take effect on a real boot -- invisible to
both pio run -e esp32dev and pio test -e native. 1 Important: mqttLink.poll()
ran even before WiFi connected, blocking loop() (including the bench-serial
commissioning console) on a synchronous connect attempt. 4 Minor: ESP.restart()
could cut off the commissioning reply (no Serial.flush()); queued MQTT
feedback could apply stale after a reconnect (deferred -- would touch
JmriFeedbackSource); LedPairDriver writes GPIO before pinMode() during
static init (informational, pre-existing from #4); CLAUDE.md doc drift
(test count, missing ToggleTurnoutStation mention).

Controller independently verified the Critical finding before dispatching a
fix: read the installed framework-arduinoespressif32 package directly
(cores/esp32/esp32-hal-misc.c confirms nvs_flash_init() lives inside
initArduino(); cores/esp32/main.cpp confirms app_main() calls initArduino()
then spawns the task that calls setup()) -- consistent with standard
ESP-IDF startup (global ctors before app_main()). Confirmed real, not a
false positive.

## Fix pass (commit bdc3b22 [! B]): fixed Critical + Important + 2 of 4
Minor (Serial.flush(), boot banner). Explicitly deferred (not fixed):
stale-feedback-on-reconnect (would require modifying already-tested
JmriFeedbackSource from an earlier sub-project) and the LedPairDriver
pre-pinMode() write (informational only, no action requested). Re-review
(sonnet, focused fix-verification): Approved, zero findings -- confirmed
NvsBootstrap is the first global declared (before systemClock/uartPort/
configStore/commissioningSession/runningConfig), #include <nvs_flash.h>
present, mqttLink.poll() correctly nested inside wifiLink.connected(),
Serial.flush() correctly placed, boot banner reports configValid state
correctly, zero incidental changes, JmriFeedbackSource/LedPairDriver/
CLAUDE.md untouched by the fix commit (controller handled CLAUDE.md and
the design spec's pre-deployment checklist directly, separately from
the fix subagent). esp32dev SUCCESS (RAM 16.5%, Flash 77.2%),
megaatmega2560 SUCCESS unchanged, native 29/29.

## PLAN COMPLETE (with one review-driven fix pass) -- all builds/tests green.
Final state: commits 5e711d8..bdc3b22 (a2177f3 Task1, 20dea41 Task2,
425bbaf Task3, bdc3b22 fix), plus controller doc commits for CLAUDE.md and
the design spec's pre-deployment checklist. Proceeding to
finishing-a-development-branch.
