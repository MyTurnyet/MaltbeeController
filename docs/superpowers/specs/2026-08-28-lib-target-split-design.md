# Split lib/McsCore by Target — Design

## Status

Design approved 2026-08-28. Not yet implemented.

## Context

The final whole-branch review of the ESP32 NodeConfig/commissioning slice
(`docs/superpowers/specs/2026-08-28-esp32-node-config-commissioning-design.md`,
"Post-implementation amendments") found that `lib/McsCore` now carries four
different preprocessor-guard idioms (`#ifdef ARDUINO`, `#ifdef ESP32`,
`#if defined(ARDUINO) && !defined(ESP32)`, `#if !defined(__AVR__)`), all
doing the same job — deciding which of this project's three PlatformIO
environments (`native`, `megaatmega2560`, `esp32dev`) compiles a given file —
because PlatformIO's Library Dependency Finder compiles *every* `.cpp` file
in a "used" library, not just files reachable from a given environment's
`main.cpp`. That review recommended resolving this structurally, as the
first task of the next ESP32 slice, before more ESP32-only files (planned
for `WiFiLink`, `MqttLink`, `TopicScheme`, `PayloadCodec`, JMRI adapters)
compound the pattern. This spec is that task.

The user has since confirmed (2026-08-28) the Mega 2560/LocoNet hardware
path is no longer a target for this project going forward — all future
firmware work targets the ESP32 38-pin board. This doesn't remove the need
for a clean `megaatmega2560` split (it must keep building; nothing has been
asked to be torn out), but it does mean nothing here should invest in the
Mega path beyond "keep it exactly as functional as it is today."

## Goal

Zero behavior change. Reorganize `lib/McsCore`'s ~30 files into three
PlatformIO private libraries, grouped by which environment(s) can actually
compile them, so that "which environment sees this file" is answered by
directory placement and `platformio.ini`, not by a human remembering the
right macro. Every existing test keeps passing, unchanged, from a new file
path.

## Design

### Library membership

**`lib/McsCore/`** — portable; compiles under all three environments
(`native`, `megaatmega2560`, `esp32dev`):

| Kind | Files |
|---|---|
| domain | `Button`, `FixedString32`, `Indicator`, `Route`, `RouteService`, `Turnout`, `TurnoutCollection`, `TurnoutIndicator`, `TurnoutService` |
| ports | `Clock`, `DigitalInput`, `DigitalOutput`, `TurnoutCommandPort` |
| application | `TurnoutControl` |
| adapters | `ArduinoClock`, `ArduinoDigitalInput`, `ArduinoDigitalOutput`, `NullTurnoutCommandPort`, `TurnoutStation` |

`TurnoutStation` already physically lives in `lib/McsCore/src/adapters/`
today, alongside the LocoNet files that are about to move out — it doesn't
move anywhere in this reorg, it simply **stays put** while its current
neighbors relocate to `McsLoconet`. Worth calling out explicitly because it
composes `Button`/`Indicator`/`Turnout`/`TurnoutIndicator`/`TurnoutControl`/
`ArduinoDigitalInput`/`ArduinoDigitalOutput` — every one of those staying in
`McsCore` — and has **no LocoNet dependency at all**, confirmed by reading
its actual includes (below). It's only ever sat next to the LocoNet files
because Mega's `main.cpp` is its one caller today, not because it needs to.

**`lib/McsLoconet/`** — compiles under `native` (pure-logic files only) and
`megaatmega2560`; never referenced by `esp32dev`:

| Kind | Files |
|---|---|
| ports | `LocoNetFeedbackSource`, `LocoNetSwitchDriver`, `LocoNetTransport` |
| adapters | `LocoNetFeedbackDecoder`, `MrrwaLocoNetFeedbackSource`, `MrrwaLocoNetSwitchDriver`, `MrrwaLocoNetTurnoutAdapter`, `PulsingLocoNetTransport` |

**`lib/McsEsp32/`** — compiles under `native` (pure-logic files only) and
`esp32dev`; never referenced by `megaatmega2560`:

