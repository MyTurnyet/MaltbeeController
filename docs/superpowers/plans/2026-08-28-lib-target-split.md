# Split lib/McsCore by Target Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reorganize `lib/McsCore`'s ~30 files into three PlatformIO private
libraries (`McsCore`, `McsLoconet`, `McsEsp32`), grouped by which of this
project's three environments (`native`, `megaatmega2560`, `esp32dev`) can
actually compile them, so "which environment sees this file" is answered by
directory placement instead of four different preprocessor-guard idioms.
Zero behavior change.

**Architecture:** `git mv` each file to its new library root (preserves git
history), fix the handful of include lines and guard directives that need
it, update `platformio.ini` so each environment's `lib_deps`/`lib_ignore`
matches the new structure, and verify all three environments build/test
green with no new test cases needed.

**Tech Stack:** C++17, PlatformIO.

## Global Constraints

- No behavior change anywhere. Every existing test's assertions stay
  byte-identical — only file paths and include/guard lines move.
- `git mv`, not delete-and-recreate, for every file move — preserves git
  blame/history.
- Within a library, includes stay relative (`../ports/X.h`) exactly as
  today. A file needing a header from a *different* library it depends on
  uses a rooted include (`"ports/X.h"`) instead — this is the only kind of
  include line this plan changes, and only for the five lines identified
  below (all in files moving to `McsLoconet`; `McsEsp32`'s files are fully
  self-contained and need no include changes).
- Guard simplification is exact, not approximate: two `#ifdef ESP32` files
  and two `#if defined(ARDUINO) && !defined(ESP32)` files become plain
  `#ifdef ARDUINO`; four `#if !defined(__AVR__)` files lose their guard
  entirely (both the opening `#if`/blank line and the closing blank/`#endif`
  line at the end of the file). No other file's guard changes.
- Do not touch `src/mega/main.cpp` or `src/esp32/main.cpp` — both already
  use rooted includes that resolve correctly against the new library
  layout with no changes (confirmed against their actual current content
  during design). If either needs a change to build, stop and treat that as
  a wrong assumption to fix, not something to route around.
- Do not touch any file under `test/` — every native test file and every
  `test/support/` fake already uses rooted includes (`"domain/X.h"`,
  `"ports/X.h"`, `"adapters/X.h"`, `"support/X.h"`) that resolve correctly
  against the new layout unchanged (confirmed during design by reading each
  one). If a test fails to compile after a move, that means an assumption
  here was wrong — stop and report it, don't paper over it by editing the
  test.
- Commit messages use this project's Arlo's Commit Notation (ACN). Both
  tasks below are pure mechanical reorganization with zero behavior change
  and full verification (all three environments green) — that's `. r`
  (provably safe refactor), not `F`/`B`, since nothing about program
  behavior is intended to change.

---

### Task 1: Split `lib/McsLoconet` out of `lib/McsCore`

**Files:**
- Move (`git mv`, no further edit needed):
  - `lib/McsCore/src/ports/LocoNetTransport.h` → `lib/McsLoconet/src/ports/LocoNetTransport.h`
  - `lib/McsCore/src/adapters/LocoNetFeedbackDecoder.cpp` → `lib/McsLoconet/src/adapters/LocoNetFeedbackDecoder.cpp`
  - `lib/McsCore/src/adapters/MrrwaLocoNetFeedbackSource.h` → `lib/McsLoconet/src/adapters/MrrwaLocoNetFeedbackSource.h`
  - `lib/McsCore/src/adapters/MrrwaLocoNetSwitchDriver.h` → `lib/McsLoconet/src/adapters/MrrwaLocoNetSwitchDriver.h`
  - `lib/McsCore/src/adapters/MrrwaLocoNetTurnoutAdapter.cpp` → `lib/McsLoconet/src/adapters/MrrwaLocoNetTurnoutAdapter.cpp`
  - `lib/McsCore/src/adapters/PulsingLocoNetTransport.cpp` → `lib/McsLoconet/src/adapters/PulsingLocoNetTransport.cpp`