| Kind | Files |
|---|---|
| domain | `CommandLineParser`, `CommissioningSession`, `NodeConfig`, `ParsedCommand` |
| ports | `ConfigStore`, `UartPort` |
| adapters | `EspUartPort`, `NvsConfigStore`, `SerialCommissioningAdapter` |

Full moves are `git mv` (preserve history), same relative path under the new
library root — e.g. `lib/McsCore/src/adapters/MrrwaLocoNetSwitchDriver.h` →
`lib/McsLoconet/src/adapters/MrrwaLocoNetSwitchDriver.h`.

### Cross-library includes

Rule: relative includes (`../ports/X.h`) for a neighbor within the *same*
library, same as today. Rooted includes (`"ports/X.h"`) for a header owned
by a *different* library this one depends on — extending the convention
`test/support/` fakes already use for depending on the library from outside.
PlatformIO's LDF shares one combined include path across every library it
pulls into a given environment's build, so a rooted include resolves
correctly once both libraries are in scope together (confirmed: `mega/main.cpp`
already includes rooted-style from both `McsCore` — e.g. `"adapters/ArduinoClock.h"`
— and what will become `McsLoconet` — e.g. `"adapters/MrrwaLocoNetTurnoutAdapter.h"`
— today, in the same file, proving LDF already treats this as one path).

Read against every candidate file's actual current includes (not assumed),
exactly five lines need this treatment, all in files moving to `McsLoconet`
and referencing a type staying in `McsCore`:

| File (post-move path) | Change |
|---|---|
| `lib/McsLoconet/src/ports/LocoNetSwitchDriver.h` | `#include "../domain/Turnout.h"` → `#include "domain/Turnout.h"` |
| `lib/McsLoconet/src/ports/LocoNetFeedbackSource.h` | `#include "../domain/Turnout.h"` → `#include "domain/Turnout.h"` |
| `lib/McsLoconet/src/adapters/MrrwaLocoNetTurnoutAdapter.h` | `#include "../ports/TurnoutCommandPort.h"` → `#include "ports/TurnoutCommandPort.h"` |
| `lib/McsLoconet/src/adapters/LocoNetFeedbackDecoder.h` | `#include "../ports/TurnoutCommandPort.h"` → `#include "ports/TurnoutCommandPort.h"` |
| `lib/McsLoconet/src/adapters/PulsingLocoNetTransport.h` | `#include "../ports/Clock.h"` → `#include "ports/Clock.h"` |

`McsEsp32`'s files are fully self-contained (`NodeConfig` has no
dependencies; `CommissioningSession`/`ConfigStore`/`SerialCommissioningAdapter`/
`NvsConfigStore`/`EspUartPort` only depend on other `McsEsp32` files) — zero
cross-library include changes needed there, confirmed by reading each file's
includes.

Every other include in every moved file stays exactly as it is today (same
relative depth, since each file's position *within* its new library root is
unchanged — only the library root itself moved).

### Guard simplification

This is the payoff. With directory placement now doing the job, every one of
the three non-original guard idioms either reverts to the project's original
`#ifdef ARDUINO` convention or is removed entirely:

| File | Guard today | Guard after |
|---|---|---|
| `McsLoconet/adapters/MrrwaLocoNetSwitchDriver.cpp` | `#if defined(ARDUINO) && !defined(ESP32)` | `#ifdef ARDUINO` |
| `McsLoconet/adapters/MrrwaLocoNetFeedbackSource.cpp` | `#if defined(ARDUINO) && !defined(ESP32)` | `#ifdef ARDUINO` |
| `McsEsp32/adapters/EspUartPort.cpp` | `#ifdef ESP32` | `#ifdef ARDUINO` |
| `McsEsp32/adapters/NvsConfigStore.cpp` | `#ifdef ESP32` | `#ifdef ARDUINO` |
| `McsEsp32/domain/NodeConfig.cpp` | `#if !defined(__AVR__)` | *(no guard)* |
| `McsEsp32/domain/CommandLineParser.cpp` | `#if !defined(__AVR__)` | *(no guard)* |
| `McsEsp32/domain/CommissioningSession.cpp` | `#if !defined(__AVR__)` | *(no guard)* |
| `McsEsp32/adapters/SerialCommissioningAdapter.cpp` | `#if !defined(__AVR__)` | *(no guard)* |

The narrowed `#ifdef ESP32`/`#if defined(ARDUINO) && !defined(ESP32)` guards
were only ever necessary because a single shared library put files that only
work on one Arduino-flavored target next to files that only work on the
other. Once `McsLoconet` is never referenced by `esp32dev` and `McsEsp32` is
never referenced by `megaatmega2560`, `#ifdef ARDUINO` alone is exactly as
safe as the narrowed forms, because "is this environment Arduino-flavored at
all" is now the only question left for the preprocessor to answer — "which
Arduino-flavored target" is answered by which library got linked in.
Similarly, the `#if !defined(__AVR__)` files never had any Arduino
dependency to begin with; their guard existed solely to keep them out of
`megaatmega2560`'s accidental whole-library compilation, which is now
structurally impossible. End state: one guard idiom (`#ifdef ARDUINO`),
seven files, all genuine hardware shims — the same shape the project had
before this ESP32 slice began.

### `platformio.ini` changes

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

- `megaatmega2560` needs no explicit `McsCore`/`McsLoconet` `lib_deps` entry:
  `src/mega/main.cpp` already includes rooted-style from both (confirmed
  above), so LDF auto-discovers both exactly as it does today for the single
  combined library. Only the defensive `lib_ignore` is new — cheap insurance
  against a future `mega/main.cpp` change accidentally reaching into
  `McsEsp32` and silently recreating this whole problem.
- `esp32dev` needs both `McsCore` and `McsEsp32` listed explicitly, same
  reason the `McsEsp32`-only entry was added in the prior slice: `src/esp32/main.cpp`
  is still an empty stub including nothing, so nothing triggers LDF
  auto-discovery there yet.
- **Correction from an earlier draft of this section:** this project's
  `native` env currently has *no* `-I` include flag at all
  (`build_flags = -std=c++17` only, confirmed by reading the current
  `platformio.ini`) — the sibling project's `CLAUDE.md` documents needing
  one for its own `FakeX.h`→`ports/X.h` chains, but that note doesn't apply
  here; this project's tests currently work because every native test file
  already includes something from `lib/McsCore` directly, which triggers
  LDF auto-discovery for the whole file. After the split, relying on that
  same auto-discovery to *also* transitively reach a second or third library
  (e.g. a test including only `McsLoconet`'s `MrrwaLocoNetTurnoutAdapter.h`,
  which itself now needs `McsCore`'s `TurnoutCommandPort.h`) means trusting
  `deep+` mode's cross-library transitive discovery — the exact class of
  PlatformIO behavior that caused the whole-library-compilation problem this
  spec exists to fix. Rather than trust that again, `native` gets explicit
  `lib_deps` for all three libraries, removing any doubt.

## Testing / Verification

Zero behavior change — no new test assertions. Verification is: every
existing test file's content is untouched (only the `.h`/`.cpp` files they
depend on moved), and all three environments build/test green afterward:

- `pio test -e native` — all 18 existing suites pass, unchanged.
- `pio run -e megaatmega2560` — succeeds, same RAM/Flash usage as before the
  split (9.9%/2.7%), confirming `McsCore` + `McsLoconet` are still both
  compiled in and nothing was dropped.
- `pio run -e esp32dev` — succeeds, and the build output's "Dependency
  Graph" should show `McsCore`, `McsEsp32`, and `PubSubClient`, confirming
  the explicit `lib_deps` still pulls both in even though `main.cpp` is
  empty.

## Out of scope

No behavior changes of any kind — this is purely a file-organization and
build-configuration change. Slice 2b's actual new content (`WiFiLink`,
`MqttLink`, `TopicScheme`, `PayloadCodec`, JMRI turnout command/feedback
adapters) is not part of this task; it lands in `lib/McsEsp32` afterward,
inheriting the now-simple `#ifdef ARDUINO` convention (or no guard, for pure
logic) from day one instead of rediscovering the whole-library-compilation
problem again.