- Move + fix a cross-library include:
  - `lib/McsCore/src/ports/LocoNetFeedbackSource.h` → `lib/McsLoconet/src/ports/LocoNetFeedbackSource.h`
  - `lib/McsCore/src/ports/LocoNetSwitchDriver.h` → `lib/McsLoconet/src/ports/LocoNetSwitchDriver.h`
  - `lib/McsCore/src/adapters/LocoNetFeedbackDecoder.h` → `lib/McsLoconet/src/adapters/LocoNetFeedbackDecoder.h`
  - `lib/McsCore/src/adapters/MrrwaLocoNetTurnoutAdapter.h` → `lib/McsLoconet/src/adapters/MrrwaLocoNetTurnoutAdapter.h`
  - `lib/McsCore/src/adapters/PulsingLocoNetTransport.h` → `lib/McsLoconet/src/adapters/PulsingLocoNetTransport.h`
- Move + simplify a guard:
  - `lib/McsCore/src/adapters/MrrwaLocoNetFeedbackSource.cpp` → `lib/McsLoconet/src/adapters/MrrwaLocoNetFeedbackSource.cpp`
  - `lib/McsCore/src/adapters/MrrwaLocoNetSwitchDriver.cpp` → `lib/McsLoconet/src/adapters/MrrwaLocoNetSwitchDriver.cpp`
- Modify: `platformio.ini`

**Interfaces:**
- Consumes: `McsCore`'s `domain/Turnout.h` (for `TurnoutPosition`) and
  `ports/TurnoutCommandPort.h`/`ports/Clock.h` — all staying in `McsCore`,
  referenced via rooted includes after this task.
- Produces: nothing new — every class/function signature in every moved
  file is unchanged. This task only changes where these files physically
  live and how three of them spell an include/guard.

- [ ] **Step 1: Create the new library's directory structure**

```bash
mkdir -p lib/McsLoconet/src/ports lib/McsLoconet/src/adapters
```

- [ ] **Step 2: Move the six files that need no further edit**

```bash
git mv lib/McsCore/src/ports/LocoNetTransport.h lib/McsLoconet/src/ports/LocoNetTransport.h
git mv lib/McsCore/src/adapters/LocoNetFeedbackDecoder.cpp lib/McsLoconet/src/adapters/LocoNetFeedbackDecoder.cpp
git mv lib/McsCore/src/adapters/MrrwaLocoNetFeedbackSource.h lib/McsLoconet/src/adapters/MrrwaLocoNetFeedbackSource.h
git mv lib/McsCore/src/adapters/MrrwaLocoNetSwitchDriver.h lib/McsLoconet/src/adapters/MrrwaLocoNetSwitchDriver.h
git mv lib/McsCore/src/adapters/MrrwaLocoNetTurnoutAdapter.cpp lib/McsLoconet/src/adapters/MrrwaLocoNetTurnoutAdapter.cpp
git mv lib/McsCore/src/adapters/PulsingLocoNetTransport.cpp lib/McsLoconet/src/adapters/PulsingLocoNetTransport.cpp
```

- [ ] **Step 3: Move the five files needing a cross-library include fix, then fix each**

```bash
git mv lib/McsCore/src/ports/LocoNetFeedbackSource.h lib/McsLoconet/src/ports/LocoNetFeedbackSource.h
git mv lib/McsCore/src/ports/LocoNetSwitchDriver.h lib/McsLoconet/src/ports/LocoNetSwitchDriver.h
git mv lib/McsCore/src/adapters/LocoNetFeedbackDecoder.h lib/McsLoconet/src/adapters/LocoNetFeedbackDecoder.h
git mv lib/McsCore/src/adapters/MrrwaLocoNetTurnoutAdapter.h lib/McsLoconet/src/adapters/MrrwaLocoNetTurnoutAdapter.h
git mv lib/McsCore/src/adapters/PulsingLocoNetTransport.h lib/McsLoconet/src/adapters/PulsingLocoNetTransport.h
```

In `lib/McsLoconet/src/ports/LocoNetFeedbackSource.h`, change:
```cpp
#include "../domain/Turnout.h"
```
to:
```cpp
#include "domain/Turnout.h"
```

In `lib/McsLoconet/src/ports/LocoNetSwitchDriver.h`, change:
```cpp
#include "../domain/Turnout.h"
```
to:
```cpp
#include "domain/Turnout.h"
```

In `lib/McsLoconet/src/adapters/LocoNetFeedbackDecoder.h`, change:
```cpp
#include "../ports/TurnoutCommandPort.h"
```
to:
```cpp
#include "ports/TurnoutCommandPort.h"
```

In `lib/McsLoconet/src/adapters/MrrwaLocoNetTurnoutAdapter.h`, change:
```cpp
#include "../ports/TurnoutCommandPort.h"
```
to:
```cpp
#include "ports/TurnoutCommandPort.h"
```

In `lib/McsLoconet/src/adapters/PulsingLocoNetTransport.h`, change:
```cpp
#include "../ports/Clock.h"
```
to:
```cpp
#include "ports/Clock.h"
```

- [ ] **Step 4: Move the two files needing a guard simplification, then simplify each**

```bash
git mv lib/McsCore/src/adapters/MrrwaLocoNetFeedbackSource.cpp lib/McsLoconet/src/adapters/MrrwaLocoNetFeedbackSource.cpp
git mv lib/McsCore/src/adapters/MrrwaLocoNetSwitchDriver.cpp lib/McsLoconet/src/adapters/MrrwaLocoNetSwitchDriver.cpp
```

In `lib/McsLoconet/src/adapters/MrrwaLocoNetFeedbackSource.cpp`, change line 1 from:
```cpp
#if defined(ARDUINO) && !defined(ESP32)
```
to:
```cpp
#ifdef ARDUINO
```

In `lib/McsLoconet/src/adapters/MrrwaLocoNetSwitchDriver.cpp`, change line 1 from:
```cpp
#if defined(ARDUINO) && !defined(ESP32)
```
to:
```cpp
#ifdef ARDUINO
```

- [ ] **Step 5: Update `platformio.ini`'s `[env:native]` to explicitly depend on both libraries**

Change:
```ini
[env:native]
platform = native
test_framework = custom
test_build_src = false
build_flags = -std=c++17
```
to:
```ini
[env:native]
platform = native
test_framework = custom
test_build_src = false
build_flags = -std=c++17
lib_deps =
    McsCore
    McsLoconet
```

- [ ] **Step 6: Run the full native suite**

Run: `pio test -e native`
Expected: PASS — all 18 existing suites green, unchanged (this includes
`test_mrrwa_loconet_turnout_adapter`, `test_pulsing_loconet_transport`, and
`test_loconet_feedback_decoder`, which are the three suites this task's
moved files actually affect).

- [ ] **Step 7: Build the Mega target**

Run: `pio run -e megaatmega2560`
Expected: SUCCESS, same RAM/Flash usage as before this task (9.9%/2.7%) —
confirms `src/mega/main.cpp`'s rooted includes still resolve against both
`McsCore` and the new `McsLoconet` via LDF auto-discovery, with no
`platformio.ini` change needed for this environment.

- [ ] **Step 8: Build the ESP32 target**

Run: `pio run -e esp32dev`
Expected: SUCCESS. The "Dependency Graph" in the output should show only
`PubSubClient` and `McsCore` (not `McsLoconet` — it isn't in this
environment's `lib_deps` and nothing in `src/esp32/main.cpp` references it),
confirming the LocoNet files are no longer part of the ESP32 build at all
now that they've physically moved out of `McsCore`.

- [ ] **Step 9: Commit**

```bash
git add lib/McsLoconet lib/McsCore platformio.ini
git commit -m "$(cat <<'EOF'
. r Split lib/McsLoconet out of lib/McsCore

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01V5UwJaTXi2GaVyh96vtKBt
EOF
)"
```

(Use `git add lib/McsLoconet lib/McsCore platformio.ini` rather than `-A` —
this stages the moves in both the old and new locations plus the
`platformio.ini` edit, nothing else. Run `git status` first if unsure what's
staged.)

---

### Task 2: Split `lib/McsEsp32` out of `lib/McsCore`

**Files:**
- Move (`git mv`, no further edit needed):
  - `lib/McsCore/src/domain/CommandLineParser.h` → `lib/McsEsp32/src/domain/CommandLineParser.h`
  - `lib/McsCore/src/domain/CommissioningSession.h` → `lib/McsEsp32/src/domain/CommissioningSession.h`
  - `lib/McsCore/src/domain/NodeConfig.h` → `lib/McsEsp32/src/domain/NodeConfig.h`
  - `lib/McsCore/src/domain/ParsedCommand.h` → `lib/McsEsp32/src/domain/ParsedCommand.h`
  - `lib/McsCore/src/ports/ConfigStore.h` → `lib/McsEsp32/src/ports/ConfigStore.h`
  - `lib/McsCore/src/ports/UartPort.h` → `lib/McsEsp32/src/ports/UartPort.h`
  - `lib/McsCore/src/adapters/EspUartPort.h` → `lib/McsEsp32/src/adapters/EspUartPort.h`
  - `lib/McsCore/src/adapters/NvsConfigStore.h` → `lib/McsEsp32/src/adapters/NvsConfigStore.h`
  - `lib/McsCore/src/adapters/SerialCommissioningAdapter.h` → `lib/McsEsp32/src/adapters/SerialCommissioningAdapter.h`
- Move + simplify/remove a guard:
  - `lib/McsCore/src/domain/CommandLineParser.cpp` → `lib/McsEsp32/src/domain/CommandLineParser.cpp`
  - `lib/McsCore/src/domain/CommissioningSession.cpp` → `lib/McsEsp32/src/domain/CommissioningSession.cpp`
  - `lib/McsCore/src/domain/NodeConfig.cpp` → `lib/McsEsp32/src/domain/NodeConfig.cpp`
  - `lib/McsCore/src/adapters/EspUartPort.cpp` → `lib/McsEsp32/src/adapters/EspUartPort.cpp`
  - `lib/McsCore/src/adapters/NvsConfigStore.cpp` → `lib/McsEsp32/src/adapters/NvsConfigStore.cpp`
  - `lib/McsCore/src/adapters/SerialCommissioningAdapter.cpp` → `lib/McsEsp32/src/adapters/SerialCommissioningAdapter.cpp`
- Modify: `platformio.ini`

**Interfaces:**
- Consumes: nothing from `McsCore` or `McsLoconet` — every file in this
  library is self-contained (confirmed during design by reading each file's
  includes: `NodeConfig` has no dependencies; `CommissioningSession`/
  `ConfigStore`/`SerialCommissioningAdapter`/`NvsConfigStore`/`EspUartPort`
  only depend on other files also moving to `McsEsp32`).
- Produces: nothing new — every signature in every moved file is unchanged.

- [ ] **Step 1: Create the new library's directory structure**

```bash
mkdir -p lib/McsEsp32/src/domain lib/McsEsp32/src/ports lib/McsEsp32/src/adapters
```

- [ ] **Step 2: Move the nine files that need no further edit**

```bash
git mv lib/McsCore/src/domain/CommandLineParser.h lib/McsEsp32/src/domain/CommandLineParser.h
git mv lib/McsCore/src/domain/CommissioningSession.h lib/McsEsp32/src/domain/CommissioningSession.h
git mv lib/McsCore/src/domain/NodeConfig.h lib/McsEsp32/src/domain/NodeConfig.h
git mv lib/McsCore/src/domain/ParsedCommand.h lib/McsEsp32/src/domain/ParsedCommand.h
git mv lib/McsCore/src/ports/ConfigStore.h lib/McsEsp32/src/ports/ConfigStore.h
git mv lib/McsCore/src/ports/UartPort.h lib/McsEsp32/src/ports/UartPort.h
git mv lib/McsCore/src/adapters/EspUartPort.h lib/McsEsp32/src/adapters/EspUartPort.h
git mv lib/McsCore/src/adapters/NvsConfigStore.h lib/McsEsp32/src/adapters/NvsConfigStore.h
git mv lib/McsCore/src/adapters/SerialCommissioningAdapter.h lib/McsEsp32/src/adapters/SerialCommissioningAdapter.h
```

- [ ] **Step 3: Move and simplify the two `#ifdef ESP32` hardware shims**

```bash
git mv lib/McsCore/src/adapters/EspUartPort.cpp lib/McsEsp32/src/adapters/EspUartPort.cpp
git mv lib/McsCore/src/adapters/NvsConfigStore.cpp lib/McsEsp32/src/adapters/NvsConfigStore.cpp
```

In `lib/McsEsp32/src/adapters/EspUartPort.cpp`, change line 1 from:
```cpp
#ifdef ESP32
```
to:
```cpp
#ifdef ARDUINO
```

In `lib/McsEsp32/src/adapters/NvsConfigStore.cpp`, change line 1 from:
```cpp
#ifdef ESP32
```
to:
```cpp
#ifdef ARDUINO
```

- [ ] **Step 4: Move and remove the guard from the four pure-logic files**

```bash
git mv lib/McsCore/src/domain/CommandLineParser.cpp lib/McsEsp32/src/domain/CommandLineParser.cpp
git mv lib/McsCore/src/domain/CommissioningSession.cpp lib/McsEsp32/src/domain/CommissioningSession.cpp
git mv lib/McsCore/src/domain/NodeConfig.cpp lib/McsEsp32/src/domain/NodeConfig.cpp
git mv lib/McsCore/src/adapters/SerialCommissioningAdapter.cpp lib/McsEsp32/src/adapters/SerialCommissioningAdapter.cpp
```

In `lib/McsEsp32/src/domain/CommandLineParser.cpp`, remove the guard —
change the file's first three lines from:
```cpp
#if !defined(__AVR__)

#include "CommandLineParser.h"
```
to:
```cpp
#include "CommandLineParser.h"
```
and remove the trailing guard — change the file's last two lines from:
```cpp

#endif
```
to nothing (delete both lines, so the file ends with the closing `}` of
`CommandLineParser::parse`).

In `lib/McsEsp32/src/domain/CommissioningSession.cpp`, remove the guard —
change the file's first three lines from:
```cpp
#if !defined(__AVR__)

#include "CommissioningSession.h"
```
to:
```cpp
#include "CommissioningSession.h"
```
and remove the trailing guard — change the file's last two lines from:
```cpp

#endif
```
to nothing (delete both lines, so the file ends with the closing `}` of
`CommissioningSession::rebootRequested`).

In `lib/McsEsp32/src/domain/NodeConfig.cpp`, remove the guard — change the
file's first three lines from:
```cpp
#if !defined(__AVR__)

#include "NodeConfig.h"
```
to:
```cpp
#include "NodeConfig.h"
```
and remove the trailing guard — change the file's last two lines from:
```cpp

#endif
```
to nothing (delete both lines, so the file ends with the closing `}` of
`NodeConfig::validate`).

In `lib/McsEsp32/src/adapters/SerialCommissioningAdapter.cpp`, remove the
guard — change the file's first three lines from:
```cpp
#if !defined(__AVR__)

#include "SerialCommissioningAdapter.h"
```
to:
```cpp
#include "SerialCommissioningAdapter.h"
```
and remove the trailing guard — change the file's last two lines from:
```cpp

#endif
```
to nothing (delete both lines, so the file ends with the closing `}` of
`SerialCommissioningAdapter::rebootRequested`).

- [ ] **Step 5: Update `platformio.ini`: finalize all three environments**

Change `[env:megaatmega2560]` from:
```ini
[env:megaatmega2560]
platform = atmelavr
board = megaatmega2560
framework = arduino
monitor_speed = 115200
lib_ldf_mode = deep+
lib_deps = https://github.com/mrrwa/LocoNet.git#1.1.13
build_src_filter = -<*> +<mega/*>
```
to:
```ini
[env:megaatmega2560]
platform = atmelavr
board = megaatmega2560
framework = arduino
monitor_speed = 115200
lib_ldf_mode = deep+
lib_deps = https://github.com/mrrwa/LocoNet.git#1.1.13
lib_ignore = McsEsp32
build_src_filter = -<*> +<mega/*>
```

Change `[env:esp32dev]` from:
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_ldf_mode = deep+
lib_deps =
    knolleary/PubSubClient@^2.8
    McsCore
build_src_filter = -<*> +<esp32/*>
```
to:
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_ldf_mode = deep+
lib_deps =
    knolleary/PubSubClient@^2.8
    McsCore
    McsEsp32
lib_ignore = McsLoconet
build_src_filter = -<*> +<esp32/*>
```

Change `[env:native]` from:
```ini
[env:native]
platform = native
test_framework = custom
test_build_src = false
build_flags = -std=c++17
lib_deps =
    McsCore
    McsLoconet
```
to:
```ini
[env:native]
platform = native
test_framework = custom
test_build_src = false
build_flags = -std=c++17
lib_deps =
    McsCore
    McsLoconet
    McsEsp32
```

- [ ] **Step 6: Run the full native suite**

Run: `pio test -e native`
Expected: PASS — all 18 existing suites green, unchanged (this includes
`test_node_config`, `test_command_line_parser`,
`test_commissioning_session`, and `test_serial_commissioning_adapter`,
which are the four suites this task's moved files actually affect).

- [ ] **Step 7: Build the Mega target**

Run: `pio run -e megaatmega2560`
Expected: SUCCESS, same RAM/Flash usage as before this task (9.9%/2.7%) —
confirms the new `lib_ignore = McsEsp32` doesn't remove anything Mega
actually needed (it never referenced `McsEsp32`'s content), and that
`McsCore`/`McsLoconet` (from Task 1) still resolve correctly.

- [ ] **Step 8: Build the ESP32 target**

Run: `pio run -e esp32dev`
Expected: SUCCESS. The "Dependency Graph" in the output should now show
`PubSubClient`, `McsCore`, and `McsEsp32` (not `McsLoconet`, per the new
`lib_ignore`), confirming the explicit `lib_deps` pulls in exactly the two
libraries this environment needs, even with `src/esp32/main.cpp` still an
empty stub.

- [ ] **Step 9: Commit**

```bash
git add lib/McsEsp32 lib/McsCore platformio.ini
git commit -m "$(cat <<'EOF'
. r Split lib/McsEsp32 out of lib/McsCore

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01V5UwJaTXi2GaVyh96vtKBt
EOF
)"
```

---

## Definition of Done for this Plan

- [ ] `pio test -e native` passes, all 18 suites, unchanged assertions.
- [ ] `pio run -e megaatmega2560` builds cleanly, same RAM/Flash usage as
      before this plan (9.9%/2.7%).
- [ ] `pio run -e esp32dev` builds cleanly, dependency graph shows
      `McsCore` + `McsEsp32` only (not `McsLoconet`).
- [ ] `lib/McsCore/src/` contains no LocoNet or ESP32-specific files —
      only the portable set listed in Task 1/2's "no further edit" and
      "unchanged" file lists, plus whatever was never touched by either
      task (`Button`, `FixedString32`, `Indicator`, `Route`,
      `RouteService`, `Turnout`, `TurnoutCollection`, `TurnoutIndicator`,
      `TurnoutService`, `Clock`, `DigitalInput`, `DigitalOutput`,
      `TurnoutCommandPort`, `TurnoutControl`, `ArduinoClock`,
      `ArduinoDigitalInput`, `ArduinoDigitalOutput`, `NullTurnoutCommandPort`,
      `TurnoutStation`).
- [ ] Exactly one preprocessor-guard idiom remains across all three
      libraries: `#ifdef ARDUINO`, on exactly eight files —
      `ArduinoClock.cpp`, `ArduinoDigitalInput.cpp`, `ArduinoDigitalOutput.cpp`,
      `TurnoutStation.cpp` in `McsCore` (unchanged by this plan, already
      this guard); `MrrwaLocoNetFeedbackSource.cpp`,
      `MrrwaLocoNetSwitchDriver.cpp` in `McsLoconet` (simplified by Task 1);
      `EspUartPort.cpp`, `NvsConfigStore.cpp` in `McsEsp32` (simplified by
      Task 2). Verify with:
      `grep -rl "^#if" lib/*/src --include=*.cpp` — should list exactly
      these eight paths, each with `#ifdef ARDUINO` as its guard line.
- [ ] Two commits, both `. r`, one per task above.
- [ ] `git log --follow` on any moved file (e.g.
      `git log --follow lib/McsEsp32/src/domain/NodeConfig.cpp`) shows its
      full history from before the move, confirming `git mv` preserved
      blame.
